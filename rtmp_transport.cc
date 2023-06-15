//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_transport.h"
#include "autofree.h"
#include "logger.h"
#include "utils.h"

const uint32_t RTMP_DEFAULT_CHUNKSIZE = 128;
const uint32_t RTMP_DEFAULT_CACHE_BUFFER_SIZE = (32*1024);
const uint8_t RTMP_DEFAULT_CHUNK_SIZE = 16;

RtmpMessage::RtmpMessage()
{
    rtmp_body = nullptr;
    rtmp_body_len = 0;
}

RtmpMessage::~RtmpMessage()
{
    if (rtmp_body)
    {
        delete[] rtmp_body;
    }
}

void RtmpMessage::create_msg(RtmpHeader *header, uint8_t *body, uint32_t length)
{
    rtmp_header = *header;
    rtmp_body = body;
    rtmp_body_len = length;
}

RtmpMessageTransport::RtmpMessageTransport(NetCore::BaseSocket *socket)
{
    out_chunk_size = RTMP_DEFAULT_CHUNKSIZE;
    in_chunk_size = RTMP_DEFAULT_CHUNKSIZE;
    send_cache_buf = new uint8_t[RTMP_DEFAULT_CACHE_BUFFER_SIZE];
    chunk_cache_.clear();
    for (int i = 0; i < RTMP_DEFAULT_CHUNK_SIZE; i++)
    {
        chunk_cache_.insert(std::make_pair(i, new RtmpChunkData()));
    }
    socket_ = socket;
}

RtmpMessageTransport::~RtmpMessageTransport()
{
    if (send_cache_buf)
    {
        delete[] send_cache_buf;
    }
    for (auto iter = chunk_cache_.begin(); iter != chunk_cache_.end(); ++iter)
    {
        RtmpChunkData *data = iter->second;
        delete data;
    }
    chunk_cache_.clear();
    socket_ = nullptr;
}

int RtmpMessageTransport::sendRtmpMessage(RtmpBasePacket *pkg, int streamid)
{
    uint8_t *payload = nullptr;
    int size;
    int ret;
    RtmpHeader header;

    header.chunk_stream_id = pkg->get_cs_id();
    header.msg_type_id = pkg->get_msg_type();
    header.msg_stream_id = streamid;
    header.msg_length = pkg->get_pkg_len();
    ret = pkg->encode(payload, size);
    if (ret < 0)
    {
        return -1;
    }
    do_send_message(&header, payload, size);
    delete[] payload;
    return 0;
}

int RtmpMessageTransport::recvRtmpMessage(const char *data, int length, RtmpMessage **pmsg)
{
    int offset = 0;
    int ret;
    bool first_recv_msg = false;
    uint32_t cs_id;
    RtmpChunkData *chunk = nullptr;

    RtmpHeader header;
    ret = header.decode((uint8_t*)data+offset, length-offset);
    if (ret < 0)
    {
        return ret;
    }
    offset += ret;
    cs_id = header.chunk_stream_id;
    if (chunk_cache_.find(cs_id) != chunk_cache_.end())
    {
        chunk = chunk_cache_[cs_id];
    }
    else
    {
        chunk = new RtmpChunkData();
        chunk_cache_.insert(std::make_pair(cs_id, chunk));
    }
    if (chunk->get_current_len() <= 0)
    {
        // this chunk first recv msg;
        first_recv_msg = true;
    }
    chunk->h.chunk_type = header.chunk_type;
    chunk->h.chunk_stream_id = header.chunk_stream_id;
    if (header.chunk_type <= RTMP_CHUNK_FMT2_TYPE)
    {
        chunk->time_delta = header.timestamp;
        if (header.timestamp < RTMP_EXTENDED_TIMESTAMP)
        {
            if (header.chunk_type == RTMP_CHUNK_FMT0_TYPE)
            {
                chunk->h.timestamp = header.timestamp;
            }
            else
            {
                chunk->h.timestamp += header.timestamp;
            }
        }
        if (header.chunk_type <= RTMP_CHUNK_FMT1_TYPE)
        {
            if (!first_recv_msg && header.msg_length != chunk->h.msg_length)
            {
                // msg length changed it is not allow
                ELOG("can not change message len from %d to %d\n", chunk->h.msg_length, header.msg_length);
                return -2;
            }
            chunk->h.msg_length = header.msg_length;
            chunk->h.msg_type_id = header.msg_type_id;
            if (header.chunk_stream_id == RTMP_CHUNK_FMT0_TYPE)
            {
                chunk->h.msg_stream_id = header.msg_stream_id;
            }
        }
    }
    else
    {
        // fmt3
        // only base header
        if (first_recv_msg && chunk->time_delta < RTMP_EXTENDED_TIMESTAMP)
        {
            WLOG("update timestamp in fmt3 other data use last time\n");
            chunk->h.timestamp += chunk->time_delta;
        }
    }
    if (chunk->time_delta >= RTMP_EXTENDED_TIMESTAMP)
    {
        // have 4 bytes extend time
        if (length - offset < 4)
        {
            return -1;
        }
        uint32_t time;
        char *pp = (char*)&time;
        pp[3] = data[offset++];
        pp[2] = data[offset++];
        pp[1] = data[offset++];
        pp[0] = data[offset++];
        // use 31bits timestamp
        time &= 0x7fffffff;
        chunk->h.timestamp = time;
    }
    bool finish = false;
    ret = do_recv_payload(chunk, (uint8_t*)data+offset, length-offset, finish);
    if (ret < 0)
    {
        chunk->h.timestamp -= chunk->time_delta;
        return ret;
    }
    offset += ret;
    if (finish)
    {
        // rtmp message recv complete
        RtmpMessage *rtmpmsg = new RtmpMessage();
        rtmpmsg->create_msg(&chunk->h, chunk->getPaylaod(), chunk->h.msg_length);
        *pmsg = rtmpmsg;
        chunk->reset();
    }
    return offset;
}

int RtmpMessageTransport::do_send_message(RtmpHeader *header, uint8_t *payload, int length)
{
    uint8_t *start = payload;
    uint8_t *end = payload+length;
    uint8_t header_data[16] = {0};
    int header_length = 0;
    int index = 0;

    while(start < end)
    {
        if (start == payload)
        {
            // first packet use fmt0 chunk header 12(16) bytes
            header->chunk_type = 0;
        }
        else
        {
            // use fmt3 chunk header 1(5) bytes
            header->chunk_type = 3;
        }
        header_length = header->encode(header_data, sizeof(header_data));
        memcpy(send_cache_buf+index, header_data, header_length);
        index += header_length;
        int len = UTILS_MIN(out_chunk_size, (int)(end-start));
        memcpy(send_cache_buf+index, start, len);
        index += len;
        start += len;
        int cache_left = RTMP_DEFAULT_CACHE_BUFFER_SIZE - index;
        if (cache_left <= RTMP_CHUNK_FMT0_HEADER_MAX_SIZE)
        {
            // cache not enough send data free it
            // send data
            socket_->sendData((const char*)send_cache_buf, index);
            index = 0;
        }
    }
    if (index > 0)
    {
        // send data
        socket_->sendData((const char*)send_cache_buf, index);
    }
    return 0;
}

int RtmpMessageTransport::do_recv_payload(RtmpChunkData *chunk, uint8_t *data, int length, bool &finish)
{
    int offset = 0;
    int payload_len = 0;

    finish = false;
    payload_len = chunk->h.msg_length - chunk->get_current_len();
    payload_len = UTILS_MIN(payload_len, in_chunk_size);
    if (payload_len > length - offset)
    {
        return -1;
    }
    if (chunk->get_current_len() <= 0)
    {
        chunk->create_payload(chunk->h.msg_length);
    }
    chunk->copy_payload(data+offset, payload_len);
    offset += payload_len;
    if (chunk->get_current_len() == chunk->h.msg_length)
    {
        ILOG("complete recv rtmp message\n");
        finish = true;
    }
    return offset;
}


//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_transport.h"
#include "utils.h"

const uint32_t RTMP_DEFAULT_CHUNKSIZE = 128;
const uint32_t RTMP_DEFAULT_CACHE_BUFFER_SIZE = (32*1024);

RtmpMessageTransport::RtmpMessageTransport(NetCore::BaseSocket *socket)
{
    out_chunk_size = RTMP_DEFAULT_CHUNKSIZE;
    cache_buf = new uint8_t[RTMP_DEFAULT_CACHE_BUFFER_SIZE];
    socket_ = socket;
}

RtmpMessageTransport::~RtmpMessageTransport()
{
    if (cache_buf)
    {
        delete[] cache_buf;
    }
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
    pkg->set_header(&header);
    ret = pkg->encode(payload, size);
    if (ret < 0)
    {
        return -1;
    }
    do_send_message(pkg->get_header(), payload, size);
    delete[] payload;
    return 0;
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
        memcpy(cache_buf+index, header_data, header_length);
        index += header_length;
        int len = UTILS_MIN(out_chunk_size, (int)(end-start));
        memcpy(cache_buf+index, start, len);
        index += len;
        start += len;
        int cache_left = RTMP_DEFAULT_CACHE_BUFFER_SIZE - index;
        if (cache_left <= RTMP_CHUNK_FMT0_HEADER_MAX_SIZE)
        {
            // cache not enough send data free it
            // send data
            socket_->sendData((const char*)cache_buf, index);
            index = 0;
        }
    }
    if (index > 0)
    {
        // send data
        socket_->sendData((const char*)cache_buf, index);
    }
    return 0;
}
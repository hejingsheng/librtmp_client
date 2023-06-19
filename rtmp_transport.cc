//
// Created by hejingsheng on 2023/6/9.
//

#include "rtmp_transport.h"
#include "autofree.h"
#include "logger.h"
#include "utils.h"

const uint32_t RTMP_DEFAULT_CHUNKSIZE = 128;
const uint32_t RTMP_MAX_CHUNKSIZE = 65535;
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
    in_buffer_length = 0;
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
    on_send_message(pkg);
    delete[] payload;
    return 0;
}

int RtmpMessageTransport::recvRtmpMessage(const char *data, int length, RtmpBasePacket **ppkg)
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
        AutoFree(RtmpMessage, rtmpmsg);
        rtmpmsg->create_msg(&chunk->h, chunk->getPaylaod(), chunk->h.msg_length);
        on_recv_message(rtmpmsg);
        decode_msg(rtmpmsg, ppkg);
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
        index = 0;
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

int RtmpMessageTransport::on_recv_message(RtmpMessage *msg)
{
    int ret = 0;
    RtmpBasePacket *pkg = nullptr;

    switch(msg->rtmp_header.msg_type_id)
    {
        case RTMP_MSG_SetChunkSize:
        case RTMP_MSG_UserControlMessage:
        case RTMP_MSG_WindowAcknowledgementSize:
            ret = decode_msg(msg, &pkg);
            if (ret < 0)
            {
                return ret;
            }
            break;
        case RTMP_MSG_VideoMessage:
        case RTMP_MSG_AudioMessage:
            DLOG("recv audio/video msg\n");
        default:
            DLOG("recv other(%d) msg\n", msg->rtmp_header.msg_type_id);
            return 0;
    }
    if (pkg == nullptr)
    {
        ELOG("pkg decode error\n");
        return -1;
    }
    AutoFree(RtmpBasePacket, pkg);
    if (msg->rtmp_header.msg_type_id == RTMP_MSG_UserControlMessage)
    {
        RtmpUserControlPacket *packet = dynamic_cast<RtmpUserControlPacket*>(pkg);
        if (packet != nullptr)
        {
            if (packet->event_type == RtmpPCUCSetBufferLength)
            {
                in_buffer_length = packet->event_data;
            }
            if (packet->event_type == RtmpPCUCPingRequest)
            {
                // send ping message
            }
        }
    }
    else if (msg->rtmp_header.msg_type_id == RTMP_MSG_WindowAcknowledgementSize)
    {
        RtmpSetWindowAckSizePacket *packet = dynamic_cast<RtmpSetWindowAckSizePacket*>(pkg);
        if (packet != nullptr)
        {
            if (packet->window_ack_size > 0)
            {
                in_ack_size.window = (uint32_t)packet->window_ack_size;
                ILOG("set window in ack size %d\n", packet->window_ack_size);
            }
        }
    }
    else if (msg->rtmp_header.msg_type_id == RTMP_MSG_SetChunkSize)
    {
        RtmpSetChunkSizePacket *packet = dynamic_cast<RtmpSetChunkSizePacket*>(pkg);
        if (packet != nullptr)
        {
            if (packet->chunk_size < RTMP_DEFAULT_CHUNKSIZE || packet->chunk_size > RTMP_MAX_CHUNKSIZE)
            {
                WLOG("chunk should in [%d, %d], this chunk size is %d\n", RTMP_DEFAULT_CHUNKSIZE, RTMP_MAX_CHUNKSIZE, packet->chunk_size);
            }
            if (packet->chunk_size < RTMP_DEFAULT_CHUNKSIZE)
            {
                ELOG("chunk size is invalid\n");
                return -2;
            }
            ILOG("set chunk size %d\n", packet->chunk_size);
            in_chunk_size = packet->chunk_size;
        }
    }
    else
    {
        ILOG("other msg type %d\n", msg->rtmp_header.msg_type_id);
        return 0;
    }
    return 0;
}

int RtmpMessageTransport::on_send_message(RtmpBasePacket *pkg)
{
    switch(pkg->get_msg_type())
    {
        case RTMP_MSG_SetChunkSize: {
            RtmpSetChunkSizePacket *packet = dynamic_cast<RtmpSetChunkSizePacket *>(pkg);
            if (packet) {
                out_chunk_size = packet->chunk_size;
            }
        }
            break;
        case RTMP_MSG_WindowAcknowledgementSize: {
            RtmpSetWindowAckSizePacket *packet = dynamic_cast<RtmpSetWindowAckSizePacket *>(pkg);
            if (packet) {
                out_ack_size.window = packet->window_ack_size;
            }
        }
            break;
        case RTMP_MSG_AMF0CommandMessage:
        case RTMP_MSG_AMF3CommandMessage: {
            if (true) {
                RtmpConnectPacket *packet = dynamic_cast<RtmpConnectPacket *>(pkg);
                if (packet) {
                    requestsMap_[packet->number] = packet->command_name;
                }
            }
            if (true) {
                RtmpCreateStreamPacket *packet = dynamic_cast<RtmpCreateStreamPacket *>(pkg);
                if (packet) {
                    requestsMap_[packet->number] = packet->command_name;
                }
            }
            if (true) {
                RtmpFMLEStartPacket *packet = dynamic_cast<RtmpFMLEStartPacket *>(pkg);
                if (packet) {
                    requestsMap_[packet->number] = packet->command_name;
                }
            }
        }
            break;
        default:
            return 0;
    }
    return 0;
}

int RtmpMessageTransport::decode_msg(RtmpMessage *msg, RtmpBasePacket **ppacket)
{
    int ret = 0;
    int offset = 0;
    RtmpBasePacket *pkg = nullptr;
    uint8_t *body = msg->rtmp_body;
    uint32_t bodylen = msg->rtmp_body_len;

    if (msg->rtmp_header.msg_type_id == RTMP_MSG_AMF0CommandMessage
        || msg->rtmp_header.msg_type_id == RTMP_MSG_AMF3CommandMessage
        || msg->rtmp_header.msg_type_id == RTMP_MSG_AMF0DataMessage
        || msg->rtmp_header.msg_type_id == RTMP_MSG_AMF3DataMessage) {
        if (msg->rtmp_header.msg_type_id == RTMP_MSG_AMF3CommandMessage) {
            offset++;
        }
        std::string command = "";
        ret = rtmp_amf0_read_string(body+offset, bodylen-offset, command);
        if (ret < 0) {
            ELOG("deocde command error\n");
            return ret;
        }
        offset += ret;
        if (command == "_result" || command == "_error") {
            // rtmp result response or error response
            double number = 0.0;
            ret = rtmp_amf0_read_number(body+offset, bodylen-offset, number);
            if (ret < 0) {
                ELOG("deocde number error for %s\n", command.c_str());
                return ret;
            }
            // reset offset to 0 decode again
            offset = 0;
            if (msg->rtmp_header.msg_type_id == RTMP_MSG_AMF3CommandMessage) {
                offset++;
            }
            if (requestsMap_.find(number) == requestsMap_.end()) {
                ELOG("not find this request %s\n", command.c_str());
                return -2;
            }
            std::string request_name = requestsMap_[number];
            if (request_name == "connect") {
                pkg = new RtmpConnectResponsePacket();
                ret = pkg->decode(body+offset, bodylen-offset);
                if (ret < 0) {
                    ELOG("decode connect res error\n");
                    return ret;
                }
                offset += ret;
            } else if (request_name == "createStream") {
                pkg = new RtmpCreateStreamResponsePacket(0, 0);
                ret = pkg->decode(body+offset, bodylen-offset);
                if (ret < 0) {
                    ELOG("decode create stream res error\n");
                    return ret;
                }
                offset += ret;
            } else if (request_name == "releaseStream") {
                pkg = new RtmpFMLEStartResponsePacket(0);
                ret = pkg->decode(body+offset, bodylen-offset);
                if (ret < 0) {
                    ELOG("decode release stream res error\n");
                    return ret;
                }
                offset += ret;
            } else if (request_name == "FCPublish") {
                pkg = new RtmpFMLEStartResponsePacket(0);
                ret = pkg->decode(body+offset, bodylen-offset);
                if (ret < 0) {
                    ELOG("decode fcpublish res error\n");
                    return ret;
                }
                offset += ret;
            } else if (request_name == "FCUnpublish") {
                pkg = new RtmpFMLEStartResponsePacket(0);
                ret = pkg->decode(body+offset, bodylen-offset);
                if (ret < 0) {
                    ELOG("decode fcunpublish res error\n");
                    return ret;
                }
                offset += ret;
            } else {
                ELOG("unkonw request name\n");
                return -2;
            }
            *ppacket = pkg;
            return offset;
        }
        // reset offset to 0 decode again
        offset = 0;
        if (msg->rtmp_header.msg_type_id == RTMP_MSG_AMF3CommandMessage) {
            offset++;
        }
        if (command == "connect") {
            pkg = new RtmpConnectPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "createStream") {
            pkg = new RtmpCloseStreamPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "play") {
            pkg = new RtmpPlayPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "pause") {
            pkg = new RtmpPausePacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "releaseStream") {
            pkg = new RtmpFMLEStartPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "FCPublish") {
            pkg = new RtmpFMLEStartPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "publish") {
            pkg = new RtmpPublishPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "FCUnpublish") {
            pkg = new RtmpFMLEStartPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "@setDataFrame") {
            pkg = new RtmpOnMetaDataPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "onMetaData") {
            pkg = new RtmpOnMetaDataPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (command == "closeStream") {
            pkg = new RtmpCloseStreamPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else if (msg->rtmp_header.msg_type_id == RTMP_MSG_AMF0CommandMessage
                    || msg->rtmp_header.msg_type_id == RTMP_MSG_AMF3CommandMessage) {
            pkg = new RtmpCallPacket();
            ret = pkg->decode(body+offset, bodylen-offset);
            if (ret < 0) {
                return ret;
            }
            offset += ret;
        } else {
            ELOG("error command name %s\n", command.c_str());
            return -2;
        }
        *ppacket = pkg;
        return offset;
    } else if (msg->rtmp_header.msg_type_id == RTMP_MSG_UserControlMessage) {
        pkg = new RtmpUserControlPacket();
        ret = pkg->decode(body+offset, bodylen-offset);
        offset += ret;
    } else if (msg->rtmp_header.msg_type_id == RTMP_MSG_WindowAcknowledgementSize) {
        pkg = new RtmpSetWindowAckSizePacket();
        ret = pkg->decode(body+offset, bodylen-offset);
        offset += ret;
    } else if (msg->rtmp_header.msg_type_id == RTMP_MSG_Acknowledgement) {
        pkg = new RtmpAcknowledgementPacket();
        ret = pkg->decode(body+offset, bodylen-offset);
        offset += ret;
    } else if (msg->rtmp_header.msg_type_id == RTMP_MSG_SetChunkSize) {
        pkg = new RtmpSetChunkSizePacket();
        ret = pkg->decode(body+offset, bodylen-offset);
        offset += ret;
    } else if (msg->rtmp_header.msg_type_id == RTMP_MSG_SetPeerBandwidth) {
        pkg = new RtmpSetPeerBandwidthPacket();
        ret = pkg->decode(body+offset, bodylen-offset);
        offset += ret;
    } else {
        WLOG("drop unknow msg type=%d\n", msg->rtmp_header.msg_type_id);
        return -2;
    }
    *ppacket = pkg;
    return offset;
}


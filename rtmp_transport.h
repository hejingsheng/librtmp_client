//
// Created by hejingsheng on 2023/6/9.
//

#ifndef RTMP_CLIENT_RTMP_TRANSPORT_H
#define RTMP_CLIENT_RTMP_TRANSPORT_H

#include <string>
#include <unordered_map>
#include "NetCore.h"
#include "app_protocol/rtmp/rtmp_stack_packet.h"

class RtmpMessage
{
public:
    RtmpMessage();
    virtual ~RtmpMessage();

public:
    void create_msg(RtmpHeader *header, uint8_t *body, uint32_t length);

public:
    RtmpHeader rtmp_header;
    uint8_t *rtmp_body;
    uint32_t rtmp_body_len;
};

struct RtmpAckWindowSize
{
    uint32_t window;
    uint32_t recv_bytes;
    uint32_t seq;

    RtmpAckWindowSize() {
        window = recv_bytes = seq = 0;
    }
};

class RtmpMessageTransport
{
public:
    RtmpMessageTransport(NetCore::BaseSocket *socket);
    virtual ~RtmpMessageTransport();

public:
    int sendRtmpMessage(RtmpBasePacket *pkg, int streamid);
    int recvRtmpMessage(const char *data, int length, RtmpBasePacket **pmsg);

private:
    int do_send_message(RtmpHeader *header, uint8_t *payload, int length);
    int do_recv_payload(RtmpChunkData *chunk, uint8_t *data, int length, bool &finish);
    int on_recv_message(RtmpMessage *msg);
    int on_send_message(RtmpBasePacket *pkg);
    int decode_msg(RtmpMessage *msg, RtmpBasePacket **ppacket);

private:
    std::unordered_map<double, std::string> requestsMap_;

private:
    int32_t in_buffer_length;
    uint32_t in_chunk_size;
    RtmpAckWindowSize in_ack_size;
    std::unordered_map<uint32_t, RtmpChunkData*> chunk_cache_;

private:
    uint32_t out_chunk_size;
    RtmpAckWindowSize out_ack_size;
    uint8_t *send_cache_buf;

private:
    NetCore::BaseSocket *socket_;
};

#endif //RTMP_CLIENT_RTMP_TRANSPORT_H

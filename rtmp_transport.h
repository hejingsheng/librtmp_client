//
// Created by hejingsheng on 2023/6/9.
//

#ifndef RTMP_CLIENT_RTMP_TRANSPORT_H
#define RTMP_CLIENT_RTMP_TRANSPORT_H

#include <string>
#include "NetCore.h"
#include "app_protocol/rtmp/rtmp_stack_packet.h"

class RtmpMessageTransport
{
public:
    RtmpMessageTransport(NetCore::BaseSocket *socket);
    virtual ~RtmpMessageTransport();

public:
    int sendRtmpMessage(RtmpBasePacket *pkg, int streamid);

private:
    int do_send_message(RtmpHeader *header, uint8_t *payload, int length);

private:
    uint32_t out_chunk_size;
    uint8_t *cache_buf;
    NetCore::BaseSocket *socket_;
};

#endif //RTMP_CLIENT_RTMP_TRANSPORT_H

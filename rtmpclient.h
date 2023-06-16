//
// Created by hejingsheng on 2023/6/7.
//

#ifndef RTMP_CLIENT_RTMPCLIENT_H
#define RTMP_CLIENT_RTMPCLIENT_H

#include <string>
#include "net/NetCore.h"
#include "rtmp/rtmp_stack_handshake.h"
#include "app_protocol/rtmp/rtmp_stack_packet.h"
#include "rtmp_transport.h"
#include "DataBuf.h"

enum RtmpClientHandshakeStatus {
    RTMP_HANDSHAKE_CLIENT_START,
    RTMP_HANDSHAKE_SEND_C0C1,
    RTMP_HANDSHAKE_RECV_S0S1S2,
    RTMP_HANDSHAKE_SEND_C2,
    RTMP_HANDSHAKE_CLIENT_DONE = RTMP_HANDSHAKE_SEND_C2,
};

enum RtmpServerHandshakeStatus {
    RTMP_HANDSHAKE_SERVER_START,
    RTMP_HANDSHAKE_RECV_C0C1,
    RTMP_HANDSHAKE_SEND_S0S1S2,
    RTMP_HANDSHAKE_RECV_C2,
    RTMP_HANDSHAKE_SERVER_DONE,
};

class RtmpPublishClient : public NetCore::ISocketCallback {

public:
    RtmpPublishClient(std::string app, std::string live);
    ~RtmpPublishClient();

public:
    void setRtmpServer(NetCore::IPAddr &server);
    void start();
    void stop();

protected:
    virtual int onConnect(int status, NetCore::BaseSocket *pSock);
    virtual int onRecvData(const char *data, int size, const struct sockaddr* addr, NetCore::BaseSocket *pSock);
    virtual int onClose(NetCore::BaseSocket *pSock);

private:
    void doHandshake(const char *data, int size);
    void connectApp();

private:
    void sendRtmpPacket(RtmpBasePacket *pkg, int streamid);
    void sendData(const char *data, int len);
    int processData(const char *data, int length);
    int processRtmpMsg(RtmpMessage *msg);
    int decode_msg(RtmpHeader *header, uint8_t *body, int bodylen);

private:
    std::string rtmp_app_;
    std::string rtmp_live_;

private:
    rtmp_handshake handshake;
    RtmpClientHandshakeStatus status_;
    RtmpMessageTransport *rtmp_transport_;

private:
    NetCore::IPAddr serveraddr_;
    NetCore::TcpSocket *rtmp_socket_;
    DataCacheBuf *data_cache_;
};

#endif //RTMP_CLIENT_RTMPCLIENT_H

//
// Created by hejingsheng on 2023/6/7.
//

#include <string>
#include "rtmpclient.h"
#include "base/logger.h"

RtmpPublishClient::RtmpPublishClient(std::string app, std::string live)
{
    rtmp_app_ = app;
    rtmp_live_ = live;
    status_ = RTMP_HANDSHAKE_CLIENT_START;
    data_cache_ = new DataCacheBuf();
}

RtmpPublishClient::~RtmpPublishClient()
{
    delete data_cache_;
}

void RtmpPublishClient::setRtmpServer(NetCore::IPAddr &server)
{
    serveraddr_ = server;
}

void RtmpPublishClient::start()
{
    rtmp_socket_ = new NetCore::TcpSocket(NETIOMANAGER->loop_);
    rtmp_socket_->registerCallback(this);
    rtmp_socket_->connectServer(serveraddr_);
}

void RtmpPublishClient::stop()
{

}

int RtmpPublishClient::onConnect(int status, NetCore::BaseSocket *pSock)
{
    if (status == 0)
    {
        ILOG("rtmp server connect success, start handshake\n");
        doHandshake(nullptr, 0);
    }
    else
    {

    }
    return 0;
}

int RtmpPublishClient::onRecvData(const char *data, int size, const struct sockaddr* addr, NetCore::BaseSocket *pSock)
{
    int ret;
    if (status_ < RTMP_HANDSHAKE_CLIENT_DONE)
    {
        doHandshake(data, size);
    }
    return 0;
}

int RtmpPublishClient::onClose(NetCore::BaseSocket *pSock)
{
    return 0;
}

void RtmpPublishClient::doHandshake(const char *data, int size)
{
    int ret;
    switch(status_)
    {
        case RTMP_HANDSHAKE_CLIENT_START:
            handshake.create_c0c1(schema_type1);
            rtmp_socket_->sendData(handshake.c0c1, 1537);
            status_ = RTMP_HANDSHAKE_SEND_C0C1;
            break;
        case RTMP_HANDSHAKE_SEND_C0C1:
            data_cache_->push_data((char*)data, size);
            if (data_cache_->require(3073))
            {
                ret = handshake.process_s0s1s2(data_cache_->data(), data_cache_->len());
                if (ret < 0) {
                    WLOG("s0s1s2 recv not complete or error %d\n", ret);
                }
                data_cache_->pop_data(3073);
                handshake.create_c2();
                rtmp_socket_->sendData(handshake.c2, 1536);
                status_ = RTMP_HANDSHAKE_SEND_C2;
                ILOG("rtmp handshake finish\n");
            }
            break;
        default:
            break;
    }

}

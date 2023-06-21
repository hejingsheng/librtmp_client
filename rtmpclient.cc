//
// Created by hejingsheng on 2023/6/7.
//

#include <string>
#include "rtmpclient.h"
#include "base/logger.h"
#include "app_protocol/rtmp/rtmp_stack_packet.h"
#include "autofree.h"

RtmpPublishClient::RtmpPublishClient(std::string app, std::string stream)
{
    rtmp_app_ = app;
    rtmp_stream_ = stream;
    status_ = RTMP_HANDSHAKE_CLIENT_START;
    data_cache_ = new DataCacheBuf();
    rtmp_transport_ = nullptr;
}

RtmpPublishClient::~RtmpPublishClient()
{
    delete data_cache_;
    if (rtmp_transport_)
    {
        delete rtmp_transport_;
    }
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
    rtmp_transport_ = new RtmpMessageTransport(rtmp_socket_);
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
    else
    {
        ILOG("recv %d data\n", size);
        processData(data, size);
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
            sendData(handshake.c0c1, 1537);
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
                sendData(handshake.c2, 1536);
                status_ = RTMP_HANDSHAKE_SEND_C2;
                ILOG("rtmp handshake finish\n");
                connectApp();
            }
            break;
        default:
            break;
    }
}

void RtmpPublishClient::connectApp()
{
    RtmpConnectPacket *pkg = new RtmpConnectPacket();
    pkg->command_object->set("app", RtmpAmf0Any::str(rtmp_app_.c_str()));
    pkg->command_object->set("type", RtmpAmf0Any::str("nonprivate"));
    pkg->command_object->set("flashVer", RtmpAmf0Any::str("Jack He Rtmp Client(v0.0.1)"));
    pkg->command_object->set("tcUrl", RtmpAmf0Any::str("rtmp://8.135.38.10:1935/live"));
    sendRtmpPacket(pkg, 0);

    {
        RtmpSetWindowAckSizePacket *pkg = new RtmpSetWindowAckSizePacket();
        pkg->window_ack_size = 2500000;
        sendRtmpPacket(pkg, 0);
    }
}

void RtmpPublishClient::createStream(std::string stream)
{
    if (true) {
        RtmpFMLEStartPacket *pkg = RtmpFMLEStartPacket::create_release_stream(stream);
        sendRtmpPacket(pkg, 0);
    }

    if (true) {
        RtmpFMLEStartPacket *pkg = RtmpFMLEStartPacket::create_FC_publish(stream);
        sendRtmpPacket(pkg, 0);
    }

    if (true) {
        RtmpCreateStreamPacket *pkg = new RtmpCreateStreamPacket();
        pkg->number = 4;
        sendRtmpPacket(pkg, 0);
    }
}

void RtmpPublishClient::publish(std::string stream, int streamid)
{
    if (true) {
        RtmpSetChunkSizePacket *pkg = new RtmpSetChunkSizePacket();
        pkg->chunk_size = 60000;
        sendRtmpPacket(pkg, 0);
    }

    if (true) {
        RtmpPublishPacket *pkg = new RtmpPublishPacket();
        pkg->stream_name = stream;
        sendRtmpPacket(pkg, streamid);
    }
}

void RtmpPublishClient::sendRtmpPacket(RtmpBasePacket *pkg, int streamid)
{
    rtmp_transport_->sendRtmpMessage(pkg, streamid);
    delete pkg;
}

void RtmpPublishClient::sendData(const char *data, int len)
{
    rtmp_socket_->sendData(data, len);
}

int RtmpPublishClient::processData(const char *data, int length)
{
    RtmpBasePacket *packet = nullptr;
    int ret = 0;
    data_cache_->push_data((char*)data, length);
    while (data_cache_->len() > 0)
    {
        packet = nullptr;
        ret = rtmp_transport_->recvRtmpMessage(data_cache_->data(), data_cache_->len(), &packet);
        if (ret < 0)
        {
            return ret;
        }
        data_cache_->pop_data(ret);
        if (packet != nullptr) {
            AutoFree(RtmpBasePacket, packet);
            RtmpConnectResponsePacket *pkg = dynamic_cast<RtmpConnectResponsePacket*>(packet);
            if (pkg != nullptr) {
                ILOG("recv connect response\n");
                createStream(rtmp_stream_);
            }
            RtmpCreateStreamResponsePacket *pkg1 = dynamic_cast<RtmpCreateStreamResponsePacket*>(packet);
            if (pkg1 != nullptr) {
                ILOG("recv create stream response stream id=%d\n", (int)pkg1->streamid_);
                publish(rtmp_stream_, (int)pkg1->streamid_);
            }
        }
    }
    return ret;
}

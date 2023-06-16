//
// Created by hejingsheng on 2023/6/7.
//

#include <string>
#include "rtmpclient.h"
#include "base/logger.h"
#include "app_protocol/rtmp/rtmp_stack_packet.h"
#include "autofree.h"

RtmpPublishClient::RtmpPublishClient(std::string app, std::string live)
{
    rtmp_app_ = app;
    rtmp_live_ = live;
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
    AutoFree(RtmpConnectPacket, pkg);
    pkg->command_object->set("app", RtmpAmf0Any::str(rtmp_app_.c_str()));
    pkg->command_object->set("type", RtmpAmf0Any::str("nonprivate"));
    pkg->command_object->set("flashVer", RtmpAmf0Any::str("Jack He Rtmp Client(v0.0.1)"));
    pkg->command_object->set("tcUrl", RtmpAmf0Any::str("rtmp://8.135.38.10:1935/live"));
    sendRtmpPacket(pkg, 0);
}

void RtmpPublishClient::sendRtmpPacket(RtmpBasePacket *pkg, int streamid)
{
    rtmp_transport_->sendRtmpMessage(pkg, streamid);
}

void RtmpPublishClient::sendData(const char *data, int len)
{
    rtmp_socket_->sendData(data, len);
}

int RtmpPublishClient::processData(const char *data, int length)
{
    RtmpMessage *msg = nullptr;
    int ret = 0;
    data_cache_->push_data((char*)data, length);
    while (data_cache_->len() > 0)
    {
        ret = rtmp_transport_->recvRtmpMessage(data_cache_->data(), data_cache_->len(), &msg);
        if (ret < 0)
        {
            return ret;
        }
        data_cache_->pop_data(ret);
        if (msg != nullptr) {
            ILOG("msg type=%d\n", msg->rtmp_header.msg_type_id);
            processRtmpMsg(msg);
        }
    }
    return ret;
}

int RtmpPublishClient::processRtmpMsg(RtmpMessage *msg)
{
    int ret = 0;
    switch(msg->rtmp_header.msg_type_id)
    {
        case RTMP_MSG_SetChunkSize:
        case RTMP_MSG_UserControlMessage:
        case RTMP_MSG_WindowAcknowledgementSize:
            ret = decode_msg(&msg->rtmp_header, msg->rtmp_body, msg->rtmp_body_len);
            break;
        case RTMP_MSG_VideoMessage:
        case RTMP_MSG_AudioMessage:
            DLOG("recv audio/video msg\n");
        default:
            return 0;
    }
    return 0;
}

int RtmpPublishClient::decode_msg(RtmpHeader *header, uint8_t *body, int bodylen)
{
    int ret = 0;
    int offset = 0;

    if (header->msg_type_id == RTMP_MSG_AMF0CommandMessage
        || header->msg_type_id == RTMP_MSG_AMF3CommandMessage
        || header->msg_type_id == RTMP_MSG_AMF0DataMessage
        || header->msg_type_id == RTMP_MSG_AMF3DataMessage)
    {
        if (header->msg_type_id == RTMP_MSG_AMF3CommandMessage)
        {
            offset++;
        }
        std::string command = "";
        ret = rtmp_amf0_read_string(body+offset, bodylen-offset, command);
        if (ret < 0)
        {
            ELOG("deocde command error\n");
            return ret;
        }
        offset += ret;
        if (command == "_result" || command == "_error")
        {
            double number = 0.0;
            ret = rtmp_amf0_read_number(body+offset, bodylen-offset, number);
            if (ret < 0)
            {
                ELOG("deocde number error for %s\n", command.c_str());
                return ret;
            }
        }
    }
    return ret;
}

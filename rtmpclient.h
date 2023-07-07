//
// Created by hejingsheng on 2023/6/7.
//

#ifndef RTMP_CLIENT_RTMPCLIENT_H
#define RTMP_CLIENT_RTMPCLIENT_H

#include <string>
#include <list>
#include <mutex>
#include "net/NetCore.h"
#include "rtmp/rtmp_stack_handshake.h"
#include "app_protocol/rtmp/rtmp_stack_packet.h"
#include "rtmp_transport.h"
#include "DataBuf.h"
#include "av_device.h"
#include "av_codec.h"

enum RtmpClientHandshakeStatus {
    RTMP_HANDSHAKE_CLIENT_START,
    RTMP_HANDSHAKE_SEND_C0C1,
    RTMP_HANDSHAKE_SEND_C2,
    RTMP_HANDSHAKE_CLIENT_DONE = RTMP_HANDSHAKE_SEND_C2,
};

enum RtmpClientPushPullStatus {
    RTMP_CONNECT_APP,
    RTMP_CREATE_STREAM,
    RTMP_PUSH_OR_PULL,
};

class RtmpPublishClient : public NetCore::ISocketCallback, public VideoDataTransport {

public:
    RtmpPublishClient(std::string app, std::string stream);
    ~RtmpPublishClient();

public:
    void setRtmpServer(NetCore::IPAddr &server);
    void start();
    void stop();

protected:
    virtual int onConnect(int status, NetCore::BaseSocket *pSock);
    virtual int onRecvData(const char *data, int size, const struct sockaddr* addr, NetCore::BaseSocket *pSock);
    virtual int onClose(NetCore::BaseSocket *pSock);

protected:
    virtual int YuvDataIsAvailable(const void* yuvData, const uint32_t len, const int32_t width, const int32_t height);

private:
    void doHandshake(const char *data, int size);
    void connectApp();
    void createStream(std::string stream);
    void publish(std::string stream, int streamid);
    void sendMetaData();

private:
    void sendRtmpPacket(RtmpBasePacket *pkg, int streamid);
    void sendData(const char *data, int len);
    int processData(const char *data, int length);

private:
    void onTimer();
    static void on_uv_timer(uv_timer_t *handle);

private:
    std::string rtmp_app_;
    std::string rtmp_stream_;

private:
    rtmp_handshake handshake;
    RtmpClientHandshakeStatus status_;
    RtmpClientPushPullStatus pushPullStatus_;
    RtmpMessageTransport *rtmp_transport_;
    int streamid;

private:
    NetCore::IPAddr serveraddr_;
    NetCore::TcpSocket *rtmp_socket_;
    DataCacheBuf *data_cache_;
    uv_timer_t *timer_;

private:
    BaseDevices *video_device_;
    VideoCodec *video_codec_;
    int64_t video_pts;
    int64_t video_dts;
    uint32_t timestamp;

private:
    std::list<MediaPacketShareData*> datalist;
    std::recursive_mutex data_mutex;
};

#endif //RTMP_CLIENT_RTMPCLIENT_H

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

class RtmpClient : public NetCore::ISocketCallback {

public:
    RtmpClient(std::string rtmpurl, int dir, bool audio);
    virtual ~RtmpClient();

public:
    virtual void start(uint32_t w, uint32_t h, uint32_t b);
    virtual void stop();

protected:
    virtual void startPushStream();
    virtual void startPullStream();
    virtual void stopPushStream();
    virtual void stopPullStream();

protected:
    virtual void onPublishStart();
    virtual void onPublishStop();

protected:
    virtual int onConnect(int status, NetCore::BaseSocket *pSock);
    virtual int onRecvData(const char *data, int size, const struct sockaddr* addr, NetCore::BaseSocket *pSock);
    virtual int onClose(NetCore::BaseSocket *pSock);

protected:
    virtual void onStoped();

protected:
    void doHandshake(const char *data, int size);
    void connectApp();
    void createStream(std::string stream);

protected:
    void sendRtmpPacket(RtmpBasePacket *pkg, int streamid);
    void sendData(const char *data, int len);
    void processData(const char *data, int length);

protected:
    std::string rtmp_app_;
    std::string rtmp_stream_;

protected:
    rtmp_handshake handshake;
    RtmpClientHandshakeStatus status_;
    RtmpClientPushPullStatus pushPullStatus_;
    RtmpMessageTransport *rtmp_transport_;
    int streamid;
    bool havestop;

protected:
    NetCore::IPAddr serveraddr_;
    NetCore::TcpSocket *rtmp_socket_;
    DataCacheBuf *data_cache_;

protected:
    int dir; // 0 push  1 pull
    bool audio;
    uint32_t width;
    uint32_t heigth;
    uint32_t bitrate;
};

class RtmpPublishClient : public RtmpClient, public VideoDataTransport, public AudioDataTransport {

public:
    RtmpPublishClient(std::string url, bool audio);
    virtual ~RtmpPublishClient();

protected:
    virtual void startPushStream();
    virtual void stopPushStream();
    virtual void onPublishStart();
    virtual void onPublishStop();

protected:
    virtual int YuvDataIsAvailable(const void* yuvData, const uint32_t len, const int32_t width, const int32_t height);
    virtual int RecordDataIsAvailable(const void* audioData,
                                      const int32_t nSamples,
                                      const int32_t nBitsPerSample,
                                      const int32_t nChannels,
                                      const uint32_t nSampleRate,
                                      const int32_t nFrameDurationMs);
    virtual int PlayNeedMordData(const int32_t nSamples,
                                 const int32_t nBitsPerSample,
                                 const int32_t nChannels,
                                 const uint32_t nSampleRate,
                                 const int32_t nFrameDurationMs,
                                 void *audioData,
                                 int32_t &nSamplesOut);

private:
    void sendMetaData();
    void publish(std::string stream, int streamid);

private:
    void onTimer();
    static void on_uv_timer(uv_timer_t *handle);

private:
    BaseDevices *video_device_;
    VideoCodec *video_codec_;
    int64_t video_pts;
    int64_t video_dts;
    BaseDevices *audio_device_;
    AudioCodec *audio_codec_;
    int64_t audio_pts;
    int64_t audio_dts;
    uint32_t video_timestamp;
    uint32_t audio_timestamp;

private:
    uv_timer_t *timer_;

private:
    std::list<MediaPacketShareData*> datalist;
    std::recursive_mutex data_mutex;
};


#endif //RTMP_CLIENT_RTMPCLIENT_H

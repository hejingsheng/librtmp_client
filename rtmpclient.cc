//
// Created by hejingsheng on 2023/6/7.
//

#include <string>
#include "rtmpclient.h"
#include "base/logger.h"
#include "app_protocol/rtmp/rtmp_stack_packet.h"
#include "autofree.h"

RtmpClient::RtmpClient(std::string rtmpurl, int dir, bool audio) {
    size_t pos = rtmpurl.find("rtmp://");
    std::string url = "";
    std::string addr = rtmpurl.substr(pos+strlen("rtmp://"));
    pos = addr.find_first_of('/');
    url = addr.substr(pos+1);
    addr = addr.substr(0, pos);
    pos = addr.find_first_of(':');
    if (pos != std::string::npos) {
        serveraddr_.ip = addr.substr(0, pos);
        serveraddr_.port = atoi(addr.substr(pos+1).c_str());
    }
    else {
        serveraddr_.ip = addr;
        serveraddr_.port = 1935;
    }
    pos = url.find_first_of('/');
    rtmp_stream_ = url.substr(0, pos);
    rtmp_app_ = url.substr(pos+1);
    this->dir = dir;

    status_ = RTMP_HANDSHAKE_CLIENT_START;
    pushPullStatus_ = RTMP_CONNECT_APP;
    data_cache_ = new DataCacheBuf();
    rtmp_transport_ = nullptr;

    this->audio = audio;
}

RtmpClient::~RtmpClient() {
    DLOG("destroy rtmp client\n");
    delete data_cache_;
    if (rtmp_transport_) {
        delete rtmp_transport_;
    }
}

void RtmpClient::start(uint32_t w, uint32_t h, uint32_t b) {
    rtmp_socket_ = new NetCore::TcpSocket(NETIOMANAGER->loop_);
    rtmp_socket_->registerCallback(this);
    rtmp_socket_->connectServer(serveraddr_);
    rtmp_transport_ = new RtmpMessageTransport(rtmp_socket_);
    if (dir == 0) {
        width = w;
        heigth = h;
        bitrate = b;
    }
}

void RtmpClient::stop() {
    if (dir == 0) {
        stopPushStream();
    }
    else {
        stopPullStream();
    }
}

void RtmpClient::startPushStream() {

}

void RtmpClient::startPullStream() {

}

void RtmpClient::stopPushStream() {

}

void RtmpClient::stopPullStream() {

}

void RtmpClient::onPublishStart() {

}

void RtmpClient::onPublishStop() {

}

void RtmpClient::onStoped() {
    if (rtmp_socket_) {
        rtmp_socket_->close();
    }
    else {
        delete this;
    }
}

int RtmpClient::onConnect(int status, NetCore::BaseSocket *pSock) {
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

int RtmpClient::onRecvData(const char *data, int size, const struct sockaddr *addr, NetCore::BaseSocket *pSock) {
    if (status_ < RTMP_HANDSHAKE_CLIENT_DONE) {
        doHandshake(data, size);
    }
    else {
        ILOG("recv %d data\n", size);
        processData(data, size);
    }
    return 0;
}

int RtmpClient::onClose(NetCore::BaseSocket *pSock) {
    rtmp_socket_ = nullptr;
    if (!havestop) {
        havestop = true;
        onPublishStop();
    }
    else {
        delete this;
    }
    return 0;
}

void RtmpClient::doHandshake(const char *data, int size) {
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

void RtmpClient::connectApp()
{
    RtmpConnectPacket *pkg = new RtmpConnectPacket();
    pkg->command_object->set("app", RtmpAmf0Any::str(rtmp_stream_.c_str()));
    pkg->command_object->set("type", RtmpAmf0Any::str("nonprivate"));
    pkg->command_object->set("flashVer", RtmpAmf0Any::str("Jack He Rtmp Client(v0.0.1)"));
    std::string url = "rtmp://"+serveraddr_.ip+":"+std::to_string(serveraddr_.port)+"/"+rtmp_stream_;
    pkg->command_object->set("tcUrl", RtmpAmf0Any::str(url.c_str()));
    sendRtmpPacket(pkg, 0);
    if (true) {
        RtmpSetWindowAckSizePacket *pkg = new RtmpSetWindowAckSizePacket();
        pkg->window_ack_size = 2500000;
        sendRtmpPacket(pkg, 0);
    }
    pushPullStatus_ = RTMP_CONNECT_APP;
}

void RtmpClient::createStream(std::string stream)
{
    if (dir == 0) {
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
    else {
        RtmpCreateStreamPacket *pkg = new RtmpCreateStreamPacket();
        pkg->number = 2;
        sendRtmpPacket(pkg, 0);
        if (true) {
            RtmpCallPacket *pkg = new RtmpCallPacket();
            pkg->command_name = "_checkbw";
            pkg->number = 3;
            pkg->object = RtmpAmf0Any::null();
            pkg->args = nullptr;
            sendRtmpPacket(pkg, 0);
        }
    }
    pushPullStatus_ = RTMP_CREATE_STREAM;
}

void RtmpClient::sendRtmpPacket(RtmpBasePacket *pkg, int streamid)
{
    rtmp_transport_->sendRtmpMessage(pkg, streamid);
    delete pkg;
}

void RtmpClient::sendData(const char *data, int len)
{
    rtmp_socket_->sendData(data, len);
}

void RtmpClient::processData(const char *data, int length)
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
            return;
        }
        data_cache_->pop_data(ret);
        if (packet != nullptr) {
            AutoFree(RtmpBasePacket, packet);
            if (pushPullStatus_ == RTMP_CONNECT_APP) {
                RtmpConnectResponsePacket *pkg = dynamic_cast<RtmpConnectResponsePacket *>(packet);
                if (pkg != nullptr) {
                    ILOG("recv connect response\n");
                    createStream(rtmp_app_);
                }
            }
            else if (pushPullStatus_ == RTMP_CREATE_STREAM) {
                RtmpCreateStreamResponsePacket *pkg = dynamic_cast<RtmpCreateStreamResponsePacket *>(packet);
                if (pkg != nullptr) {
                    ILOG("recv create stream response stream id=%d\n", (int) pkg->streamid_);
                    streamid = (int)pkg->streamid_;
                    if (dir == 0) {
                        startPushStream();
                    }
                    else {
                        startPullStream();
                    }
                }
            }
            else if (pushPullStatus_ == RTMP_PUSH_OR_PULL) {
                RtmpCallPacket *pkg = dynamic_cast<RtmpCallPacket *>(packet);
                if (pkg != nullptr) {
                    ILOG("recv call data command name=%s\n", pkg->command_name.c_str());
                    if (pkg->command_name == "onStatus") {
                        RtmpAmf0Object *obj = pkg->args->to_object();
                        std::string value = obj->get_property("code")->to_str();
                        ILOG("name=code, val=%s\n", value.c_str());
                        if (value == "NetStream.Publish.Start") {
                            onPublishStart();
                        }
                        else if (value == "NetStream.Unpublish.Success") {
                            havestop = true;
                            onPublishStop();
                        }
                    }

                }
            }
        }
    }
}


RtmpPublishClient::RtmpPublishClient(std::string url, bool audio) : RtmpClient(url, 0, audio) {
    video_device_ = nullptr;
    video_codec_ = nullptr;
    video_pts = video_dts = 0;
    if (audio) {
        audio_device_ = nullptr;
        audio_codec_ = nullptr;
        audio_dts = audio_pts = 0;
    }
    video_timestamp = 0;
    audio_timestamp = 0;
    datalist.clear();

    timer_ = new uv_timer_t;
    timer_->data = static_cast<void*>(this);
    uv_timer_init(NETIOMANAGER->loop_, timer_);
}

RtmpPublishClient::~RtmpPublishClient() {
    DLOG("destroy rtmp publish client\n");
    delete timer_;
    if (video_device_) {
        delete video_device_;
    }
    if (audio_device_) {
        delete video_codec_;
    }
    for (auto iter = datalist.begin(); iter != datalist.end(); ++iter) {
        MediaPacketShareData *data = *iter;
        if (data) {
            delete data;
        }
    }
    datalist.clear();
}

void RtmpPublishClient::startPushStream() {
    publish(rtmp_stream_, streamid);
}

void RtmpPublishClient::stopPushStream() {
    RtmpFMLEStartPacket *pkg = RtmpFMLEStartPacket::create_release_stream(rtmp_stream_);
    sendRtmpPacket(pkg, streamid);
}

void RtmpPublishClient::onPublishStart() {
    video_device_ = DevicesFactory::CreateVideoDevice("video", width, heigth, 25);
    video_device_->registerVideoCallback(this);
    video_device_->Init();

    video_codec_ = new VideoCodec(VIDEO_CODEC_NAME);
    video_codec_->initCodec(width, heigth, bitrate, 25);
    video_pts = video_dts = 0;

    if (audio) {
        audio_device_ = DevicesFactory::CreateAudioDevice("audio", 8000, 16, 1, 20);
        audio_device_->registerAudioCallback(this);
        audio_device_->Init();

        audio_codec_ = new AudioCodec("pcm_mulaw");
        audio_codec_->initCodec(8000, 64000, 1);
        audio_dts = audio_pts = 0;
    }
    sendMetaData();
}

void RtmpPublishClient::onPublishStop() {
    if (video_device_ && video_device_->Recording()) {
        video_device_->StopRecord();
    }
    if (audio_device_ && audio_device_->Recording()) {
        audio_device_->StopRecord();
    }
    uv_timer_stop(timer_);
    uv_close((uv_handle_t *) timer_, [](uv_handle_t *handle) {
        RtmpPublishClient *data = static_cast<RtmpPublishClient *>(handle->data);
        data->onStoped();
    });
}

int RtmpPublishClient::YuvDataIsAvailable(const void* yuvData, const uint32_t len, const int32_t width, const int32_t height)
{
    std::vector<MediaPacketShareData *> outpkts;
    int ret;
    ret = video_codec_->encode((char *) yuvData, len, video_pts++, video_dts++, outpkts);
    if (ret >= 0) {
        if (!outpkts.empty()) {
            for (auto iter = outpkts.begin(); iter != outpkts.end(); ++iter) {
                MediaPacketShareData *data = *iter;
                MediaPacketShareData *copy = data->copy();
                {
                    std::lock_guard<std::recursive_mutex> lock(data_mutex);
                    datalist.push_back(copy);
                }
            }
            video_codec_->freePackets(outpkts);
        }
    }
    return 0;
}

int RtmpPublishClient::RecordDataIsAvailable(const void* audioData,
                                  const int32_t nSamples,
                                  const int32_t nBitsPerSample,
                                  const int32_t nChannels,
                                  const uint32_t nSampleRate,
                                  const int32_t nFrameDurationMs)
{
    if (audio) {
        std::vector<MediaPacketShareData *> outpkts;
        int ret;
        int len = nChannels * nSamples * nBitsPerSample / 8;
        ret = audio_codec_->encode((char *) audioData, len, audio_pts++, audio_dts++, outpkts);
        if (ret >= 0) {
            if (!outpkts.empty()) {
                for (auto iter = outpkts.begin(); iter != outpkts.end(); ++iter) {
                    MediaPacketShareData *data = *iter;
                    MediaPacketShareData *copy = data->copy();
                    {
                        std::lock_guard<std::recursive_mutex> lock(data_mutex);
                        datalist.push_back(copy);
                    }
                }
                audio_codec_->freePackets(outpkts);
            }
        }
    }
    return 0;
}

int RtmpPublishClient::PlayNeedMordData(const int32_t nSamples,
                             const int32_t nBitsPerSample,
                             const int32_t nChannels,
                             const uint32_t nSampleRate,
                             const int32_t nFrameDurationMs,
                             void *audioData,
                             int32_t &nSamplesOut)
{
    return 0;
}

void RtmpPublishClient::sendMetaData()
{
    RtmpOnMetaDataPacket *pkg = new RtmpOnMetaDataPacket();
    pkg->metadata->set("duration", RtmpAmf0Any::number(0));
    pkg->metadata->set("width", RtmpAmf0Any::number(width));
    pkg->metadata->set("height", RtmpAmf0Any::number(heigth));
    pkg->metadata->set("framerate", RtmpAmf0Any::number(25));
    pkg->metadata->set("videocodecid", RtmpAmf0Any::number(7));
    if (audio) {
        pkg->metadata->set("audiocodecid", RtmpAmf0Any::number(8));
        pkg->metadata->set("audiosamplerate", RtmpAmf0Any::number(8000));
        pkg->metadata->set("audiosamplesize", RtmpAmf0Any::number(16));
        pkg->metadata->set("stereo", RtmpAmf0Any::boolean(false));
    }
    sendRtmpPacket(pkg, streamid);
    video_device_->InitRecordDevice();
    video_device_->StartRecord();
    if (audio) {
        audio_device_->InitRecordDevice();
        audio_device_->StartRecord();
    }
    uv_timer_start(timer_, &RtmpPublishClient::on_uv_timer, 20, 20);
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
    pushPullStatus_ = RTMP_PUSH_OR_PULL;
}

void RtmpPublishClient::onTimer()
{
    std::lock_guard<std::recursive_mutex> lock(data_mutex);
    while (datalist.size() > 0)
    {
        MediaPacketShareData *share = datalist.front();
        MediaPacketShareData::MediaPacketData *media = share->mediaPacketData;
        if (media != nullptr) {
            if (media->type == 1) {
                // video
                VideoMediaPacketData *data = dynamic_cast<VideoMediaPacketData*>(media->media);
                if (data != nullptr) {
                    if (data->keyframe_) {
                        RtmpAVCPacket *pkg = new RtmpAVCPacket();
                        pkg->sps = data->sps_;
                        pkg->spslen = data->spslen_;
                        pkg->pps = data->pps_;
                        pkg->ppslen = data->ppslen_;
                        sendRtmpPacket(pkg, streamid);
                    }
                    RtmpVideoPacket *pkg = new RtmpVideoPacket();
                    pkg->timestamp = video_timestamp;
                    video_timestamp += 40;
                    pkg->keyframe = data->keyframe_;
                    pkg->nalu = data->data_;
                    pkg->nalulen = data->datalen_;
                    sendRtmpPacket(pkg, streamid);
                }
            }
            else if (media->type == 0) {
                // audio
                AudioMediaPacketData *data = dynamic_cast<AudioMediaPacketData*>(media->media);
                if (data != nullptr) {
                    RtmpAudioPacket *pkg = new RtmpAudioPacket();
                    pkg->timestamp = audio_timestamp;
                    audio_timestamp += 20;
                    pkg->data = data->data_;
                    pkg->datalen = data->datalen_;
                    sendRtmpPacket(pkg, streamid);
                }
            }
        }
        datalist.pop_front();
        delete share;
    }
}

void RtmpPublishClient::on_uv_timer(uv_timer_t *handle)
{
    RtmpPublishClient *data = static_cast<RtmpPublishClient*>(handle->data);
    data->onTimer();
}

RtmpPlayClient::RtmpPlayClient(std::string url, bool audio) : RtmpClient(url, 1, audio)
{

}

RtmpPlayClient::~RtmpPlayClient() {

}

void RtmpPlayClient::startPullStream() {
    play(rtmp_app_, streamid);
}

void RtmpPlayClient::stopPullStream() {

}

void RtmpPlayClient::play(std::string stream, int streamid) {
    if (true) {
        RtmpCallPacket *pkg = new RtmpCallPacket();
        pkg->command_name = "getStreamLength";
        pkg->number = 4;
        pkg->object = RtmpAmf0Any::null();
        pkg->args = RtmpAmf0Any::str(rtmp_app_.c_str());
        sendRtmpPacket(pkg, 0);
    }
    if (true) {
        RtmpPlayPacket *pkg = new RtmpPlayPacket();
        pkg->number = 5;
        pkg->stream_name = stream;
        pkg->start = -1;
        sendRtmpPacket(pkg, streamid);
    }
    if (true) {
        RtmpUserControlPacket *pkg = new RtmpUserControlPacket();
        pkg->event_type = RtmpPCUCSetBufferLength;
        pkg->extra_data = 3;
        sendRtmpPacket(pkg, 0);
    }
}

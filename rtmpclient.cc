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
    pushPullStatus_ = RTMP_CONNECT_APP;
    data_cache_ = new DataCacheBuf();
    rtmp_transport_ = nullptr;
    timer_ = new uv_timer_t;
    timer_->data = static_cast<void*>(this);
    uv_timer_init(NETIOMANAGER->loop_, timer_);

    video_device_ = DevicesFactory::CreateVideoDevice("video", 640, 480, 25);
    video_device_->registerVideoCallback(this);
    video_device_->Init();

    video_codec_ = new VideoCodec(VIDEO_CODEC_NAME);
    video_codec_->initCodec(640, 480, 400000, 25);
    video_pts = video_dts = 0;
    timestamp = 0;
    datalist.clear();
}

RtmpPublishClient::~RtmpPublishClient()
{
    delete data_cache_;
    if (rtmp_transport_)
    {
        delete rtmp_transport_;
    }
    if (timer_)
    {
        delete timer_;
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
    RtmpFMLEStartPacket *pkg = RtmpFMLEStartPacket::create_release_stream(rtmp_stream_);
    sendRtmpPacket(pkg, streamid);
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
    uv_timer_stop(timer_);
    uv_close((uv_handle_t*)timer_, [](uv_handle_t* handle){
       ILOG("timer close finish");
    });
    return 0;
}

int RtmpPublishClient::YuvDataIsAvailable(const void* yuvData, const uint32_t len, const int32_t width, const int32_t height)
{
    std::vector<MediaPacketShareData*> outpkts;
    int ret;
    ret = video_codec_->encode((char*)yuvData, len, video_pts++, video_dts++, outpkts);
    if (ret >= 0) {
        if (!outpkts.empty()) {
            for (auto iter = outpkts.begin(); iter != outpkts.end(); ++iter)
            {
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
    std::string url = "rtmp://"+serveraddr_.ip+":"+std::to_string(serveraddr_.port)+"/"+rtmp_app_;
    pkg->command_object->set("tcUrl", RtmpAmf0Any::str(url.c_str()));
    sendRtmpPacket(pkg, 0);
    if (true) {
        RtmpSetWindowAckSizePacket *pkg = new RtmpSetWindowAckSizePacket();
        pkg->window_ack_size = 2500000;
        sendRtmpPacket(pkg, 0);
    }
    pushPullStatus_ = RTMP_CONNECT_APP;
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
    pushPullStatus_ = RTMP_CREATE_STREAM;
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

void RtmpPublishClient::sendMetaData()
{
    RtmpOnMetaDataPacket *pkg = new RtmpOnMetaDataPacket();
    pkg->metadata->set("duration", RtmpAmf0Any::number(0));
    pkg->metadata->set("width", RtmpAmf0Any::number(768));
    pkg->metadata->set("height", RtmpAmf0Any::number(320));
    pkg->metadata->set("framerate", RtmpAmf0Any::number(25));
    pkg->metadata->set("videocodecid", RtmpAmf0Any::number(RTMP_VIDEO_CODEC_H264));
    pkg->metadata->set("framerate", RtmpAmf0Any::number(RTMP_AUDIO_CODEC_G711U));
    //pkg->metadata->set("audiosamplerate", RtmpAmf0Any::number(8000));
    //pkg->metadata->set("audiosamplesize", RtmpAmf0Any::number(16));
    //pkg->metadata->set("stereo", RtmpAmf0Any::boolean(true));
    sendRtmpPacket(pkg, streamid);
    video_device_->InitRecordDevice();
    video_device_->StartRecord();
    uv_timer_start(timer_, &RtmpPublishClient::on_uv_timer, 40, 40);
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
            if (pushPullStatus_ == RTMP_CONNECT_APP) {
                RtmpConnectResponsePacket *pkg = dynamic_cast<RtmpConnectResponsePacket *>(packet);
                if (pkg != nullptr) {
                    ILOG("recv connect response\n");
                    createStream(rtmp_stream_);
                }
            }
            else if (pushPullStatus_ == RTMP_CREATE_STREAM) {
                RtmpCreateStreamResponsePacket *pkg = dynamic_cast<RtmpCreateStreamResponsePacket *>(packet);
                if (pkg != nullptr) {
                    ILOG("recv create stream response stream id=%d\n", (int) pkg->streamid_);
                    streamid = (int)pkg->streamid_;
                    publish(rtmp_stream_, (int) pkg->streamid_);
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
                            sendMetaData();
                        }
                        else if (value == "NetStream.Unpublish.Success") {
                            ILOG("close tcp socket\n");
                            rtmp_socket_->close();
                        }
                    }

                }
            }
        }
    }
    return ret;
}

void RtmpPublishClient::onTimer()
{
    std::lock_guard<std::recursive_mutex> lock(data_mutex);
    if (datalist.size() > 0)
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
                    pkg->timestamp = timestamp;
                    timestamp += 40;
                    pkg->keyframe = data->keyframe_;
                    pkg->nalu = data->data_;
                    pkg->nalulen = data->datalen_;
                    sendRtmpPacket(pkg, streamid);
                }
            }
            else if (media->type == 0) {
                // audio
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

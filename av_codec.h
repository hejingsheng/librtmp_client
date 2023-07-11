//
// Created by hejingsheng on 2023/7/6.
//

#ifndef RTMP_CLIENT_AV_CODEC_H
#define RTMP_CLIENT_AV_CODEC_H

#include <string>
#include <vector>
extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
}

const int RTC_MEDIA_TYPE_AUDIO = 0;
const int RTC_MEDIA_TYPE_VIDEO = 1;

const std::string AUDIO_CODEC_NAME = "libopus";

class MediaData
{
public:
    MediaData();
    virtual ~MediaData();

public:
    void copyData(uint8_t *data, int len);
    uint8_t* getData(int &length);

public:
    uint8_t *data_;
    int datalen_;
    int64_t pts;
    int64_t dts;

public:
    int type_;  // 0 audio 1 video
};

class MediaFrameData : public MediaData
{
public:
    MediaFrameData(int type);
    virtual ~MediaFrameData();
};

class AudioMediaPacketData : public MediaData
{
public:
    AudioMediaPacketData();
    virtual ~AudioMediaPacketData();
};

class VideoMediaPacketData : public MediaData
{
public:
    VideoMediaPacketData();
    virtual ~VideoMediaPacketData();

public:
    void copySpspps(uint8_t *data, int len);
    uint8_t* getSps(int &length);
    uint8_t* getPps(int &length);
    VideoMediaPacketData *copy();

public:
    bool keyframe_;
    uint8_t *sps_;
    uint8_t *pps_;
    int spslen_;
    int ppslen_;
};

class MediaPacketShareData
{
public:
    class MediaPacketData
    {
    public:
        MediaData *media;
        int type; // 0 audio 1 video
        int shareCnt;

    public:
        MediaPacketData();
        virtual ~MediaPacketData();
    };

public:
    MediaPacketData *mediaPacketData;

public:
    MediaPacketShareData();
    virtual ~MediaPacketShareData();

public:
    int getMediaType();
    MediaPacketShareData *copy();
    void create(MediaData *media, int type);
};

class AudioCodec
{
public:
    AudioCodec(std::string codec_name);
    virtual ~AudioCodec();

public:
    int initCodec(int32_t sampleRate, int32_t bitRate, int32_t channels);
    int encode(char *pcm, int len, int64_t pts, int64_t dts, std::vector<MediaPacketShareData*> &pkts);
    int decode(char *data, int len);
    int freePackets(std::vector<MediaPacketShareData*> &pkts);

private:
    int initEncodeCodec(int32_t sampleRate, int32_t bitRate, int32_t channels);
    int initDecodeCodec();

private:
    std::string codec_name_;

private:
    AVCodecContext *encode_codec_ctx;
    AVFrame *encode_frame;
    AVPacket *encode_pkt;

    AVCodecContext *decode_codec_ctx;
    AVFrame *decode_frame;
    AVPacket *decode_pkt;

    char av_errors[1024];

};

const std::string VIDEO_CODEC_NAME = "libx264";
class VideoCodec
{
public:
    VideoCodec(std::string codec_name);
    virtual ~VideoCodec();

public:
    int initCodec(uint32_t w, uint32_t h, int32_t bitRate, int32_t framerate);
    int encode(char *yuv, int len, int64_t pts, int64_t dts, std::vector<MediaPacketShareData*> &pkts);
    int decode(char *data, int len);
    int freePackets(std::vector<MediaPacketShareData*> &pkts);

private:
    int initEncodeCodec(uint32_t w, uint32_t h, int32_t bitRate, int32_t framerate);
    int initDecodeCodec();
    void parseH264(uint8_t *h264, int len, int64_t pts, int64_t dts, std::vector<MediaPacketShareData*> &pkts);

private:
    std::string codec_name_;

private:
    AVCodecContext *encode_codec_ctx;
    AVFrame *encode_frame;
    AVPacket *encode_pkt;

    AVCodecContext *decode_codec_ctx;
    AVFrame *decode_frame;
    AVPacket *decode_pkt;

    char av_errors[1024];
};


#endif //RTMP_CLIENT_AV_CODEC_H

//
// Created by hejingsheng on 2023/7/6.
//

#include "av_codec.h"
#include "logger.h"

char startcode[4] = {0x00,0x00,0x00,0x01};
static void write_to_file(void *data, int len)
{
    FILE *fp;
    fp = fopen("record.h264", "ab");
    //fwrite(&len, sizeof(int), 1, fp);
    fwrite(data, len, 1, fp);
    fclose(fp);
}

MediaData::MediaData()
{
    data_ = nullptr;
    datalen_ = 0;
    pts = dts = 0;
    type_ = -1;
}
MediaData::~MediaData()
{
    if (data_)
    {
        delete[] data_;
    }
    data_ = nullptr;
    datalen_ = 0;
}

void MediaData::copyData(uint8_t *data, int len)
{
    data_ = new uint8_t[len];
    memcpy(data_, data, len);
    datalen_ = len;
}

uint8_t* MediaData::getData(int &length)
{
    length = datalen_;
    return data_;
}

MediaFrameData::MediaFrameData(int type)
{
    type_ = type;
}

MediaFrameData::~MediaFrameData()
{

}

AudioMediaPacketData::AudioMediaPacketData()
{
    type_ = RTC_MEDIA_TYPE_AUDIO;
}

AudioMediaPacketData::~AudioMediaPacketData()
{

}

VideoMediaPacketData::VideoMediaPacketData()
{
    sps_ = pps_ = nullptr;
    spslen_ = ppslen_ = 0;
    keyframe_ = false;
    type_ = RTC_MEDIA_TYPE_VIDEO;
}

VideoMediaPacketData::~VideoMediaPacketData()
{
    if (sps_)
    {
        delete[] sps_;
    }
    sps_ = nullptr;
    spslen_ = 0;
    if (pps_)
    {
        delete[] pps_;
    }
    pps_ = nullptr;
    ppslen_ = 0;
}

void VideoMediaPacketData::copySpspps(uint8_t *data, int len)
{
    int i = 4;
    uint8_t *sps = data + 4;
    uint8_t *pps;
    while (i < len)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01)
        {
            pps = data + i + 4;
            break;
        }
        i++;
    }
    sps_ = new uint8_t[i - 4];
    memcpy(sps_, sps, i - 4);
    spslen_ = i - 4;
    pps_ = new uint8_t[len - i - 4];
    memcpy(pps_, pps, len - i - 4);
    ppslen_ = len - i - 4;
}

uint8_t* VideoMediaPacketData::getSps(int &length)
{
    length = spslen_;
    return sps_;
}

uint8_t* VideoMediaPacketData::getPps(int &length)
{
    length = ppslen_;
    return pps_;
}

VideoMediaPacketData * VideoMediaPacketData::copy()
{
    VideoMediaPacketData *pkt = new VideoMediaPacketData();
    pkt->keyframe_ = keyframe_;
    if (keyframe_)
    {
        pkt->sps_ = new uint8_t[spslen_];
        memcpy(pkt->sps_, sps_, spslen_);
        pkt->spslen_ = spslen_;
        pkt->pps_ = new uint8_t[ppslen_];
        memcpy(pkt->pps_, pps_, ppslen_);
        pkt->ppslen_ = ppslen_;
    }
    pkt->dts = dts;
    pkt->pts = pts;
    pkt->copyData(data_, datalen_);
    return nullptr;
}

MediaPacketShareData::MediaPacketData::MediaPacketData() {
    media = nullptr;
    type = 0;
    shareCnt = 0;
}

MediaPacketShareData::MediaPacketData::~MediaPacketData() {
    if (media) {
        delete media;
    }
}

MediaPacketShareData::MediaPacketShareData() {
    mediaPacketData = nullptr;
}

MediaPacketShareData::~MediaPacketShareData() {
    if (mediaPacketData) {
        mediaPacketData->shareCnt--;
        if (mediaPacketData->shareCnt <= 0) {
            delete mediaPacketData;
        }
        mediaPacketData = nullptr;
    }
}

int MediaPacketShareData::getMediaType() {
    if (mediaPacketData) {
        return mediaPacketData->type;
    }
    return 0;
}

MediaPacketShareData* MediaPacketShareData::copy() {
    MediaPacketShareData *data = new MediaPacketShareData();
    data->mediaPacketData = mediaPacketData;
    mediaPacketData->shareCnt++;
    return data;
}

void MediaPacketShareData::create(MediaData *media, int type)
{
    mediaPacketData = new MediaPacketData();
    mediaPacketData->shareCnt = 1;
    mediaPacketData->media = media;
    mediaPacketData->type = type;
}

AudioCodec::AudioCodec(std::string codec_name) {
    codec_name_ = codec_name;
    encode_codec_ctx = nullptr;
    encode_pkt = nullptr;
    encode_frame = nullptr;
    decode_codec_ctx = nullptr;
    decode_pkt = nullptr;
    decode_frame = nullptr;
}

AudioCodec::~AudioCodec() {
    if (encode_codec_ctx) {
        avcodec_free_context(&encode_codec_ctx);
        encode_codec_ctx = nullptr;
    }
    if (encode_pkt) {
        av_packet_free(&encode_pkt);
        encode_pkt = nullptr;
    }
    if (encode_frame) {
        av_frame_free(&encode_frame);
        encode_frame = nullptr;
    }
    if (decode_codec_ctx) {
        avcodec_free_context(&decode_codec_ctx);
        decode_codec_ctx = NULL;
    }
    if (decode_frame) {
        av_frame_free(&decode_frame);
        decode_frame = nullptr;
    }
    if (decode_pkt) {
        av_packet_free(&decode_pkt);
        decode_pkt = nullptr;
    }
}

int AudioCodec::initCodec(int32_t sampleRate, int32_t bitRate, int32_t channels) {
    int ret = 0;

    // init encoder
    ret = initEncodeCodec(sampleRate, bitRate, channels);
    if (ret < 0) {
        return ret;
    }

    // init decoder
    ret = initDecodeCodec();
    if (ret < 0)
    {
        return -1;
    }
    return 0;
}

int AudioCodec::encode(char *pcm, int len, int64_t pts, int64_t dts, std::vector<MediaPacketShareData *> &pkts) {
    int ret;

    av_init_packet(encode_pkt);
    encode_pkt->data = NULL;
    encode_pkt->size = 0;
    av_frame_make_writable(encode_frame);
    memcpy(encode_frame->data[0], pcm, len);
    //memcpy(encode_frame->data[1], pcm + len / 2, len / 2);
    encode_frame->pts = pts;

    ret = avcodec_send_frame(encode_codec_ctx, encode_frame);
    if (ret < 0) {
        ELOG("send frame fail\n");
        av_strerror(ret, av_errors, 1024);
        return -1;
    }
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(encode_codec_ctx, encode_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        else if (ret < 0) {
            ELOG("encode fail\n");
            return -1;
        }
        // get encode data
        if (encode_pkt->pts < 0)
        {
            WLOG("drop pts < 0 %ld\n", encode_pkt->pts);
            return 0;
        }
        AudioMediaPacketData *media = new AudioMediaPacketData();
        int64_t pts = encode_pkt->pts;
        int64_t dts = encode_pkt->dts;
        media->copyData(encode_pkt->data, encode_pkt->size);
        //write_to_file(encode_pkt->data, encode_pkt->size);
        //decode((char*)encode_pkt->data, encode_pkt->size);
        media->pts = pts;
        media->dts = dts;
        MediaPacketShareData *data = new MediaPacketShareData();
        data->create(media, 0);
        pkts.push_back(data);
        av_packet_unref(encode_pkt);
    }
    return 0;
}

int AudioCodec::decode(char *data, int len) {
    int ret;

    decode_pkt->data = (uint8_t*)(data);
    decode_pkt->size = len;
    ret = avcodec_send_packet(decode_codec_ctx, decode_pkt);
    if (ret < 0)
    {
        ELOG("send packet fail\n");
        return -1;
    }
    while (ret >= 0)
    {
        ret = avcodec_receive_frame(decode_codec_ctx, decode_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return 0;
        }
        else if (ret < 0)
        {
            ELOG("decode fail\n");
            return -1;
        }
        // get decode data
        //write_to_file(decode_frame->data[0], decode_frame->nb_samples * 2 * decode_codec_ctx->channels);
    }
    return 0;
}

int AudioCodec::freePackets(std::vector<MediaPacketShareData *> &pkts) {
    for (auto iter = pkts.begin(); iter != pkts.end(); iter++) {
        MediaPacketShareData *pkg = *iter;
        if (pkg) {
            delete pkg;
        }
    }
    return 0;
}

int AudioCodec::initEncodeCodec(int32_t sampleRate, int32_t bitRate, int32_t channels) {
    int ret;

    // init encoder
    const AVCodec *audio_codec = avcodec_find_encoder_by_name(codec_name_.c_str());
    //const AVCodec *audio_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_MULAW);
    if (audio_codec == NULL) {
        ELOG("not find audio codec %s\n", codec_name_.c_str());
        return -1;
    }
    encode_codec_ctx = avcodec_alloc_context3(audio_codec);
    if (encode_codec_ctx == NULL) {
        ELOG("not alloc audio codec context\n");
        return -1;
    }
    encode_codec_ctx->sample_rate = sampleRate;
    encode_codec_ctx->bit_rate = bitRate;
    encode_codec_ctx->channels = channels;
    encode_codec_ctx->sample_fmt = audio_codec->sample_fmts[0];
    ret = avcodec_open2(encode_codec_ctx, audio_codec, NULL);
    if (ret < 0) {
        ELOG("audio codec open fail\n");
        av_strerror(ret, av_errors, 1024);
        return -1;
    }
    encode_frame = av_frame_alloc();
    if (encode_frame == NULL) {
        ELOG("frame alloc fail\n");
        return -1;
    }
    encode_frame->nb_samples = encode_codec_ctx->frame_size;
    encode_frame->nb_samples = 160;
    encode_frame->format = encode_codec_ctx->sample_fmt;
    encode_frame->channels = encode_codec_ctx->channels;
    av_channel_layout_copy(&encode_frame->ch_layout, &encode_codec_ctx->ch_layout);
    //encode_frame->channel_layout = encode_codec_ctx->channel_layout;
    //encode_frame->time_base = encode_codec_ctx->time_base;
    ret = av_frame_get_buffer(encode_frame, 0);
    if (ret < 0) {
        ELOG("frame buf alloc fail\n");
        return -1;
    }
    encode_pkt = av_packet_alloc();
    if (encode_pkt == NULL) {
        ELOG("pkg alloc fail\n");
        return -1;
    }
    return 0;
}

int AudioCodec::initDecodeCodec() {
    int ret;
    const AVCodec *audio_codec = avcodec_find_decoder_by_name(codec_name_.c_str());
    if (!audio_codec)
    {
        ELOG("can not find %s codec\n", codec_name_.c_str());
        return -1;
    }
    decode_codec_ctx = avcodec_alloc_context3(audio_codec);
    if (!decode_codec_ctx)
    {
        ELOG("can not find alloc codec ctx\n");
        return -1;
    }

    ret = avcodec_open2(decode_codec_ctx, audio_codec, NULL);
    if (ret < 0)
    {
        av_strerror(ret, av_errors, 1024);
        ELOG("codec open fail %s\n", av_errors);
        return -1;
    }
    decode_codec_ctx->channel_layout = av_get_default_channel_layout(decode_codec_ctx->channels);
    decode_frame = av_frame_alloc();
    if (decode_frame == NULL)
    {
        ELOG("frame alloc fail\n");
        return -1;
    }
    decode_pkt = av_packet_alloc();
    if (decode_pkt == NULL)
    {
        ELOG("pkg alloc fail\n");
        return -1;
    }
    return 0;
}

VideoCodec::VideoCodec(std::string codec_name) {
    codec_name_ = codec_name;
    encode_codec_ctx = nullptr;
    encode_pkt = nullptr;
    encode_frame = nullptr;
    decode_codec_ctx = nullptr;
    decode_pkt = nullptr;
    decode_frame = nullptr;
}

VideoCodec::~VideoCodec() {
    if (encode_codec_ctx) {
        avcodec_free_context(&encode_codec_ctx);
        encode_codec_ctx = nullptr;
    }
    if (encode_pkt) {
        av_packet_free(&encode_pkt);
        encode_pkt = nullptr;
    }
    if (encode_frame) {
        av_frame_free(&encode_frame);
        encode_frame = nullptr;
    }
    if (decode_codec_ctx) {
        avcodec_free_context(&decode_codec_ctx);
        decode_codec_ctx = NULL;
    }
    if (decode_frame) {
        av_frame_free(&decode_frame);
        decode_frame = nullptr;
    }
    if (decode_pkt) {
        av_packet_free(&decode_pkt);
        decode_pkt = nullptr;
    }
}

int VideoCodec::initCodec(uint32_t w, uint32_t h, int32_t bitRate, int32_t framerate) {
    int ret;

    // init encoder
    ret = initEncodeCodec(w, h, bitRate, framerate);
    if (ret < 0)
    {
        return -1;
    }

    // init decoder
    ret = initDecodeCodec();
    if (ret < 0)
    {
        return -1;
    }
    return 0;
}

int VideoCodec::encode(char *yuv, int len, int64_t pts, int64_t dts, std::vector<MediaPacketShareData *> &pkts) {
    int ret;
    int w,h;

    av_init_packet(encode_pkt);
    encode_pkt->data = NULL;
    encode_pkt->size = 0;
    av_frame_make_writable(encode_frame);
    w = encode_codec_ctx->width;
    h = encode_codec_ctx->height;
    memcpy(encode_frame->data[0], yuv, w*h);
    memcpy(encode_frame->data[1], yuv + w * h, w*h / 4);
    memcpy(encode_frame->data[2], yuv + w * h * 5 / 4, w*h / 4);
    encode_frame->pts = pts;

    ret = avcodec_send_frame(encode_codec_ctx, encode_frame);
    if (ret < 0) {
        ELOG("send frame fail\n");
        return -1;
    }
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(encode_codec_ctx, encode_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        else if (ret < 0) {
            ELOG("encode fail\n");
            return -1;
        }
        // get encode data
        int64_t pts = encode_pkt->pts;
        int64_t dts = encode_pkt->dts;
        parseH264(encode_pkt->data, encode_pkt->size, pts, dts, pkts);
        av_packet_unref(encode_pkt);
    }
    return 0;
}

int VideoCodec::decode(char *data, int len) {
    return 0;
}

int VideoCodec::freePackets(std::vector<MediaPacketShareData *> &pkts) {
    for (auto iter = pkts.begin(); iter != pkts.end(); ++iter)
    {
        MediaPacketShareData *data = *iter;
        if (data)
        {
            delete data;
        }
    }
    pkts.clear();
    return 0;
}

int VideoCodec::initEncodeCodec(uint32_t w, uint32_t h, int32_t bitRate, int32_t framerate) {
    int ret;

    // init encoder
    const AVCodec *video_codec = avcodec_find_encoder_by_name(codec_name_.c_str());
    if (video_codec == NULL) {
        ELOG("not find video codec %s\n", codec_name_.c_str());
        return -1;
    }
    encode_codec_ctx = avcodec_alloc_context3(video_codec);
    if (encode_codec_ctx == NULL) {
        ELOG("can not alloc video codec\n");
        return -1;
    }
    encode_codec_ctx->width = w;
    encode_codec_ctx->height = h;
    encode_codec_ctx->bit_rate = bitRate;
    encode_codec_ctx->framerate = (AVRational) { framerate, 1 };;
    encode_codec_ctx->has_b_frames = 0;
    encode_codec_ctx->max_b_frames = 0;
    encode_codec_ctx->gop_size = 10;
    encode_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    encode_codec_ctx->profile = FF_PROFILE_H264_BASELINE;
    encode_codec_ctx->level = 0x1f;
    encode_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    encode_codec_ctx->time_base = (AVRational) { 1, framerate};
    ret = avcodec_open2(encode_codec_ctx, video_codec, NULL);
    if (ret < 0) {
        av_strerror(ret, av_errors, 1024);
        ELOG("codec open fail %s\n", av_errors);
        return -1;
    }
    encode_frame = av_frame_alloc();
    if (encode_frame == NULL) {
        ELOG("frame alloc fail\n");
        return -1;
    }
    encode_frame->format = encode_codec_ctx->pix_fmt;
    encode_frame->width = encode_codec_ctx->width;
    encode_frame->height = encode_codec_ctx->height;
    ret = av_frame_get_buffer(encode_frame, 0);
    if (ret < 0) {
        ELOG("out of memery\n");
        return -1;
    }
    encode_pkt = av_packet_alloc();
    if (encode_pkt == NULL) {
        ELOG("pkg alloc fail\n");
        return -1;
    }
    return 0;
    return 0;
}

int VideoCodec::initDecodeCodec() {
    return 0;
}

void VideoCodec::parseH264(uint8_t *h264, int len, int64_t pts, int64_t dts, std::vector<MediaPacketShareData *> &pkts) {
    int i = 0;
    std::vector<uint8_t> nalus;
    uint8_t naluHeader;
    uint8_t nalType;
    bool findStartCode = false;

    nalus.clear();
    while (i < len)
    {
        if (h264[i] == 0x00 && h264[i + 1] == 0x00 && h264[i + 2] == 0x01 && (i + 2) < len)
        {
            // start code 0x00 0x00 0x01
            i += 3;
            findStartCode = true;
        }
        else if (h264[i] == 0x00 && h264[i + 1] == 0x00 && h264[i + 2] == 0x00 && h264[i + 3] == 0x01 && (i + 3) < len)
        {
            // start code 0x00 0x00 0x00 0x01
            i += 4;
            findStartCode = true;
        }
        else
        {
            findStartCode = false;
            nalus.push_back(h264[i]);
            i++;
        }
        if (findStartCode)
        {
            if (!nalus.empty())
            {
                VideoMediaPacketData *media = new VideoMediaPacketData();
                media->copyData(&nalus[0], nalus.size());
                media->pts = pts;
                media->dts = dts;
                naluHeader = nalus[0];
                nalType = naluHeader & 0x1f;
                if (nalType == 5)
                {
                    media->keyframe_ = true;
                    media->copySpspps(encode_codec_ctx->extradata, encode_codec_ctx->extradata_size);
                    //write_to_file(encode_codec_ctx->extradata, encode_codec_ctx->extradata_size);
                }
                //write_to_file(startcode, 4);
                //write_to_file(&nalus[0], nalus.size());
                MediaPacketShareData *data = new MediaPacketShareData();
                data->create(media, 1);
                pkts.push_back(data);
                nalus.clear();
            }
        }
    }
    if (!nalus.empty())
    {
        VideoMediaPacketData *media = new VideoMediaPacketData();
        media->copyData(&nalus[0], nalus.size());
        media->pts = pts;
        media->dts = dts;
        naluHeader = nalus[0];
        nalType = naluHeader & 0x1f;
        if (nalType == 5)
        {
            media->keyframe_ = true;
            media->copySpspps(encode_codec_ctx->extradata, encode_codec_ctx->extradata_size);
            //write_to_file(encode_codec_ctx->extradata, encode_codec_ctx->extradata_size);
        }
        //write_to_file(startcode, 4);
        //write_to_file(&nalus[0], nalus.size());
        MediaPacketShareData *data = new MediaPacketShareData();
        data->create(media, 1);
        pkts.push_back(data);
        nalus.clear();
    }
}


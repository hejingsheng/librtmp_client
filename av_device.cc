//
// Created by hejingsheng on 2023/7/6.
//

#include "av_device.h"
#include "logger.h"

static void write_to_file(void *data, int len)
{
    FILE *fp;
    fp = fopen("record.yuv", "ab");
    fwrite(data, len, 1, fp);
    fclose(fp);
}

static uint32_t currentPos = 0;
static void generalPcmData(void *audio_samples, int32_t num_samples, int32_t sampleRate)
{
    //int16_t *samples = nullptr;
    //int16_t *ptr16Out = nullptr;
    //int16_t *ptr16In = nullptr;
    //float first = 0;
    //float tincr = 2 * M_PI * 110.0 / sampleRate;
    ///* increment frequency by 110 Hz per second */
    //float tincr2 = 2 * M_PI * 110.0 / sampleRate / sampleRate;

    //ptr16Out = (int16_t *)audio_samples;
    //for (int i = 0; i < num_samples; i++) {
    //	*ptr16Out = (int16_t)(sin(first) * 10000); // left
    //	ptr16Out++;
    //	//*ptr16Out = 0; // right (same as left sample)
    //	//ptr16Out++;
    //	first += tincr;
    //	tincr += tincr2;
    //}
    //write_to_file(audio_samples, num_samples * 2);
    FILE *fp;
    int16_t *ptr16Out = nullptr;

    fp = fopen("source1.pcm", "r");
    fseek(fp, currentPos, 0);
    int16_t sample[960];
    fread(sample, sizeof(int16_t), 960, fp);
    currentPos += (960 * sizeof(int16_t));
    if (currentPos >= 5760000)
    {
        currentPos = 0;
    }
    ptr16Out = (int16_t *)audio_samples;
    for (int i = 0; i < num_samples; i++) {
        *ptr16Out = sample[i]; // left
        ptr16Out++;
        //*ptr16Out = sample[i]; // right (same as left sample)
        //ptr16Out++;
    }
    fclose(fp);
    //write_to_file(audio_samples, num_samples * 2 * 2);
}

static int frameNo = 0;
static void generalYuvData(uint8_t *yuv, uint32_t w, uint32_t h)
{
    uint8_t *yuv_y, *yuv_u, *yuv_v;
    int x, y, i;
    yuv_y = yuv;
    yuv_u = yuv + w * h;
    yuv_v = yuv + w * h * 5 / 4;
    i = frameNo;
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            yuv_y[y * w + x] = x + y + i * 3;
        }
    }
    for (y = 0; y < h/2; y++)
    {
        for (x = 0; x < w/2; x++)
        {
            yuv_u[y*w / 2 + x] = 128 + y + i * 2;
            yuv_v[y*w / 2 + x] = 64 + x + i * 5;
        }
    }
    frameNo++;
    if (frameNo == 25)
    {
        frameNo = 0;
    }
}

BaseDevices::BaseDevices(std::string deviceName) : deviceName_(deviceName)
{
    bInit_ = false;
}

BaseDevices::~BaseDevices()
{

}

int BaseDevices::Init()
{
    bInit_ = true;
    return 0;
}

bool BaseDevices::Initialized() const
{
    return bInit_;
}

void BaseDevices::registerAudioCallback(AudioDataTransport *callback)
{

}

void BaseDevices::registerVideoCallback(VideoDataTransport *callback)
{

}

AudioDevices::AudioDevices(std::string deviceName, uint32_t sampleRate, int32_t bits, int32_t channels, int32_t frameduration) : BaseDevices(deviceName)
{
    deviceType_ = "audio";
    nSampleRate_ = sampleRate;
    nBitsPerSample_ = bits;
    nChannels_ = channels;
    nFrameDuration_ = frameduration;
    playing_ = recording_ = false;
    play_is_initialized_ = rec_is_initialized_ = false;
    audio_callback_ = nullptr;
    start_ = false;
    pcmPlayData_ = pcmRecordData_ = nullptr;
    //pNS_inst = nullptr;
}

AudioDevices::~AudioDevices()
{
    ILOG("destroy audio device\n");
    StopPlay();
    StopRecord();
    if (audioDeviceThread_.joinable())
    {
        sleepMutex_.lock();
        audioThreadCond_.notify_all();
        sleepMutex_.unlock();
        audioDeviceThread_.join();
    }
    if (pcmRecordData_)
    {
        delete[] pcmRecordData_;
    }
    pcmRecordData_ = nullptr;
    if (pcmPlayData_)
    {
        delete[] pcmPlayData_;
    }
    pcmPlayData_ = nullptr;
    //if (pNS_inst)
    //{
    //	WebRtcNsx_Free(pNS_inst);
    //	pNS_inst = nullptr;
    //}
}

int AudioDevices::InitPlayDevice()
{
    int32_t nBytesPerSample = nBitsPerSample_ / 8;
    int32_t nSamplePerFrame = nSampleRate_ / (1000 / nFrameDuration_);
    pcmPlayData_ = new char[nBytesPerSample * nSamplePerFrame * nChannels_];
    play_is_initialized_ = true;
    return 0;
}

int AudioDevices::StartPlay()
{
    if (!play_is_initialized_)
    {
        ELOG("play device is not init\n");
        return -1;
    }
    std::lock_guard<std::recursive_mutex> lock(audioThreadMutex_);
    playing_ = true;
    if (!start_)
    {
        start_ = true;
        audioDeviceThread_ = std::thread(&AudioDevices::AudioDeviceThread, this);
    }
    else
    {
        WLOG("audio thread have start\n");
    }
    return 0;
}

int AudioDevices::StopPlay()
{
    std::lock_guard<std::recursive_mutex> lock(audioThreadMutex_);
    playing_ = false;
    return 0;
}

bool AudioDevices::Playing() const
{
    return playing_;
}

int AudioDevices::InitRecordDevice()
{
    int32_t nBytesPerSample = nBitsPerSample_ / 8;
    int32_t nSamplePerFrame = nSampleRate_ / (1000 / nFrameDuration_);
    pcmRecordData_ = new char[nBytesPerSample * nSamplePerFrame * nChannels_];

    //��ʼ����������ģ��
    //pNS_inst = WebRtcNsx_Create();

    //if (0 != WebRtcNsx_Init(pNS_inst, nSampleRate_))
    //{
    //	return -1;
    //}

    //if (0 != WebRtcNsx_set_policy(pNS_inst, 2))
    //{
    //	return -1;
    //}

    rec_is_initialized_ = true;
    return 0;
}

int AudioDevices::StartRecord()
{
    if (!rec_is_initialized_)
    {
        ELOG("record device is not init\n");
        return -1;
    }
    std::lock_guard<std::recursive_mutex> lock(audioThreadMutex_);
    recording_ = true;
    if (!start_)
    {
        start_ = true;
        audioDeviceThread_ = std::thread(&AudioDevices::AudioDeviceThread, this);
    }
    else
    {
        WLOG("audio thread have start\n");
    }
    return 0;
}

int AudioDevices::StopRecord()
{
    std::lock_guard<std::recursive_mutex> lock(audioThreadMutex_);
    recording_ = false;
    return 0;
}

bool AudioDevices::Recording() const
{
    return recording_;
}

void AudioDevices::registerAudioCallback(AudioDataTransport *callback)
{
    audio_callback_ = callback;
}

bool AudioDevices::ShouldStopAudioThread()
{
    return !recording_ && !playing_;
}

void AudioDevices::GetPlayFrameData()
{
    int ret = 0;
    if (audio_callback_ != nullptr)
    {
        int32_t nSamplePerFrame = nSampleRate_ / (1000 / nFrameDuration_);
        int32_t out = 0;
        ret = audio_callback_->PlayNeedMordData(nSamplePerFrame, nBitsPerSample_, nChannels_, nSampleRate_, nFrameDuration_, pcmPlayData_, out);
        if (ret != 0)
        {
            ELOG("need more data error\n");
            return;
        }
        if (nSamplePerFrame != out)
        {
            //ELOG("Sample is error\n");
            return;
        }
    }
}

void AudioDevices::SendRecordFrameData()
{
    int ret = 0;
    if (audio_callback_ != nullptr)
    {
        int32_t nSamplePerFrame = nSampleRate_ / (1000 / nFrameDuration_);
        generalPcmData(pcmRecordData_, nSamplePerFrame, nSampleRate_);
        //int16_t *srcData = new int16_t[nSamplePerFrame];
        //memcpy(srcData, pcmRecordData_, nSamplePerFrame * sizeof(int16_t));
        //WebRtcNsx_Process(pNS_inst, &srcData, 1, (short* const*)(&pcmRecordData_));
        //delete[] srcData;
        //write_to_file(pcmRecordData_, nSamplePerFrame * sizeof(int16_t));
        ret = audio_callback_->RecordDataIsAvailable(pcmRecordData_, nSamplePerFrame, nBitsPerSample_, nChannels_, nSampleRate_, nFrameDuration_);
        if (ret != 0)
        {
            ELOG("record data error\n");
            return;
        }
    }
}

void AudioDevices::AudioDeviceThread()
{
    ILOG("start audio device thread\n");
    while (start_)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(audioThreadMutex_);
            if (playing_)
            {
                // get play data
                GetPlayFrameData();
            }
            if (recording_)
            {
                // send record data
                SendRecordFrameData();
            }
            if (ShouldStopAudioThread())
            {
                start_ = false;
                break;
            }
        }
        sleepMutex_.lock();
        audioThreadCond_.wait_for(sleepMutex_, std::chrono::milliseconds(nFrameDuration_ - 1));
        sleepMutex_.unlock();
    }
    ILOG("audio device thread end\n");
}

VideoDevices::VideoDevices(std::string deviceName, uint32_t width, uint32_t height, uint32_t framerate) : BaseDevices(deviceName)
{
    deviceType_ = "video";
    mWidth_ = width;
    mHeight_ = height;
    mFrameRate_ = framerate;
    nFrameDuration_ = 1000 / mFrameRate_;
    playing_ = recording_ = false;
    play_is_initialized_ = rec_is_initialized_ = false;
    video_callback_ = nullptr;
    start_ = false;
    yuvData_ = nullptr;
}

VideoDevices::~VideoDevices()
{
    ILOG("destroy video device\n");
    StopPlay();
    StopRecord();
    if (videoDeviceThread_.joinable())
    {
        sleepMutex_.lock();
        videoThreadCond_.notify_all();
        sleepMutex_.unlock();
        videoDeviceThread_.join();
    }
    if (yuvData_)
    {
        delete[] yuvData_;
    }
    yuvData_ = nullptr;
}

int VideoDevices::InitPlayDevice()
{
    return 0;
}

int VideoDevices::StartPlay()
{
    return 0;
}

int VideoDevices::StopPlay()
{
    return 0;
}

bool VideoDevices::Playing() const
{
    return false;
}

int VideoDevices::InitRecordDevice()
{
    yuvData_ = new uint8_t[mWidth_*mHeight_ * 3 / 2];
    rec_is_initialized_ = true;
    return 0;
}

int VideoDevices::StartRecord()
{
    if (!rec_is_initialized_)
    {
        ELOG("record device is not init\n");
        return -1;
    }
    std::lock_guard<std::recursive_mutex> lock(videoThreadMutex_);
    recording_ = true;
    if (!start_)
    {
        start_ = true;
        videoDeviceThread_ = std::thread(&VideoDevices::VideoDeviceThread, this);
    }
    else
    {
        WLOG("audio thread have start\n");
    }
    return 0;
}

int VideoDevices::StopRecord()
{
    std::lock_guard<std::recursive_mutex> lock(videoThreadMutex_);
    recording_ = false;
    return 0;
}

bool VideoDevices::Recording() const
{
    return recording_;
}

void VideoDevices::registerVideoCallback(VideoDataTransport *callback)
{
    video_callback_ = callback;
}

void VideoDevices::SendRecordFrameData()
{
    int ret = 0;
    if (video_callback_ != nullptr)
    {
        generalYuvData(yuvData_, mWidth_, mHeight_);
        //write_to_file(yuvData_, mWidth_*mHeight_ * 3 / 2);
        ret = video_callback_->YuvDataIsAvailable(yuvData_, mWidth_*mHeight_ * 3 / 2, mWidth_, mHeight_);
        if (ret != 0)
        {
            ELOG("record data error\n");
            return;
        }
    }
}

void VideoDevices::VideoDeviceThread()
{
    ILOG("start video device thread\n");
    while (start_)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(videoThreadMutex_);
            if (playing_)
            {
                // get play data
                //GetPlayFrameData();
            }
            if (recording_)
            {
                // send record data
                SendRecordFrameData();
            }
            if (ShouldStopVideoThread())
            {
                start_ = false;
                break;
            }
        }
        sleepMutex_.lock();
        videoThreadCond_.wait_for(sleepMutex_, std::chrono::milliseconds(nFrameDuration_ - 1));
        sleepMutex_.unlock();
    }
    ILOG("audio device thread end\n");
}

bool VideoDevices::ShouldStopVideoThread()
{
    return !recording_ && !playing_;
}

BaseDevices* DevicesFactory::CreateAudioDevice(std::string deviceName, uint32_t sampleRate, int32_t bits, int32_t channels, int32_t duration)
{
    BaseDevices *device;
    device = new AudioDevices(deviceName, sampleRate, bits, channels, duration);
    return device;
}

BaseDevices* DevicesFactory::CreateVideoDevice(std::string deviceName, uint32_t width, uint32_t height, uint32_t framerate)
{
    BaseDevices *device;
    device = new VideoDevices(deviceName, width, height, framerate);
    return device;
}

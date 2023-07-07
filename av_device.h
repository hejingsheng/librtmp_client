//
// Created by hejingsheng on 2023/7/6.
//

#ifndef RTMP_CLIENT_AV_DEVICE_H
#define RTMP_CLIENT_AV_DEVICE_H

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
//#include "noise_suppression_x.h"

enum DeviceType
{
    RTC_AUDIO_DEVICE = 0,
    RTC_VIDEO_DEVICE = 1,
};

class AudioDataTransport
{
public:
    virtual int RecordDataIsAvailable(const void* audioData,
                                      const int32_t nSamples,
                                      const int32_t nBitsPerSample,
                                      const int32_t nChannels,
                                      const uint32_t nSampleRate,
                                      const int32_t nFrameDurationMs) = 0;
    virtual int PlayNeedMordData(const int32_t nSamples,
                                 const int32_t nBitsPerSample,
                                 const int32_t nChannels,
                                 const uint32_t nSampleRate,
                                 const int32_t nFrameDurationMs,
                                 void *audioData,
                                 int32_t &nSamplesOut) = 0;

    AudioDataTransport() = default;
    virtual ~AudioDataTransport() = default;
};

class VideoDataTransport
{
public:
    virtual int YuvDataIsAvailable(const void* yuvData, const uint32_t len, const int32_t width, const int32_t height) = 0;

    VideoDataTransport() = default;
    virtual ~VideoDataTransport() = default;
};

class BaseDevices
{
public:
    BaseDevices(std::string deviceName);
    virtual ~BaseDevices();

public:
    virtual int Init();
    virtual bool Initialized() const;

    virtual int InitPlayDevice() = 0;
    virtual int StartPlay() = 0;
    virtual int StopPlay() = 0;
    virtual bool Playing() const = 0;

    virtual int InitRecordDevice() = 0;
    virtual int StartRecord() = 0;
    virtual int StopRecord() = 0;
    virtual bool Recording() const = 0;

    virtual void registerAudioCallback(AudioDataTransport *callback);
    virtual void registerVideoCallback(VideoDataTransport *callback);

protected:
    std::string deviceName_;
    std::string deviceType_;
    bool bInit_;
};

class AudioDevices : public BaseDevices
{
public:
    AudioDevices(std::string deviceName, uint32_t sampleRate, int32_t bits, int32_t channels, int32_t frameduration);
    virtual ~AudioDevices();

public:
    virtual int InitPlayDevice();
    virtual int StartPlay();
    virtual int StopPlay();
    virtual bool Playing() const;

    virtual int InitRecordDevice();
    virtual int StartRecord();
    virtual int StopRecord();
    virtual bool Recording() const;

    virtual void registerAudioCallback(AudioDataTransport *callback);

private:
    // Pulls frames from the registered RtcAudioDataTransport.
    void GetPlayFrameData();
    // Pushes frames to the registered RtcAudioDataTransport.
    void SendRecordFrameData();
    void AudioDeviceThread();
    bool ShouldStopAudioThread();

private:
    bool recording_;  // True when audio is being pushed from the instance.
    bool playing_;    // True when audio is being pulled by the instance.

    bool play_is_initialized_;  // True when the instance is ready to pull audio.
    bool rec_is_initialized_;   // True when the instance is ready to push audio.

    uint32_t nSampleRate_;
    int32_t nBitsPerSample_;
    int32_t nChannels_;
    int32_t nFrameDuration_;
    char *pcmRecordData_;
    char *pcmPlayData_;

    AudioDataTransport *audio_callback_;

private:
    std::thread audioDeviceThread_;
    std::recursive_mutex audioThreadMutex_;
    std::recursive_mutex sleepMutex_;
    std::condition_variable_any audioThreadCond_;
    bool start_;

private:
    //NsxHandle *pNS_inst;
};

class VideoDevices : public BaseDevices
{
public:
    VideoDevices(std::string deviceName, uint32_t width, uint32_t height, uint32_t framerate);
    virtual ~VideoDevices();

public:
    virtual int InitPlayDevice();
    virtual int StartPlay();
    virtual int StopPlay();
    virtual bool Playing() const;

    virtual int InitRecordDevice();
    virtual int StartRecord();
    virtual int StopRecord();
    virtual bool Recording() const;

    virtual void registerVideoCallback(VideoDataTransport *callback);

private:
    // Pulls frames from the registered RtcAudioDataTransport.
    //void GetPlayFrameData();
    // Pushes frames to the registered RtcAudioDataTransport.
    void SendRecordFrameData();
    void VideoDeviceThread();
    bool ShouldStopVideoThread();

private:
    bool recording_;  // True when audio is being pushed from the instance.
    bool playing_;    // True when audio is being pulled by the instance.

    bool play_is_initialized_;  // True when the instance is ready to pull audio.
    bool rec_is_initialized_;   // True when the instance is ready to push audio.

    uint32_t mWidth_;
    uint32_t mHeight_;
    uint32_t mFrameRate_;
    int32_t nFrameDuration_;
    uint8_t *yuvData_;

    VideoDataTransport *video_callback_;

private:
    std::thread videoDeviceThread_;
    std::recursive_mutex videoThreadMutex_;
    std::recursive_mutex sleepMutex_;
    std::condition_variable_any videoThreadCond_;
    bool start_;

};

class DevicesFactory {
public:
    static BaseDevices* CreateAudioDevice(std::string deviceName, uint32_t sampleRate, int32_t bits, int32_t channels, int32_t duration);
    static BaseDevices* CreateVideoDevice(std::string deviceName, uint32_t width, uint32_t height, uint32_t framerate);

private:
    ~DevicesFactory() = default;
};

#endif //RTMP_CLIENT_AV_DEVICE_H

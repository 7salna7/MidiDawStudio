#include <jni.h>
#include <oboe/Oboe.h>
#include <android/log.h>
#include <android/amidi.h>
#include <atomic>
#include <vector>
#include <algorithm>

#define LOG_TAG "MidiDawEngine"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define MAX_CHANNELS 16
#define BUFFER_SIZE_COEFF 2

struct AudioChannel {
    const float* sampleData = nullptr;
    int32_t totalSamples = 0;
    std::atomic<int32_t> currentPosition{0};
    std::atomic<bool> isPlaying{false};
    std::atomic<float> channelVolume{1.0f};
};

class MidiDawEngine : public oboe::AudioStreamDataCallback {
public:
    MidiDawEngine() {
        mChannels.resize(MAX_CHANNELS);
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            mChannels[i].isPlaying.store(false);
        }
    }

    void start() {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output)
               ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
               ->setSharingMode(oboe::SharingMode::Exclusive)
               ->setFormat(oboe::AudioFormat::Float)
               ->setChannelCount(oboe::ChannelCount::Stereo)
               ->setDataCallback(this);

        oboe::Result result = builder.openStream(mStream);
        if (result == oboe::Result::OK && mStream) {
            int32_t bufferSize = mStream->getFramesPerBurst() * BUFFER_SIZE_COEFF;
            mStream->setBufferSizeInFrames(bufferSize);
            mStream->start();
        } else {
            LOGE("CRITICAL: Failed to open Oboe stream: %s", oboe::convertToText(result));
        }
    }

    void triggerChannelSample(int channelId, const float* buffer, int32_t sampleCount, float vol) {
        if (channelId < 0 || channelId >= MAX_CHANNELS) return;
        mChannels[channelId].sampleData = buffer;
        mChannels[channelId].totalSamples = sampleCount;
        mChannels[channelId].channelVolume.store(vol);
        mChannels[channelId].currentPosition.store(0);
        mChannels[channelId].isPlaying.store(true);
    }

    void setVolumeAutomation(int channelId, float vol) {
        if (channelId >= 0 && channelId < MAX_CHANNELS) {
            mChannels[channelId].channelVolume.store(vol);
        }
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override {
        float *output = static_cast<float *>(audioData);
        int32_t totalChannels = audioStream->getChannelCount();

        std::fill_n(output, numFrames * totalChannels, 0.0f);

        for (int32_t frame = 0; frame < numFrames; ++frame) {
            float accumLeft = 0.0f;
            float accumRight = 0.0f;

            for (auto& chan : mChannels) {
                if (!chan.isPlaying.load()) continue;

                int32_t currentPos = chan.currentPosition.load();
                if (currentPos >= chan.totalSamples) {
                    chan.isPlaying.store(false);
                    continue;
                }

                float sample = chan.sampleData[currentPos] * chan.channelVolume.load();
                accumLeft += sample;
                accumRight += sample;

                chan.currentPosition.store(currentPos + 1);
            }

            output[frame * totalChannels] = accumLeft;
            if (totalChannels > 1) {
                output[frame * totalChannels + 1] = accumRight;
            }
        }
        return oboe::DataCallbackResult::Continue;
    }

private:
    std::shared_ptr<oboe::AudioStream> mStream;
    std::vector<AudioChannel> mChannels;
};

static MidiDawEngine *engineInstance = nullptr;

extern "C" JNIEXPORT void JNICALL
Java_com_webapp_mididawstudio_NativeAudioEngine_nativeInit(JNIEnv *env, jobject thiz) {
    if (!engineInstance) {
        engineInstance = new MidiDawEngine();
        engineInstance->start();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_webapp_mididawstudio_NativeAudioEngine_nativeTriggerPad(JNIEnv *env, jobject thiz, jint channel_id, jfloatArray pcm_array, jint size, jfloat volume) {
    if (!engineInstance) return;
    jfloat* samples = env->GetFloatArrayElements(pcm_array, nullptr);
    engineInstance->triggerChannelSample(channel_id, samples, size, volume);
    env->ReleaseFloatArrayElements(pcm_array, samples, JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL
Java_com_webapp_mididawstudio_NativeAudioEngine_nativeSetAutomationVolume(JNIEnv *env, jobject thiz, jint channel_id, jfloat volume) {
    if (engineInstance) {
        engineInstance->setVolumeAutomation(channel_id, volume);
    }
}

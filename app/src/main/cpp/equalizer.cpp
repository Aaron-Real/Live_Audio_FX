#include <jni.h>
#include <string>
#include <android/log.h>
#include <Superpowered.h>
#include <Superpowered3BandEQ.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Superpowered/OpenSource/SuperpoweredAndroidAudioIO.h"

#define LOG_TAG "SuperpoweredEqualizer"

static Superpowered::ThreeBandEQ* eq = nullptr;
static SuperpoweredAndroidAudioIO* audioIO = nullptr;
static float* floatBuffer = nullptr;

// Audio processing callback
static bool audioProcessing(void* clientData, short int* audioInputOutput, int numberOfFrames, int samplerate) {
    if (!eq || !floatBuffer) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "EQ or floatBuffer null");
        return false;
    }

    // Log input sample for debugging
    if (numberOfFrames > 0) {
        char logMsg[64];
        snprintf(logMsg, sizeof(logMsg), "Input sample[0]: %d, frames: %d, rate: %d", audioInputOutput[0], numberOfFrames, samplerate);
        __android_log_write(ANDROID_LOG_DEBUG, LOG_TAG, logMsg);
    }

    // Convert 16-bit short to 32-bit float
    for (int i = 0; i < numberOfFrames * 2; i++) {
        floatBuffer[i] = (float)audioInputOutput[i] / 32768.0f;
    }

    // Process audio through the equalizer
    bool hasOutput = eq->process(floatBuffer, floatBuffer, numberOfFrames);

    // Amplify output (2.5x for audibility)
    for (int i = 0; i < numberOfFrames * 2; i++) {
        floatBuffer[i] *= 2.5f;
    }

    // Convert back to 16-bit short with clipping
    for (int i = 0; i < numberOfFrames * 2; i++) {
        float sample = floatBuffer[i] * 32768.0f;
        audioInputOutput[i] = (short int)(sample > 32767.0f ? 32767 : (sample < -32768.0f ? -32768 : sample));
    }

    // Log output sample
    if (numberOfFrames > 0) {
        char logMsg[64];
        snprintf(logMsg, sizeof(logMsg), "Output sample[0]: %d, hasOutput: %d", audioInputOutput[0], hasOutput);
        __android_log_write(ANDROID_LOG_DEBUG, LOG_TAG, logMsg);
    }

    return hasOutput;
}

// Initialize Superpowered and equalizer
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_initEqualizer(JNIEnv* env, jobject /* this */, jstring licenseKey) {
    const char* key = env->GetStringUTFChars(licenseKey, nullptr);
    Superpowered::Initialize(key);
    env->ReleaseStringUTFChars(licenseKey, key);

    // Initialize equalizer
    eq = new Superpowered::ThreeBandEQ(44100);
    eq->enabled = true;
    eq->low = 2.0f;  // +6 dB boost
    eq->mid = 1.0f;  // Flat
    eq->high = 1.0f; // Flat

    // Initialize audio I/O with default input stream
    audioIO = new SuperpoweredAndroidAudioIO(
            44100,           // Sample rate
            256,             // Buffer size
            true,            // Enable input
            true,            // Enable output
            audioProcessing, // Processing callback
            nullptr,         // Client data
            SL_ANDROID_RECORDING_PRESET_GENERIC, // Input stream type (generic mic)
            SL_ANDROID_STREAM_MEDIA // Output stream type
    );

    // Allocate float buffer
    floatBuffer = (float*)malloc(256 * 2 * sizeof(float));
    if (!floatBuffer) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "Failed to allocate floatBuffer");
    }

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Equalizer initialized");
}

// Set low band gain
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setLowGain(JNIEnv* env, jobject /* this */, jfloat gain) {
    if (eq) {
        eq->low = gain;
        char logMsg[32];
        snprintf(logMsg, sizeof(logMsg), "Low gain set to: %f", gain);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, logMsg);
    }
}

// Set mid band gain
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setMidGain(JNIEnv* env, jobject /* this */, jfloat gain) {
    if (eq) {
        eq->mid = gain;
        char logMsg[32];
        snprintf(logMsg, sizeof(logMsg), "Mid gain set to: %f", gain);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, logMsg);
    }
}

// Set high band gain
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setHighGain(JNIEnv* env, jobject /* this */, jfloat gain) {
    if (eq) {
        eq->high = gain;
        char logMsg[32];
        snprintf(logMsg, sizeof(logMsg), "High gain set to: %f", gain);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, logMsg);
    }
}

// Start audio processing
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_startAudio(JNIEnv* env, jobject /* this */) {
    if (audioIO) {
        audioIO->start();
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Audio started");
    } else {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "audioIO is null");
    }
}

// Stop audio processing
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_stopAudio(JNIEnv* env, jobject /* this */) {
    if (audioIO) {
        audioIO->stop();
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Audio stopped");
    }
}

// Cleanup
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_cleanup(JNIEnv* env, jobject /* this */) {
    if (audioIO) {
        audioIO->stop();
        delete audioIO;
        audioIO = nullptr;
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "audioIO cleaned up");
    }
    if (eq) {
        delete eq;
        eq = nullptr;
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "EQ cleaned up");
    }
    if (floatBuffer) {
        free(floatBuffer);
        floatBuffer = nullptr;
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "floatBuffer cleaned up");
    }
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Cleanup completed");
}
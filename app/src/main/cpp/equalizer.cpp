#include <jni.h>
#include <string>
#include <android/log.h>
#include <Superpowered.h>
#include <Superpowered3BandEQ.h>
#include <SuperpoweredCompressor.h>             // ← new
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Superpowered/OpenSource/SuperpoweredAndroidAudioIO.h"

#define LOG_TAG "SuperpoweredEqualizer"

static Superpowered::ThreeBandEQ*      eq         = nullptr;
static Superpowered::Compressor*       compressor = nullptr;  // ← new
static SuperpoweredAndroidAudioIO*     audioIO    = nullptr;
static float*                          floatBuffer= nullptr;

// Audio callback: EQ → Compressor → amplify → back to PCM
static bool audioProcessing(void* clientData, short int* ioData, int frames, int samplerate) {
    if (!eq || !compressor || !floatBuffer) return false;

    // 1) PCM16 → float32
    for (int i = 0; i < frames * 2; i++)
        floatBuffer[i] = ioData[i] / 32768.0f;

    // 2) 3-band EQ
    bool eqOutput = eq->process(floatBuffer, floatBuffer, frames);

    // 3) Compressor (in-place)
    compressor->samplerate = samplerate;
    bool compOutput = compressor->process(floatBuffer, floatBuffer, frames);

    // 4) make it louder
    for (int i = 0; i < frames * 2; i++)
        floatBuffer[i] *= 2.5f;

    // 5) float32 → PCM16
    for (int i = 0; i < frames * 2; i++) {
        float s = floatBuffer[i] * 32768.0f;
        ioData[i] = (short)(s > 32767.f ? 32767 : (s < -32768.f ? -32768 : s));
    }

    // return true if either effect wrote to the buffer
    return eqOutput || compOutput;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_initEqualizer(JNIEnv* env, jobject /* this */, jstring licenseKeyJ) {
    const char* licenseKey = env->GetStringUTFChars(licenseKeyJ, nullptr);
    Superpowered::Initialize(licenseKey);
    env->ReleaseStringUTFChars(licenseKeyJ, licenseKey);

    // --- ThreeBandEQ setup ---
    eq = new Superpowered::ThreeBandEQ(44100);
    eq->enabled = true;
    eq->low     = 2.0f;
    eq->mid     = 1.0f;
    eq->high    = 1.0f;

    // --- Compressor setup ---
    compressor = new Superpowered::Compressor(44100);
    compressor->enabled       = false;   // start bypassed
    compressor->thresholdDb   = -20.0f;
    compressor->ratio         = 2.0f;
    compressor->attackSec     = 0.01f;
    compressor->releaseSec    = 0.1f;
    compressor->inputGainDb   = 0.0f;
    compressor->outputGainDb  = 0.0f;
    compressor->wet           = 1.0f;
    compressor->hpCutOffHz    = 20.0f;

    // --- Audio I/O ---
    audioIO = new SuperpoweredAndroidAudioIO(
            44100,            // sample rate
            256,              // buffer size
            true, true,       // input + output
            audioProcessing,  // your callback
            nullptr,
            SL_ANDROID_RECORDING_PRESET_GENERIC,
            SL_ANDROID_STREAM_MEDIA
    );

    floatBuffer = (float*)malloc(256 * 2 * sizeof(float));
    if (!floatBuffer) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "alloc floatBuffer failed");
    }

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "EQ + Compressor initialized");
}

// EQ setters (unchanged)
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setLowGain(JNIEnv*, jobject, jfloat g) { if (eq) eq->low = g; }
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setMidGain(JNIEnv*, jobject, jfloat g) { if (eq) eq->mid = g; }
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setHighGain(JNIEnv*, jobject, jfloat g) { if (eq) eq->high = g; }

// --- Compressor JNI API ---
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setCompressorEnabled(JNIEnv*, jobject, jboolean e) {
    if (compressor) compressor->enabled = e;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setCompressorThreshold(JNIEnv*, jobject, jfloat t) {
    if (compressor) compressor->thresholdDb = t;
}
extern "C" JNIEXPORT jfloat JNICALL
Java_com_aaron_equalizer_MainActivity_getCompressorGainReduction(JNIEnv*, jobject) {
    return compressor ? compressor->getGainReductionDb() : 0.0f;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_startAudio(JNIEnv*, jobject) {
    if (audioIO) audioIO->start();
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_stopAudio(JNIEnv*, jobject) {
    if (audioIO) audioIO->stop();
}

extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_cleanup(JNIEnv*, jobject) {
    if (audioIO)    { delete audioIO;    audioIO    = nullptr; }
    if (eq)         { delete eq;         eq         = nullptr; }
    if (compressor) { delete compressor; compressor = nullptr; }
    if (floatBuffer){ free(floatBuffer); floatBuffer = nullptr; }
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Cleaned up all audio objects");
}

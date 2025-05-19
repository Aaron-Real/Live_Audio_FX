#include <jni.h>
#include <string>
#include <android/log.h>
#include <Superpowered.h>
#include <Superpowered3BandEQ.h>
#include <SuperpoweredCompressor.h>
#include <SuperpoweredAutomaticVocalPitchCorrection.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Superpowered/OpenSource/SuperpoweredAndroidAudioIO.h"

#define LOG_TAG "SuperpoweredEqualizer"

static Superpowered::ThreeBandEQ*                    eq               = nullptr;
static Superpowered::Compressor*                     compressor       = nullptr;
static Superpowered::AutomaticVocalPitchCorrection*  pitchCorrector   = nullptr;
static SuperpoweredAndroidAudioIO*                   audioIO          = nullptr;
static float*                                        floatBuffer      = nullptr;
static bool                                          isPitchOn        = false;

// Audio callback: EQ → Compressor → Pitch Correction → amplify → back to PCM
static bool audioProcessing(void* clientData, short int* ioData, int frames, int samplerate) {
    if (!eq || !compressor || !pitchCorrector || !floatBuffer) return false;

    // 1) PCM16 → float32
    for (int i = 0; i < frames * 2; i++)
        floatBuffer[i] = ioData[i] * (1.0f / 32768.0f);

    // 2) 3-band EQ
    bool eqOutput = eq->process(floatBuffer, floatBuffer, frames);

    // 3) Compressor (in-place)
    compressor->samplerate = samplerate;
    bool compOutput = compressor->process(floatBuffer, floatBuffer, frames);

    // 4) Automatic Pitch Correction (in-place)
    bool pitchOutput = false;
    if (isPitchOn) {
        pitchCorrector->samplerate = samplerate;
        pitchOutput = pitchCorrector->process(
                floatBuffer,    // input
                floatBuffer,    // output
                true,           // interleaved stereo
                frames          // number of frames
        );
    }

    // 5) make it louder
    for (int i = 0; i < frames * 2; i++)
        floatBuffer[i] *= 2.5f;

    // 6) float32 → PCM16
    for (int i = 0; i < frames * 2; i++) {
        float s = floatBuffer[i] * 32768.0f;
        ioData[i] = (short)(s > 32767.f ? 32767 : (s < -32768.f ? -32768 : s));
    }

    return eqOutput || compOutput || pitchOutput;
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

    // --- Automatic Vocal Pitch Correction setup ---
    pitchCorrector = new Superpowered::AutomaticVocalPitchCorrection();
    pitchCorrector->samplerate   = 44100;
    pitchCorrector->scale        = Superpowered::AutomaticVocalPitchCorrection::CMAJOR;
    pitchCorrector->range        = Superpowered::AutomaticVocalPitchCorrection::WIDE;
    pitchCorrector->speed        = Superpowered::AutomaticVocalPitchCorrection::SUBTLE;
    pitchCorrector->clamp        = Superpowered::AutomaticVocalPitchCorrection::LOOSE;
    pitchCorrector->frequencyOfA = 440.0f;
    pitchCorrector->reset();  // clear internal state

    // --- Audio I/O ---
    audioIO = new SuperpoweredAndroidAudioIO(
            44100,            // sample rate
            256,              // buffer size
            true, true,       // enable input + output
            audioProcessing,  // your audio callback
            nullptr,          // clientData
            SL_ANDROID_RECORDING_PRESET_GENERIC,
            SL_ANDROID_STREAM_MEDIA
    );

    floatBuffer = (float*)malloc(256 * 2 * sizeof(float));
    if (!floatBuffer) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "alloc floatBuffer failed");
    }

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "EQ + Compressor + PitchCorrect initialized");
}

// EQ setters
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setLowGain(JNIEnv*, jobject, jfloat g) {
    if (eq) eq->low = g;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setMidGain(JNIEnv*, jobject, jfloat g) {
    if (eq) eq->mid = g;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setHighGain(JNIEnv*, jobject, jfloat g) {
    if (eq) eq->high = g;
}

// Compressor JNI API
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

// Pitch Correction JNI API
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionEnabled(JNIEnv*, jobject, jboolean e) {
    isPitchOn = e;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionScale(JNIEnv*, jobject, jint s) {
    if (pitchCorrector) pitchCorrector->scale = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerScale>((unsigned int) s);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionRange(JNIEnv*, jobject, jint r) {
    if (pitchCorrector) pitchCorrector->range = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerRange>((unsigned int) r);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionSpeed(JNIEnv*, jobject, jint sp) {
    if (pitchCorrector) pitchCorrector->speed = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerSpeed>((unsigned int) sp);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionClamp(JNIEnv*, jobject, jint c) {
    if (pitchCorrector) pitchCorrector->clamp = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerClamp>((unsigned int) c);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionFrequencyOfA(JNIEnv*, jobject, jfloat f) {
    if (pitchCorrector) pitchCorrector->frequencyOfA = f;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_aaron_equalizer_MainActivity_getCustomScaleNote(JNIEnv*, jobject, jint note) {
    return pitchCorrector != nullptr && pitchCorrector->getCustomScaleNote((unsigned char) note);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setCustomScaleNote(JNIEnv*, jobject, jint note, jboolean en) {
    if (pitchCorrector)
        pitchCorrector->setCustomScaleNote((unsigned char)note, en);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_resetPitchCorrection(JNIEnv*, jobject) {
    if (pitchCorrector) pitchCorrector->reset();
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
    if (audioIO)        { delete audioIO;        audioIO        = nullptr; }
    if (eq)             { delete eq;             eq               = nullptr; }
    if (compressor)     { delete compressor;     compressor       = nullptr; }
    if (pitchCorrector) { delete pitchCorrector; pitchCorrector   = nullptr; }
    if (floatBuffer)    { free(floatBuffer);     floatBuffer      = nullptr; }
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Cleaned up all audio objects");
}

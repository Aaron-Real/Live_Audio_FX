#include <jni.h>
#include <string>
#include <android/log.h>
#include <Superpowered.h>
#include <Superpowered3BandEQ.h>
#include <SuperpoweredCompressor.h>
#include <SuperpoweredAutomaticVocalPitchCorrection.h>
#include <SuperpoweredGuitarDistortion.h>
#include <SuperpoweredReverb.h>
#include <SuperpoweredSpatializer.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "Superpowered/OpenSource/SuperpoweredAndroidAudioIO.h"

#define LOG_TAG "SuperpoweredEqualizer"

// Effect objects
static Superpowered::ThreeBandEQ*                   eq             = nullptr;
static Superpowered::Compressor*                    compressor     = nullptr;
static Superpowered::AutomaticVocalPitchCorrection* pitchCorrector = nullptr;
static Superpowered::GuitarDistortion*              distortion     = nullptr;
static Superpowered::Reverb*                        reverb         = nullptr;
static Superpowered::Spatializer*                   spatializer    = nullptr;
static SuperpoweredAndroidAudioIO*                  audioIO        = nullptr;
static float*                                       floatBuffer    = nullptr;
static float*                                       tempBuffer     = nullptr;
static float*                                       spatializerReverbBuffer = nullptr;
static bool                                         isPitchOn      = false;
static bool                                         isReverbOn     = false;
static bool                                         isSpatialOn    = false;
static float                                        distortionWet  = 1.0f;

// Audio callback: Pitch → EQ → Compressor → Distortion → Reverb → Spatializer → boost → PCM16
static bool audioProcessing(void* clientData, short int* ioData, int frames, int samplerate) {
    if (!floatBuffer) return false;

    // 1) PCM16 → float32
    for (int i = 0; i < frames * 2; i++)
        floatBuffer[i] = ioData[i] * (1.0f / 32768.0f);

    // 2) Pitch Correction
    if (isPitchOn && pitchCorrector) {
        pitchCorrector->samplerate = samplerate;
        pitchCorrector->process(floatBuffer, floatBuffer, true, frames);
    }

    // 3) EQ
    if (eq) eq->process(floatBuffer, floatBuffer, frames);

    // 4) Compressor
    if (compressor) {
        compressor->samplerate = samplerate;
        compressor->process(floatBuffer, floatBuffer, frames);
    }

    // 5) Distortion (wet/dry mix)
    if (distortion) {
        for (int i = 0; i < frames * 2; i++) tempBuffer[i] = floatBuffer[i];
        distortion->samplerate = samplerate;
        distortion->process(floatBuffer, floatBuffer, frames);
        for (int i = 0; i < frames * 2; i++)
            floatBuffer[i] = floatBuffer[i] * distortionWet
                             + tempBuffer[i] * (1.0f - distortionWet);
    }

    // 6) Reverb
    if (isReverbOn && reverb) {
        reverb->process(floatBuffer, floatBuffer, frames);
    }

    // 7) Spatializer
    if (isSpatialOn && spatializer) {
        spatializer->samplerate = samplerate;
        spatializer->process(
                floatBuffer,              // inputLeft (or interleaved)
                nullptr,                  // inputRight (nullptr = interleaved)
                floatBuffer,              // outputLeft
                nullptr,                  // outputRight
                (unsigned int)frames,     // numberOfFrames
                false                     // outputAdd = replacement
        );
        // global reverb from Spatializer
        if (spatializerReverbBuffer && Superpowered::Spatializer::reverbProcess(spatializerReverbBuffer, frames)) {
            for (int i = 0; i < frames * 2; i++)
                floatBuffer[i] += spatializerReverbBuffer[i];
        }
    }

    // 8) boost
    for (int i = 0; i < frames * 2; i++)
        floatBuffer[i] *= 2.5f;

    // 9) float32 → PCM16
    for (int i = 0; i < frames * 2; i++) {
        float s = floatBuffer[i] * 32768.0f;
        ioData[i] = (short)(s > 32767.f ? 32767 : (s < -32768.f ? -32768 : s));
    }

    return true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_initEqualizer(JNIEnv* env, jobject, jstring licenseKeyJ) {
    const char* key = env->GetStringUTFChars(licenseKeyJ, nullptr);
    Superpowered::Initialize(key);
    env->ReleaseStringUTFChars(licenseKeyJ, key);

    // Pitch
    pitchCorrector = new Superpowered::AutomaticVocalPitchCorrection();
    pitchCorrector->samplerate = 44100;
    pitchCorrector->scale      = Superpowered::AutomaticVocalPitchCorrection::CMAJOR;
    pitchCorrector->range      = Superpowered::AutomaticVocalPitchCorrection::WIDE;
    pitchCorrector->speed      = Superpowered::AutomaticVocalPitchCorrection::SUBTLE;
    pitchCorrector->clamp      = Superpowered::AutomaticVocalPitchCorrection::LOOSE;
    pitchCorrector->frequencyOfA = 440.0f;
    pitchCorrector->reset();

    // EQ
    eq = new Superpowered::ThreeBandEQ(44100);
    eq->enabled = true;
    eq->low     = 2.0f;
    eq->mid     = 1.0f;
    eq->high    = 1.0f;

    // Compressor
    compressor = new Superpowered::Compressor(44100);
    compressor->enabled     = false;
    compressor->thresholdDb = -20.0f;
    compressor->ratio       = 2.0f;
    compressor->attackSec   = 0.01f;
    compressor->releaseSec  = 0.1f;
    compressor->inputGainDb = 0.0f;
    compressor->outputGainDb= 0.0f;
    compressor->wet         = 1.0f;
    compressor->hpCutOffHz  = 20.0f;

    // Distortion
    distortion = new Superpowered::GuitarDistortion(44100);
    distortion->gainDecibel     = 0.0f;
    distortion->drive           = 0.0f;
    distortion->bassFrequency   = 100.0f;
    distortion->trebleFrequency = 6000.0f;

    // Reverb
    reverb = new Superpowered::Reverb(44100, 44100);
    reverb->enabled    = false;
    reverb->mix        = 0.4f;
    reverb->roomSize   = 0.8f;
    reverb->damp       = 0.5f;
    reverb->predelayMs = 0.0f;
    reverb->lowCutHz   = 0.0f;
    reverb->width      = 1.0f;

    // Spatializer
    spatializer = new Superpowered::Spatializer(44100);
    Superpowered::Spatializer::reverbWidth    = 1.0f;
    Superpowered::Spatializer::reverbDamp     = 0.5f;
    Superpowered::Spatializer::reverbRoomSize = 0.8f;
    Superpowered::Spatializer::reverbPredelayMs = 0.0f;
    Superpowered::Spatializer::reverbLowCutHz = 0.0f;

    // Audio I/O
    audioIO = new SuperpoweredAndroidAudioIO(
            44100, 256, true, true,
            audioProcessing, nullptr,
            SL_ANDROID_RECORDING_PRESET_GENERIC,
            SL_ANDROID_STREAM_MEDIA
    );

    // Buffers
    floatBuffer             = (float*)malloc(256 * 2 * sizeof(float));
    tempBuffer              = (float*)malloc(256 * 2 * sizeof(float));
    spatializerReverbBuffer = (float*)malloc(256 * 2 * sizeof(float));
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Initialized all effects");
}

// --- EQ setters ---
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setLowGain(JNIEnv *, jobject, jfloat g) {
    eq->low = g;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setMidGain(JNIEnv *, jobject, jfloat g) {
    eq->mid = g;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setHighGain(JNIEnv *, jobject, jfloat g) {
    eq->high = g;
}

// --- Compressor JNI API ---
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setCompressorEnabled(JNIEnv *, jobject, jboolean e) {
    compressor->enabled = e;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setCompressorThreshold(JNIEnv *, jobject, jfloat t) {
    compressor->thresholdDb = t;
}
extern "C" JNIEXPORT jfloat JNICALL
Java_com_aaron_equalizer_MainActivity_getCompressorGainReduction(JNIEnv *, jobject) {
    return compressor->getGainReductionDb();
}

// --- Pitch Correction JNI API ---
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionEnabled(JNIEnv *, jobject, jboolean e) {
    isPitchOn = e;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionScale(JNIEnv *, jobject, jint s) {
    pitchCorrector->scale = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerScale>((unsigned int) s);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionRange(JNIEnv *, jobject, jint r) {
    pitchCorrector->range = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerRange>((unsigned int) r);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionSpeed(JNIEnv *, jobject, jint sp) {
    pitchCorrector->speed = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerSpeed>((unsigned int) sp);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionClamp(JNIEnv *, jobject, jint c) {
    pitchCorrector->clamp = static_cast<Superpowered::AutomaticVocalPitchCorrection::TunerClamp>((unsigned int) c);
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setPitchCorrectionFrequencyOfA(JNIEnv *, jobject, jfloat f) {
    pitchCorrector->frequencyOfA = f;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_resetPitchCorrection(JNIEnv *, jobject) {
    pitchCorrector->reset();
}

// --- Distortion JNI API (matches your sliders in MainActivity.kt) ---
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setDistortionEnabled(JNIEnv *, jobject, jboolean e) {
    distortion->enabled = e;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setDistortionInputGain(JNIEnv *, jobject, jfloat g) {
    distortion->gainDecibel = g;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setDistortionOutputGain(JNIEnv *, jobject, jfloat g) {
    distortion->drive = g;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setDistortionToneHz(JNIEnv *, jobject, jfloat hz) {
    // map “tone” slider to bass or treble cutoff
    if (hz <= 250.0f) distortion->bassFrequency = hz;
    else if (hz >= 6000.0f) distortion->trebleFrequency = hz;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setDistortionWet(JNIEnv *, jobject, jfloat w) {
    distortionWet = w;
}

// --- Reverb JNI API ---
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setReverbEnabled(JNIEnv *, jobject, jboolean e) {
    isReverbOn = e;
    if (reverb) reverb->enabled = e;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setReverbMix(JNIEnv *, jobject, jfloat m) {
    if (reverb) reverb->mix = m;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setReverbRoomSize(JNIEnv *, jobject, jfloat r) {
    if (reverb) reverb->roomSize = r;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setReverbDamp(JNIEnv *, jobject, jfloat d) {
    if (reverb) reverb->damp = d;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setReverbPredelay(JNIEnv *, jobject, jfloat p) {
    if (reverb) reverb->predelayMs = p;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setReverbLowCut(JNIEnv *, jobject, jfloat hz) {
    if (reverb) reverb->lowCutHz = hz;
}
extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setReverbWidth(JNIEnv *, jobject, jfloat w) {
    if (reverb) reverb->width = w;
}


// Spatializer JNI
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setStereoEnhancerEnabled(JNIEnv*, jobject, jboolean e) {
    isSpatialOn = e;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setStereoEnhancerWidth(JNIEnv*, jobject, jfloat w) {
    Superpowered::Spatializer::reverbWidth = w;
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_setStereoEnhancerDamp(JNIEnv*, jobject, jfloat d) {
    Superpowered::Spatializer::reverbDamp = d;
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
    delete audioIO;    audioIO    = nullptr;
    delete eq;         eq         = nullptr;
    delete compressor; compressor = nullptr;
    delete pitchCorrector; pitchCorrector = nullptr;
    delete distortion; distortion = nullptr;
    delete reverb;     reverb     = nullptr;
    delete spatializer; spatializer = nullptr;
    free(floatBuffer);              floatBuffer              = nullptr;
    free(tempBuffer);               tempBuffer               = nullptr;
    free(spatializerReverbBuffer);  spatializerReverbBuffer  = nullptr;
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Cleaned up all audio objects");
}
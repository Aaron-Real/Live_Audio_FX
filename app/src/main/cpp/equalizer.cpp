#include <jni.h>
#include <string>
#include <android/log.h>
#include <Superpowered.h>
#include <Superpowered3BandEQ.h>
#include <SuperpoweredCompressor.h>
#include <SuperpoweredAutomaticVocalPitchCorrection.h>
#include <SuperpoweredGuitarDistortion.h>             // ← new
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SuperpoweredReverb.h>
#include "Superpowered/OpenSource/SuperpoweredAndroidAudioIO.h"

#define LOG_TAG "SuperpoweredEqualizer"

// Effect objects
static Superpowered::ThreeBandEQ *eq = nullptr;
static Superpowered::Compressor *compressor = nullptr;
static Superpowered::AutomaticVocalPitchCorrection *pitchCorrector = nullptr;
static Superpowered::GuitarDistortion *distortion = nullptr;
static Superpowered::Reverb *reverb = nullptr;
static SuperpoweredAndroidAudioIO *audioIO = nullptr;
static float *floatBuffer = nullptr;
static float *tempBuffer = nullptr;   // for wet mix
static bool isPitchOn = false;
static float distortionWet = 1.0f;     // 0…1 mix
static bool isReverbOn = false;

// Audio callback: Pitch → EQ → Compressor → Distortion → boost → out
static bool audioProcessing(void *clientData, short int *ioData, int frames, int samplerate) {
    if (!eq || !compressor || !pitchCorrector || !distortion || !floatBuffer || !tempBuffer)
        return false;

    // 1) PCM16 → float32
    for (int i = 0; i < frames * 2; i++) {
        floatBuffer[i] = ioData[i] * (1.0f / 32768.0f);
    }

    // 2) Automatic Pitch Correction
    if (isPitchOn) {
        pitchCorrector->samplerate = samplerate;
        pitchCorrector->process(floatBuffer, floatBuffer, true, frames);
    }

    // 3) 3-band EQ
    eq->process(floatBuffer, floatBuffer, frames);

    // 4) Compressor
    compressor->samplerate = samplerate;
    compressor->process(floatBuffer, floatBuffer, frames);

    // 5) Distortion (in-place w/ wet mix)
    //   copy pre-distorted signal
    for (int i = 0; i < frames * 2; i++) {
        tempBuffer[i] = floatBuffer[i];
    }
    distortion->samplerate = samplerate;
    distortion->process(floatBuffer, floatBuffer, frames);
    //   mix dry & wet
    for (int i = 0; i < frames * 2; i++) {
        floatBuffer[i] = floatBuffer[i] * distortionWet
                         + tempBuffer[i] * (1.0f - distortionWet);
    }

    // 6) Reverb (in-place)
    if (isReverbOn && reverb) {
        reverb->process(floatBuffer, floatBuffer, frames);
    }

    // 7) boost for audibility
    for (int i = 0; i < frames * 2; i++) {
        floatBuffer[i] *= 2.5f;
    }

    // 8) float32 → PCM16
    for (int i = 0; i < frames * 2; i++) {
        float s = floatBuffer[i] * 32768.0f;
        ioData[i] = (short) (s > 32767.f ? 32767 : (s < -32768.f ? -32768 : s));
    }

    return true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_initEqualizer(JNIEnv *env, jobject /* this */,
                                                    jstring licenseKeyJ) {
    const char *licenseKey = env->GetStringUTFChars(licenseKeyJ, nullptr);
    Superpowered::Initialize(licenseKey);
    env->ReleaseStringUTFChars(licenseKeyJ, licenseKey);

    // Pitch Correction
    pitchCorrector = new Superpowered::AutomaticVocalPitchCorrection();
    pitchCorrector->samplerate = 44100;
    pitchCorrector->scale = Superpowered::AutomaticVocalPitchCorrection::CMAJOR;
    pitchCorrector->range = Superpowered::AutomaticVocalPitchCorrection::WIDE;
    pitchCorrector->speed = Superpowered::AutomaticVocalPitchCorrection::SUBTLE;
    pitchCorrector->clamp = Superpowered::AutomaticVocalPitchCorrection::LOOSE;
    pitchCorrector->frequencyOfA = 440.0f;
    pitchCorrector->reset();

    // 3-Band EQ
    eq = new Superpowered::ThreeBandEQ(44100);
    eq->enabled = true;
    eq->low = 2.0f;
    eq->mid = 1.0f;
    eq->high = 1.0f;

    // Compressor
    compressor = new Superpowered::Compressor(44100);
    compressor->enabled = false;
    compressor->thresholdDb = -20.0f;
    compressor->ratio = 2.0f;
    compressor->attackSec = 0.01f;
    compressor->releaseSec = 0.1f;
    compressor->inputGainDb = 0.0f;
    compressor->outputGainDb = 0.0f;
    compressor->wet = 1.0f;
    compressor->hpCutOffHz = 20.0f;

    // Guitar Distortion
    distortion = new Superpowered::GuitarDistortion(44100);
    distortion->enabled = false;  // bypassed by default
    distortion->gainDecibel = 0.0f;   // –96…24 dB :contentReference[oaicite:1]{index=1}
    distortion->drive = 0.0f;   // 0…1
    distortion->bassFrequency = 100.0f; // 1…250 Hz
    distortion->trebleFrequency = 6000.0f; // 6000…samplerate/2
    // cabinet & EQ bands use their defaults

    // --- Reverb ---
    reverb = new Superpowered::Reverb(44100, 44100);
    reverb->enabled = false;   // start bypassed
    reverb->mix = 0.4f;    // dry/wet balance
    reverb->roomSize = 0.8f;    // 0…1
    reverb->damp = 0.5f;    // 0…1 high-freq damping
    reverb->predelayMs = 0.0f;    // 0…500 ms
    reverb->lowCutHz = 0.0f;    // Hz
    reverb->width = 1.0f;    // stereo width

    // Audio I/O
    audioIO = new SuperpoweredAndroidAudioIO(
            44100, 256, true, true,
            audioProcessing, nullptr,
            SL_ANDROID_RECORDING_PRESET_GENERIC,
            SL_ANDROID_STREAM_MEDIA
    );

    // Buffers
    floatBuffer = (float *) malloc(256 * 2 * sizeof(float));
    tempBuffer = (float *) malloc(256 * 2 * sizeof(float));
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG,
                        "Init: Pitch→EQ→Compressor→Distortion");
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

// --- Audio control ---
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_startAudio(JNIEnv *, jobject) {
    audioIO->start();
}
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_stopAudio(JNIEnv *, jobject) {
    audioIO->stop();
}

// --- Cleanup ---
extern "C" JNIEXPORT void JNICALL
Java_com_aaron_equalizer_MainActivity_cleanup(JNIEnv *, jobject) {
    delete audioIO;
    audioIO = nullptr;
    delete eq;
    eq = nullptr;
    delete compressor;
    compressor = nullptr;
    delete pitchCorrector;
    pitchCorrector = nullptr;
    delete distortion;
    distortion = nullptr;
    delete reverb;
    reverb = nullptr;
    free(floatBuffer);
    floatBuffer = nullptr;
    free(tempBuffer);
    tempBuffer = nullptr;
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG,
                        "Cleaned up all audio objects");
}

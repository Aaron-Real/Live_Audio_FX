# Live Audio FX

![Platform](https://img.shields.io/badge/platform-Android-green)
![Kotlin](https://img.shields.io/badge/Kotlin-Android-blue)
![C++](https://img.shields.io/badge/NDK-C++-orange)
![Audio](https://img.shields.io/badge/Audio-Superpowered-purple)

## What This Project Does

Live Audio FX is an Android audio processing application built with Kotlin, C++, and the Superpowered Audio SDK. The project demonstrates real-time audio effect processing using native audio pipelines through the Android NDK.

The app processes live audio input and applies audio effects with low latency performance.

## Why This Project Is Useful

This project helps developers learn:

* Real-time audio processing on Android
* Android NDK integration
* JNI communication between Kotlin and C++
* Native DSP processing workflows
* Superpowered audio engine integration
* Low latency audio streaming
* Audio effect implementation with CMake

Key features:

* Live audio effect processing
* Native C++ audio engine
* Superpowered SDK integration
* ARM64 and ARMv7 support
* CMake-based native builds
* Kotlin Android UI layer

## Tech Stack

* Kotlin
* Android SDK
* Android NDK
* C++
* JNI
* CMake
* Superpowered Audio SDK
* Gradle Kotlin DSL

## Project Structure

```text
app/src/main
├── cpp/
│   ├── equalizer.cpp
│   ├── CMakeLists.txt
│   └── Superpowered/
├── java/com/aaron/equalizer/
├── jniLibs/
└── AndroidManifest.xml
```

## Getting Started

### Requirements

Install the following tools before running the project:

* Android Studio
* Android SDK 35
* Android NDK
* CMake
* Java 11 or newer

### Clone the Repository

```bash
git clone <your-repository-url>
cd Live_Audio_FX
```

### Open the Project

1. Open Android Studio
2. Select Open Project
3. Choose the project directory
4. Wait for Gradle sync

### Enable NDK Support

Inside Android Studio:

1. Open SDK Manager
2. Install:

   * Android NDK
   * CMake
3. Sync the project again

### Build the App

Using Android Studio:

1. Connect an Android device or emulator
2. Click Run

Using Gradle:

```bash
./gradlew assembleDebug
```

## Native Audio Engine

The native audio processing logic is implemented inside:

```text
app/src/main/cpp/equalizer.cpp
```

The project uses:

* JNI bindings for Android communication
* Superpowered DSP libraries
* Native audio callbacks for real-time processing

## Superpowered SDK

The project includes the Superpowered Audio SDK for advanced audio processing.

SDK location:

```text
app/src/main/cpp/Superpowered/
```
## Supported Architectures

The project includes native libraries for:

* armeabi-v7a
* arm64-v8a
* x86
* x86_64

## Build Configuration

Native build configuration:

```text
app/src/main/cpp/CMakeLists.txt
```

Gradle configuration:

```text
app/build.gradle.kts
```

## Usage

1. Launch the app
2. Start live audio input
3. Apply audio effects
4. Listen to processed output in real time

Support

[Android Developers NDK Docs](https://developer.android.com/ndk)

[CMake Documentation](https://cmake.org/documentation/)

[Superpowered Docs](https://superpowered.com/docs/)


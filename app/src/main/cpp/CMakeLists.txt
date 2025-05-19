cmake_minimum_required(VERSION 3.22.1)

project("equalizer")

include_directories(${CMAKE_SOURCE_DIR}/Superpowered ${CMAKE_SOURCE_DIR}/Superpowered/OpenSource)

add_library(
        equalizer
        SHARED
        equalizer.cpp
        Superpowered/OpenSource/SuperpoweredAndroidAudioIO.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(opensles-lib OpenSLES)

set(SUPERPOWERED_LIB_DIR ${CMAKE_SOURCE_DIR}/../jniLibs)

if (${ANDROID_ABI} STREQUAL "arm64-v8a")
    add_library(superpowered STATIC IMPORTED)
    set_target_properties(superpowered PROPERTIES IMPORTED_LOCATION ${SUPERPOWERED_LIB_DIR}/arm64-v8a/libSuperpoweredAndroidarm64-v8a.a)
elseif (${ANDROID_ABI} STREQUAL "armeabi-v7a")
    add_library(superpowered STATIC IMPORTED)
    set_target_properties(superpowered PROPERTIES IMPORTED_LOCATION ${SUPERPOWERED_LIB_DIR}/armeabi-v7a/libSuperpoweredAndroidarmeabi-v7a.a)
elseif (${ANDROID_ABI} STREQUAL "x86")
    add_library(superpowered STATIC IMPORTED)
    set_target_properties(superpowered PROPERTIES IMPORTED_LOCATION ${SUPERPOWERED_LIB_DIR}/x86/libSuperpoweredAndroidX86.a)
elseif (${ANDROID_ABI} STREQUAL "x86_64")
    add_library(superpowered STATIC IMPORTED)
    set_target_properties(superpowered PROPERTIES IMPORTED_LOCATION ${SUPERPOWERED_LIB_DIR}/x86_64/libSuperpoweredAndroidX86_64.a)
endif()

target_link_libraries(
        equalizer
        superpowered
        ${log-lib}
        ${android-lib}
        ${opensles-lib}
        c++_shared
)
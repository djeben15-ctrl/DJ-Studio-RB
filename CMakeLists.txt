cmake_minimum_required(VERSION 3.22)

project(DJStudioRB VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)

FetchContent_MakeAvailable(JUCE)

# SoundTouch provides pitch-preserving tempo changes for Master Tempo / Key Lock.
set(SOUNDSTRETCH OFF CACHE BOOL "" FORCE)
set(OPENMP OFF CACHE BOOL "" FORCE)
set(INTEGER_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SoundTouch
    GIT_REPOSITORY https://github.com/RTE-Dev/soundtouch.git
    GIT_TAG 3e2a9e1d5c381bf6cf597c3050fbb8a2aa97fa49
)
FetchContent_MakeAvailable(SoundTouch)

juce_add_gui_app(DJStudioRB
    PRODUCT_NAME "DJ Studio RB"
    COMPANY_NAME "DJ Studio RB"
    BUNDLE_ID "com.djstudiorb.app"
)

target_sources(DJStudioRB
    PRIVATE
        Source/Main.cpp
)

target_compile_definitions(DJStudioRB
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        SOUNDTOUCH_FLOAT_SAMPLES
)

target_link_libraries(DJStudioRB
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_dsp
        SoundTouch
)

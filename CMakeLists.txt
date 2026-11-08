
cmake_minimum_required(VERSION 3.22)

project(DJStudioRB VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
)

FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(DJStudioRB
    PRODUCT_NAME "DJ Studio RB"
    COMPANY_NAME "DJ Studio RB"
)

target_sources(DJStudioRB
    PRIVATE
        Source/Main.cpp
)

target_compile_definitions(DJStudioRB
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(DJStudioRB
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_dsp
)

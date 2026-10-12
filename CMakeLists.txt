cmake_minimum_required(VERSION 3.22)

# Su Windows include le librerie di sistema nel plugin (non serve installare
# il Visual C++ Redistributable sul computer di chi lo usa)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Lumen VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Su Mac compila un binario universale (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: usa una copia locale se indicata (-DJUCE_DIR=...), altrimenti la scarica
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(LUMEN_FORMATS VST3)
if(APPLE)
    list(APPEND LUMEN_FORMATS AU)
endif()

juce_add_plugin(Lumen
    COMPANY_NAME "Vibe Audio"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.vibeaudio.lumen"
    PLUGIN_MANUFACTURER_CODE Vibe
    PLUGIN_CODE Lmn1
    PRODUCT_NAME "Lumen"
    DESCRIPTION "Optical tube compressor"
    FORMATS ${LUMEN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${LUMEN_COPY_AFTER_BUILD})

target_sources(Lumen PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Lumen PRIVATE Source)

target_compile_definitions(Lumen PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Lumen
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Test offline del motore DSP (nessuna dipendenza da JUCE)
option(LUMEN_BUILD_TESTS "Compila i test del DSP" OFF)
if(LUMEN_BUILD_TESTS)
    add_executable(LumenDspTests tests/DspTests.cpp)
    target_include_directories(LumenDspTests PRIVATE Source)
endif()

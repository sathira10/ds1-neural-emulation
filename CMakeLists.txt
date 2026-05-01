cmake_minimum_required(VERSION 4.2)
project(boss_ds1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add dependencies
include(cmake/CPM.cmake)

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.12
)
    
juce_add_plugin(MidiRouter
    VERSION                     0.0.0
    COMPANY_NAME                "Partial Audio"
    BUNDLE_ID                   "com.PartialAudio.ds1"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PLUGIN_MANUFACTURER_CODE    PaLK
    PLUGIN_CODE                 P003
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Boss DS-1")

target_sources(MidiRouter
    PRIVATE
        Source/Core/PluginEditor.cpp
        Source/Core/PluginProcessor.cpp
        Source/DSP/ML_Engine.cpp
        Source/DSP/OversamplingWrapper.cpp)

target_include_directories(MidiRouter
    PRIVATE
        Source/Core
        Source/DSP)

target_compile_definitions(MidiRouter
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MidiRouter
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

cmake_minimum_required(VERSION 4.2)
project(boss_ds1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.12
)

# --- RTNeural (Eigen backend) ---
set(RTNEURAL_EIGEN ON  CACHE BOOL "" FORCE)
set(RTNEURAL_XSIMD OFF CACHE BOOL "" FORCE)
set(RTNEURAL_STL   OFF CACHE BOOL "" FORCE)
CPMAddPackage(
    NAME RTNeural
    GITHUB_REPOSITORY jatinchowdhury18/RTNeural
    GIT_TAG 1fb1f075a5d66e85bfc8f488c3f3626840cb3a1d
)

# Copy juce-framework-frontend into the WebUI tree at configure time so that
# the npm local-file dependency resolves without relying on the volatile _deps path.
file(COPY "${JUCE_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/"
     DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WebUI/node_modules_local/juce-framework-frontend")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Assets")

find_program(NPM_EXECUTABLE npm REQUIRED)

file(GLOB_RECURSE WEBUI_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WebUI/src/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WebUI/index.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WebUI/package.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WebUI/vite.config.ts")

set(WEBUI_ZIP "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Assets/boss-ds1-gui_1.0.0.zip")

add_custom_command(
    OUTPUT  "${WEBUI_ZIP}"
    COMMAND "${NPM_EXECUTABLE}" install
    COMMAND "${NPM_EXECUTABLE}" run build
    COMMAND "${NPM_EXECUTABLE}" run zip
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/WebUI"
    DEPENDS ${WEBUI_SOURCES}
    COMMENT "Building Boss DS-1 WebUI (React/Vite)...")

add_custom_target(BossDS1WebUIBuild DEPENDS "${WEBUI_ZIP}")

juce_add_binary_data(BossDS1WebUI
    HEADER_NAME DS1BinaryData.h
    NAMESPACE   DS1BinaryData
    SOURCES     "${WEBUI_ZIP}")

add_dependencies(BossDS1WebUI BossDS1WebUIBuild)

juce_add_binary_data(BossDS1Models
    HEADER_NAME DS1ModelData.h
    NAMESPACE   DS1ModelData
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models/lstm_model.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models/gru_model.json")

juce_add_plugin(Boss_DS1
    VERSION                     0.0.0
    COMPANY_NAME                "Partial Audio"
    BUNDLE_ID                   "com.PartialAudio.ds1"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    NEEDS_WEB_BROWSER           TRUE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PLUGIN_MANUFACTURER_CODE    PaLK
    PLUGIN_CODE                 P003
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "Boss DS-1")

target_sources(Boss_DS1
    PRIVATE
        Source/Core/PluginEditor.cpp
        Source/Core/PluginProcessor.cpp
        Source/DSP/ML_Engine.cpp
        Source/DSP/OversamplingWrapper.cpp)

target_include_directories(Boss_DS1
    PRIVATE
        Source/Core
        Source/DSP)

target_compile_definitions(Boss_DS1
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Boss_DS1
    PRIVATE
        BossDS1WebUI
        BossDS1Models
        RTNeural
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

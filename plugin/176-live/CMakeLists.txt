cmake_minimum_required(VERSION 3.24)
project(UA176Live VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${LIVE176_DECK}")
    message(FATAL_ERROR "Set LIVE176_DECK to the generated Live176Deck.h")
endif()
configure_file("${LIVE176_DECK}" "${CMAKE_CURRENT_BINARY_DIR}/Live176Deck.h" COPYONLY)

add_library(UA176LiveCircuit STATIC Live176.cpp)
set_target_properties(UA176LiveCircuit PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(UA176LiveCircuit PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(UA176LiveCircuit PRIVATE /arch:AVX2)
else()
    target_compile_options(UA176LiveCircuit PRIVATE -mavx2 -mfma)
endif()
add_executable(UA176LiveSmoke Smoke.cpp)
target_link_libraries(UA176LiveSmoke PRIVATE UA176LiveCircuit)

option(UA176_LIVE_BUILD_VST3 "Build the JUCE 176 Live plug-in" ON)
if(UA176_LIVE_BUILD_VST3)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.13
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
    juce_add_binary_data(UA176LivePanel HEADER_NAME UA176Art.h NAMESPACE UA176Art SOURCES ../176/Faceplate.svg)
    juce_add_plugin(UA176Live
        COMPANY_NAME "Tube Comp"
        BUNDLE_ID "com.tubecomp.176live"
        PLUGIN_MANUFACTURER_CODE "Tcmp"
        PLUGIN_CODE "L176"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        FORMATS VST3
        PRODUCT_NAME "176 Live")
    target_sources(UA176Live PRIVATE ../176/Plugin.cpp)
    target_compile_definitions(UA176Live PRIVATE UA176_LIVE=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(UA176Live PRIVATE UA176LiveCircuit UA176LivePanel juce::juce_audio_processors juce::juce_gui_extra
        juce::juce_recommended_config_flags)
endif()

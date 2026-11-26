cmake_minimum_required(VERSION 3.24)
project(UA176Live VERSION 0.2.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)

if(NOT EXISTS "${LIVE176_DECK}")
    message(FATAL_ERROR "Set LIVE176_DECK to the generated Live176Deck.h")
endif()
configure_file("${LIVE176_DECK}" "${CMAKE_CURRENT_BINARY_DIR}/Live176Deck.h" COPYONLY)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(UA176PhaseReference PhaseReference.cpp Live176.cpp)
target_include_directories(UA176PhaseReference PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(UA176PhaseReference PRIVATE UA176_PHASE_REFERENCE=1)
if(MSVC)
    target_compile_options(UA176PhaseReference PRIVATE /arch:AVX2)
else()
    target_compile_options(UA176PhaseReference PRIVATE -mavx2 -mfma)
endif()
set(phaseProfiles "${CMAKE_CURRENT_BINARY_DIR}/PhaseProfiles.h")
add_custom_command(OUTPUT "${phaseProfiles}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/PhaseProfiles.py"
        "$<TARGET_FILE:UA176PhaseReference>" "${phaseProfiles}"
    DEPENDS UA176PhaseReference "${CMAKE_CURRENT_SOURCE_DIR}/PhaseProfiles.py"
    VERBATIM)

set(oversampleData "${CMAKE_CURRENT_BINARY_DIR}/OversampleData.h")
add_custom_command(OUTPUT "${oversampleData}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/PhaseProfiles.py"
        "$<TARGET_FILE:UA176PhaseReference>" "${oversampleData}" --oversampling
    DEPENDS UA176PhaseReference "${CMAKE_CURRENT_SOURCE_DIR}/PhaseProfiles.py"
    VERBATIM)
add_library(UA176LiveCircuit STATIC Live176.cpp Oversampled176.cpp GainReductionMeter.cpp
    "${phaseProfiles}" "${oversampleData}")
target_link_libraries(UA176LiveCircuit PUBLIC Threads::Threads)
set_target_properties(UA176LiveCircuit PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(UA176LiveCircuit PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(UA176LiveCircuit PRIVATE /arch:AVX2)
else()
    target_compile_options(UA176LiveCircuit PRIVATE -mavx2 -mfma)
endif()
add_executable(UA176LiveSmoke Smoke.cpp)
target_link_libraries(UA176LiveSmoke PRIVATE UA176LiveCircuit)
add_executable(UA176OversamplingSmoke OversamplingSmoke.cpp)
target_link_libraries(UA176OversamplingSmoke PRIVATE UA176LiveCircuit)

option(UA176_LIVE_BUILD_VST3 "Build the JUCE 176 Live plug-in" ON)
if(UA176_LIVE_BUILD_VST3)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.13
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
    set(panel "${CMAKE_CURRENT_SOURCE_DIR}/../176/Faceplate.svg")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${panel}")
    file(READ "${panel}" livePanel)
    string(REGEX REPLACE "<g id=\"legacy-power\">([^<]*<[^/][^>]*>)*[^<]*</g>" "" livePanel "${livePanel}")
    string(REGEX REPLACE "<text x=\"570[.]00\" y=\"109[.]00\"[^>]*>VU</text>" "" livePanel "${livePanel}")
    string(REGEX REPLACE "(<text x=\"370[.]00\" y=\"123[.]00\"[^>]*>)VERNIER" "\\1MIX" livePanel "${livePanel}")
    string(REGEX REPLACE "(<text x=\"772[.]00\" y=\"123[.]00\"[^>]*>)VERNIER" "\\1SC HPF" livePanel "${livePanel}")
    set(livePanelFile "${CMAKE_CURRENT_BINARY_DIR}/Faceplate.svg")
    file(WRITE "${livePanelFile}" "${livePanel}")
    juce_add_binary_data(UA176LivePanel HEADER_NAME UA176Art.h NAMESPACE UA176Art SOURCES "${livePanelFile}")
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

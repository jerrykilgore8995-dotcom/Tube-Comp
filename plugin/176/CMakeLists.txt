cmake_minimum_required(VERSION 3.24)
project(UA176Spice VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(spice "${CMAKE_CURRENT_SOURCE_DIR}/../../spice")
file(GLOB ua_deck_dependencies CONFIGURE_DEPENDS "${spice}/176/*.inc" "${spice}/176/*.lib")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/UA176Deck.h"
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/make_deck.py"
            "${spice}/176/powered.cir" "${CMAKE_CURRENT_BINARY_DIR}/UA176Deck.h"
    DEPENDS make_deck.py "${spice}/176/powered.cir" "${spice}/models/tube_cells.lib" ${ua_deck_dependencies}
    VERBATIM)

juce_add_binary_data(UA176Panel HEADER_NAME UA176Art.h NAMESPACE UA176Art SOURCES Faceplate.svg)
juce_add_plugin(UA176Spice
    COMPANY_NAME "Tube Comp"
    BUNDLE_ID "com.tubecomp.176spice"
    PLUGIN_MANUFACTURER_CODE "Tcmp"
    PLUGIN_CODE "S176"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3
    PRODUCT_NAME "176")
target_sources(UA176Spice PRIVATE Plugin.cpp SpiceStream.cpp "${CMAKE_CURRENT_BINARY_DIR}/UA176Deck.h")
target_include_directories(UA176Spice PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(UA176Spice PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(UA176Spice PRIVATE UA176Panel juce::juce_audio_processors juce::juce_gui_extra
    juce::juce_recommended_config_flags Threads::Threads ${CMAKE_DL_LIBS})

add_executable(UA176SpiceSmoke SpiceSmoke.cpp SpiceStream.cpp "${CMAKE_CURRENT_BINARY_DIR}/UA176Deck.h")
target_include_directories(UA176SpiceSmoke PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(UA176SpiceSmoke PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

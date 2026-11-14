cmake_minimum_required(VERSION 3.24)
project(TubeComp VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.17-security
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(TubeComp
  COMPANY_NAME "Tube Comp"
  BUNDLE_ID "com.tubecomp.rs124"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE "Tcmp"
  PLUGIN_CODE "Rs12"
  FORMATS VST3
  PRODUCT_NAME "Tube Comp RS124")
target_sources(TubeComp PRIVATE
  PluginProcessor.cpp PluginProcessor.h
  PluginEditor.cpp PluginEditor.h
  TubeCompDSP.h
  ../gui/TubeCompFaceplate.h)
target_include_directories(TubeComp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../gui)
target_compile_definitions(TubeComp PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TubeComp PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)
add_executable(TubeCompDSPTests ../tests/TubeCompDSPTests.cpp)
target_include_directories(TubeCompDSPTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
  target_compile_options(TubeCompDSPTests PRIVATE /W4 /EHsc)
else()
  target_compile_options(TubeCompDSPTests PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()
add_test(NAME TubeCompDSPTests COMMAND TubeCompDSPTests)


# Screenshot the complete host editor (not the isolated faceplate).
juce_add_console_app(TubeCompFullEditorPreview PRODUCT_NAME "Tube Comp Complete Editor Preview")
target_sources(TubeCompFullEditorPreview PRIVATE PreviewFullEditor.cpp PluginProcessor.cpp PluginEditor.cpp)
target_include_directories(TubeCompFullEditorPreview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../gui)
target_compile_definitions(TubeCompFullEditorPreview PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TubeCompFullEditorPreview PRIVATE
 juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)

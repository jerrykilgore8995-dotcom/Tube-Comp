cmake_minimum_required(VERSION 3.24)
project(TubeCompFrontPanel VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
 GIT_TAG 8.0.17-security GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_console_app(TubeCompFaceplates PRODUCT_NAME "Tube Comp Faceplate Preview")
target_sources(TubeCompFaceplates PRIVATE Preview.cpp TubeCompFaceplate.h)
target_compile_definitions(TubeCompFaceplates PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(TubeCompFaceplates PRIVATE
 juce::juce_gui_extra juce::juce_recommended_config_flags
 juce::juce_recommended_warning_flags)

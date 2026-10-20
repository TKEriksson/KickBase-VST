cmake_minimum_required(VERSION 3.22)
project(StrudelSpike VERSION 0.0.1)

# Spike: kör Strudel i JUCE:s QuickJS (juce_javascript). Eget program, rör inte Kickbase.
# STRUDEL_JUCE_DIR = befintlig JUCE 8.0.15-källa (undviker nedladdning); annars hämtas den.
include(FetchContent)
if(STRUDEL_JUCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${STRUDEL_JUCE_DIR}")
endif()
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_console_app(StrudelSpike PRODUCT_NAME "StrudelSpike")
target_sources(StrudelSpike PRIVATE Spike.cpp)
target_compile_definitions(StrudelSpike PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(StrudelSpike PRIVATE juce::juce_core juce::juce_javascript psapi juce::juce_recommended_config_flags)

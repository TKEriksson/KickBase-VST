cmake_minimum_required(VERSION 3.22)

project(MyOwnFruit VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Skriver build/<config>/compile_commands.json, som VS Code läser för att förstå
# flaggor och makron (t.ex. JucePlugin_Name) som CMake definierar.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# JUCE hämtas automatiskt vid första konfigureringen.
# Dokumentation: https://github.com/juce-framework/JUCE/blob/8.0.15/docs/CMake%20API.md
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(CORE_SOURCES
    Source/Core/TransportClock.cpp
)

# Gemensamt för alla mål: samma definitioner och flaggor.
function(myownfruit_configure target)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    target_link_libraries(${target} PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endfunction()

# MyOwnFruit: instrumentet som läggs i Channel Rack. Spelar mönstret när en ton hålls.
juce_add_plugin(MyOwnFruit
    COMPANY_NAME "Tobias Eriksson"
    BUNDLE_ID com.tobiaseriksson.myownfruit
    PLUGIN_MANUFACTURER_CODE Tbes
    PLUGIN_CODE Mofr
    FORMATS VST3 Standalone
    PRODUCT_NAME "MyOwnFruit"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    # Att kopiera till C:\Program Files\Common Files\VST3 kräver admin, så det görs för hand.
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(MyOwnFruit PRIVATE
    ${CORE_SOURCES}
    Source/Instrument/InstrumentProcessor.cpp
    Source/Instrument/InstrumentEditor.cpp
)
target_link_libraries(MyOwnFruit PRIVATE juce::juce_audio_utils)
myownfruit_configure(MyOwnFruit)

# MyOwnFruit FX: effekten som läggs i mixern och hackar upp ljudet från en annan synth.
juce_add_plugin(MyOwnFruitFX
    COMPANY_NAME "Tobias Eriksson"
    BUNDLE_ID com.tobiaseriksson.myownfruitfx
    PLUGIN_MANUFACTURER_CODE Tbes
    PLUGIN_CODE Mofx
    FORMATS VST3
    PRODUCT_NAME "MyOwnFruit FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(MyOwnFruitFX PRIVATE
    ${CORE_SOURCES}
    Source/Fx/FxProcessor.cpp
    Source/Fx/FxEditor.cpp
)
target_link_libraries(MyOwnFruitFX PRIVATE juce::juce_audio_processors)
myownfruit_configure(MyOwnFruitFX)

# Tester för kärnan (ingen plugin, bara ett konsolprogram). Körs av build.bat.
juce_add_console_app(MyOwnFruitTests PRODUCT_NAME "MyOwnFruitTests")
target_sources(MyOwnFruitTests PRIVATE
    ${CORE_SOURCES}
    Tests/TransportClockTests.cpp
)
target_link_libraries(MyOwnFruitTests PRIVATE juce::juce_audio_basics)
myownfruit_configure(MyOwnFruitTests)

cmake_minimum_required(VERSION 3.22)

# CMP0141 NEW: låter oss välja felsökningsformat (se längre ner) med CMAKE_MSVC_DEBUG_INFORMATION_FORMAT.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(Kickbase VERSION 0.1.0)

# Kompilatorcache: om sccache (eller ccache) finns används den automatiskt. Då går en ren ombyggnad eller ett byte av gren
# mycket snabbare, eftersom oförändrade filer hämtas ur cachen. Installera med `choco install sccache` eller `winget install Mozilla.sccache`.
find_program(KICKBASE_COMPILER_CACHE NAMES sccache ccache)
if (KICKBASE_COMPILER_CACHE)
    message(STATUS "Kompilatorcache: ${KICKBASE_COMPILER_CACHE}")
    set(CMAKE_C_COMPILER_LAUNCHER ${KICKBASE_COMPILER_CACHE})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${KICKBASE_COMPILER_CACHE})
endif()

# Felsökningsinformation i varje objektfil (/Z7) i stället för en delad PDB (/Zi): med många kompileringar parallellt får
# annars alla vänta på samma PDB-server. Ger snabbare byggen; länkaren skriver fortfarande den färdiga .pdb:n.
if (MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()

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

# JUCE:s modul-filer (juce_core.cpp ...) får inte förkompilerade headers: de måste själva vara först att inkludera sina headers.
file(GLOB KICKBASE_JUCE_MODULE_CPPS "${juce_SOURCE_DIR}/modules/*/*.cpp")
set_source_files_properties(${KICKBASE_JUCE_MODULE_CPPS} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Ljudkärnan: klocka, mönster och motorer. Inget gränssnitt, går att testa fristående.
set(CORE_SOURCES
    Source/Core/TransportClock.cpp
    Source/Core/Pattern.cpp
    Source/Core/KickEngine.cpp
    Source/Core/InstrumentSynth.cpp
    Source/Core/InstrumentVoices.cpp
    Source/Core/Wavetable.cpp
    Source/Core/WavetableEdit.cpp
    Source/Core/NodeEnvelope.cpp
    Source/Core/StepScheduler.cpp
    Source/Core/Sidechain.cpp
    Source/Core/Modulation.cpp
    Source/Core/Settings.cpp
    Source/Core/Tb303Filter.cpp
    Source/Core/FilterModels.cpp
    Source/Core/FilterUnit.cpp
    Source/ThirdParty/open303/rosic_TeeBeeFilter.cpp
    Source/ThirdParty/open303/rosic_OnePoleFilter.cpp
    Source/ThirdParty/open303/rosic_RealFunctions.cpp
    Source/ThirdParty/open303/GlobalFunctions.cpp
    Source/ThirdParty/open303/rosic_NumberManipulations.cpp
    Source/Core/LibraryManifest.cpp
    Source/Core/LibraryImport.cpp
    Source/Core/LibraryIndex.cpp
)

# Plugin-delarna som båda pluginsen delar, inklusive gränssnittet.
set(PLUGIN_SOURCES
    ${CORE_SOURCES}
    Source/Core/Effects.cpp
    Source/Core/SourceEq.cpp
    Source/Core/MixAnalysis.cpp
    Source/Core/Master.cpp
    Source/Core/Mixer.cpp
    Source/Core/OfflineRender.cpp
    Source/Core/AudioAnalysis.cpp
    Source/Core/OscPresets.cpp
    Source/Core/RollFiles.cpp
    Source/Core/SampleSlot.cpp
    Source/Core/ToolLayer.cpp
    Source/Core/LiveCode.cpp
    Source/Core/BridgeLink.cpp
    Source/Core/Params.cpp
    Source/Core/Presets.cpp
    Source/Core/PluginBase.cpp
    Source/Ui/KickbaseLookAndFeel.cpp
    Source/Ui/KickLibraryPanel.cpp
    Source/Ui/EnvelopeView.cpp
    Source/Ui/StepSequencer.cpp
    Source/Ui/PianoRoll.cpp
    Source/Ui/Modules.cpp
    Source/Ui/PresetButton.cpp
    Source/Ui/ChannelViews.cpp
    Source/Ui/EffectsPage.cpp
    Source/Ui/SidechainPage.cpp
    Source/Ui/MasterPage.cpp
    Source/Ui/MixerPage.cpp
    Source/Ui/LfoPage.cpp
    Source/Ui/MatrixPage.cpp
    Source/Ui/EnvPage.cpp
    Source/Ui/SettingsPage.cpp
    Source/Ui/OptimizationsPanel.cpp
    Source/Ui/AboutPanel.cpp
    Source/Ui/SpectrumPage.cpp
    Source/Ui/LogoPainter.cpp
    Source/Core/WavetablePack.cpp
    Source/Core/AiChat.cpp
    Source/Core/AiProvider.cpp
    Source/Ui/AiChatPage.cpp
    Source/Ui/LiveCodePage.cpp
    Source/Ui/Components.cpp
    Source/Ui/Help.cpp
    Source/Ui/FilterPanel.cpp
    Source/Ui/HarmonicEditor.cpp
    Source/Ui/OscPanel.cpp
    Source/Ui/FmPanel.cpp
    Source/Ui/FmPresets.cpp
    Source/Ui/KickDisplay.cpp
    Source/Ui/Scopes.cpp
    Source/Ui/SoundstagePage.cpp
)

set(INSTRUMENT_SOURCES
    Source/Instrument/InstrumentProcessor.cpp
    Source/Instrument/InstrumentEditor.cpp
)

set(FX_SOURCES
    Source/Fx/FxProcessor.cpp
    Source/Fx/FxEditor.cpp
)

# Guiderna som MCP-bryggan serverar till AI:n (get_guide), inbakade i programmet.
# (Resources/InitPreset.mofp: fabrikspresetet Init för instrumentet, ett vanligt Full-preset som läses vid start.)
# Versionen som ABOUT visar: projektets VERSION (bara den här filen kompileras om när den ändras).
set_source_files_properties(Source/Ui/AboutPanel.cpp PROPERTIES COMPILE_DEFINITIONS "KICKBASE_VERSION=\"${PROJECT_VERSION}\"")

juce_add_binary_data(KickbaseGuides SOURCES Resources/AGENT_GUIDE.md Resources/psytrance-kick-bass-reference.md Resources/kick-bass-school.md Resources/InitPreset.mofp Resources/strudel/strudel.all.js LICENSE THIRD-PARTY.md)

# Gemensamt för alla mål: samma definitioner och flaggor.
function(kickbase_configure target)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    target_link_libraries(${target} PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
endfunction()

# Link-time optimization ger snabbare plugins men tar flera minuter att länka,
# så bara pluginsen får det (inte tester och verktyg).
function(kickbase_configure_plugin target)
    kickbase_configure(${target})
    target_link_libraries(${target} PUBLIC juce::juce_recommended_lto_flags)
endfunction()

# Kickbase: instrumentet som läggs i Channel Rack. Spelar mönstret när en ton hålls.
juce_add_plugin(Kickbase
    COMPANY_NAME "TKDEV"
    BUNDLE_ID com.tkdev.kickbase
    PLUGIN_MANUFACTURER_CODE Tbes
    PLUGIN_CODE Mofr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Kickbase"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    # Att kopiera till C:\Program Files\Common Files\VST3 kräver admin, så det görs för hand.
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Kickbase PRIVATE
    ${PLUGIN_SOURCES}
    ${INSTRUMENT_SOURCES}
    Source/Instrument/InstrumentEntry.cpp
)
target_link_libraries(Kickbase PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_javascript KickbaseGuides)
target_precompile_headers(Kickbase PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Source/PchPlugin.h>")
kickbase_configure_plugin(Kickbase)

# Kickbase FX: effekten som läggs i mixern och hackar upp ljudet från en annan synth.
juce_add_plugin(KickbaseFX
    COMPANY_NAME "TKDEV"
    BUNDLE_ID com.tkdev.kickbasefx
    PLUGIN_MANUFACTURER_CODE Tbes
    PLUGIN_CODE Mofx
    FORMATS VST3
    PRODUCT_NAME "Kickbase FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(KickbaseFX PRIVATE
    ${PLUGIN_SOURCES}
    ${FX_SOURCES}
    Source/Fx/FxEntry.cpp
)
target_link_libraries(KickbaseFX PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_javascript KickbaseGuides)
target_precompile_headers(KickbaseFX PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Source/PchPlugin.h>")
kickbase_configure_plugin(KickbaseFX)

# Tester för kärnan (ingen plugin, bara ett konsolprogram). Körs av build.bat.
juce_add_console_app(KickbaseTests PRODUCT_NAME "KickbaseTests")
target_sources(KickbaseTests PRIVATE
    ${CORE_SOURCES}
    Tests/TransportClockTests.cpp
    Tests/EngineTests.cpp
    Tests/MasterTests.cpp
    Tests/ModulationTests.cpp
    Tests/FmPresetTests.cpp
    Tests/PersistFmTests.cpp
    Source/Ui/FmPresets.cpp
    Source/Core/Params.cpp
    Source/Core/Master.cpp
    Source/Core/SourceEq.cpp
    Source/Core/Effects.cpp
    Source/Core/RollFiles.cpp
    Tests/TestMain.cpp
)
target_link_libraries(KickbaseTests PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp)
target_precompile_headers(KickbaseTests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Source/PchCore.h>")
kickbase_configure(KickbaseTests)

# KickbaseMcp: MCP-bryggan (stdio) som AI-appar som Claude Desktop startar. Kör Kickbase utan DAW eller kopplar upp sig
# mot en Kickbase i DAW:en. Samma program innehåller också utvecklingsverktyget (renderar ljud offline, tar skärmbilder,
# analyserar): det startas som `KickbaseTool.exe` (en kopia som görs efter bygget) eller `KickbaseMcp.exe tool <kommando>`.
# De delar all kod, så den kompileras bara en gång.
juce_add_console_app(KickbaseMcp PRODUCT_NAME "KickbaseMcp")
target_sources(KickbaseMcp PRIVATE
    ${PLUGIN_SOURCES}
    ${INSTRUMENT_SOURCES}
    ${FX_SOURCES}
    Tools/McpServer.cpp
    Tools/Tool.cpp
    Tools/Analyzer.cpp
    Tools/Profile.cpp
    Tools/AudioTests.cpp
    Tools/StateTests.cpp
    Tools/LibraryTests.cpp
    Tools/Freesound.cpp
    Tools/Tb303Tests.cpp
    Tools/Tb303Unit.cpp
    Tools/FilterModelTests.cpp
    Tools/PatternTests.cpp
)
target_compile_definitions(KickbaseMcp PRIVATE JucePlugin_Name="Kickbase" JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(KickbaseMcp PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_javascript KickbaseGuides)
target_precompile_headers(KickbaseMcp PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Source/PchPlugin.h>")
kickbase_configure(KickbaseMcp)
add_custom_command(TARGET KickbaseMcp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/KickbaseTool_artefacts/$<CONFIG>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:KickbaseMcp>" "${CMAKE_BINARY_DIR}/KickbaseTool_artefacts/$<CONFIG>/KickbaseTool.exe"
    VERBATIM)

# KickbaseHostTest: laddar den byggda VST3-filen som en värd, spelar, öppnar och stänger editorn och laddar ur den
# (fångar krascher och hängningar vid avslut, som FL Studio annars visar).
juce_add_console_app(KickbaseHostTest PRODUCT_NAME "KickbaseHostTest")
target_sources(KickbaseHostTest PRIVATE Tools/HostTest.cpp)
target_compile_definitions(KickbaseHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(KickbaseHostTest PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_basics)
kickbase_configure(KickbaseHostTest)

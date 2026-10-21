cmake_minimum_required(VERSION 3.22)

project(OpenEDMFilter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(OEF_BUILD_PLUGIN "Build the plugin" ON)
option(OEF_BUILD_TESTS "Build the unit tests" ON)

include(FetchContent)

add_library(openedm_core STATIC
    Source/Core/MultiModeFilter.cpp
    Source/Core/Modulators.cpp
    Source/Core/Effects.cpp
    Source/Core/Oversampler.cpp
    Source/Core/FilterEngine.cpp)
target_include_directories(openedm_core PUBLIC Source/Core)
set_target_properties(openedm_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(OEF_BUILD_PLUGIN)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)

    juce_add_binary_data(FilterAssets SOURCES assets/analog-cyber-faceplate.png)
    set_target_properties(FilterAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(OpenEDMFilter
        COMPANY_NAME "Alan Marcero"
        BUNDLE_ID "com.alanmarcero.openedmfilter"
        PRODUCT_NAME "openEDMFilter"
        PLUGIN_MANUFACTURER_CODE Amrc
        PLUGIN_CODE Oedf
        FORMATS VST3 AU LV2 Standalone
        LV2URI "https://github.com/alanmarcero/openEDMFilter"
        VST3_CATEGORIES Fx Filter
        AU_MAIN_TYPE kAudioUnitType_Effect
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(OpenEDMFilter PRIVATE
        Source/Plugin/Parameters.cpp
        Source/Plugin/Presets.cpp
        Source/Plugin/EdmLookAndFeel.cpp
        Source/Plugin/Controls.cpp
        Source/Plugin/Panels.cpp
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp)

    target_include_directories(OpenEDMFilter PRIVATE
        Source/Core
        Source/Plugin)

    target_compile_definitions(OpenEDMFilter PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(OpenEDMFilter
        PRIVATE
            openedm_core
            FilterAssets
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(OEF_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.9.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(openedm_tests
        Tests/MultiModeFilterTests.cpp
        Tests/ModulatorsTests.cpp
        Tests/EffectsTests.cpp
        Tests/FilterEngineTests.cpp
        Tests/OversamplerTests.cpp)

    target_include_directories(openedm_tests PRIVATE Tests)
    target_link_libraries(openedm_tests PRIVATE openedm_core Catch2::Catch2WithMain)

    enable_testing()
    include(Catch)
    catch_discover_tests(openedm_tests)
endif()

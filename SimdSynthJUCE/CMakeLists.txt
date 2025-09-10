cmake_minimum_required(VERSION 3.15)

project(SimdSynth VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JUCE
find_path(JUCE_DIR modules/juce_audio_basics/juce_audio_basics.h
    PATHS "${CMAKE_SOURCE_DIR}/JUCE" "/Users/jayvaughan/Documents/Development/Plugins/JUCE"
    REQUIRED
)

# Add JUCE modules
add_library(juce STATIC IMPORTED)
set_target_properties(juce PROPERTIES
    IMPORTED_LOCATION "${JUCE_DIR}/modules"
)

# Define plugin
juce_add_plugin(SimdSynth
    VERSION "1.0.0"
    COMPANY_NAME "YourCompany"
    PLUGIN_MANUFACTURER_CODE "Your"
    PLUGIN_CODE "SimS"
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "SimdSynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

# Source files
target_sources(SimdSynth
    PRIVATE
        Source/SimdSynth.cpp
        Source/SimdSynthProcessor.cpp
)

# Include directories
target_include_directories(SimdSynth
    PRIVATE
        ${JUCE_DIR}/modules
)

# Link JUCE modules
target_link_libraries(SimdSynth
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# SIMD compiler flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    target_compile_options(SimdSynth PRIVATE -msse -msse2)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_options(SimdSynth PRIVATE -march=armv8-a+simd)
endif()

# Enable strict warnings
target_compile_options(SimdSynth PRIVATE -Wall -Wextra -Wpedantic)

set(CMAKE_VERBOSE_MAKEFILE ON)

cmake_minimum_required(VERSION 3.15)

project(SimdSynth VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# By default we don't want Xcode schemes to be made for modules, etc
#set(CMAKE_XCODE_GENERATE_SCHEME OFF)
# set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Support macOS down to High Sierra")

include(FetchContent)

# Fetch JUCE
FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9    # Replace with the desired JUCE version
)

FetchContent_MakeAvailable(juce)

# Define plugin
juce_add_plugin(SimdSynth
        VERSION "1.0.0"
        COMPANY_NAME "seclorum"
        PLUGIN_MANUFACTURER_CODE "SCLR"
        PLUGIN_CODE "SmSy"
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "SimdSynth"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
)

# Add the generated header path (e.g., JuceHeader.h)
juce_generate_juce_header(SimdSynth)
target_include_directories(SimdSynth PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(SimdSynth INTERFACE ${juce_generated_headers_directory})

# Source files
target_sources(SimdSynth
        PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginEntry.cpp
        Source/PresetManager.cpp
        Source/PresetManager.h
)

# Link JUCE modules
target_link_libraries(SimdSynth
        PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Define compile definitions
target_compile_definitions(SimdSynth
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Copy plugin to standard plugin directories after build
juce_enable_copy_plugin_step(SimdSynth)

message("Processor: ${CMAKE_SYSTEM_PROCESSOR}")

# SIMD compiler flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    target_compile_options(SimdSynth PRIVATE -msse -msse2 -msse4.1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    if(APPLE)
        # For Apple Silicon (M1, M2, etc.), NEON is enabled by default, but we can ensure it
        target_compile_options(SimdSynth PRIVATE -mcpu=apple-m1)
    else()
        # For other ARM64 systems (e.g., Linux), use a generic ARMv8-A with NEON
        target_compile_options(SimdSynth PRIVATE -march=armv8-a -mfpu=neon)
    endif()
endif()

# Alternative - TODO: Check on other arm targets, etc.
## SIMD compiler flags
#if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
#    target_compile_options(SimdSynth PRIVATE -msse -msse2 -msse4.1)
#elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
#    # NEON is typically enabled by default on ARM64, but we can ensure compatibility
#    target_compile_options(SimdSynth PRIVATE -march=armv8-a)
#endif()

# Enable strict warnings
target_compile_options(SimdSynth PRIVATE -Wall -Wextra -Wpedantic)
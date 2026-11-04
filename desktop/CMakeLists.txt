# Millstone desktop build: VST3 + AU + Standalone from the shared engine, via JUCE.
# Configure from this folder:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
# JUCE and the Open MPC kit (for wrapper/engine.h and the params generator) are fetched automatically.
cmake_minimum_required(VERSION 3.22)
# These two must come before project(): it creates both cache entries, empty, and a later set() would not replace them.
if(CMAKE_HOST_APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS supported")
endif()
project(Millstone VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)

include(FetchContent)
# Pin JUCE to a released tag (at least two weeks old).
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
# The Open MPC kit supplies wrapper/engine.h and tools/gen_vst.py; it has no CMakeLists, so this only populates it.
FetchContent_Declare(mpcvst
    GIT_REPOSITORY https://github.com/open-mpc/mpc-vst-plugins.git
    GIT_TAG bf1054ce08460cf42b10f96985e73141d64a71af
    GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(juce mpcvst)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Generate params.h from the single source of truth (vst/params.json), exactly as the MPC build does.
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${mpcvst_SOURCE_DIR}/tools/gen_vst.py ${REPO}/vst/vst.json --params-h
    RESULT_VARIABLE gen_result
    OUTPUT_VARIABLE gen_out ERROR_VARIABLE gen_out)
if(NOT gen_result EQUAL 0)
    message(FATAL_ERROR "gen_vst.py --params-h failed:\n${gen_out}")
endif()

# Read the engine source list from vst.json so it never drifts from the MPC build.
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c
        "import json;print(';'.join(json.load(open('${REPO}/vst/vst.json'))['build']['sources']))"
    OUTPUT_VARIABLE ENGINE_SRC_REL OUTPUT_STRIP_TRAILING_WHITESPACE)
set(ENGINE_SOURCES "")
foreach(s ${ENGINE_SRC_REL})
    list(APPEND ENGINE_SOURCES ${REPO}/${s})
endforeach()

# Build the shared engine as its own static library so JUCE's flags (C++17, strict) don't reach it: the
# Plaits sources need GNU C++14 (zero-size arrays), -DTEST (portable math, not ARM asm) and -include cstdio.
# Modules that are desktop-only until the MPC build can be tested again.
list(APPEND ENGINE_SOURCES
    ${REPO}/src/third_party/eurorack/tides2/poly_slope_generator.cc
    ${REPO}/src/third_party/eurorack/tides2/resources.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/exciter.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/multistage_envelope.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/ominous_voice.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/part.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/resonator.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/string.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/tube.cc
    ${REPO}/src/third_party/eurorack/elements/dsp/voice.cc
    ${REPO}/src/third_party/eurorack/elements/resources.cc)
add_library(millstone_engine STATIC ${ENGINE_SOURCES})
set_target_properties(millstone_engine PROPERTIES
    CXX_STANDARD 14 CXX_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(millstone_engine PRIVATE TEST MILLSTONE_DESKTOP)
target_compile_options(millstone_engine PRIVATE -include cstdio -w)
target_include_directories(millstone_engine PRIVATE
    ${REPO}/src ${REPO}/src/third_party/eurorack ${REPO}/vst/build ${mpcvst_SOURCE_DIR}/wrapper)

# The effects after the engine, on their own so that the test tools can reach them directly. The plate reverb is Rings', which needs
# the same -DTEST as the engine; C++17 for std::clamp.
add_library(millstone_fx STATIC
    src/fx/Chorus.cpp src/fx/FxChain.cpp src/fx/Phaser.cpp src/fx/Reverb.cpp src/fx/SpringReverb.cpp src/fx/TapeDelay.cpp)
set_target_properties(millstone_fx PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(millstone_fx PRIVATE TEST)
target_compile_options(millstone_fx PRIVATE -include cstdio)
target_include_directories(millstone_fx PRIVATE src/fx ${REPO}/src/third_party/eurorack)
target_include_directories(millstone_fx INTERFACE src/fx)

juce_add_plugin(Millstone
    PRODUCT_NAME "Millstone"
    COMPANY_NAME "Glaser"
    BUNDLE_ID "com.glaser.millstone"
    PLUGIN_MANUFACTURER_CODE Glsr
    PLUGIN_CODE Mlst
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Millstone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PluginEditor.cpp)

# Embedded so the plugin is self-contained. The panel layout is desktop-only; the presets are shared with the MPC build.
juce_add_binary_data(MillstoneData SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/panel.conf ${REPO}/vst/presets.json)

target_include_directories(Millstone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${REPO}/src
    ${REPO}/src/third_party/eurorack
    ${REPO}/vst/build
    ${mpcvst_SOURCE_DIR}/wrapper)

target_compile_definitions(Millstone PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Millstone PRIVATE
    millstone_engine
    millstone_fx
    MillstoneData
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# The linker's signature covers only the binary, so a finished bundle fails verification. Sign each one as its last
# build step, after JUCE has added its resources. "-" is an ad-hoc signature; pass a Developer ID to distribute.
if(APPLE)
    set(MILLSTONE_SIGN_IDENTITY "-" CACHE STRING "codesign identity for the plugin bundles")
    foreach(format VST3 AU Standalone)
        add_custom_command(TARGET Millstone_${format} POST_BUILD
            COMMAND codesign --force --sign ${MILLSTONE_SIGN_IDENTITY} "$<TARGET_BUNDLE_DIR:Millstone_${format}>"
            COMMAND codesign --verify --strict "$<TARGET_BUNDLE_DIR:Millstone_${format}>"
            VERBATIM)
    endforeach()
endif()

# Developer check: renders each page to a PNG and exercises the controls. Not part of a normal build.
option(MILLSTONE_UI_SNAPSHOT "Build the UI snapshot test tool" OFF)
if(MILLSTONE_UI_SNAPSHOT)
    add_executable(millstone_ui_snapshot tools/ui_snapshot.cpp)
    target_include_directories(millstone_ui_snapshot PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${REPO}/src
        ${REPO}/src/third_party/eurorack
        ${REPO}/vst/build
        ${mpcvst_SOURCE_DIR}/wrapper)
    target_link_libraries(millstone_ui_snapshot PRIVATE Millstone millstone_engine MillstoneData juce::juce_audio_utils)
    add_executable(millstone_fx_test tools/fx_test.cpp)
    set_target_properties(millstone_fx_test PROPERTIES CXX_STANDARD 17)
    target_link_libraries(millstone_fx_test PRIVATE millstone_fx)
    add_executable(millstone_fx_plugin_test tools/fx_plugin_test.cpp)
    target_include_directories(millstone_fx_plugin_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${REPO}/src
        ${REPO}/src/third_party/eurorack
        ${REPO}/vst/build
        ${mpcvst_SOURCE_DIR}/wrapper)
    target_link_libraries(millstone_fx_plugin_test PRIVATE Millstone millstone_engine MillstoneData juce::juce_audio_utils)
    add_executable(millstone_behaviour_test tools/behaviour_test.cpp)
    target_include_directories(millstone_behaviour_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${REPO}/src
        ${REPO}/src/third_party/eurorack
        ${REPO}/vst/build
        ${mpcvst_SOURCE_DIR}/wrapper)
    target_link_libraries(millstone_behaviour_test PRIVATE Millstone millstone_engine MillstoneData juce::juce_audio_utils)
    add_executable(millstone_stress_test tools/stress_test.cpp)
    target_include_directories(millstone_stress_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${REPO}/src
        ${REPO}/src/third_party/eurorack
        ${REPO}/vst/build
        ${mpcvst_SOURCE_DIR}/wrapper)
    target_link_libraries(millstone_stress_test PRIVATE Millstone millstone_engine MillstoneData juce::juce_audio_utils)
    add_executable(millstone_preset_test tools/preset_test.cpp)
    target_include_directories(millstone_preset_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${REPO}/src
        ${REPO}/src/third_party/eurorack
        ${REPO}/vst/build
        ${mpcvst_SOURCE_DIR}/wrapper)
    target_link_libraries(millstone_preset_test PRIVATE Millstone millstone_engine MillstoneData juce::juce_audio_utils)
endif()

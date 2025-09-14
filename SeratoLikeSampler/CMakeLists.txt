
cmake_minimum_required(VERSION 3.22)
project(Noob_Tools VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(JUCE_FETCH_VST3_SDK "Automatically fetch the Steinberg VST3 SDK" ON)

# Time/Pitch and analysis feature flags
option(USE_SIGNALSMITH "Use SignalSmith time/pitch (header-only)" ON)
option(USE_RUBBERBAND  "Use Rubber Band library (best quality)" OFF)
option(USE_AUBIO       "Use Aubio for onset/tempo/key" OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(JUCE)

# Optional: SignalSmith (header-only). Pulled at configure time if enabled.
if (USE_SIGNALSMITH)
  message(STATUS "SignalSmith stretch enabled")
  FetchContent_Declare(
      signalsmith_stretch
      GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
      GIT_TAG main
  )
  FetchContent_MakeAvailable(signalsmith_stretch)
endif()

juce_add_plugin(Noob_Tools
    COMPANY_NAME "CRF"
    PLUGIN_MANUFACTURER_CODE CFLT
    PLUGIN_CODE SLSM
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "Noob_Tools"
)

juce_generate_juce_header(Noob_Tools)

target_sources(Noob_Tools PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/AudioEngine.cpp
    Source/AudioEngine.h
    Source/PadVoice.cpp
    Source/PadVoice.h
    Source/SamplePool.cpp
    Source/SamplePool.h
    Source/Slicer.cpp
    Source/Slicer.h
    Source/TimeStretch.h
    Source/WaveformCache.cpp
    Source/WaveformCache.h
    Source/Params.h
    Source/Utilities.h
    Source/SliceListComponent.h
    Source/SamplerLookAndFeel.cpp
    Source/SamplerLookAndFeel.h
    # DSP scaffolding
    Source/DSP/TimePitch/TimePitchEngine.h
)

# Embed assets from the Resources folder (e.g., logo.png) into the binary
set(PROJECT_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/Resources")
if (EXISTS "${PROJECT_RESOURCES_DIR}")
  file(GLOB_RECURSE RESOURCE_FILES
       RELATIVE "${CMAKE_SOURCE_DIR}"
       "${PROJECT_RESOURCES_DIR}/*")
  if (RESOURCE_FILES)
    juce_add_binary_data(SamplerAssets SOURCES ${RESOURCE_FILES})
    target_link_libraries(Noob_Tools PRIVATE SamplerAssets)
  endif()
endif()

target_compile_definitions(Noob_Tools PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_REPORT_APP_USAGE=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# Feature defines for conditional compilation in code
if (USE_SIGNALSMITH)
  target_compile_definitions(Noob_Tools PRIVATE USE_SIGNALSMITH=1)
  # Add include path to fetched repo (header-only)
  if (DEFINED signalsmith_stretch_SOURCE_DIR)
    target_include_directories(Noob_Tools PRIVATE ${signalsmith_stretch_SOURCE_DIR})
  endif()
  # Add include path for the Signalsmith linear dependency (provides signalsmith-linear/stft.h)
  if (DEFINED signalsmith_linear_SOURCE_DIR)
    target_include_directories(Noob_Tools PRIVATE ${signalsmith_linear_SOURCE_DIR}/include)
  endif()
  # Forward optional acceleration toggles to our target if set in the cache
  if (DEFINED SIGNALSMITH_USE_PFFFT AND SIGNALSMITH_USE_PFFFT)
    target_compile_definitions(Noob_Tools PRIVATE SIGNALSMITH_USE_PFFFT=1)
  endif()
  if (DEFINED SIGNALSMITH_USE_PFFFT_DOUBLE AND SIGNALSMITH_USE_PFFFT_DOUBLE)
    target_compile_definitions(Noob_Tools PRIVATE SIGNALSMITH_USE_PFFFT_DOUBLE=1)
  endif()
  if (DEFINED SIGNALSMITH_USE_IPP AND SIGNALSMITH_USE_IPP)
    target_compile_definitions(Noob_Tools PRIVATE SIGNALSMITH_USE_IPP=1)
  endif()
endif()

# Optional: Rubber Band via package manager (e.g., vcpkg)
if (USE_RUBBERBAND)
  find_package(rubberband CONFIG QUIET)
  if (rubberband_FOUND)
    target_link_libraries(Noob_Tools PRIVATE rubberband::rubberband)
    target_compile_definitions(Noob_Tools PRIVATE USE_RUBBERBAND=1)
  else()
    message(STATUS "rubberband not found; set USE_RUBBERBAND=OFF or provide via vcpkg")
  endif()
endif()

# Optional: Aubio via package manager
if (USE_AUBIO)
  find_package(aubio CONFIG QUIET)
  if (aubio_FOUND)
    target_link_libraries(Noob_Tools PRIVATE aubio)
    target_compile_definitions(Noob_Tools PRIVATE USE_AUBIO=1)
  else()
    message(STATUS "aubio not found; set USE_AUBIO=OFF or provide via vcpkg")
  endif()
endif()
# Optional: Enable ASIO on Windows by providing the ASIO SDK location
# Usage: configure with -DASIO_SDK_DIR="C:/SDKs/ASIOSDK" or set in VS CMake Settings.
if (WIN32 AND DEFINED ASIO_SDK_DIR AND EXISTS "${ASIO_SDK_DIR}")
  # Try to resolve the actual ASIO SDK root that contains 'common/asio.h'
  set(_asio_candidates
      "${ASIO_SDK_DIR}"
      "${ASIO_SDK_DIR}/ASIOSDK2"
      "${ASIO_SDK_DIR}/ASIO"
      "${ASIO_SDK_DIR}/asiosdk"
      "${ASIO_SDK_DIR}/vst3sdk/ASIOSDK2"
      "${ASIO_SDK_DIR}/vst3sdk/ASIO"
  )
  unset(ASIO_SDK_ROOT CACHE)
  foreach(_r IN LISTS _asio_candidates)
    if (EXISTS "${_r}/common/asio.h")
      set(ASIO_SDK_ROOT "${_r}" CACHE PATH "Resolved ASIO SDK root")
      break()
    endif()
  endforeach()
  if (DEFINED ASIO_SDK_ROOT)
    message(STATUS "ASIO enabled. Using SDK at: ${ASIO_SDK_ROOT}")
    target_compile_definitions(Noob_Tools PRIVATE JUCE_ASIO=1)
    target_include_directories(Noob_Tools PRIVATE
      "${ASIO_SDK_ROOT}"
      "${ASIO_SDK_ROOT}/common"
      "${ASIO_SDK_ROOT}/host"
      "${ASIO_SDK_ROOT}/host/pc"
    )
  else()
    message(STATUS "ASIO_SDK_DIR set ('${ASIO_SDK_DIR}') but 'common/asio.h' not found. ASIO disabled.")
  endif()
endif()

target_link_libraries(Noob_Tools PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

# Output layout: build/x64/<Config>
set(OUTPUT_BASE "${CMAKE_BINARY_DIR}/x64/$<CONFIG>")
if (TARGET Noob_Tools_Standalone)
  set_target_properties(Noob_Tools_Standalone PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_BASE}"
    OUTPUT_NAME "Noob_Tools"
  )
endif()
if (TARGET Noob_Tools_VST3)
  set_target_properties(Noob_Tools_VST3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_BASE}/VST3"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_BASE}/VST3"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_BASE}/VST3"
    OUTPUT_NAME "Noob_Tools"
    VST3_COPY_DIR "${OUTPUT_BASE}/VST3"
  )
endif()

if (MSVC)
  target_compile_options(Noob_Tools PRIVATE /W4 /permissive-)
endif()

# Copy Resources (if present) to Standalone and VST3 outputs
set(PROJECT_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/Resources")
if (EXISTS "${PROJECT_RESOURCES_DIR}")
  message(STATUS "Resources found: ${PROJECT_RESOURCES_DIR}")
  if (TARGET Noob_Tools_Standalone)
    add_custom_command(TARGET Noob_Tools_Standalone POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_BASE}/Resources"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_RESOURCES_DIR}" "${OUTPUT_BASE}/Resources"
      COMMENT "Copying Resources to Standalone output")
  endif()
  if (TARGET Noob_Tools_VST3)
    add_custom_command(TARGET Noob_Tools_VST3 POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_BASE}/VST3/Noob_Tools.vst3/Contents/Resources"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_RESOURCES_DIR}" "${OUTPUT_BASE}/VST3/Noob_Tools.vst3/Contents/Resources"
      COMMENT "Copying Resources to VST3 bundle")
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS Noob_Tools
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

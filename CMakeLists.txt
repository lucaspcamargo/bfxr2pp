cmake_minimum_required(VERSION 3.15)
project(bfxr2pp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BFXR2_BUILD_TOOLS "Build CLI render tool" ON)
option(BFXR2_ENABLE_JSON "Include built-in JSON parameter reader" ON)
option(BFXR2_BUILD_EXAMPLES "Build example applications" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

if(EMSCRIPTEN)
  # for the web, build executables as html
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(CMAKE_EXECUTABLE_EXTENSION ".html")
endif()

# Core sources
set(BFXR2_SRCS
  src/bfxr_dsp.cpp
  src/params.cpp
  src/presets.cpp
  src/wav.cpp
  src/akwf_tables.cpp
)

set(BFXR2_HEADERS
  include/bfxr2/bfxr_dsp.h
  include/bfxr2/params.h
  include/bfxr2/presets.h
  include/bfxr2/prng.h
  include/bfxr2/param_reader.h
  include/bfxr2/wav.h
  include/bfxr2/akwf_tables.h
)

if(BFXR2_ENABLE_JSON)
  list(APPEND BFXR2_SRCS src/json_min.cpp)
  list(APPEND BFXR2_HEADERS include/bfxr2/json_min.h)
endif()

# Core library
add_library(bfxr2 ${BFXR2_SRCS})
add_library(bfxr2::bfxr2 ALIAS bfxr2)

target_include_directories(bfxr2
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(BFXR2_ENABLE_JSON)
  target_compile_definitions(bfxr2 PUBLIC BFXR2_ENABLE_JSON=1)
endif()

set_target_properties(bfxr2 PROPERTIES
  OUTPUT_NAME "bfxr2"
  EXPORT_NAME "bfxr2"
  SOVERSION 1
  VERSION ${PROJECT_VERSION}
)

# Executables
if(BFXR2_BUILD_TOOLS)
  if(NOT BFXR2_ENABLE_JSON)
    message(FATAL_ERROR "BFXR2_BUILD_TOOLS requires BFXR2_ENABLE_JSON=ON")
  endif()
  add_executable(bfxr_render src/cli_main.cpp)
  target_link_libraries(bfxr_render PRIVATE bfxr2)
endif()

# Examples
if(BFXR2_BUILD_EXAMPLES)
  if(NOT EMSCRIPTEN)
    find_package(SDL3 CONFIG QUIET)
    if(NOT SDL3_FOUND)
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
      set(BFXR2_SDL3_TARGET PkgConfig::SDL3)
    else()
      set(BFXR2_SDL3_TARGET SDL3::SDL3)
    endif()
  endif()

  add_executable(bfxr2pp_sdl_play examples/sdl_playback/main.cpp)
  if(EMSCRIPTEN)
    target_compile_options(bfxr2pp_sdl_play PRIVATE "-sUSE_SDL=3")
    set_target_properties(bfxr2pp_sdl_play PROPERTIES LINK_FLAGS "-s USE_SDL=3 --shell-file ${CMAKE_SOURCE_DIR}/ems/shell.html")
  endif()
  target_link_libraries(bfxr2pp_sdl_play PRIVATE bfxr2 ${BFXR2_SDL3_TARGET})

  # Fetch Dear ImGui
  include(FetchContent)
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.8
  )
  FetchContent_MakeAvailable(imgui)

  add_library(imgui_sdl3 STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(imgui_sdl3 PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  if(EMSCRIPTEN)
    target_compile_options(imgui_sdl3 PRIVATE "-sUSE_SDL=3")
  endif()
  target_link_libraries(imgui_sdl3 PUBLIC ${BFXR2_SDL3_TARGET})

  add_executable(bfxr2pp_gui examples/gui/main.cpp)
  target_link_libraries(bfxr2pp_gui PRIVATE bfxr2 imgui_sdl3 ${BFXR2_SDL3_TARGET})

  if(EMSCRIPTEN)
    target_compile_options(bfxr2pp_gui PRIVATE "-sUSE_SDL=3")
    set_target_properties(bfxr2pp_gui PROPERTIES LINK_FLAGS "-s USE_SDL=3 --shell-file ${CMAKE_SOURCE_DIR}/ems/shell.html")
  endif()
endif()

# Installation & CMake Export
install(TARGETS bfxr2
  EXPORT bfxr2Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${BFXR2_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bfxr2
)

if(BFXR2_BUILD_TOOLS)
  install(TARGETS bfxr_render
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

# Export config files for find_package(bfxr2 CONFIG)
set(BFXR2_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/bfxr2")

install(EXPORT bfxr2Targets
  FILE bfxr2Targets.cmake
  NAMESPACE bfxr2::
  DESTINATION ${BFXR2_CONFIG_INSTALL_DIR}
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bfxr2Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/bfxr2Config.cmake"
  INSTALL_DESTINATION ${BFXR2_CONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/bfxr2ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/bfxr2Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/bfxr2ConfigVersion.cmake"
  DESTINATION ${BFXR2_CONFIG_INSTALL_DIR}
)

# Export targets from the build tree as well for direct build-dir consumption
export(EXPORT bfxr2Targets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/bfxr2Targets.cmake"
  NAMESPACE bfxr2::
)
export(PACKAGE bfxr2)

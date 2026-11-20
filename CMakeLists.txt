cmake_minimum_required(VERSION 3.25.0)

project(fairklaus
    VERSION 1.0.0
    DESCRIPTION "PunishChild 660 - VST3 compressor, complete WDF circuit model of the Fairchild 670 (DAFx-12)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(FAIRKLAUS_BUILD_TESTS "Build DSP tests and host simulation" ON)
option(FAIRKLAUS_WITH_EDITOR "Build the VSTGUI editor (needs X11/cairo/freetype on Linux)" ON)

#--- VST3 SDK ---------------------------------------------------------------
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)

set(SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk)

# Editor nur wenn GUI-Deps vorhanden (Linux: X11 + cairo + freetype + fontconfig)
set(FAIRKLAUS_EDITOR_AVAILABLE OFF)
if(FAIRKLAUS_WITH_EDITOR)
    if(APPLE OR WIN32)
        set(FAIRKLAUS_EDITOR_AVAILABLE ON)
    else()
        find_package(X11 QUIET)
        find_package(Freetype QUIET)
        find_path(CAIRO_INCLUDE_DIR cairo.h QUIET)
        find_library(CAIRO_LIBRARY cairo QUIET)
        find_package(Fontconfig QUIET)
        if(X11_FOUND AND Freetype_FOUND AND CAIRO_INCLUDE_DIR AND CAIRO_LIBRARY AND Fontconfig_FOUND)
            set(FAIRKLAUS_EDITOR_AVAILABLE ON)
        else()
            message(STATUS "[fairklaus] GUI deps not found -> building WITHOUT custom editor (headless build).")
            message(STATUS "[fairklaus] Install e.g.: libx11-dev libxcb1-dev libxkbcommon-dev libcairo2-dev libfreetype-dev libfontconfig-dev")
        endif()
    endif()
endif()

set(SMTG_ENABLE_VSTGUI_SUPPORT ${FAIRKLAUS_EDITOR_AVAILABLE} CACHE BOOL "" FORCE)

if(NOT EXISTS ${SDK_ROOT}/CMakeLists.txt)
    message(FATAL_ERROR "vst3sdk submodule missing - run: git submodule update --init --recursive")
endif()

add_subdirectory(${SDK_ROOT} ${CMAKE_BINARY_DIR}/vst3sdk)

add_subdirectory(src)

if(FAIRKLAUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

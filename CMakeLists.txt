cmake_minimum_required(VERSION 3.22)
project(Earware VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# PLATFORM DETECTION
# ============================================
if(WIN32)
    set(EARWARE_PLATFORM "Windows")
    set(EARWARE_PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(EARWARE_PLATFORM "macOS")
    set(EARWARE_PLATFORM_MACOS TRUE)
elseif(UNIX)
    set(EARWARE_PLATFORM "Linux")
    set(EARWARE_PLATFORM_LINUX TRUE)
else()
    set(EARWARE_PLATFORM "Unknown")
endif()

message(STATUS "--- EARWARE BUILD SYSTEM ---")
message(STATUS "Platform: ${EARWARE_PLATFORM}")

# ============================================
# WEBVIEW BACKEND (per platform)
# ============================================
if(EARWARE_PLATFORM_WINDOWS)
    set(JUCE_USE_WIN_WEBVIEW2 ON CACHE BOOL "" FORCE)
    add_definitions(-DJUCE_USE_WIN_WEBVIEW2=1)
    message(STATUS "WebView: WebView2 (Windows)")
elseif(EARWARE_PLATFORM_MACOS)
    set(JUCE_USE_WIN_WEBVIEW2 OFF CACHE BOOL "" FORCE)
    message(STATUS "WebView: WKWebView (macOS)")
elseif(EARWARE_PLATFORM_LINUX)
    set(JUCE_USE_WIN_WEBVIEW2 OFF CACHE BOOL "" FORCE)
    message(STATUS "WebView: WebKitGTK (Linux)")
endif()

# ============================================
# JUCE (git submodule)
# ============================================
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_tools/JUCE")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE submodule missing at ${JUCE_DIR}. Run: git submodule update --init --recursive")
endif()

# Apply the required Linux WebView fix (see patches/) before JUCE is configured.
# The patch changes how JUCE's Linux WebBrowserComponent measures JSON byte
# length for its CommandReceiver IPC, which fixes the WebView UI on Linux.
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C "${JUCE_DIR}" apply --check "${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-8.0.12-commandreceiver-utf8.patch"
        RESULT_VARIABLE JUCE_PATCH_CHECK_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(JUCE_PATCH_CHECK_RESULT EQUAL 0)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -C "${JUCE_DIR}" apply "${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-8.0.12-commandreceiver-utf8.patch"
            RESULT_VARIABLE JUCE_PATCH_APPLY_RESULT
        )
        if(NOT JUCE_PATCH_APPLY_RESULT EQUAL 0)
            message(WARNING "Failed to apply JUCE patch (it may already be applied)")
        else()
            message(STATUS "Applied JUCE CommandReceiver patch")
        endif()
    elseif(NOT JUCE_PATCH_CHECK_RESULT EQUAL 1)
        message(STATUS "JUCE patch already applied (skipping)")
    endif()
endif()

add_subdirectory("${JUCE_DIR}")

# ============================================
# EARWARE PLUGIN
# ============================================
add_subdirectory("plugins/Earware")

message(STATUS "--- END ---")

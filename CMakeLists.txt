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

# Apply the required Linux WebView fix before JUCE is configured.
#
# JUCE 8.0.12's Linux WebBrowserComponent measures its CommandReceiver IPC JSON
# by CHARACTER COUNT instead of UTF-8 BYTE COUNT, truncating the WebView init
# message whenever it contains multibyte characters. The UI then fails to
# render (blank/white window). This change (see patches/) makes JUCE use
# String::getNumBytesAsUTF8().
#
# This is applied deterministically: if the fix is missing we replace the exact
# source line, and we FAIL LOUDLY if the source is not what we expect — a build
# can never silently ship an unfixed JUCE.
if(EARWARE_PLATFORM_LINUX)
    set(JUCE_LINUX_WEBVIEW_CPP "${JUCE_DIR}/modules/juce_gui_extra/native/juce_WebBrowserComponent_linux.cpp")

    if(EXISTS "${JUCE_LINUX_WEBVIEW_CPP}")
        file(READ "${JUCE_LINUX_WEBVIEW_CPP}" JUCE_LINUX_WEBVIEW_CONTENT)

        if(NOT JUCE_LINUX_WEBVIEW_CONTENT MATCHES "json\\.getNumBytesAsUTF8")
            set(JUCE_JSON_LENGTH_LINE "auto jsonLength = static_cast<size_t> (json.length());")
            set(JUCE_MEMCPY_LINE "memcpy (dst, json.toRawUTF8(), jsonLength);")
            string(FIND "${JUCE_LINUX_WEBVIEW_CONTENT}" "${JUCE_JSON_LENGTH_LINE}" JUCE_JSON_LENGTH_POS)
            string(FIND "${JUCE_LINUX_WEBVIEW_CONTENT}" "${JUCE_MEMCPY_LINE}" JUCE_MEMCPY_POS)

            if(JUCE_JSON_LENGTH_POS EQUAL -1)
                message(FATAL_ERROR
                    "JUCE Linux WebView fix: could not locate the CommandReceiver JSON length line in "
                    "${JUCE_LINUX_WEBVIEW_CPP}. Expected the stock JUCE 8.0.12 source.")
            endif()

            string(REPLACE
                "${JUCE_JSON_LENGTH_LINE}"
                "auto utf8       = json.toRawUTF8();\n        auto jsonLength = json.getNumBytesAsUTF8();"
                JUCE_LINUX_WEBVIEW_CONTENT "${JUCE_LINUX_WEBVIEW_CONTENT}")

            if(JUCE_MEMCPY_POS GREATER -1)
                string(REPLACE
                    "${JUCE_MEMCPY_LINE}"
                    "memcpy (dst, utf8, jsonLength);"
                    JUCE_LINUX_WEBVIEW_CONTENT "${JUCE_LINUX_WEBVIEW_CONTENT}")
            endif()

            file(WRITE "${JUCE_LINUX_WEBVIEW_CPP}" "${JUCE_LINUX_WEBVIEW_CONTENT}")
            message(STATUS "Applied JUCE CommandReceiver UTF-8 fix (Linux WebView)")
        else()
            message(STATUS "JUCE CommandReceiver UTF-8 fix already present")
        endif()
    endif()
endif()

add_subdirectory("${JUCE_DIR}")

# ============================================
# EARWARE PLUGIN
# ============================================
add_subdirectory("plugins/Earware")

message(STATUS "--- END ---")

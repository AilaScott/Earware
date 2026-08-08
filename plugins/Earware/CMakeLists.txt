cmake_minimum_required(VERSION 3.15)

# ============================================
# PLATFORM-SPECIFIC CONFIGURATION
# ============================================
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")
set(LV2_URI "")

if(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEB_BROWSER TRUE)
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/Earware")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# ============================================
# BINARY DATA (Web UI Resources)
# ============================================
juce_add_binary_data(Earware_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
)

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(Earware
    COMPANY_NAME "Aila"
    PLUGIN_MANUFACTURER_CODE Aila
    PLUGIN_CODE Earw
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Earware"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
)

# ============================================
# SOURCE FILES
# ============================================
target_sources(Earware
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ParametricEQData.cpp
)

# ============================================
# INCLUDE DIRECTORIES
# ============================================
target_include_directories(Earware
    PRIVATE
        Source
)

# ============================================
# LINK LIBRARIES
# ============================================
target_link_libraries(Earware
    PRIVATE
        Earware_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ============================================
# COMPILE DEFINITIONS
# ============================================
target_compile_definitions(Earware
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        _USE_MATH_DEFINES=1
)

if(WIN32)
    target_compile_definitions(Earware
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

if(APPLE)
    target_compile_definitions(Earware
        PUBLIC
            JUCE_USE_CURL=0
    )
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(Earware
        PUBLIC
            JUCE_USE_CURL=0
            JUCE_JACK=1
    )
endif()

if(UNIX AND NOT APPLE)
add_executable(earware_rendertest)

target_sources(earware_rendertest
    PRIVATE
        Source/RenderTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ParametricEQData.cpp
)

target_include_directories(earware_rendertest
    PRIVATE
        Source
)

target_link_libraries(earware_rendertest
    PRIVATE
        Earware_WebUI
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
)

target_compile_definitions(earware_rendertest
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_JACK=1
        JUCE_USE_CURL=0
)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND AND UNIX AND NOT APPLE)
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
    pkg_check_modules(WEBKIT QUIET webkit2gtk-4.1)
    if(NOT WEBKIT_FOUND)
        pkg_check_modules(WEBKIT QUIET webkit2gtk-4.0)
    endif()
    target_include_directories(earware_rendertest PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    target_link_libraries(earware_rendertest PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
endif()
endif()

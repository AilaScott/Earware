# Earware — Windows Build Manual

Target: Windows 10/11 x64, builds **VST3** and **Standalone**, toolchain **MSVC** (Visual Studio 2022).

## What already works out of the box

- `plugins/Earware/CMakeLists.txt:11-13` already selects `VST3 Standalone` formats and sets `NEEDS_WEBVIEW2 TRUE` when `WIN32`.
- The top-level `CMakeLists.txt:34-37` already defines `JUCE_USE_WIN_WEBVIEW2=1` on Windows.
- The VST3 SDK is vendored inside JUCE 8 — no separate VST3 SDK download is needed.
- Plugin sources contain no platform `#ifdef`s; the DSP and editor code is portable.

## Prerequisites

1. **Visual Studio 2022** with the *Desktop development with C++* workload (MSVC compiler + Windows SDK).
2. **CMake 3.22+**.
3. **Git**.
4. **WebView2 SDK** — mandatory. On Windows, JUCE calls `find_package(WebView2 REQUIRED)` (`_tools/JUCE/extras/Build/CMake/JUCEUtils.cmake:300`) for any target with `NEEDS_WEBVIEW2 TRUE`; CMake configuration **fails** if it is not found.
5. **Access to the private JUCE submodule mirror.** `_tools/JUCE` points at `https://github.com/AilaScott/JUCE.git` (private). `git submodule update` requires you to be authenticated as a GitHub account with read access to it.

## Required code changes

Two known changes must be applied before a Windows build will compile. (The same list is embedded in `WINDOWS_BUILD_AGENT.md`.)

### 1. Guard the headless render test for Linux only

`plugins/Earware/CMakeLists.txt` — the `earware_rendertest` target defines `JUCE_JACK=1` (line 161). JACK exists only on Linux; the file will not compile on MSVC.

Wrap the entire `earware_rendertest` block (lines 127-174) in `if(UNIX AND NOT APPLE)`. This target is a Linux development tool, so the full-block guard is the simplest correct fix. (Alternatively, gate only the `JUCE_JACK=1` definition.)

### 2. Platform-neutral render output path

`plugins/Earware/Source/RenderTest.cpp:187` hardcodes `/tmp/earware_rt_%s.wav`. `/tmp` does not exist on Windows. If the render test is ever built on Windows, change the path to `juce::File::getSpecialLocation (juce::File::tempDirectory)`. (Moot if change #1 keeps the target Linux-only, but it is good hygiene either way.)

## Installing the WebView2 SDK

### Option A — PowerShell into the default NuGet cache (auto-detected)

```powershell
Register-PackageSource -provider NuGet -name nugetRepository -location https://www.nuget.org/api/v2
Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository
```

JUCE's `FindWebView2.cmake` then auto-detects the package at `%USERPROFILE%\AppData\Local\PackageManagement\NuGet\Packages\Microsoft.Web.WebView2*`.

### Option B — `nuget` CLI + explicit location

```powershell
nuget install Microsoft.Web.WebView2 -Version 1.0.3485.44
```

Then pass the extracted package location at configure time:

```
-DJUCE_WEBVIEW2_PACKAGE_LOCATION=C:\path\to\extracted\package
```

## Build steps

```powershell
git clone https://github.com/AilaScott/audio-plugin-coder.git
cd audio-plugin-coder
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Earware_VST3
cmake --build build --config Release --target Earware_Standalone
```

Add `-DJUCE_WEBVIEW2_PACKAGE_LOCATION=...` to the configure line when using Option B above.

## Artifacts

- VST3: `build/plugins/Earware/Earware_artefacts/Release/VST3/Earware.vst3`
- Standalone: `build/plugins/Earware/Earware_artefacts/Release/Standalone/Earware.exe`

## Install and test

- Copy the `Earware.vst3` folder to `C:\Program Files\Common Files\VST3\` and rescan your DAW.
- WebView2 runtime: the Evergreen runtime ships preinstalled on Windows 10/11. `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` (set in `plugins/Earware/CMakeLists.txt`) only removes the loader dependency, not the runtime — offline or older systems need the runtime installed separately.

## Troubleshooting

- **`WebView2 wasn't found` / configure error from `find_package(WebView2 REQUIRED)`**: install via Option A above, or point `-DJUCE_WEBVIEW2_PACKAGE_LOCATION` at an extracted package (Option B).
- **`JUCE_JACK` compile errors**: apply change #1 (Linux-only guard).
- **`fopen` / `/tmp` runtime failure in the render test**: apply change #2.
- **`git submodule update` fails on `_tools/JUCE`**: you are not authenticated as an account that can read `AilaScott/JUCE`. Run `gh auth login` (or configure a PAT for that account) and retry.
- **Prefer MSVC.** MinGW-w64 is not recommended for this plugin: JUCE's WebView2 code includes MSVC-only headers (`wrl.h`, `wrl/wrappers/corewrappers.h`), which MinGW-w64 does not ship.

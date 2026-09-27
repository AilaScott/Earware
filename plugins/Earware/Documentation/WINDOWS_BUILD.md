# Earware — Windows Build Manual

Target: Windows 10/11 x64, builds **VST3**, toolchain **MSVC** (Visual Studio 2022).

## What already works out of the box

- `plugins/Earware/CMakeLists.txt:10-12` already selects the `VST3` format and sets `NEEDS_WEBVIEW2 TRUE` when `WIN32`.
- The top-level `CMakeLists.txt:27-31` already defines `JUCE_USE_WIN_WEBVIEW2=1` on Windows.
- The VST3 SDK is vendored inside JUCE 8 — no separate VST3 SDK download is needed.
- The DSP code is portable (no platform `#ifdef`s). The editor has one Windows-only block (`PluginEditor.cpp:39`, `#if JUCE_WINDOWS`) for the WebView2 backend and temp user-data folder.

## Prerequisites

1. **Visual Studio 2022** with the *Desktop development with C++* workload (MSVC compiler + Windows SDK).
2. **CMake 3.22+**.
3. **Git**.
4. **WebView2 SDK** — mandatory. On Windows, JUCE calls `find_package(WebView2 REQUIRED)` (`_tools/JUCE/extras/Build/CMake/JUCEUtils.cmake:300`) for any target with `NEEDS_WEBVIEW2 TRUE`; CMake configuration **fails** if it is not found.
5. **Access to the JUCE submodule.** `_tools/JUCE` points at the official `https://github.com/juce-framework/JUCE.git`, pinned to the 8.0.12 release commit. `git submodule update --init --recursive` works with public repo access (no fork needed).

## Required code changes

One known change must be applied before a Windows build will compile. It is **already applied in this repo** — it is listed here for reference. (The same list is embedded in `WINDOWS_BUILD_AGENT.md`.)

### 1. Guard the headless render test for Linux only

`plugins/Earware/CMakeLists.txt` — the `earware_rendertest` target defines `JUCE_JACK=1` (line 159). JACK exists only on Linux; the file will not compile on MSVC.

The entire `earware_rendertest` block is wrapped in `if(UNIX AND NOT APPLE)` (line 124 onwards). This target is a Linux development tool, so the full-block guard is the simplest correct fix. (Done.)

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
git clone https://github.com/AilaScott/Earware.git
cd Earware
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Earware_VST3
```

Add `-DJUCE_WEBVIEW2_PACKAGE_LOCATION=...` to the configure line when using Option B above.

## Artifacts

- VST3: `build/plugins/Earware/Earware_artefacts/Release/VST3/Earware.vst3`

## Install and test

- Copy the `Earware.vst3` folder to `C:\Program Files\Common Files\VST3\` and rescan your DAW.
- WebView2 runtime: the Evergreen runtime ships preinstalled on Windows 10/11. `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` (set in `plugins/Earware/CMakeLists.txt`) only removes the loader dependency, not the runtime — offline or older systems need the runtime installed separately.

## Troubleshooting

- **`WebView2 wasn't found` / configure error from `find_package(WebView2 REQUIRED)`**: install via Option A above, or point `-DJUCE_WEBVIEW2_PACKAGE_LOCATION` at an extracted package (Option B).
- **`JUCE_JACK` compile errors**: confirm the `earware_rendertest` block is guarded by `if(UNIX AND NOT APPLE)` (change #1).
- **`git submodule update` fails on `_tools/JUCE`**: network or GitHub access issue — the submodule is public, no authentication needed.
- **Prefer MSVC.** MinGW-w64 is not recommended for this plugin: JUCE's WebView2 code includes MSVC-only headers (`wrl.h`, `wrl/wrappers/corewrappers.h`), which MinGW-w64 does not ship.

# Earware Windows Build — Agent Prompt

Copy everything inside the block below into your agent session on a Windows machine.

````
You are compiling the Earware JUCE audio plugin for Windows on this machine
(Windows 10/11 x64, MSVC).

SCOPE:
- Build the Earware_VST3 target locally with Visual Studio 2022 / MSVC.
- Do NOT modify any .github/workflows files.
- Do NOT modify the JUCE framework (_tools/JUCE).
- Do NOT commit, push, or otherwise publish anything.
- If you must change plugin source to get the build to compile, keep the change minimal and explain it.

SOURCE OF TRUTH:
Read plugins/Earware/Documentation/WINDOWS_BUILD.md in this repo first and follow it.

STEPS:
1. Verify prerequisites: VS2022 with the "Desktop development with C++" workload, CMake 3.22+, Git.
2. Ensure submodules are present:
     git submodule update --init --recursive
   NOTE: _tools/JUCE points at the public repo https://github.com/juce-framework/JUCE.git
   (pinned to the 8.0.12 release commit) — no GitHub authentication needed.
3. Confirm the two known changes are already applied (they are, in this repo):
   a) plugins/Earware/CMakeLists.txt: the earware_rendertest target (it sets JUCE_JACK=1,
      line ~159) is already wrapped so it only builds on Linux (UNIX AND NOT APPLE).
      JACK does not compile on MSVC.
   b) plugins/Earware/Source/RenderTest.cpp (line ~187): hardcodes /tmp/earware_rt_*.wav,
      but the target is Linux-only so this is not applicable on Windows.
   If either change is missing, apply it and note that.
4. Ensure the WebView2 SDK is available. Configure hard-fails without it
   (find_package(WebView2 REQUIRED) in JUCEUtils.cmake). If missing, install it with:
     Register-PackageSource -provider NuGet -name nugetRepository -location https://www.nuget.org/api/v2
     Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository
   or: nuget install Microsoft.Web.WebView2 -Version 1.0.3485.44
       and pass -DJUCE_WEBVIEW2_PACKAGE_LOCATION=<extracted package dir> at configure time.
5. Configure and build:
     cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
     cmake --build build --config Release --target Earware_VST3
6. VERIFY: report the absolute path of the produced artifact
   (build/plugins/Earware/Earware_artefacts/Release/VST3/Earware.vst3).
   If a step fails, paste the full error output and stop rather than guessing at workarounds.

REPORT BACK (concise):
- The exact commands you ran.
- Changes a) and b): already present, or applied by you.
- Build result (pass/fail) and the artifact path.
- Any warnings or deviations from WINDOWS_BUILD.md.
````

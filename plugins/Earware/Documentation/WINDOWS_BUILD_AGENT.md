# Earware Windows Build — Agent Prompt

Copy everything inside the block below into your agent session on a Windows machine.

````
You are compiling the Earware JUCE audio plugin for Windows on this machine
(Windows 10/11 x64, MSVC).

SCOPE:
- Build the Earware_VST3 and Earware_Standalone targets locally with Visual Studio 2022 / MSVC.
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
   NOTE: _tools/JUCE points at the PRIVATE repo https://github.com/AilaScott/JUCE.git. You must be
   authenticated as a GitHub account with read access to it (e.g. gh auth login) or this step fails.
3. Apply these two known changes if they are not already applied:
   a) plugins/Earware/CMakeLists.txt: wrap the earware_rendertest target (it sets JUCE_JACK=1,
      line ~161) so it only builds on Linux (UNIX AND NOT APPLE). JACK does not compile on MSVC.
   b) plugins/Earware/Source/RenderTest.cpp (line ~187): replace the hardcoded /tmp/earware_rt_*.wav
      output path with a platform-neutral temp directory, e.g.
      juce::File::getSpecialLocation (juce::File::tempDirectory).
   If either change is already present, note that and move on.
4. Ensure the WebView2 SDK is available. Configure hard-fails without it
   (find_package(WebView2 REQUIRED) in JUCEUtils.cmake). If missing, install it with:
     Register-PackageSource -provider NuGet -name nugetRepository -location https://www.nuget.org/api/v2
     Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.3485.44 -Source nugetRepository
   or: nuget install Microsoft.Web.WebView2 -Version 1.0.3485.44
       and pass -DJUCE_WEBVIEW2_PACKAGE_LOCATION=<extracted package dir> at configure time.
5. Configure and build:
     cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
     cmake --build build --config Release --target Earware_VST3
     cmake --build build --config Release --target Earware_Standalone
6. VERIFY: report the absolute paths of the produced artifacts
   (build/plugins/Earware/Earware_artefacts/Release/VST3/Earware.vst3 and
    build/plugins/Earware/Earware_artefacts/Release/Standalone/Earware.exe).
   If a step fails, paste the full error output and stop rather than guessing at workarounds.

REPORT BACK (concise):
- The exact commands you ran.
- Changes a) and b): applied by you, or already present.
- Build result per target (pass/fail) and the artifact paths.
- Any warnings or deviations from WINDOWS_BUILD.md.
````

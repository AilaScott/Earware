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
Read plugins/Earware/Documentation/WINDOWS_BUILD.md first and follow it exactly.
It holds the prerequisites, WebView2 SDK / NuGet setup, exact CMake commands, and the
artifact path. This file intentionally does NOT duplicate those details (versions,
commands, paths) so the two documents cannot drift — take them from WINDOWS_BUILD.md,
not from memory.

STEPS:
1. Read WINDOWS_BUILD.md and follow its prerequisites, WebView2 setup, and build commands.
2. Ensure submodules are present:
     git submodule update --init --recursive
   _tools/JUCE points at the public repo https://github.com/juce-framework/JUCE.git
   (pinned to the 8.0.12 release commit) — no GitHub authentication needed.
3. Confirm the known change is already present (it is, in this repo):
   plugins/Earware/CMakeLists.txt wraps the earware_rendertest target (JUCE_JACK=1)
   in if(UNIX AND NOT APPLE), since JACK does not compile on MSVC.
   If this change is missing, apply it and note that.
4. If configure fails on find_package(WebView2 REQUIRED), run the NuGet install
   commands given in WINDOWS_BUILD.md.
5. Build the Earware_VST3 target, then VERIFY: report the absolute path of the
   produced .vst3 artifact (layout given in WINDOWS_BUILD.md).
   If a step fails, paste the full error output and stop rather than guessing at workarounds.

REPORT BACK (concise):
- The exact commands you ran.
- rendertest guard: already present, or applied by you.
- Build result (pass/fail) and the artifact path.
- Any warnings or deviations from WINDOWS_BUILD.md.
````

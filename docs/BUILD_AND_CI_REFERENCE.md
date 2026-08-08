# Earware — Build & CI Reference

Authoring notes for the coding agent. Captures the repo layout, the build/CI
problems being fixed, their root causes (verified against JUCE 8.0.12 source
and the live GitHub repo/release), and the approved implementation plan.

## Repo (AilaScott/Earware, Linux-first)

- `CMakeLists.txt` — root build: platform flags, WebView backend per platform,
  JUCE submodule, applies the Linux WebView fix, adds the plugin.
- `plugins/Earware/CMakeLists.txt` — `juce_add_plugin`; per-platform formats;
  `earware_rendertest` (Linux-only headless DSP test).
- `plugins/Earware/Source/` — `PluginProcessor` / `PluginEditor` /
  `ParametricEQData` (+ `ui/public` WebView assets embedded via
  `juce_add_binary_data`).
- `_tools/JUCE` — git submodule, official `juce-framework/JUCE` pinned to 8.0.12
  (commit `29396c22c9`).
- `.github/workflows/build-release.yml` — release + workflow_dispatch CI.
- `patches/juce-8.0.12-commandreceiver-utf8.patch` — the Linux WebView fix,
  applied at configure time.
- `docs/BUILD_AND_CI_REFERENCE.md` — this file.

## The three problems being fixed

1. Linux VST3 GUI renders blank/WHITE in the CI release artifact. JUCE's Linux
   `WebBrowserComponent` paints white (`fallbackPaint`, `Colours::white`) when
   the WebKit subprocess cannot render the page.
2. The workflow zips `build/plugins/Earware/Earware_artefacts/Release/VST3/`
   verbatim, so release zips carry that deeply nested (wrong) path instead of a
   top-level `Earware.vst3/`.
3. CI matrix is incomplete/wrong: no Windows job; Linux is VST3-only; macOS has
   no deployment target, builds all targets but ships two, and is unsigned.

## Root causes (verified)

- **White Linux GUI** = JUCE's `CommandReceiver::sendCommand` writes a *char
  count* instead of a *UTF-8 byte count* for the WebView init JSON
  (`juce_WebBrowserComponent_linux.cpp`, the `jsonLength` line). With any
  multibyte characters the init message truncates and the page never renders.
  Fix: use `json.getNumBytesAsUTF8()`.
  - The reference repo (audio-plugin-coder) bakes this fix into its **private**
    `AilaScott/JUCE` fork and points `_tools/JUCE` at it.
  - The Earware repo uses official JUCE 8.0.12 + applies the patch via
    `git apply` inside CMake (root `CMakeLists.txt:49-72`). That mechanism can
    silently no-op (e.g. `--check` returns 1), shipping an unfixed JUCE.
    → make it deterministic.
- **`withBackend(webview2)` + `withWinWebView2Options`** in
  `PluginEditor.cpp:37-44` are honored **only on Windows**
  (`juce_WebBrowserComponent_windows.cpp:1308`). On Linux/macOS the backend
  enum is a no-op: `areOptionsSupported` only accepts `defaultBackend` and the
  platform always uses WebKit / WKWebView. Keep the block Windows-only.
- **Release v0.1.0 is stale/mixed**: the Linux asset was built Aug 4 from the
  reference repo (flat zip, and it works); Windows assets came from the initial
  release; macOS came from the 17:35 CI run. No current asset reflects the new
  repo's CI. Do not treat the release as ground truth.

## JUCE facts learned

- `Backend` enum: `defaultBackend`, `ie`, `webview2`. On Windows,
  `defaultBackend` → `Win32WebView` (IE); `webview2` → Chromium (requires
  `JUCE_USE_WIN_WEBVIEW2`).
- Linux plugin WebView uses the embedded `juce_linux_subprocess_helper`
  (auto-built via `_juce_create_embedded_linux_subprocess_target` when the
  target is a plugin on Linux with `NEEDS_WEB_BROWSER`). Already works.
- macOS: WKWebView always; `WebKit.framework` auto-linked via `juce_gui_extra`
  (`OSXFrameworks: WebKit`).
- JUCE 8 minimum macOS deployment target ≈ 10.13; the reference sets
  `-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`.

## Approved changes (final plan)

1. **Root `CMakeLists.txt`**: replace the `git apply` block with a
   deterministic CMake `file(READ)` → check → `string(REPLACE)` →
   `file(WRITE)` on `juce_WebBrowserComponent_linux.cpp`; skip if
   `json.getNumBytesAsUTF8()` is already present; replace the unfixed line
   otherwise; `FATAL_ERROR` if neither form is found.
2. **`PluginEditor.cpp`**: `withBackend(webview2)` +
   `withWinWebView2Options(...)` + `withKeepPageLoadedWhenBrowserIsHidden()`
   only on Windows; macOS/Linux use the default backend
   (`withNativeIntegrationEnabled` + relays), matching the verified reference.
3. **`plugins/Earware/CMakeLists.txt`**: formats → Linux `VST3`,
   Windows `VST3`, macOS `VST3 AU`; delete `LV2_URI` var and `LV2URI` arg;
   keep `AU_MAIN_TYPE`.
4. **`.github/workflows/build-release.yml`**: stage a clean `dist/` and collect
   with `find` so zips contain a top-level `Earware.vst3`; add a **Windows**
   job (MSVC + WebView2 SDK via NuGet or `-DJUCE_WEBVIEW2_PACKAGE_LOCATION`);
   Linux job builds VST3 and runs `earware_rendertest` under xvfb; macOS job
   builds VST3+AU with `-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13`, explicit targets,
   and ad-hoc `codesign`; use `actions/upload-artifact@v5` (+ optional
   `gh release upload`); asset names per README
   (`Earware_Linux_x64_VST3.zip`, `Earware_Windows_x64_VST3.zip`,
   `Earware_macOS_Universal_VST3.zip`, `Earware_macOS_Universal_AU.zip`).
5. **`README.md` + `USER_MANUAL.md`**: formats → VST3 (Linux/Windows),
   VST3+AU (macOS); drop LV2 and Standalone everywhere; fix the Downloads table
   and build-targets list.

## Verification steps

- Configure + build locally, then run `earware_rendertest` under xvfb.
- Confirm the packaged zip contains `Earware.vst3` at the top level (not under
  `build/plugins/...`).
- Grep the repo for `LV2`/`lv2`/`Standalone`: only `status.json` history may
  retain LV2 mentions.

## Notes / open items

- LV2 removed entirely (user: legacy — only Reaper/Ardour use it).
- Standalone removed (user: not wanted for release).
- macOS GUI runtime is still unverified — flag this before calling macOS done.
- Ad-hoc macOS codesign is included (optional-but-recommended); notarization
  needs a Developer ID and is out of scope.
- `AilaScott/JUCE` is **private** — do NOT point the submodule at it for a
  public repo; keep official JUCE + the deterministic patch.

# Earware — Build & CI Reference

Authoring notes for the coding agent. Captures the repo layout, DSP architecture, build/CI pipeline, and known facts verified against JUCE 8.0.12 source and the live GitHub repo.

## Repo (AilaScott/Earware, Linux-first)

- `CMakeLists.txt` — root build: platform flags, WebView backend per platform, JUCE submodule, applies the Linux WebView fix, adds the plugin.
- `plugins/Earware/CMakeLists.txt` — `juce_add_plugin`; per-platform formats; `earware_rendertest` (headless DSP test suite).
- `plugins/Earware/Source/` — `PluginProcessor` / `PluginEditor` / `ParametricEQData` / `LinearPhaseFIR` (+ `ui/public` WebView assets embedded via `juce_add_binary_data`).
- `_tools/JUCE` — git submodule, official `juce-framework/JUCE` pinned to 8.0.12 (commit `29396c22c9`).
- `.github/workflows/build-release.yml` — release + workflow_dispatch CI (Linux/Windows/macOS).
- `patches/juce-8.0.12-commandreceiver-utf8.patch` — the Linux WebView fix, applied at configure time.
- `docs/BUILD_AND_CI_REFERENCE.md` — this file.

## DSP Architecture

### Signal chain

```
Input → juce::dsp::Convolution (2048-tap linear-phase FIR) → Output
```

The original 10-cascaded minimum-phase IIR biquads + preamp gain were replaced entirely by a single FIR convolution. No toggle or dual path.

### FIR design (LinearPhaseFIR.h)

1. **Magnitude sampling** — for each of 1025 DFT bins (0 Hz → Nyquist), evaluate the 10-biquad cascade transfer function `H(f) = preamp × ∏ biquadResponse(f)` using `biquadResponse()`.
2. **Zero-phase spectrum** — build conjugate-symmetric magnitude-only spectrum (no phase), so the IR is symmetric (linear phase).
3. **Direct IDFT** — `ir[n] = (1/N) Σ |H[k]| · e^(j2πkn/N)` with cosine shortcut for real-valued output.
4. **fftShift** — swap halves to center the impulse at index N/2 (removes causal delay, centers group delay).
5. **No windowing** — rectangular (identity). A Hamming window was tried and caused a systematic +5.36 dB offset; removing it fixed DC gain accuracy.
6. **DC gain normalization** — scale IR tap sum to `preampLin × ∏ biquadDCGain()` where LSC returns shelf gain, PK/HSC return 1.0.

### Key constants

| Parameter | Value |
|-----------|-------|
| `irLength` | 2048 taps |
| Group delay | 1024 samples (23.2 ms at 44.1 kHz) |
| Biquad formula | `A = sqrt(10^(gain/20))`, matching RBJ Audio EQ Cookbook / JUCE `makePeakFilter`/`makeLowShelf`/`makeHighShelf` |
| Sample rate | Computed per `currentSampleRate` at runtime; `earwareComputeCurve` uses fixed 48 kHz (display only) |

### Biquad frequency response — critical correction

`biquadResponse()` (LinearPhaseFIR.h) and `iirBiquadResponse()` (RenderTest.cpp) **must use the filter's center frequency (`s.freq`) for coefficient computation** (alpha, cosC), and **only use the evaluation frequency (`freq`) for the z-transform evaluation** (`z1 = e^(-j2π·freq/sr)`). Using the evaluation frequency for both was the root cause of a 6.5 dB output-level mismatch between FIR and IIR reference paths.

### Bypass handling

A ring buffer (`dryDelayBuffer`, 8192 samples) delays the dry signal by `convLatency` samples during bypass transitions, ensuring click-free toggling despite the FIR's latency. The `bypassRamp` (5 ms smoothed value) crossfades between wet and delay-compensated dry.

### Preamp

Preamp gain is baked into the FIR DC gain normalization (audio path) but **not** included in the GUI curve display (`earwareComputeCurve` starts at `0.0f`, not `preset->preampGain`). The AutoEQ reference graphics show raw EQ response without preamp.

## Render test suite (`earware_rendertest`)

Linux-only target (`if(UNIX AND NOT APPLE)` in `plugins/Earware/CMakeLists.txt`). Pink-noise generation in `testOutputLevelMatch` uses a seeded `juce::Random(12345)`; all other tests are deterministic by construction. 7 categories:

| Test | What it verifies | Pass criterion (from `RenderTest.cpp`) |
|------|-----------------|----------------------------------------|
| Magnitude Accuracy | FIR vs analytical IIR response across 256 log-spaced frequencies (20 Hz → 0.49·sr), 4 models; skips bins where IIR < −30 dB | `maxErr < 1.0 dB` (L350) |
| Output Level Match | FIR vs IIR time-domain RMS on seeded pink noise, 3 models (0, 226, 630) | `diff < 0.1 dB` (L434) |
| DC Gain | FIR tap sum equals `preamp × ∏ biquadDCGains` | `err < 0.01` (L507) |
| IR Symmetry | `ir[center−k] == ir[center+k]` about N/2 (linear phase) | `maxErr < 0.001` (L545) |
| Group Delay | Spot-check via phase derivative at 1 kHz vs N/2 (1024) | `deviation < 1.0 samples` (L592) |
| Phase Linearity | Unwrapped phase vs `−ω·N/2` across 63 frequencies (100 Hz → 0.45·sr), 4 models | `maxPhaseError < 0.2 rad` (L644) |
| Latency | `getLatencySamples()` after model load | `latency > 0` (L672) — prints 1024 but does not assert it |

Test models: 0 (flat), 1, 226 (AKG K240 Studio), 630 (Audio-Technica ATH-M30).

### Reference implementations in RenderTest.cpp

- `iirBiquadResponse()` — analytical complex response of a single biquad (used for magnitude comparison).
- `iirProcessReference()` — time-domain TDF-II IIR processing using JUCE coefficients (5 normalized values per biquad after `assignImpl` strips a0).
- `firMagnitudeDB()` — magnitude from FIR IR via DFT.
- `iirMagnitudeDB()` — product of `iirBiquadResponse` across all stages + preamp.

`main()` also runs `directIIRTest()` (coefficient sanity dump) plus 5 render cases that write WAVs to the temp directory (`juce::File::tempDirectory`) before invoking the 7 test categories.

## UI / WebView

- Model dropdown search strips non-alphanumeric characters (`/[^a-z0-9]/g`) from both query and model names before matching, so "HD650" finds "Sennheiser-HD-650".
- Model list: 6,034 entries (index 0 = "No Model Selected" neutral preset).
- Curve drawn on canvas: 256 log-spaced points, x-axis log-frequency (20 Hz–20 kHz), y-axis linear dB (±12 dB range, 0 dB at vertical center).
- Data flows via `evaluateJavascript` from a 30 Hz editor timer; JS signals readiness via `earwareReady` event.

## Build / CI

### Local build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Earware_VST3
# Linux only — the earware_rendertest target is guarded by if(UNIX AND NOT APPLE):
cmake --build build --config Release --target earware_rendertest
./build/plugins/Earware/earware_rendertest
```

CI uses `xvfb-run` prefix for Linux configure/build/test steps.

### Workflow (`build-release.yml`)

Triggers: `release: published` or `workflow_dispatch` (with optional `tag` input).

| Platform | Runner | Targets | Special |
|----------|--------|---------|---------|
| Linux | ubuntu-latest | VST3 + rendertest | apt deps (WebKitGTK/GTK3/Jack/ALSA), xvfb, runs tests headless |
| Windows | windows-2022 | VST3 | WebView2 SDK via NuGet, MSVC |
| macOS | macos-14 | VST3 + AU | Universal (arm64;x86_64), deployment target 10.13, ad-hoc codesign |

Artifacts: `Earware_Linux_x64_VST3.zip`, `Earware_Windows_x64_VST3.zip`, `Earware_macOS_Universal_VST3.zip`, `Earware_macOS_Universal_AU.zip`. Uploads to release when tag is available.

## JUCE facts

- `assignImpl` (IIR Coefficients): removes a0 from 6-element input, divides all others by a0 → `getRawCoefficients()` returns 5 values `{b0/a0, b1/a0, b2/a0, a1/a0, a2/a0}`.
- JUCE TDF-II processing: `output = input·b0 + lv1; lv1 = input·b1 - output·a1 + lv2; lv2 = input·b2 - output·a2`.
- `minimumDecibels = -300.0` (not -100).
- Linux WebView: `CommandReceiver::sendCommand` must write UTF-8 byte count (patched via `patches/`).
- Windows WebView: `withBackend(webview2)` + `withWinWebView2Options` honored only on Windows; Linux/macOS use default backend (WebKit/WKWebView) — platform code ignores the enum.
- macOS: WKWebView always; `WebKit.framework` auto-linked via `juce_gui_extra`.
- JUCE 8 minimum macOS deployment target ≈ 10.13.

## Resolved issues

- **White Linux GUI** — JUCE `CommandReceiver` wrote char count instead of UTF-8 byte count; patched deterministically in root CMakeLists.
- **Flat zip paths** — workflow stages `dist/` with top-level `Earware.vst3`.
- **IIR→FIR magnitude mismatch** — biquad coefficient computation used evaluation frequency instead of filter center frequency (see DSP Architecture).
- **GUI curve preamp offset** — `earwareComputeCurve` included `preampGain` in visual display; now starts at 0.
- **Model search hyphens** — non-alphanumeric characters stripped from both sides before matching.

## Open items

- macOS GUI runtime unverified — flag before calling macOS done.
- Ad-hoc codesign included; notarization needs a Developer ID (out of scope).
- `AilaScott/JUCE` is **private** — do NOT point submodule at it; keep official JUCE + deterministic patch.
- `status.json` retains historical LV2 mentions; LV2 and Standalone are removed from build.

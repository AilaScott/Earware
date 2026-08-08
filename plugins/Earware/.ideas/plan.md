# Earware — Implementation Plan

## Complexity Score: 2 (Moderate)

**Rationale:** Core DSP is standard biquad IIR filtering with pre-computed
coefficients. Main effort is in building the model index (736 ParametricEQ
files), the WebView searchable dropdown, and the canvas-drawn EQ curve. No
advanced DSP (feedback, modulation, spectral processing) is involved.

---

## Implementation Strategy: Single-Pass

All DSP and UI implemented in one phase. The plugin has only 2 parameters
(model choice + bypass) and the DSP chain is deterministic.

---

## Implementation Steps

### Step 1: Data Pre-processing Script
- Write a Python or CMake script that scans all 736 ParametricEQ.txt files
- Parses each into compact binary representation
- Generates `ParametricEQData.h/.cpp` with embedded index
- Total embedded data: ~370 KB (736 × ~500 bytes)

### Step 2: Project Setup
- Create `CMakeLists.txt` following CloudWash's WebView pattern
- `NEEDS_WEBVIEW2=TRUE` on Windows, `NEEDS_WEB_BROWSER=TRUE` on Linux
- Set `JUCE_WEB_BROWSER=1` on all platforms
- Add `juce_add_binary_data()` for parametric EQ data and web UI assets

### Step 3: PluginProcessor
- **Parameter Layout:**
  - `model`: `juce::AudioParameterChoice` with 736 model names
  - `bypass`: `juce::AudioParameterBool`
- **DSP Members:**
  - `juce::dsp::Gain<float>` preampGain (with 10ms ramp)
  - `std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>, 10>` filterChain
  - `juce::SmoothedValue<float>` bypassRamp (5ms)
- **Processing:**
  - On `model` change: parse new preset, update filter coefficients
  - `processBlock`: apply preamp → 10 biquads → bypass ramp
- **State:**
  - Save/restore `model` + `bypass` via `getStateInformation`/`setStateInformation`
- **WebView Data:** Provide `getCurveData()` returning JSON with frequency/equalization arrays

### Step 4: PluginEditor (WebView)
- Create `EarwareAudioProcessorEditor` using `juce::WebBrowserComponent`
- Follow CloudWash's member ordering: Attachments → WebView → Relays
- **Resource Provider:** Embed HTML/CSS/JS via `juce_add_binary_data()` and serve via resource provider lambda
- **JS Bridge:**
  - On model change: call `evaluateJavascript("setCurve(" + jsonData + ")")`
  - On bypass change: call `evaluateJavascript("setBypass(" + bypass + ")")`
  - Receive model selection via `withEventListener("modelSelected", ...)` or native message port
- **Timer:** 60Hz timer pushes current param values (model name, bypass state, curve data) to JS

### Step 5: Web UI (HTML/CSS/JS) — neubrutalism style
- **Layout** (~500×400px):
  - Background: solid `#ffde00` yellow
  - Grid of small black dots (CSS radial-gradient or canvas-drawn)
  - Top bar: bright blue (`#0066ff`) searchable `<select>` or `<input>` + `<datalist>` for model
  - Center: white card with 3px solid black border, containing:
    - SVG or canvas-drawn EQ curve (black polyline)
    - X-axis: log frequency (20Hz–20kHz)
    - Y-axis: gain (±12dB)
  - Bottom-right: bypass toggle button (black border, white fill when off, yellow when on)
- **All text**: black, sans-serif
- **Model dropdown**: `<input>` with search filtering + `<datalist>` for 736 options

### Step 6: CSV Curve Data
- Embed selected model's CSV `equalization` data or load from binary
- Provides 696 frequency/equalization points for smooth curve rendering
- Sent to JS as JSON array

---

## Dependencies

### Required JUCE Modules:
- `juce_audio_basics` — Audio buffer management
- `juce_audio_processors` — Plugin framework, parameters
- `juce_audio_utils` — Audio utilities
- `juce_dsp` — IIR::Filter, Gain, SmoothedValue
- `juce_core` — File I/O, threading, memory
- `juce_graphics` — Drawing primitives
- `juce_gui_basics` — UI basics
- `juce_gui_extra` — WebBrowserComponent

### External:
- **Windows:** WebView2 Runtime (pre-installed Win11+, [redist](https://developer.microsoft.com/en-us/microsoft-edge/webview2/))
- **Linux:** `libwebkit2gtk-4.1-dev` (apt package)
- **macOS:** WKWebView (system, no deps)

### Build Tools:
- CMake + JUCE 8.0.12 (git submodule at `_tools/JUCE`)

---

## Risk Assessment

| Risk | Level | Mitigation |
|---|---|---|
| ParametricEQ.txt format inconsistency | Low | Verified: all 736 files have identical format (11 lines, same schema) |
| Biquad filter instability at extreme Q | Low | All coefficients are pre-computed by oratory1990 within stable range |
| Embedding 370 KB of data | Low | `juce_add_binary_data()` handles this trivially |
| WebView2 on Windows (runtime missing) | Medium | Document redist link; bundle installer in ship phase |
| WebKitGTK on Linux (package missing) | Low | Standard package in all major distros |
| Member ordering for WebView destruction | Low | Proven CloudWash pattern: Attachments → WebView → Relays |

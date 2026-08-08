# Earware — DSP Architecture Specification

## Overview
Earware is a preset-based corrective equalizer that applies oratory1990's
headphone correction curves as a series of biquad IIR filters. The user
selects a headphone model from 736+ presets and the plugin applies the
inverse frequency response correction to flatten the headphones to a
neutral target.

---

## Core Components

### 1. Model Index System
- Scans `oratory1990/{over-ear,in-ear,earbud}/` directories at build time
- Builds a compact index: `std::vector<ModelEntry>` where each entry has:
  - `name`: Display name (e.g., "AKG K371")
  - `category`: over-ear / in-ear / earbud
  - `parametricData`: Pre-parsed Preamp + 10 FilterStage values
- Embedded as binary data via `juce_add_binary_data()` (~370 KB total)
- Provides O(log n) lookup by choice index (0-735)

### 2. Parametric EQ Parser
- Parses each `[ModelName] ParametricEQ.txt` line:
  ```
  Preamp: -5.6 dB
  Filter 1: ON LSC Fc 105 Hz Gain -2.7 dB Q 0.70
  Filter 2: ON PK Fc 182 Hz Gain -2.3 dB Q 1.23
  ...
  ```
  Into `EqPreset` containing:
  - `preampGain`: float (dB)
  - `filters[10]`: each with `type {LSC, HSC, PK}`, `freq` (Hz), `gain` (dB), `q`

### 3. Biquad Filter Chain
- Converts each `FilterStage` into `juce::dsp::IIR::Coefficients<float>`
- Creates a `juce::dsp::ProcessorChain` of stereo biquads
- Filter type mapping:
  - `PK` → `makePeakFilter(sampleRate, freq, q, gain)`
  - `LSC` → `makeLowShelf(sampleRate, freq, q, gain)`
  - `HSC` → `makeHighShelf(sampleRate, freq, q, gain)`
- Uses `ProcessorDuplicator` for stereo processing on each stage
- Preamp gain via `juce::dsp::Gain<float>` with 10ms ramp

### 4. Bypass Handler
- Toggle bypass with `juce::SmoothedValue<float>` (5ms linear ramp)
- Prevents clicks when enabling/disabling

### 5. EQ Curve Provider (WebView Bridge)
- Reads `frequency` and `equalization` columns from model's `.csv`
- Sends curve data to WebView via `evaluateJavascript()` as JSON arrays
- Fallback: estimate curve from parametric EQ coefficients if CSV unavailable

---

## Processing Chain

```
Input (stereo)
  → DC Blocker (prevent DC offset accumulation)
  → Preamp Gain (apply preamp from ParametricEQ.txt)
  → Biquad Filter 1 (PK/LSC/HSC)
  → Biquad Filter 2
  → ...
  → Biquad Filter 10
  → Bypass Ramp (linear 5ms transition)
  → Output (stereo)
```

---

## Parameter Mapping

| Parameter | Component | Function | DSP Range | Display |
|---|---|---|---|---|
| `model` | Model Index | Select headphone model → load filters | 0–735 (int) | Model Name String |
| `bypass` | Bypass Handler | On/Off toggle | 0–1 (bool) | Bypass / On |

---

## Complexity Assessment

**Score: 2 (Moderate)**

**Rationale:**
- DSP is standard biquad chaining — well-understood, proven algorithms
- Model index has 736 entries but data format is uniform (all 11 lines, same schema)
- UI complexity is moderate: custom searchable dropdown + canvas curve drawing
- No real-time allocation, no feedback loops, no non-linear DSP
- Data is pre-compiled at build time (no runtime file I/O for the 736 presets)

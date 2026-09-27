# Earware — User Manual

Earware is a preset-based corrective EQ plugin that applies [AutoEq](https://github.com/jaakkopasanen/AutoEq)'s recommended headphone corrections via a linear-phase FIR convolution.

## Installation

- **VST3:** Copy `Earware.vst3` to your VST3 directory (`~/.vst3/` on Linux, `%COMMONPROGRAMFILES%\VST3\` on Windows).
- **AU (macOS):** Copy `Earware.component` to `/Library/Audio/Plug-Ins/Components/`.

## Quick Start

1. Open Earware in your DAW.
2. Click the dropdown at the top to search for your headphone model (6033 models available across over-ear, in-ear, and earbud categories, 6034 entries including "No Model Selected"). No correction is applied until you pick a model — you can always reset to "No Model Selected" from the list.
3. Toggle **Bypass** to compare corrected vs. uncorrected audio.
4. The EQ curve is displayed on the canvas — the black line shows the correction applied.

## Controls

| Control | Description |
|---------|-------------|
| Model dropdown | Custom searchable dropdown (6033 models from AutoEq's recommended results + "No Model Selected" neutral default, 6034 entries total). Click to see the full list, type to filter (hyphens and spaces are ignored), keyboard ↑/↓/Enter/Esc supported |
| Bypass | Toggle corrective EQ on/off |

## How It Works

Each headphone model has a pre-computed ParametricEQ profile consisting of:
- **Preamp gain** — overall level adjustment (baked into the FIR so DC gain matches the target; not shown on the curve display)
- **10 biquad filters** — peaking (PK), low-shelf (LSC), and high-shelf (HSC) filters that define the target frequency response

The plugin uses these coefficients to design a **2048-tap linear-phase FIR filter** (magnitude-sampled from the 10-biquad cascade), which is convolved with your audio to flatten your headphones to a neutral target.

Linear phase means the correction adds **no phase distortion**, but it introduces a fixed latency of **1024 samples (23.2 ms at 44.1 kHz)**. Your DAW should report and compensate for this automatically. Bypass is click-free (delay-compensated).

## Format Support

- VST3 (Linux, Windows, macOS)
- AU (macOS)

## Resources

- AutoEq recommended results: https://github.com/jaakkopasanen/AutoEq/blob/master/results/README.md

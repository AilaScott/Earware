# Earware — User Manual

Earware is a preset-based corrective EQ plugin that applies [AutoEq](https://github.com/jaakkopasanen/AutoEq)'s recommended headphone corrections via a linear-phase FIR convolution.

## Installation

- **VST3:** Copy the whole `Earware.vst3` folder to your VST3 directory (`~/.vst3/` on Linux, `%COMMONPROGRAMFILES%\VST3\` on Windows, `~/Library/Audio/Plug-Ins/VST3/` on macOS).
- **AU (macOS):** Copy `Earware.component` to `/Library/Audio/Plug-Ins/Components/`.

Windows builds need no additional runtime — no VC++ Redistributable install is required.

## Quick Start

1. Open Earware in your DAW.
2. Click the dropdown at the top to search for your headphone model (6033 models from AutoEq's recommended results — including over-ear, in-ear and earbud models — 6034 entries including "No Model Selected"). No correction is applied until you pick a model — you can always reset to "No Model Selected" from the list. The dropdown shows at most **200 entries at a time**, so type a few characters to narrow it down.
3. Toggle **Bypass** to compare corrected vs. uncorrected audio.
4. The EQ curve is displayed on the canvas — the black line shows the correction applied.

## Controls

| Control | Description |
|---------|-------------|
| Model dropdown | Custom searchable dropdown (6033 models from AutoEq's recommended results + "No Model Selected" neutral default, 6034 entries total). Click to open the list — up to 200 entries are shown at once, so type to filter. Matching ignores non-alphanumeric characters on both sides and falls back to substring search, so "HD650" finds "Sennheiser HD 650". Keyboard ↑/↓/Enter/Esc supported |
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

## Troubleshooting

- **The plugin does not appear in my DAW** — Make sure you copied the whole `Earware.vst3` folder (on Windows, the folder contains `Contents/x86_64-win/Earware.vst3`). Then rescan your plugin list in the DAW's preferences and restart the DAW if needed.
- **"The effect plug-in could not be loaded"** — Close the DAW completely and reopen it, then rescan your plugin list. A stale plugin cache can list the plugin even when it fails to load.
- **"Model list failed to load" in the status bar** — The headphone list did not reach the plugin UI. This is a known issue on the macOS AU build. Please [open an issue](https://github.com/AilaScott/Earware/issues/new) with your OS and DAW.

## Resources

- AutoEq recommended results: https://github.com/jaakkopasanen/AutoEq/blob/master/results/README.md

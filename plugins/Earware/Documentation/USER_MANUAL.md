# Earware — User Manual

Earware is a preset-based corrective EQ plugin that applies oratory1990 headphone correction curves via parametric EQ filters.

## Installation

- **VST3:** Copy `Earware.vst3` to your VST3 directory (`~/.vst3/` on Linux, `%COMMONPROGRAMFILES%\VST3\` on Windows).
- **AU (macOS):** Copy `Earware.component` to `/Library/Audio/Plug-Ins/Components/`.

## Quick Start

1. Open Earware in your DAW.
2. Click the dropdown at the top to search for your headphone model (736 models available across over-ear, in-ear, and earbud categories).
3. Toggle **Bypass** to compare corrected vs. uncorrected audio.
4. The EQ curve is displayed on the canvas — green line shows the correction applied.

## Controls

| Control | Description |
|---------|-------------|
| Model dropdown | Searchable list of 736 headphone models from oratory1990's database |
| Bypass | Toggle corrective EQ on/off |

## How It Works

Each headphone model has a pre-computed ParametricEQ profile consisting of:
- **Preamp gain** — overall level adjustment
- **10 biquad filters** — peaking (PK), low-shelf (LSC), and high-shelf (HSC) filters that shape the frequency response

The plugin loads these coefficients and applies them in series to flatten your headphones to a neutral target.

## Format Support

- VST3 (Linux, Windows, macOS)
- AU (macOS)

## Resources

- oratory1990 headphone database: https://github.com/jaakkopasanen/AutoEq

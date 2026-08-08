# Earware

Earware is a preset-based corrective EQ plugin that applies [oratory1990](https://github.com/jaakkopasanen/AutoEq) headphone correction curves via parametric EQ filters.

Pick your headphone model from a searchable library of **736 presets** and Earware applies the matching EQ curve (10 biquad filters + preamp) to flatten your headphones toward a neutral target.

## Features

- **736 headphone presets** — over-ear, in-ear, and earbud models from oratory1990's measured database
- **HTML/JS UI** — vector-drawn EQ curve display, searchable model dropdown, one-click bypass
- **Bypass** toggle for A/B comparison of corrected vs. uncorrected audio
- Plugin formats: **VST3**, **LV2**, **AU**, and **Standalone**

## Downloads

Pre-built binaries are available on the [Releases](https://github.com/AilaScott/Earware/releases) page:

| Platform | Formats | Artifacts |
|----------|---------|-----------|
| Linux x64 | VST3, LV2 | `Earware_Linux_x64_VST3.zip`, `Earware_Linux_x64_LV2.zip` |
| Windows x64 | VST3, Standalone | `Earware_Windows_x64_VST3.zip`, `Earware_Windows_x64_Standalone.zip` |
| macOS (Universal) | VST3, AU | `Earware_macOS_Universal_VST3.zip`, `Earware_macOS_Universal_AU.zip` |

## Platforms

Earware is developed **Linux-first**: the preferred/reference target and where it receives the most testing. It is confirmed working in **Reaper on both Linux and Windows**.

| Platform | Formats | Status |
|----------|---------|--------|
| Linux x64 | VST3, LV2, Standalone | Primary target — tested in Reaper |
| Windows x64 | VST3, Standalone | Supported — tested in Reaper |
| macOS (Universal) | VST3, AU, Standalone | Supported — not yet tested |

## Requirements

- **CMake** ≥ 3.22
- A C++20 compiler
- **JUCE 8.0.12** (included as a git submodule, with a small vendored patch — see [patches/](patches/))
- Per-platform WebView backend:
  - **Windows:** WebView2 SDK (resolved automatically via NuGet at configure time)
  - **Linux:** WebKitGTK (`libwebkit2gtk-4.1-dev`, `libgtk-3-dev`, `libjack-jackd2-dev`)
  - **macOS:** WKWebView (system)

## Building

```bash
git clone https://github.com/AilaScott/Earware.git
cd Earware
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Build targets:

- `Earware_VST3` — VST3 plugin
- `Earware_Standalone` — standalone application (also built as `Earware_LV2` / `Earware_AU` where supported)
- `earware_rendertest` — Linux-only headless render test

The standalone app and VST3 share the same UI. On Windows, a WebView2 user-data folder is created under the system temp directory on first launch.

## Installation

- **VST3:** copy `Earware.vst3` to your VST3 directory (`~/.vst3/` on Linux, `%COMMONPROGRAMFILES%\VST3\` on Windows).
- **LV2:** copy `Earware.lv2` to `~/.lv2/`.
- **Standalone:** run the executable directly.

See [plugins/Earware/Documentation/USER_MANUAL.md](plugins/Earware/Documentation/USER_MANUAL.md) for usage, and [plugins/Earware/Documentation/WINDOWS_BUILD.md](plugins/Earware/Documentation/WINDOWS_BUILD.md) for detailed Windows build notes.

## Layout

```
Earware/
├── CMakeLists.txt            # root build: platform flags, JUCE, plugin
├── patches/                  # vendored JUCE patches applied at configure time
├── _tools/JUCE/              # git submodule — juce-framework/JUCE @ 8.0.12
└── plugins/Earware/          # the plugin
    ├── Source/               # DSP + editor code
    │   └── ui/public/        # HTML/JS UI embedded into the binary
    ├── generate_data.py      # regenerates ParametricEQData.cpp from AutoEq data
    └── Documentation/        # user manual, build docs
```

## License

GPL-3.0. This plugin uses [JUCE](https://juce.com) (GPL-3.0) and headphone data from [oratory1990/AutoEq](https://github.com/jaakkopasanen/AutoEq).

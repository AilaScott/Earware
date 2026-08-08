# Earware — UI Specification v1

## Design Summary
Neubrutalism-style corrective EQ with bold yellow background, black dot grid,
white EQ curve card with hard black borders, and a bright blue searchable
dropdown. High contrast, flat, unapologetically bold.

## Layout
- **Window Size:** 600x450px
- **Style:** Neubrutalism — flat colors, hard borders, no rounded corners, bold contrast
- **Framework:** WebView (HTML5 Canvas + JUCE integration)

## Layout Structure

```
┌──────────────────────────────────────────────────────────────┐
│  EARWARE  by aila                         ┌──────────────┐ │
│                                            │ [BYPASS]     │ │
│  ┌──────────────────────────────────────┐  └──────────────┘ │
│  │ [HEADPHONE MODEL ▼]                  │                    │
│  │  Searchable dropdown (bright blue)   │                    │
│  └──────────────────────────────────────┘                    │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐    │
│  │                                                      │    │
│  │              EQ CORRECTION CURVE                     │    │
│  │              (Canvas-drawn polyline)                 │    │
│  │                                                      │    │
│  │   +12dB ─┤                                    ├────  │    │
│  │     0dB ─┤                                    ├────  │    │
│  │   -12dB ─┤                                    ├────  │    │
│  │        20Hz    100Hz    1kHz    10kHz    20kHz      │    │
│  │                                                      │    │
│  └──────────────────────────────────────────────────────┘    │
│                                                              │
│  Model: AKG K371                           Status: Active    │
└──────────────────────────────────────────────────────────────┘
```

## Component Specifications

### 1. Background
- **Color:** `#ffde00` (bold yellow), solid fill
- **Dot Grid:** Small black dots (`#000000`, 4px diameter) spaced in a grid pattern (every 32px)
- **Coverage:** Full 600x450px area

### 2. Header / Title Bar
- **Plugin Title:** "EARWARE" (left-aligned, 22px, bold, black, uppercase) with "by aila" subtitle (10px, medium weight, black, right next to title)
- **Bypass Toggle:** Right-aligned, rectangular button
  - Size: 90x32px
  - Border: 2px solid black
  - Background: white (`#ffffff`) when off, yellow (`#ffde00`) when on
  - Text: "BYPASS", black, 11px bold
  - Cursor: pointer

### 3. Model Selector (Searchable Dropdown)
- **Position:** Below header, full-width
- **Component:** HTML `<input>` with `<datalist>` for 736-model autocomplete
- **Size:** Full width with 20px horizontal margin, 40px tall
- **Background:** `#0066ff` (bright blue)
- **Text:** White (`#ffffff`), 14px, bold
- **Border:** 3px solid `#000000` (hard black border)
- **Placeholder:** "Select headphone model..."
- **Dropdown Arrow:** Custom black arrow, right-aligned
- **Interaction:** Typing filters results in real-time, clicking shows full list

### 4. EQ Curve Card
- **Position:** Center section, below model selector
- **Size:** ~560x240px (with 20px margin on each side)
- **Background:** White (`#ffffff`)
- **Border:** 3px solid `#000000` (hard black border)
- **Canvas:** HTML5 Canvas for curve rendering
- **Curve:** Black polyline, 2px stroke width
- **Axis:** 
  - X-axis: Log frequency from 20Hz to 20kHz
  - Y-axis: Gain from -12dB to +12dB
- **Crosshair:** Thin gray (`#cccccc`) horizontal line at 0dB
- **Scales:** Minimal tick marks in gray (`#999999`), 11px font

### 5. Status Bar (Bottom)
- **Position:** Below EQ card
- **Content:** Current model name (left) + "Active"/"Bypassed" status (right)
- **Text:** Black (`#000000`), 12px, medium weight

## Color Palette

### Primary Colors
- **Background:** `#ffde00` (bold yellow)
- **Card:** `#ffffff` (white)
- **Dropdown:** `#0066ff` (bright blue)
- **Borders/Text:** `#000000` (black)
- **Dot Grid:** `#000000` (black, 20% opacity)

### Text Colors
- **All Text:** `#000000` (black)
- **Dropdown Text:** `#ffffff` (white)
- **Placeholder:** `rgba(255, 255, 255, 0.6)`

### Interactive Colors
- **Dropdown Hover:** `#0052cc` (slightly darker blue)
- **Bypass On:** `#ffde00` (yellow background)
- **Bypass Off:** `#ffffff` (white background)
- **Bypass Border:** `#000000` (black)

### Curve Colors
- **EQ Line:** `#000000` (black, 2px)
- **Zero dB Line:** `#cccccc` (light gray, 1px)
- **Axis Ticks:** `#999999` (medium gray)

## Typography
- **Font Family:** system-ui, -apple-system, 'Segoe UI', sans-serif
- **Plugin Title:** 22px, 800 weight (bold), uppercase
- **Dropdown Text:** 14px, 700 weight
- **Dropdown Options:** 13px, 400 weight
- **Status Text:** 12px, 500 weight
- **Bypass Button:** 11px, 700 weight, uppercase

## Spacing & Layout Grid
- **Window Padding:** 20px
- **Card Margin:** 20px (from window edges)
- **Header Height:** 50px (title + bypass)
- **Dropdown Height:** 40px
- **Card Height:** 240px
- **Dot Grid Spacing:** 32px between dots
- **Dot Diameter:** 4px

## Canvas Integration Notes (WebView)
- **Canvas:** EQ curve rendering
- **Update Strategy:** On model change, redraw curve with 696 frequency/equalization points
- **JUCE Bridge:** Model selection via `window.__JUCE__.backend.setParameterValue("model", index)`

## File Structure
```
plugins/Earware/Design/
├── v1-ui-spec.md          (this file)
├── v1-style-guide.md      (detailed style reference)
└── v1-test.html           (working HTML preview)
```

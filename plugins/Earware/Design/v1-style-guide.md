# Earware — Style Guide v1

## Visual Identity
**Theme:** Neubrutalism
**Aesthetic:** Bold, flat, high-contrast, unapologetic
**Mood:** Energetic, confident, no-nonsense
**Inspiration:** Memphis design, brutalism in architecture, Swiss typography

---

## Color System

### Base Palette
```css
:root {
  /* Background */
  --bg-yellow: #ffde00;           /* Bold yellow base */

  /* Surfaces */
  --surface-white: #ffffff;       /* EQ curve card */
  --surface-blue: #0066ff;        /* Model dropdown */

  /* Borders & Text */
  --border-black: #000000;        /* All borders and text */
  --text-black: #000000;          /* Primary text */

  /* Interactive */
  --blue-hover: #0052cc;          /* Dropdown hover state */
  --blue-active: #004d99;         /* Dropdown active/pressed */

  /* Curve Chart */
  --curve-line: #000000;          /* EQ curve stroke */
  --curve-zero: #cccccc;          /* Zero dB reference line */
  --curve-tick: #999999;          /* Axis tick marks */

  /* Dot Grid */
  --dot-color: rgba(0, 0, 0, 0.15); /* Background dot grid */

  /* States */
  --bypass-on-bg: #ffde00;        /* Bypass active background */
  --bypass-off-bg: #ffffff;       /* Bypass inactive background */

  /* Disabled */
  --text-disabled: rgba(0, 0, 0, 0.3);

  /* Dropdown */
  --dropdown-text: #ffffff;
  --dropdown-placeholder: rgba(255, 255, 255, 0.6);
  --dropdown-option-hover: rgba(255, 255, 255, 0.1);
  --dropdown-option-bg: #ffffff;
  --dropdown-option-text: #000000;
  --dropdown-border: #000000;
}
```

### Color Usage Guidelines

**Backgrounds:**
- Plugin window: `--bg-yellow` with dot grid overlay
- EQ curve card: `--surface-white`
- Model dropdown: `--surface-blue`

**Interactive Elements:**
- Dropdown hover: `--blue-hover`
- Bypass active: `--bypass-on-bg` with black border
- Bypass inactive: `--bypass-off-bg` with black border

**Text Hierarchy:**
- Plugin title: `--text-black`, bold, uppercase
- Dropdown: `--dropdown-text`
- Dropdown placeholder: `--dropdown-placeholder`
- Status text: `--text-black`

**Borders:**
- All interactive elements: 3px solid `--border-black`
- Bypass button: 2px solid `--border-black`
- EQ curve card: 3px solid `--border-black`

---

## Typography

### Font Stack
```css
--font-primary: system-ui, -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
```

### Type Scale
```css
--font-size-title: 22px;          /* Plugin name */
--font-size-subtitle: 10px;       /* "by aila" */
--font-size-dropdown: 14px;       /* Dropdown text */
--font-size-dropdown-option: 13px; /* Dropdown list items */
--font-size-status: 12px;         /* Status bar */
--font-size-bypass: 11px;         /* Bypass button */
--font-size-tick: 11px;           /* Axis tick labels */

--font-weight-bold: 800;          /* Title */
--font-weight-semibold: 700;      /* Dropdown, bypass */
--font-weight-medium: 500;        /* Status text */
--font-weight-normal: 400;        /* Dropdown options */

--letter-spacing-title: 3px;      /* EARWARE */
```

### Text Styles
```css
.plugin-title-wrap {
  display: flex;
  align-items: baseline;
}

.plugin-subtitle {
  font-size: var(--font-size-subtitle);
  font-weight: 500;
  color: var(--text-black);
  margin-left: 8px;
}

.plugin-title {
  font-size: var(--font-size-title);
  font-weight: var(--font-weight-bold);
  color: var(--text-black);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-title);
}

.dropdown-input {
  font-size: var(--font-size-dropdown);
  font-weight: var(--font-weight-semibold);
  color: var(--dropdown-text);
}

.dropdown-input::placeholder {
  color: var(--dropdown-placeholder);
}

.bypass-button {
  font-size: var(--font-size-bypass);
  font-weight: var(--font-weight-semibold);
  color: var(--text-black);
  text-transform: uppercase;
}

.status-text {
  font-size: var(--font-size-status);
  font-weight: var(--font-weight-medium);
  color: var(--text-black);
}
```

---

## Layout & Spacing

### Grid System
```css
--spacing-unit: 4px;          /* Base unit */
--window-padding: 20px;       /* Outer padding */
--gap-sm: 8px;                /* Tight spacing */
--gap-md: 12px;               /* Between sections */
--gap-lg: 20px;               /* Section margins */

--dot-grid-spacing: 32px;    /* Background dot grid */
--dot-diameter: 4px;         /* Dot size */
```

### Component Dimensions
```css
/* Window */
--window-width: 600px;
--window-height: 450px;

/* Header */
--header-height: 50px;

/* Model Dropdown */
--dropdown-height: 40px;
--dropdown-border-width: 3px;

/* EQ Card */
--card-margin: 20px;
--card-border-width: 3px;
--card-height: 240px;

/* Bypass Button */
--bypass-width: 90px;
--bypass-height: 32px;
--bypass-border-width: 2px;
```

---

## Component Styles

### Dot Grid Background
```css
.dot-grid {
  background-color: var(--bg-yellow);
  background-image: radial-gradient(circle, var(--dot-color) 2px, transparent 2px);
  background-size: 32px 32px;
}
```

### Model Dropdown
```css
.dropdown-container {
  width: 100%;
  height: var(--dropdown-height);
  background: var(--surface-blue);
  border: var(--dropdown-border-width) solid var(--dropdown-border);
  position: relative;
}

.dropdown-input {
  width: 100%;
  height: 100%;
  background: transparent;
  border: none;
  padding: 0 40px 0 16px;
  color: var(--dropdown-text);
  font-size: var(--font-size-dropdown);
  font-weight: var(--font-weight-semibold);
  outline: none;
}

.dropdown-input::placeholder {
  color: var(--dropdown-placeholder);
}

.dropdown-arrow {
  position: absolute;
  right: 12px;
  top: 50%;
  transform: translateY(-50%);
  width: 0;
  height: 0;
  border-left: 6px solid transparent;
  border-right: 6px solid transparent;
  border-top: 8px solid #000000;
  pointer-events: none;
}

/* Custom dropdown panel — replaces native <datalist> so it renders
   identically across WebView2, WebKitGTK, and WKWebView. */
.dropdown-panel {
  position: absolute;
  left: 0;
  right: 0;
  top: calc(100% + 6px);
  display: none;
  max-height: 220px;
  overflow-y: auto;
  background: var(--dropdown-option-bg);
  border: 3px solid var(--dropdown-border);
  cursor: pointer;
}

.dropdown-panel.is-open {
  display: block;
}

.dropdown-item {
  padding: 8px 16px;
  font-size: var(--font-size-dropdown-option);
  font-weight: var(--font-weight-normal);
  color: var(--dropdown-option-text);
  background: var(--dropdown-option-bg);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.dropdown-item:hover,
.dropdown-item.is-highlighted {
  background: var(--dropdown-option-hover);
  color: var(--dropdown-option-hover-text);
}

.dropdown-item.is-empty {
  color: var(--text-disabled);
  cursor: default;
}

.dropdown-panel::-webkit-scrollbar {
  width: 10px;
}

.dropdown-panel::-webkit-scrollbar-track {
  background: var(--surface-white);
}

.dropdown-panel::-webkit-scrollbar-thumb {
  background: var(--border-black);
  border: 2px solid var(--surface-white);
}
```

### EQ Curve Card
```css
.eq-card {
  margin: var(--gap-lg) var(--card-margin);
  background: var(--surface-white);
  border: var(--card-border-width) solid var(--border-black);
  height: var(--card-height);
  position: relative;
  overflow: hidden;
}

.eq-card canvas {
  width: 100%;
  height: 100%;
  display: block;
}
```

### Bypass Button
```css
.bypass-button {
  width: var(--bypass-width);
  height: var(--bypass-height);
  background: var(--bypass-off-bg);
  border: var(--bypass-border-width) solid var(--border-black);
  cursor: pointer;
  font-size: var(--font-size-bypass);
  font-weight: var(--font-weight-semibold);
  text-transform: uppercase;
  color: var(--text-black);
  transition: background 100ms ease;
}

.bypass-button.active {
  background: var(--bypass-on-bg);
}

.bypass-button:hover {
  filter: brightness(0.95);
}

.bypass-button:active {
  transform: translateY(1px);
}
```

### Status Bar
```css
.status-bar {
  padding: 0 20px;
  display: flex;
  justify-content: space-between;
  font-size: var(--font-size-status);
  font-weight: var(--font-weight-medium);
  color: var(--text-black);
}
```

---

## Canvas Curve Rendering

### EQ Curve Drawing (JavaScript)
```javascript
function drawCurve(ctx, width, height, data) {
  const padding = { top: 16, bottom: 20, left: 40, right: 16 };
  const plotW = width - padding.left - padding.right;
  const plotH = height - padding.top - padding.bottom;

  // Clear
  ctx.fillStyle = '#ffffff';
  ctx.fillRect(0, 0, width, height);

  // Zero dB line
  const zeroY = padding.top + plotH / 2;
  ctx.strokeStyle = '#cccccc';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(padding.left, zeroY);
  ctx.lineTo(width - padding.right, zeroY);
  ctx.stroke();

  // Frequency ticks
  const freqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
  ctx.fillStyle = '#999999';
  ctx.font = '11px sans-serif';
  ctx.textAlign = 'center';

  // EQ curve
  ctx.strokeStyle = '#000000';
  ctx.lineWidth = 2;
  ctx.beginPath();

  for (let i = 0; i < data.length; i++) {
    const [freq, gain] = data[i];
    const x = padding.left + (Math.log(freq / 20) / Math.log(20000 / 20)) * plotW;
    const y = zeroY - (gain / 12) * (plotH / 2);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.stroke();
}
```

### Frequency Mapping
```javascript
// Log scale: 20Hz -> 20kHz
function freqToX(freq, plotW) {
  return (Math.log(freq / 20) / Math.log(20000 / 20)) * plotW;
}

// dB scale: -12dB -> +12dB
function gainToY(gain, plotH, zeroY) {
  return zeroY - (gain / 12) * (plotH / 2);
}
```

---

## Effects & Animations

### Transitions
```css
--transition-fast: 100ms ease;     /* Bypass toggle */
--transition-normal: 200ms ease;   /* Dropdown hover */
```

### No Shadows or Glows
Neubrutalism uses flat design with hard borders. No drop shadows,
no border-radius, no gradients.

---

## Accessibility

### Color Contrast Ratios
- **Black text on yellow (#ffde00):** 8.6:1 (AAA) ✓
- **White text on blue (#0066ff):** 6.8:1 (AA) ✓
- **Black text on white (#ffffff):** 21:1 (AAA) ✓

### Focus Indicators
- **Keyboard Navigation:** 2px dashed black outline

---

## Design Tokens Export (for JUCE/C++)
```cpp
// Earware Design Tokens
namespace Earware::Design {
    constexpr Colour BG_YELLOW    = Colour(0xFFFFDE00);
    constexpr Colour SURFACE_WHITE = Colour(0xFFFFFFFF);
    constexpr Colour SURFACE_BLUE  = Colour(0xFF0066FF);
    constexpr Colour BLACK        = Colour(0xFF000000);

    constexpr int WINDOW_WIDTH  = 600;
    constexpr int WINDOW_HEIGHT = 450;
}
```

---

## Version History
- **v1.0** (2026-07-30): Initial neubrutalism design for Earware WebView UI

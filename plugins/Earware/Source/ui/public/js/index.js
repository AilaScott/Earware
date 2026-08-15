import { getSliderState, getToggleState } from './juce/index.js';

let modelState = getSliderState('model');
let bypassState = getToggleState('bypass');
let modelNames = [];
let curveData = null;
let lastSelectedIndex = -1;
let ignoreNextInput = false;

let panelOpen = false;
let filteredIndices = [];
let highlightPosition = -1;

const MAX_ITEMS = 200;

const modelInput = document.getElementById('modelInput');
const modelPanel = document.getElementById('modelPanel');
const bypassBtn = document.getElementById('bypassBtn');
const eqCanvas = document.getElementById('eqCanvas');
const modelStatus = document.getElementById('modelStatus');
const bypassStatus = document.getElementById('bypassStatus');
const noCurveOverlay = document.getElementById('noCurveOverlay');

function initModelList(names) {
  modelNames = names;
}

function renderPanel() {
  const fragment = document.createDocumentFragment();
  const shown = filteredIndices.slice(0, MAX_ITEMS);

  if (shown.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'dropdown-item is-empty';
    empty.textContent = 'No matches found';
    fragment.appendChild(empty);
  } else {
    shown.forEach((index, pos) => {
      const item = document.createElement('div');
      item.className = 'dropdown-item' + (pos === highlightPosition ? ' is-highlighted' : '');
      item.textContent = modelNames[index];
      item.dataset.index = index;

      item.addEventListener('mouseenter', () => {
        if (highlightPosition !== pos) {
          highlightPosition = pos;
          updateHighlight();
        }
      });
      item.addEventListener('pointerdown', (e) => {
        if (e.button === 0) selectModel(index);
      });

      fragment.appendChild(item);
    });
  }

  modelPanel.innerHTML = '';
  modelPanel.appendChild(fragment);
}

function updateHighlight() {
  const items = modelPanel.querySelectorAll('.dropdown-item');
  items.forEach((item, pos) => {
    const on = pos === highlightPosition;
    item.classList.toggle('is-highlighted', on);
    if (on) item.scrollIntoView({ block: 'nearest' });
  });
}

function openPanel() {
  panelOpen = true;
  modelPanel.classList.add('is-open');
  const pos = lastSelectedIndex >= 0 ? filteredIndices.indexOf(lastSelectedIndex) : -1;
  highlightPosition = pos >= 0 ? pos : 0;
  renderPanel();
}

function closePanel() {
  panelOpen = false;
  modelPanel.classList.remove('is-open');
}

function applyFilter() {
  const query = modelInput.value.trim().toLowerCase();
  const indices = [];
  if (query.length === 0) {
    for (let i = 0; i < modelNames.length; i++) indices.push(i);
  } else {
    const prefix = [];
    const contains = [];
    for (let i = 0; i < modelNames.length; i++) {
      const name = modelNames[i].toLowerCase();
      if (name.startsWith(query)) prefix.push(i);
      else if (name.includes(query)) contains.push(i);
    }
    indices.push(...prefix, ...contains);
  }
  filteredIndices = indices;
}

function selectModel(index) {
  if (index < 0 || index >= modelNames.length) return;
  lastSelectedIndex = index;
  const norm = modelNames.length > 1 ? index / (modelNames.length - 1) : 0;
  modelState.setNormalisedValue(norm);
  updateModelUI(index);
  closePanel();
}

function updateModelUI(index) {
  if (index < 0 || index >= modelNames.length) return;
  lastSelectedIndex = index;
  if (index === 0) {
    modelInput.value = '';
    modelStatus.textContent = 'No model selected';
  } else {
    modelInput.value = modelNames[index];
    modelStatus.textContent = modelNames[index];
  }
}

function updateBypassUI(bypassed) {
  bypassBtn.classList.toggle('active', bypassed);
  bypassStatus.textContent = bypassed ? 'Bypassed' : 'Active';
}

bypassState.valueChangedEvent.addListener(() => {
  updateBypassUI(bypassState.getValue());
});

modelState.valueChangedEvent.addListener(() => {
  const norm = modelState.getNormalisedValue();
  const index = Math.round(norm * (modelNames.length - 1));
  if (index !== lastSelectedIndex && index >= 0 && index < modelNames.length) {
    updateModelUI(index);
    lastSelectedIndex = index;
  }
});

bypassBtn.addEventListener('click', () => {
  bypassState.setValue(!bypassState.getValue());
});

modelInput.addEventListener('focus', () => {
  modelInput.select();
  applyFilter();
  openPanel();
});

modelInput.addEventListener('input', () => {
  if (ignoreNextInput) {
    ignoreNextInput = false;
    return;
  }

  const val = modelInput.value;
  const index = modelNames.indexOf(val);
  if (index !== -1) {
    lastSelectedIndex = index;
    const norm = modelNames.length > 1 ? index / (modelNames.length - 1) : 0;
    modelState.setNormalisedValue(norm);
    modelStatus.textContent = index === 0 ? 'No model selected' : val;
    applyFilter();
  } else {
    applyFilter();
  }

  if (!panelOpen) openPanel();
  else renderPanel();
});

modelInput.addEventListener('keydown', (e) => {
  if (e.key === 'ArrowDown' || e.key === 'ArrowUp' || e.key === 'Home' || e.key === 'End') {
    e.preventDefault();
    if (!panelOpen) {
      applyFilter();
      openPanel();
      return;
    }
    const count = filteredIndices.slice(0, MAX_ITEMS).length;
    if (count === 0) return;
    if (e.key === 'ArrowDown') highlightPosition = Math.min(highlightPosition + 1, count - 1);
    else if (e.key === 'ArrowUp') highlightPosition = Math.max(highlightPosition - 1, 0);
    else if (e.key === 'Home') highlightPosition = 0;
    else highlightPosition = count - 1;
    updateHighlight();
  } else if (e.key === 'Enter') {
    if (panelOpen && highlightPosition >= 0 && highlightPosition < filteredIndices.length) {
      e.preventDefault();
      selectModel(filteredIndices[highlightPosition]);
    } else {
      const index = modelNames.indexOf(modelInput.value);
      if (index !== -1) selectModel(index);
    }
  } else if (e.key === 'Escape') {
    closePanel();
    if (lastSelectedIndex === 0) modelInput.value = '';
    else if (lastSelectedIndex >= 0) modelInput.value = modelNames[lastSelectedIndex];
  } else if (e.key === 'Tab') {
    closePanel();
  }
});

modelInput.addEventListener('blur', () => {
  closePanel();
  if (lastSelectedIndex === 0) {
    ignoreNextInput = true;
    modelInput.value = '';
  } else if (lastSelectedIndex >= 0 && lastSelectedIndex < modelNames.length) {
    ignoreNextInput = true;
    modelInput.value = modelNames[lastSelectedIndex];
  }
});

document.addEventListener('mousedown', (e) => {
  if (panelOpen && !e.target.closest('.dropdown-wrap')) closePanel();
});

function drawCurve() {
  const canvas = eqCanvas;
  const rect = canvas.parentElement.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  const w = rect.width;
  const h = rect.height;
  canvas.width = w * dpr;
  canvas.height = h * dpr;
  canvas.style.width = w + 'px';
  canvas.style.height = h + 'px';

  const ctx = canvas.getContext('2d');
  ctx.scale(dpr, dpr);

  const pad = { top: 16, bottom: 22, left: 44, right: 16 };
  const plotW = w - pad.left - pad.right;
  const plotH = h - pad.top - pad.bottom;

  ctx.fillStyle = '#ffffff';
  ctx.fillRect(0, 0, w, h);

  const zeroY = pad.top + plotH / 2;

  ctx.strokeStyle = '#cccccc';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(pad.left, zeroY);
  ctx.lineTo(w - pad.right, zeroY);
  ctx.stroke();

  const freqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
  ctx.fillStyle = '#999999';
  ctx.font = '11px ' + getComputedStyle(document.body).fontFamily;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';

  freqs.forEach(f => {
    const x = pad.left + (Math.log(f / 20) / Math.log(20000 / 20)) * plotW;
    ctx.strokeStyle = '#dddddd';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(x, pad.top);
    ctx.lineTo(x, h - pad.bottom);
    ctx.stroke();
    const label = f >= 1000 ? (f / 1000) + 'k' : f + '';
    ctx.fillStyle = '#999999';
    ctx.fillText(label, x, h - pad.bottom + 4);
  });

  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  [-12, -6, 0, 6, 12].forEach(db => {
    const y = zeroY - (db / 12) * (plotH / 2);
    if (y >= pad.top && y <= h - pad.bottom) {
      ctx.fillStyle = '#cccccc';
      ctx.fillText(db + '', pad.left - 8, y);
    }
  });

  if (curveData && curveData.length > 0 && lastSelectedIndex !== 0) {
    noCurveOverlay.style.opacity = '0';
    ctx.strokeStyle = '#000000';
    ctx.lineWidth = 2.5;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    ctx.beginPath();
    for (let i = 0; i < curveData.length; i++) {
      const [freq, gain] = curveData[i];
      const x = pad.left + (Math.log(freq / 20) / Math.log(20000 / 20)) * plotW;
      const y = zeroY - (gain / 12) * (plotH / 2);
      if (i === 0) ctx.moveTo(x, Math.max(pad.top, Math.min(h - pad.bottom, y)));
      else ctx.lineTo(x, Math.max(pad.top, Math.min(h - pad.bottom, y)));
    }
    ctx.stroke();
  } else {
    noCurveOverlay.style.opacity = '1';
  }
}

window.updateModelInfo = function (name, index) {
  if (document.activeElement === modelInput) return;
  if (index !== lastSelectedIndex && index >= 0 && index < modelNames.length) {
    updateModelUI(index);
  }
};

window.updateCurve = function (data) {
  curveData = data;
  drawCurve();
};

window.setBypass = function (bypassed) {
  bypassState.value = bypassed;
  updateBypassUI(bypassed);
};

let resizeTimer;
window.addEventListener('resize', () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(drawCurve, 50);
});

document.addEventListener('DOMContentLoaded', () => {
  if (window.__JUCE__ && window.__JUCE__.initialisationData && window.__JUCE__.initialisationData.models) {
    initModelList(window.__JUCE__.initialisationData.models);
  }

  // Don't write a model into the input here: modelState hasn't been synchronised
  // with the backend yet, so getNormalisedValue() would force the first model
  // until the real value arrives. The backend sync (valueChangedEvent) and the
  // C++ timer push populate the input/curve after this.
  updateBypassUI(bypassState.getValue());
  drawCurve();

  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.emitEvent('earwareReady', {});
  }
});
import { getSliderState, getToggleState } from './juce/index.js';

const MODEL_COUNT = 736;

let modelState = getSliderState('model');
let bypassState = getToggleState('bypass');
let modelNames = [];
let curveData = null;
let lastSelectedIndex = -1;
let ignoreNextInput = false;

const modelInput = document.getElementById('modelInput');
const modelList = document.getElementById('modelList');
const bypassBtn = document.getElementById('bypassBtn');
const eqCanvas = document.getElementById('eqCanvas');
const modelStatus = document.getElementById('modelStatus');
const bypassStatus = document.getElementById('bypassStatus');
const noCurveOverlay = document.getElementById('noCurveOverlay');

function initModelList(names) {
  modelNames = names;
  modelList.innerHTML = '';
  names.forEach(name => {
    const opt = document.createElement('option');
    opt.value = name;
    modelList.appendChild(opt);
  });
}

function updateModelUI(index) {
  if (index < 0 || index >= modelNames.length) return;
  lastSelectedIndex = index;
  const name = modelNames[index];
  modelInput.value = name;
  modelStatus.textContent = name;
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
  const index = Math.round(norm * (MODEL_COUNT - 1));
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
    const norm = MODEL_COUNT > 1 ? index / (MODEL_COUNT - 1) : 0;
    modelState.setNormalisedValue(norm);
    modelStatus.textContent = val;
  }
});

modelInput.addEventListener('blur', () => {
  if (lastSelectedIndex >= 0 && lastSelectedIndex < modelNames.length) {
    ignoreNextInput = true;
    modelInput.value = modelNames[lastSelectedIndex];
  }
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

  if (curveData && curveData.length > 0) {
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

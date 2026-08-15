#!/usr/bin/env python3
"""Generate ParametricEQData.h/cpp from the AutoEq Recommended Results list.

The AutoEq results/README.md ("Recommended Results") lists one measurement
per headphone — the highest-accuracy one available — which replaces the
original oratory1990-only dataset as Earware's default.
"""

import os
import re
import struct
import sys
from urllib.parse import unquote

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
AUTOEQ_DIR = os.path.join(BASE_DIR, '..', '..', '_tools', 'AutoEq')
RESULTS_README = os.path.join(AUTOEQ_DIR, 'results', 'README.md')
OUTPUT_H = os.path.join(BASE_DIR, 'Source', 'ParametricEQData.h')
OUTPUT_CPP = os.path.join(BASE_DIR, 'Source', 'ParametricEQData.cpp')

FILTER_RE = re.compile(
    r'Filter\s+\d+:\s+ON\s+(LSC|HSC|PK)\s+Fc\s+([\d.]+)\s+Hz\s+Gain\s+([-.\d]+)\s+dB\s+Q\s+([\d.]+)'
)
FILTER_OFF_RE = re.compile(r'Filter\s+\d+:\s+OFF')
PREAMP_RE = re.compile(r'Preamp:\s+([-.\d]+)\s+dB')
LINK_RE = re.compile(r'^- \[(.+)\]\(\./(.+)\)$')

NUM_FILTERS = 10
PAD_FILTER = ('PK', 1000.0, 0.0, 0.7071)

entries = []
errors = []

categories = {
    'over-ear': 'OverEar',
    'in-ear': 'InEar',
    'earbud': 'Earbud',
}


def category_for_path(path):
    lowered = path.lower()
    for keyword, cat_enum in categories.items():
        if keyword in lowered:
            return cat_enum
    return 'Other'


with open(RESULTS_README, 'r') as f:
    lines = f.read().splitlines()

for line in lines:
    m = LINK_RE.match(line)
    if not m:
        continue

    name, rel_path = m.group(1), unquote(m.group(2))
    model_dir = os.path.join(AUTOEQ_DIR, 'results', rel_path)
    if not os.path.isdir(model_dir):
        # AutoEq README links are occasionally stale in case (e.g. "(passive
        # mode)" vs "(Passive mode)"), so fall back to a case-insensitive match.
        parent, leaf = os.path.split(model_dir)
        if os.path.isdir(parent):
            for dir_name in os.listdir(parent):
                if dir_name.lower() == leaf.lower():
                    model_dir = os.path.join(parent, dir_name)
                    break
    if not os.path.isdir(model_dir):
        errors.append(f'{model_dir}: recommended model directory missing')
        continue

    eq_files = [fn for fn in os.listdir(model_dir) if fn.endswith(' ParametricEQ.txt')]
    if not eq_files:
        errors.append(f'{model_dir}: no ParametricEQ.txt file found')
        continue
    eq_files.sort(key=lambda fn: 0 if fn == f'{name} ParametricEQ.txt' else 1)
    txt_path = os.path.join(model_dir, eq_files[0])

    with open(txt_path, 'r') as f:
        content = f.read().splitlines()

    preamp = None
    filters = []
    for line in filter(None, content):
        preamp_match = PREAMP_RE.match(line)
        if preamp_match and preamp is None:
            preamp = float(preamp_match.group(1))
            continue
        if FILTER_OFF_RE.match(line):
            continue
        flt_match = FILTER_RE.match(line)
        if flt_match:
            filters.append((
                flt_match.group(1),
                float(flt_match.group(2)),
                float(flt_match.group(3)),
                float(flt_match.group(4)),
            ))

    if preamp is None:
        errors.append(f'{txt_path}: could not parse preamp line')
        continue
    if len(filters) > NUM_FILTERS:
        errors.append(f'{txt_path}: {len(filters)} filters exceed schema capacity')
        continue

    while len(filters) < NUM_FILTERS:
        filters.append(PAD_FILTER)

    entries.append({
        'name': name,
        'category': category_for_path(rel_path),
        'preamp': preamp,
        'filters': filters,
    })

if errors:
    for e in errors:
        print(f'ERROR: {e}', file=sys.stderr)
    print(f'\n{len(errors)} errors, {len(entries)} entries parsed', file=sys.stderr)
    sys.exit(1)

# "No Model Selected" — neutral placeholder at index 0 so the plugin applies
# no correction until the user picks a headphone. Unity preamp + 10 unity PK
# stages is exactly transparent in the DSP chain.
entries.insert(0, {
    'name': 'No Model Selected',
    'category': 'Other',
    'preamp': 0.0,
    'filters': [PAD_FILTER] * NUM_FILTERS,
})

total_models = len(entries)
print(f'Parsed {total_models} entries (1 placeholder + {total_models - 1} models)')

FLOAT_FMT = '<f'
UBYTE_FMT = '<B'
PACK_FLOAT = lambda x: struct.pack(FLOAT_FMT, x)

TYPE_MAP = {'PK': 0, 'LSC': 1, 'HSC': 2}

MODEL_ENTRY_SIZE = 4 + NUM_FILTERS * (1 + 4 + 4 + 4)  # preamp(float) + 10*(type(byte)+freq+gain+q)
BLOB_SIZE = total_models * MODEL_ENTRY_SIZE

type_chars = []
preamp_floats = []
freq_floats = []
gain_floats = []
q_floats = []

for e in entries:
    preamp_floats.append(e['preamp'])
    for ftype, freq, gain, q in e['filters']:
        type_chars.append(TYPE_MAP[ftype])
        freq_floats.append(freq)
        gain_floats.append(gain)
        q_floats.append(q)

blob = bytearray()
for i in range(total_models):
    blob.extend(PACK_FLOAT(preamp_floats[i]))
    base = i * NUM_FILTERS
    for j in range(NUM_FILTERS):
        blob.extend(struct.pack(UBYTE_FMT, type_chars[base + j]))
        blob.extend(PACK_FLOAT(freq_floats[base + j]))
        blob.extend(PACK_FLOAT(gain_floats[base + j]))
        blob.extend(PACK_FLOAT(q_floats[base + j]))

assert len(blob) == BLOB_SIZE, f'blob size mismatch: {len(blob)} != {BLOB_SIZE}'

os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)

def format_blob_array(data, name, columns=12):
    lines = []
    lines.append(f'static const unsigned char {name}[] = {{')
    for i in range(0, len(data), columns):
        chunk = data[i:i + columns]
        lines.append('    ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',')
    lines.append('};')
    return '\n'.join(lines)

blob_c_array = format_blob_array(blob, 'earwarePresetsBlob')

model_names_lines = []
model_names_lines.append('static const char* earwareModelNames[] = {')
for e in entries:
    escaped = e['name'].replace('\\', '\\\\').replace('"', '\\"')
    model_names_lines.append(f'    (const char*) u8"{escaped}",')
model_names_lines.append('};')

category_indices_lines = []
category_indices_lines.append('// Category start indices')
cats_found = {}
for i, e in enumerate(entries):
    cat = e['category']
    if cat not in cats_found:
        cats_found[cat] = i
        if cat != 'Other':
            category_indices_lines.append(f'const int earware{cat}Start = {i};')
category_indices_lines.append(f'const int earwareNumModels = {total_models};')

with open(OUTPUT_H, 'w') as f:
    f.write('''#pragma once

#include <cstdint>

enum EarwareFilterType : uint8_t {
    EarwareFilterPK   = 0,
    EarwareFilterLSC  = 1,
    EarwareFilterHSC  = 2,
};

struct EarwareFilterStage {
    EarwareFilterType type;
    float freq;
    float gain;
    float q;
};

struct EarwareEqPreset {
    float preampGain;
    EarwareFilterStage filters[10];
};

''')
    f.write(f'extern const int earwareNumModels;\n')
    for cat in ['OverEar', 'InEar', 'Earbud']:
        if f'earware{cat}Start' in '\n'.join(category_indices_lines):
            f.write(f'extern const int earware{cat}Start;\n')
    f.write('\n')
    f.write('const EarwareEqPreset* earwareGetPreset(int index);\n')
    f.write('const char* earwareGetModelName(int index);\n')
    f.write('int earwareFindModel(const char* name);\n')
    f.write('void earwareComputeCurve(int index, float* freqsOut, float* eqOut, int numPoints);\n')

with open(OUTPUT_CPP, 'w') as f:
    f.write('#include "ParametricEQData.h"\n')
    f.write('#include <cmath>\n')
    f.write('#include <cstring>\n')
    f.write('#include <algorithm>\n\n')

    f.write(blob_c_array)
    f.write('\n\n')

    f.write('\n'.join(model_names_lines))
    f.write('\n\n')

    f.write('\n'.join(category_indices_lines))
    f.write('\n\n')

    # earwareGetPreset
    f.write('''
const EarwareEqPreset* earwareGetPreset(int index)
{
    if (index < 0 || index >= earwareNumModels)
        return nullptr;

    const unsigned char* p = earwarePresetsBlob + index * ''' + str(MODEL_ENTRY_SIZE) + ''';
    static EarwareEqPreset preset;

    float preamp;
    std::memcpy(&preamp, p, 4);
    preset.preampGain = preamp;
    p += 4;

    for (int i = 0; i < 10; ++i)
    {
        preset.filters[i].type = static_cast<EarwareFilterType>(p[0]); p += 1;
        std::memcpy(&preset.filters[i].freq, p, 4); p += 4;
        std::memcpy(&preset.filters[i].gain, p, 4); p += 4;
        std::memcpy(&preset.filters[i].q, p, 4); p += 4;
    }

    return &preset;
}
''')

    # earwareGetModelName
    f.write('''
const char* earwareGetModelName(int index)
{
    if (index < 0 || index >= earwareNumModels)
        return nullptr;
    return earwareModelNames[index];
}
''')

    # earwareFindModel
    f.write('''
int earwareFindModel(const char* name)
{
    for (int i = 0; i < earwareNumModels; ++i)
    {
        if (std::strcmp(earwareModelNames[i], name) == 0)
            return i;
    }
    return -1;
}
''')

    # earwareComputeCurve
    f.write('''
static float biquadMagnitude(float freq, float sr, float b0, float b1, float b2, float a1, float a2)
{
    float w = 2.0f * 3.14159265358979323846f * freq / sr;
    float c = std::cos(w);
    float s = std::sin(w);

    float realNum = b0 + b1 * c + b2 * std::cos(2.0f * w);
    float imagNum = b1 * s + b2 * std::sin(2.0f * w);
    float realDen = 1.0f + a1 * c + a2 * std::cos(2.0f * w);
    float imagDen = a1 * s + a2 * std::sin(2.0f * w);

    float magSqNum = realNum * realNum + imagNum * imagNum;
    float magSqDen = realDen * realDen + imagDen * imagDen;

    if (magSqDen < 1e-12f) return 0.0f;
    return 10.0f * std::log10(magSqNum / magSqDen);
}

static void calcCoeffs(float sr, EarwareFilterType type, float freq, float gain, float q,
                       float& b0, float& b1, float& b2, float& a1, float& a2)
{
    float w0 = 2.0f * 3.14159265358979323846f * freq / sr;
    float alpha = std::sin(w0) / (2.0f * q);
    float A = std::pow(10.0f, gain / 40.0f);
    float cosW0 = std::cos(w0);

    switch (type)
    {
        case EarwareFilterPK:
        {
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosW0;
            b2 = 1.0f - alpha * A;
            a1 = -2.0f * cosW0;
            a2 = 1.0f - alpha / A;
            float norm = 1.0f + alpha / A;
            b0 /= norm; b1 /= norm; b2 /= norm;
            a1 /= norm; a2 /= norm;
            break;
        }
        case EarwareFilterLSC:
        {
            float sqrtA = std::sqrt(A);
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
            a2 = (A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
            float norm = (A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
            b0 /= norm; b1 /= norm; b2 /= norm;
            a1 /= norm; a2 /= norm;
            break;
        }
        case EarwareFilterHSC:
        {
            float sqrtA = std::sqrt(A);
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);
            a2 = (A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
            float norm = (A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
            b0 /= norm; b1 /= norm; b2 /= norm;
            a1 /= norm; a2 /= norm;
            break;
        }
    }
}

void earwareComputeCurve(int index, float* freqsOut, float* eqOut, int numPoints)
{
    const float sr = 48000.0f;
    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;

    auto* preset = earwareGetPreset(index);
    if (!preset)
    {
        for (int i = 0; i < numPoints; ++i)
            eqOut[i] = 0.0f;
        return;
    }

    for (int i = 0; i < numPoints; ++i)
    {
        float t = (float)i / (float)(numPoints - 1);
        float freq = minFreq * std::pow(maxFreq / minFreq, t);
        freqsOut[i] = freq;

        float sumDb = preset->preampGain;

        for (int f = 0; f < 10; ++f)
        {
            float b0, b1, b2, a1, a2;
            calcCoeffs(sr, preset->filters[f].type, preset->filters[f].freq,
                       preset->filters[f].gain, preset->filters[f].q,
                       b0, b1, b2, a1, a2);
            sumDb += biquadMagnitude(freq, sr, b0, b1, b2, a1, a2);
        }

        eqOut[i] = sumDb;
    }
}
''')

print(f'Generated {OUTPUT_H}')
print(f'Generated {OUTPUT_CPP}')
print(f'Blob size: {BLOB_SIZE} bytes ({BLOB_SIZE / 1024:.1f} KB)')
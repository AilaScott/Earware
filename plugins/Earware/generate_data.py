#!/usr/bin/env python3
"""Generate ParametricEQData.h/cpp from oratory1990 ParametricEQ.txt files."""

import os
import re
import struct
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ORATORY_DIR = os.path.join(BASE_DIR, '..', '..', 'oratory1990')
OUTPUT_H = os.path.join(BASE_DIR, 'Source', 'ParametricEQData.h')
OUTPUT_CPP = os.path.join(BASE_DIR, 'Source', 'ParametricEQData.cpp')

FILTER_RE = re.compile(
    r'Filter\s+\d+:\s+ON\s+(LSC|HSC|PK)\s+Fc\s+([\d.]+)\s+Hz\s+Gain\s+([-.\d]+)\s+dB\s+Q\s+([\d.]+)'
)
PREAMP_RE = re.compile(r'Preamp:\s+([-.\d]+)\s+dB')

entries = []
errors = []

categories = {
    'over-ear': 'OverEar',
    'in-ear': 'InEar',
    'earbud': 'Earbud',
}

for cat_name, cat_enum in categories.items():
    cat_dir = os.path.join(ORATORY_DIR, cat_name)
    if not os.path.isdir(cat_dir):
        continue
    for model_name in sorted(os.listdir(cat_dir)):
        model_dir = os.path.join(cat_dir, model_name)
        if not os.path.isdir(model_dir):
            continue
        txt_path = os.path.join(model_dir, f'{model_name} ParametricEQ.txt')
        if not os.path.isfile(txt_path):
            continue

        with open(txt_path, 'r') as f:
            lines = f.readlines()

        if len(lines) < 11:
            errors.append(f'{txt_path}: expected 11 lines, got {len(lines)}')
            continue

        preamp_match = PREAMP_RE.match(lines[0].strip())
        if not preamp_match:
            errors.append(f'{txt_path}: could not parse preamp line')
            continue

        preamp = float(preamp_match.group(1))
        filters = []

        for i in range(1, 11):
            line = lines[i].strip()
            m = FILTER_RE.match(line)
            if not m:
                errors.append(f'{txt_path}: could not parse filter {i}: {line}')
                continue

            ftype = m.group(1)
            freq = float(m.group(2))
            gain = float(m.group(3))
            q = float(m.group(4))
            filters.append((ftype, freq, gain, q))

        if len(filters) != 10:
            errors.append(f'{txt_path}: expected 10 filters, got {len(filters)}')
            continue

        entries.append({
            'name': model_name,
            'category': cat_enum,
            'preamp': preamp,
            'filters': filters,
        })

if errors:
    for e in errors:
        print(f'ERROR: {e}', file=sys.stderr)
    print(f'\n{len(errors)} errors, {len(entries)} entries parsed', file=sys.stderr)
    sys.exit(1)

total_models = len(entries)
print(f'Parsed {total_models} models successfully')

FLOAT_FMT = '<f'
UBYTE_FMT = '<B'
PACK_FLOAT = lambda x: struct.pack(FLOAT_FMT, x)

TYPE_MAP = {'PK': 0, 'LSC': 1, 'HSC': 2}

MODEL_ENTRY_SIZE = 4 + 10 * (1 + 4 + 4 + 4)  # preamp(float) + 10*(type(byte)+freq+gain+q)
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
    base = i * 10
    for j in range(10):
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
    for cat_name in ['OverEar', 'InEar', 'Earbud']:
        if f'{cat_name}Start' in globals() or any(f'earware{cat_name}Start' in line for line in category_indices_lines):
            f.write(f'extern const int earware{cat_name}Start;\n')
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

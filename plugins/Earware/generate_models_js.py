#!/usr/bin/env python3
"""Generate the UI's embedded model-name list from ParametricEQData.cpp.

The UI only ever needs model *names* — the EQ data itself stays in the C++
preset blob and is looked up by index. The names are therefore duplicated into
a JS module so the dropdown has a delivery path that needs neither host
injection nor fetch() (see #6: macOS WKWebView drops the injected
initialisation data, and fetch() against JUCE's custom juce:// scheme handler
is not reliable there either).

Source of truth is the committed ParametricEQData.cpp, not AutoEq, so this
runs without an AutoEq checkout and cannot drift from what the plugin serves.

    python3 plugins/Earware/generate_models_js.py
"""

import os
import re
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_CPP = os.path.join(BASE_DIR, 'Source', 'ParametricEQData.cpp')
OUTPUT_JS = os.path.join(BASE_DIR, 'Source', 'ui', 'public', 'js', 'models.js')

ARRAY_START_RE = re.compile(r'static const char\* earwareModelNames\[\] = \{')
ENTRY_RE = re.compile(r'\(const char\*\)\s*u8"(.*)",?\s*$')
NUM_MODELS_RE = re.compile(r'const int earwareNumModels = (\d+);')


def unescape_cpp(literal: str) -> str:
    out = []
    i = 0
    while i < len(literal):
        ch = literal[i]
        if ch == '\\' and i + 1 < len(literal):
            nxt = literal[i + 1]
            out.append({'n': '\n', 't': '\t', 'r': '\r', '"': '"', '\\': '\\'}.get(nxt, nxt))
            i += 2
            continue
        out.append(ch)
        i += 1
    return ''.join(out)


def escape_js(value: str) -> str:
    out = []
    for ch in value:
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ord(ch) < 0x20 or ord(ch) > 0x7E:
            # Non-ASCII is escaped so the file stays pure ASCII: the WebView
            # decodes it identically on every platform, and byte-level diffs of
            # names containing e.g. U+200B stay visible.
            code = ord(ch)
            if code > 0xFFFF:
                code -= 0x10000
                out.append('\\u{:04X}\\u{:04X}'.format(0xD800 + (code >> 10), 0xDC00 + (code & 0x3FF)))
            else:
                out.append('\\u{:04X}'.format(code))
        else:
            out.append(ch)
    return ''.join(out)


def main() -> int:
    with open(DATA_CPP, 'r', encoding='utf-8') as f:
        lines = f.read().splitlines()

    names = []
    in_array = False
    declared_count = None

    for line in lines:
        if in_array:
            if line.startswith('};'):
                in_array = False
                continue
            match = ENTRY_RE.match(line.strip())
            if match:
                names.append(unescape_cpp(match.group(1)))
            continue

        if ARRAY_START_RE.search(line):
            in_array = True
            continue

        count_match = NUM_MODELS_RE.search(line)
        if count_match:
            declared_count = int(count_match.group(1))

    if not names:
        print('ERROR: no model names parsed from ParametricEQData.cpp', file=sys.stderr)
        return 1

    if declared_count is not None and declared_count != len(names):
        print('ERROR: earwareNumModels={} but parsed {} names'.format(declared_count, len(names)),
              file=sys.stderr)
        return 1

    if any('\n' in n or '\r' in n for n in names):
        print('ERROR: a model name contains a line break; newline-joined format cannot hold it',
              file=sys.stderr)
        return 1

    joined = '\\n'.join(escape_js(n) for n in names)

    with open(OUTPUT_JS, 'w', encoding='ascii', newline='\n') as f:
        f.write('// GENERATED FILE - do not edit by hand.\n')
        f.write('// Source: plugins/Earware/Source/ParametricEQData.cpp (earwareModelNames)\n')
        f.write('// Regenerate: python3 plugins/Earware/generate_models_js.py\n')
        f.write('//\n')
        f.write('// Names are newline-joined rather than a JSON array: one string split at\n')
        f.write('// load is markedly cheaper to parse than 6034 array elements.\n')
        f.write('// Index positions match earwareNumModels in the C++ preset table.\n')
        f.write('\n')
        f.write('export const EARWARE_MODEL_NAMES =\n  "')
        f.write(joined)
        f.write('".split("\\n");\n')
        f.write('\n')
        f.write('export const EARWARE_MODEL_COUNT = EARWARE_MODEL_NAMES.length;\n')

    print('Generated {} ({} names)'.format(OUTPUT_JS, len(names)))
    return 0


if __name__ == '__main__':
    sys.exit(main())

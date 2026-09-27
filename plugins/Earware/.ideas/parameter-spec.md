> **HISTORICAL ARCHIVE** — pre-implementation ideation from 2026-07-30. Superseded:
> `model` range is now 0–6033 (6034 entries), not 0–735. The DSP is a linear-phase
> FIR convolution; the 10 biquad parameters no longer exist as real-time parameters.
> Kept as parameter-design history.

| ID | Name | Type | Range | Default | Unit |
|---|---|---|---|---|---|
| `model` | Headphone Model | Choice | 0–735 | 0 | index |
| `bypass` | Bypass | Toggle | 0–1 | 0 | on/off |

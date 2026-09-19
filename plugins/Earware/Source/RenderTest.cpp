#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <vector>

//==============================================================================
static void writeWav (const char* path, const std::vector<float>& left,
                      const std::vector<float>& right, double sampleRate)
{
    const auto numSamples = static_cast<uint32_t> (left.size());
    const int bitsPerSample = 16;
    const int channels = 2;
    const int bytesPerSample = bitsPerSample / 8;
    const int blockAlign = channels * bytesPerSample;
    const uint32_t dataBytes = numSamples * static_cast<uint32_t> (blockAlign);
    const uint32_t byteRate = static_cast<uint32_t> (sampleRate) * static_cast<uint32_t> (blockAlign);

    FILE* f = fopen (path, "wb");
    if (f == nullptr) { perror (path); return; }

    auto writeCstr = [&] (const char* s) { fwrite (s, 1, 4, f); };
    auto writeLE = [&] (auto v, int size)
    {
        unsigned char buf[8];
        for (int i = 0; i < size; ++i) { buf[i] = static_cast<unsigned char> (v & 0xff); v >>= 8; }
        fwrite (buf, 1, static_cast<size_t> (size), f);
    };

    writeCstr ("RIFF");
    writeLE (36 + dataBytes, 4);
    writeCstr ("WAVE");
    writeCstr ("fmt ");
    writeLE (16u, 4);
    writeLE (1u, 2);       // PCM
    writeLE (channels, 2);
    writeLE (static_cast<uint32_t> (sampleRate), 4);
    writeLE (byteRate, 4);
    writeLE (blockAlign, 2);
    writeLE (bitsPerSample, 2);
    writeCstr ("data");
    writeLE (dataBytes, 4);

    for (uint32_t i = 0; i < numSamples; ++i)
    {
        int16_t l = static_cast<int16_t> (juce::jlimit (-1.0, 1.0, (double) left[i]) * 32767.0);
        int16_t r = static_cast<int16_t> (juce::jlimit (-1.0, 1.0, (double) right[i]) * 32767.0);
        writeLE ((uint16_t) l, 2);
        writeLE ((uint16_t) r, 2);
    }

    fclose (f);
    printf ("    wrote %s (%u samples)\n", path, numSamples);
}

static double peakDB (const std::vector<float>& data)
{
    float peak = 0.0f;
    for (auto s : data) peak = std::max (peak, std::abs (s));
    return 20.0 * std::log10 (peak + 1e-12);
}

static double rmsDB (const std::vector<float>& data)
{
    double sum = 0.0;
    for (auto s : data) sum += (double) s * (double) s;
    return 20.0 * std::log10 (std::sqrt (sum / (double) data.size()) + 1e-12);
}

//==============================================================================
static std::vector<float> render (EarwareAudioProcessor& proc, bool bypassed,
                                  int modelIndex, double sampleRate, int sigFreq,
                                  int seconds)
{
    const int blockSize = 512;
    const long long numSamples = (long long) sampleRate * seconds;

    auto setParam = [&] (const char* id, float denormValue)
    {
        auto child = proc.apvts.state.getChildWithProperty ("id", id);
        if (child.isValid())
            child.setProperty ("value", denormValue, nullptr);
    };
    setParam ("model", (float) modelIndex);
    setParam ("bypass", bypassed ? 1.0f : 0.0f);
    printf ("    param check: model=%d readback=%f  bypass=%d readback=%f\n",
            modelIndex, proc.apvts.getRawParameterValue ("model")->load(),
            bypassed ? 1 : 0, proc.apvts.getRawParameterValue ("bypass")->load());

    std::vector<float> left, right;
    left.reserve (numSamples);
    right.reserve (numSamples);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    for (long long start = 0; start < numSamples; start += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const float t = (float) (start + i) / (float) sampleRate;
            const float s = 0.5f * std::sin (6.28318530718f * (float) sigFreq * t);
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
        }

        proc.processBlock (buffer, midi);

        for (int i = 0; i < blockSize; ++i)
        {
            left.push_back (buffer.getSample (0, i));
            right.push_back (buffer.getSample (1, i));
        }
    }

    return left; // both channels identical
}

//==============================================================================
static void directIIRTest()
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using Filter = juce::dsp::IIR::Filter<float>;
    using Dup = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

    for (float g : { 0.0f, 1.0f, 1.5f, 2.0f })
    {
        auto c = Coeffs::makePeakFilter (44100.0, 1000.0, 1.0, g);
        auto* raw = c->getRawCoefficients();
        printf ("direct: makePeakFilter(1k, Q1, gain=%5.1f) coeffs = [% .5f, % .5f, % .5f, % .5f, % .5f] (order=%zu)\n",
                g, raw[0], raw[1], raw[2], raw[3], raw[4], c->getFilterOrder());
    }

    Dup dup;
    juce::dsp::ProcessSpec spec; spec.sampleRate = 44100; spec.maximumBlockSize = 512; spec.numChannels = 2;
    dup.prepare (spec);
    auto c2 = Coeffs::makePeakFilter (44100.0, 1000.0, 1.0, 1.0f);
    *dup.state = *c2;

    juce::AudioBuffer<float> buf (2, 512);
    for (int i = 0; i < 512; ++i)
    {
        float s = 0.5f * std::sin (6.28318530718f * 1000.0f * (float) i / 44100.0f);
        buf.setSample (0, i, s); buf.setSample (1, i, s);
    }

    auto block = juce::dsp::AudioBlock<float> (buf);
    dup.process (juce::dsp::ProcessContextReplacing<float> (block));

    float peak = 0.0f;
    for (int i = 0; i < 512; ++i) peak = std::max (peak, std::abs (buf.getSample (0, i)));
    printf ("direct: ProcessorDuplicator out peak = %7.2f dBFS\n", 20.0 * std::log10 (peak + 1e-12));
}

//==============================================================================
// Reference IIR complex frequency response (same as LinearPhaseFIR::biquadResponse)
static std::complex<float> iirBiquadResponse (const EarwareFilterStage& s, float freq, double sampleRate)
{
    if (s.gain == 0.0f) return { 1.0f, 0.0f };

    const float A  = std::sqrt (juce::Decibels::decibelsToGain (s.gain));

    const float w0coeff = 6.2831853f * s.freq / (float) sampleRate;
    const float cosC = std::cos (w0coeff);
    const float sinC = std::sin (w0coeff);
    const float alpha = sinC * 0.5f / s.q;

    float b0, b1, b2, a0, a1, a2;

    switch (s.type)
    {
        case EarwareFilterPK:
        {
            const float a0inv = 1.0f / (1.0f + alpha / A);
            b0 = (1.0f + alpha * A) * a0inv;
            b1 = -2.0f * cosC * a0inv;
            b2 = (1.0f - alpha * A) * a0inv;
            a0 = 1.0f;
            a1 = -2.0f * cosC * a0inv;
            a2 = (1.0f - alpha / A) * a0inv;
            break;
        }
        case EarwareFilterLSC:
        {
            const float sqrtA = std::sqrt (A);
            const float beta = 2.0f * sqrtA * alpha;
            b0 =    A * ((A + 1.0f) - (A - 1.0f) * cosC + beta);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosC);
            b2 =    A * ((A + 1.0f) - (A - 1.0f) * cosC - beta);
            a0 =           (A + 1.0f) + (A - 1.0f) * cosC + beta;
            a1 =   -2.0f * ((A - 1.0f) + (A + 1.0f) * cosC);
            a2 =           (A + 1.0f) + (A - 1.0f) * cosC - beta;
            const float a0inv = 1.0f / a0;
            b0 *= a0inv; b1 *= a0inv; b2 *= a0inv;
            a1 *= a0inv; a2 *= a0inv;
            a0 = 1.0f;
            break;
        }
        case EarwareFilterHSC:
        {
            const float sqrtA = std::sqrt (A);
            const float beta = 2.0f * sqrtA * alpha;
            b0 =    A * ((A + 1.0f) + (A - 1.0f) * cosC + beta);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosC);
            b2 =    A * ((A + 1.0f) + (A - 1.0f) * cosC - beta);
            a0 =           (A + 1.0f) - (A - 1.0f) * cosC + beta;
            a1 =    2.0f * ((A - 1.0f) - (A + 1.0f) * cosC);
            a2 =           (A + 1.0f) - (A - 1.0f) * cosC - beta;
            const float a0inv = 1.0f / a0;
            b0 *= a0inv; b1 *= a0inv; b2 *= a0inv;
            a1 *= a0inv; a2 *= a0inv;
            a0 = 1.0f;
            break;
        }
        default:
            return { 1.0f, 0.0f };
    }

    const float w0eval = 6.2831853f * freq / (float) sampleRate;
    const std::complex<float> z1 (std::cos (w0eval), -std::sin (w0eval));
    const std::complex<float> z2 = z1 * z1;
    return (b0 + b1 * z1 + b2 * z2) / (a0 + a1 * z1 + a2 * z2);
}

static float iirMagnitudeDB (const EarwareEqPreset* preset, float freq, double sampleRate)
{
    const float preampLin = juce::Decibels::decibelsToGain (preset->preampGain);
    std::complex<float> H = { preampLin, 0.0f };

    for (int f = 0; f < 10; ++f)
        H *= iirBiquadResponse (preset->filters[f], freq, sampleRate);

    return 20.0f * std::log10 (std::abs (H) + 1e-12f);
}

//==============================================================================
// Reference IIR time-domain processing (minimum phase, for RMS comparison)
static void iirProcessReference (const EarwareEqPreset* preset, double sampleRate,
                                 float* data, int numSamples)
{
    const float preampLin = juce::Decibels::decibelsToGain (preset->preampGain);

    for (int f = 0; f < 10; ++f)
    {
        const auto& stage = preset->filters[f];

        if (std::abs (stage.gain) < 1e-6f)
            continue;

        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        Coeffs::Ptr coeffs;
        const float gainLin = juce::Decibels::decibelsToGain (stage.gain);

        switch (stage.type)
        {
            case EarwareFilterPK:
                coeffs = Coeffs::makePeakFilter (sampleRate, stage.freq, stage.q, gainLin);
                break;
            case EarwareFilterLSC:
                coeffs = Coeffs::makeLowShelf (sampleRate, stage.freq, stage.q, gainLin);
                break;
            case EarwareFilterHSC:
                coeffs = Coeffs::makeHighShelf (sampleRate, stage.freq, stage.q, gainLin);
                break;
        }

        auto* raw = coeffs->getRawCoefficients();
        const float b0 = raw[0], b1 = raw[1], b2 = raw[2];
        const float a1 = raw[3], a2 = raw[4];
        float lv1 = 0, lv2 = 0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float input = data[i];
            const float output = (input * b0) + lv1;
            lv1 = (input * b1) - (output * a1) + lv2;
            lv2 = (input * b2) - (output * a2);
            data[i] = output;
        }
    }

    for (int i = 0; i < numSamples; ++i)
        data[i] *= preampLin;
}

//==============================================================================
// FIR magnitude from time-domain IR via DFT
static float firMagnitudeDB (const float* ir, int irLen, float freq, double sampleRate)
{
    std::complex<float> H (0.0f, 0.0f);
    const float w = -6.2831853f * freq / (float) sampleRate;
    for (int n = 0; n < irLen; ++n)
    {
        const float angle = w * (float) n;
        H += ir[n] * std::complex<float> (std::cos (angle), std::sin (angle));
    }
    return 20.0f * std::log10 (std::abs (H) + 1e-12f);
}

//==============================================================================
static void testMagnitudeAccuracy (double sampleRate)
{
    printf ("\n=== TEST: Magnitude Response Accuracy (FIR vs IIR) ===\n");

    const int numFreqs = 256;
    const float freqMin = 20.0f;
    const float freqMax = (float) sampleRate * 0.49f;

    float irBuf[LinearPhaseFIR::irLength];

    const int testModels[] = { 0, 1, 226, 630 };
    int passCount = 0, failCount = 0;

    for (int modelIdx : testModels)
    {
        auto* preset = earwareGetPreset (modelIdx);
        if (preset == nullptr) continue;

        LinearPhaseFIR::compute (preset, sampleRate, irBuf);

        float maxErr = 0.0f;
        float worstFreq = 0.0f;

        for (int k = 0; k < numFreqs; ++k)
        {
            const float t = (float) k / (float) (numFreqs - 1);
            const float freq = freqMin * std::pow (freqMax / freqMin, t);

            const float iirDB = iirMagnitudeDB (preset, freq, sampleRate);
            const float firDB = firMagnitudeDB (irBuf, LinearPhaseFIR::irLength, freq, sampleRate);

            // Skip frequencies where IIR is deeply notched (below -30 dB)
            // — the FIR's finite length can't represent such depths
            if (iirDB < -30.0f)
                continue;

            const float err = std::abs (iirDB - firDB);

            if (err > maxErr)
            {
                maxErr = err;
                worstFreq = freq;
            }
        }

        const bool pass = maxErr < 1.0f;
        (pass ? ++passCount : ++failCount);
        printf ("  model %3d: max error = %.3f dB @ %.1f Hz  [%s]\n",
                modelIdx, maxErr, worstFreq, pass ? "PASS" : "FAIL");

        for (float freq : { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f })
        {
            if (freq >= freqMax) continue;
            printf ("    @%.0f Hz: IIR=%.3f dB  FIR=%.3f dB\n",
                    freq, iirMagnitudeDB (preset, freq, sampleRate),
                    firMagnitudeDB (irBuf, LinearPhaseFIR::irLength, freq, sampleRate));
        }
    }

    printf ("  Result: %d passed, %d failed\n", passCount, failCount);
}

//==============================================================================
static void testOutputLevelMatch (double sampleRate)
{
    printf ("\n=== TEST: Output Level Match (FIR vs IIR RMS) ===\n");

    const int numSamples = 44100;
    const int numTestModels[] = { 0, 226, 630 };
    int passCount = 0, failCount = 0;

    float irBuf[LinearPhaseFIR::irLength];

    for (int modelIdx : numTestModels)
    {
        auto* preset = earwareGetPreset (modelIdx);
        if (preset == nullptr) continue;

        LinearPhaseFIR::compute (preset, sampleRate, irBuf);

        std::vector<float> noise (numSamples);
        {
            juce::Random rng (12345);
            float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
            for (int i = 0; i < numSamples; ++i)
            {
                const float white = ((float) rng.nextFloat() - 0.5f) * 2.0f;
                b0 = 0.99886f * b0 + white * 0.0555179f;
                b1 = 0.99332f * b1 + white * 0.0750759f;
                b2 = 0.96900f * b2 + white * 0.1538520f;
                b3 = 0.86650f * b3 + white * 0.3104856f;
                b4 = 0.55000f * b4 + white * 0.5329522f;
                b5 = -0.7616f * b5 - white * 0.0168980f;
                noise[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f) * 0.11f;
                b6 = white * 0.115926f;
            }
        }

        // IIR (reference minimum phase)
        std::vector<float> iirOut (noise);
        iirProcessReference (preset, sampleRate, iirOut.data(), numSamples);

        // FIR (linear phase) — convolve with overlap-add
        std::vector<float> firOut (numSamples, 0.0f);
        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int k = 0; k < LinearPhaseFIR::irLength; ++k)
            {
                const int idx = i - k;
                if (idx >= 0)
                    sum += irBuf[k] * noise[idx];
            }
            firOut[i] = sum;
        }

        // Measure RMS (skip first irLength samples to avoid transients)
        const int skip = LinearPhaseFIR::irLength + 100;
        double iirSum = 0.0, firSum = 0.0;
        for (int i = skip; i < numSamples; ++i)
        {
            iirSum += (double) iirOut[i] * (double) iirOut[i];
            firSum += (double) firOut[i] * (double) firOut[i];
        }
        const int count = numSamples - skip;
        const float iirRmsDB = (float) (20.0 * std::log10 (std::sqrt (iirSum / count) + 1e-12));
        const float firRmsDB = (float) (20.0 * std::log10 (std::sqrt (firSum / count) + 1e-12));
        const float diff = std::abs (iirRmsDB - firRmsDB);

        const bool pass = diff < 0.1f;
        (pass ? ++passCount : ++failCount);
        printf ("  model %3d: IIR RMS=%7.2f dB  FIR RMS=%7.2f dB  diff=%.3f dB  preamp=%.1f dB  [%s]\n",
                modelIdx, iirRmsDB, firRmsDB, diff, preset->preampGain, pass ? "PASS" : "FAIL");

        if (! pass)
        {
            printf ("           IIR[skip]=%.6f %.6f %.6f  FIR[skip]=%.6f %.6f %.6f\n",
                    iirOut[skip], iirOut[skip+1], iirOut[skip+2],
                    firOut[skip], firOut[skip+1], firOut[skip+2]);
            printf ("           IIR peak=%.6f  FIR peak=%.6f\n",
                    *std::max_element (iirOut.begin() + skip, iirOut.end(),
                                       [](float a, float b){ return std::abs(a) < std::abs(b); }),
                    *std::max_element (firOut.begin() + skip, firOut.end(),
                                       [](float a, float b){ return std::abs(a) < std::abs(b); }));

            for (int f = 0; f < 10; ++f)
            {
                const auto& stage = preset->filters[f];
                if (std::abs (stage.gain) < 1e-6f) continue;
                using Coeffs = juce::dsp::IIR::Coefficients<float>;
                const float gainLin = juce::Decibels::decibelsToGain (stage.gain);
                Coeffs::Ptr c;
                switch (stage.type)
                {
                    case EarwareFilterPK:  c = Coeffs::makePeakFilter (sampleRate, stage.freq, stage.q, gainLin); break;
                    case EarwareFilterLSC: c = Coeffs::makeLowShelf  (sampleRate, stage.freq, stage.q, gainLin); break;
                    case EarwareFilterHSC: c = Coeffs::makeHighShelf (sampleRate, stage.freq, stage.q, gainLin); break;
                }
                auto* r = c->getRawCoefficients();
                printf ("    stage %d: type=%d freq=%.1f q=%.2f gain=%6.1f dB  raw=[%.6f %.6f %.6f %.6f %.6f]\n",
                        f, stage.type, stage.freq, stage.q, stage.gain,
                        r[0], r[1], r[2], r[3], r[4]);
            }
        }
    }

    printf ("  Result: %d passed, %d failed\n", passCount, failCount);
}

//==============================================================================
static void testDCGain (double sampleRate)
{
    printf ("\n=== TEST: DC Gain Correctness ===\n");

    float irBuf[LinearPhaseFIR::irLength];
    int passCount = 0, failCount = 0;

    for (int modelIdx : { 0, 1, 226, 630 })
    {
        auto* preset = earwareGetPreset (modelIdx);
        if (preset == nullptr) continue;

        LinearPhaseFIR::compute (preset, sampleRate, irBuf);

        printf ("  model %3d: first 5 IR = [%.6f, %.6f, %.6f, %.6f, %.6f]  mid = %.6f\n",
                modelIdx, irBuf[0], irBuf[1], irBuf[2], irBuf[3], irBuf[4],
                irBuf[LinearPhaseFIR::irLength / 2]);

        float tapSum = 0.0f;
        for (int i = 0; i < LinearPhaseFIR::irLength; ++i)
            tapSum += irBuf[i];

        // Target DC gain: preamp × all biquad DC gains
        float targetDC = juce::Decibels::decibelsToGain (preset->preampGain);
        for (int f = 0; f < 10; ++f)
        {
            const auto& stage = preset->filters[f];
            if (stage.type == EarwareFilterLSC && stage.gain != 0.0f)
                targetDC *= juce::Decibels::decibelsToGain (stage.gain);
        }

        const float err = std::abs (tapSum - targetDC);
        const bool pass = err < 0.01f;
        (pass ? ++passCount : ++failCount);
        printf ("  model %3d: tap sum = %.6f  target = %.6f  err = %.6f  [%s]\n",
                modelIdx, tapSum, targetDC, err, pass ? "PASS" : "FAIL");
    }

    printf ("  Result: %d passed, %d failed\n", passCount, failCount);
}

//==============================================================================
static void testIRSymmetry (double sampleRate)
{
    printf ("\n=== TEST: IR Symmetry (Linear Phase) ===\n");

    float irBuf[LinearPhaseFIR::irLength];
    int passCount = 0, failCount = 0;

    for (int modelIdx : { 0, 1, 226, 630 })
    {
        auto* preset = earwareGetPreset (modelIdx);
        if (preset == nullptr) continue;

        LinearPhaseFIR::compute (preset, sampleRate, irBuf);

        // Symmetry around center (N/2): ir[N/2-k] == ir[N/2+k] for k=1..N/2
        float maxErr = 0.0f;
        const int center = LinearPhaseFIR::irLength / 2;
        for (int k = 1; k <= center; ++k)
        {
            const int left = center - k;
            const int right = center + k;
            if (right < LinearPhaseFIR::irLength)
            {
                const float err = std::abs (irBuf[left] - irBuf[right]);
                maxErr = std::max (maxErr, err);
            }
        }

        const bool pass = maxErr < 0.001f;
        (pass ? ++passCount : ++failCount);
        printf ("  model %3d: max symmetry error = %.9f  [%s]\n",
                modelIdx, maxErr, pass ? "PASS" : "FAIL");
    }

    printf ("  Result: %d passed, %d failed\n", passCount, failCount);
}

//==============================================================================
static void testGroupDelay (double sampleRate)
{
    printf ("\n=== TEST: Constant Group Delay ===\n");

    float irBuf[LinearPhaseFIR::irLength];
    int passCount = 0, failCount = 0;
    const float expectedDelay = (float) (LinearPhaseFIR::irLength / 2);

    for (int modelIdx : { 0, 1, 226, 630 })
    {
        auto* preset = earwareGetPreset (modelIdx);
        if (preset == nullptr) continue;

        LinearPhaseFIR::compute (preset, sampleRate, irBuf);

        // For a perfectly symmetric IR, group delay is exactly (N-1)/2.
        // Verify by checking symmetry (which implies constant GD).
        // Also spot-check via phase at one frequency.
        const float checkFreq = 1000.0f;
        const float w = 6.2831853f * checkFreq / (float) sampleRate;
        const float dw = 0.001f;

        std::complex<float> H1 (0.0f, 0.0f), H2 (0.0f, 0.0f);
        for (int n = 0; n < LinearPhaseFIR::irLength; ++n)
        {
            H1 += irBuf[n] * std::complex<float> (std::cos (-(w - dw) * n), std::sin (-(w - dw) * n));
            H2 += irBuf[n] * std::complex<float> (std::cos (-(w + dw) * n), std::sin (-(w + dw) * n));
        }
        const float phase1 = std::arg (H1);
        const float phase2 = std::arg (H2);
        float dPhase = phase2 - phase1;
        // Unwrap phase difference
        if (dPhase > (float) M_PI)  dPhase -= 2.0f * (float) M_PI;
        if (dPhase < -(float) M_PI) dPhase += 2.0f * (float) M_PI;
        const float gd = -dPhase / (2.0f * dw);
        const float deviation = std::abs (gd - expectedDelay);

        const bool pass = deviation < 1.0f;
        (pass ? ++passCount : ++failCount);
        printf ("  model %3d: expected GD = %.1f  measured GD = %.1f  deviation = %.3f  [%s]\n",
                modelIdx, expectedDelay, gd, deviation, pass ? "PASS" : "FAIL");
    }

    printf ("  Result: %d passed, %d failed\n", passCount, failCount);
}

//==============================================================================
static void testPhaseLinearity (double sampleRate)
{
    printf ("\n=== TEST: Phase Linearity ===\n");

    float irBuf[LinearPhaseFIR::irLength];
    int passCount = 0, failCount = 0;
    const float expectedDelay = (float) (LinearPhaseFIR::irLength / 2);

    for (int modelIdx : { 0, 1, 226, 630 })
    {
        auto* preset = earwareGetPreset (modelIdx);
        if (preset == nullptr) continue;

        LinearPhaseFIR::compute (preset, sampleRate, irBuf);

        const int numFreqs = 64;
        const float freqMin = 100.0f;
        const float freqMax = (float) sampleRate * 0.45f;
        float maxPhaseError = 0.0f;

        for (int k = 1; k < numFreqs; ++k)
        {
            const float t = (float) k / (float) (numFreqs - 1);
            const float freq = freqMin * std::pow (freqMax / freqMin, t);
            const float w = 6.2831853f * freq / (float) sampleRate;

            std::complex<float> H (0.0f, 0.0f);
            for (int n = 0; n < LinearPhaseFIR::irLength; ++n)
            {
                const float angle = -w * (float) n;
                H += irBuf[n] * std::complex<float> (std::cos (angle), std::sin (angle));
            }

            const float phase = std::arg (H);
            const float expectedPhase = -w * expectedDelay;

            // Unwrap phase difference
            float diff = phase - expectedPhase;
            diff = std::fmod (diff + (float) M_PI, 2.0f * (float) M_PI) - (float) M_PI;
            maxPhaseError = std::max (maxPhaseError, std::abs (diff));
        }

        const bool pass = maxPhaseError < 0.2f;
        (pass ? ++passCount : ++failCount);
        printf ("  model %3d: max phase error = %.4f rad  [%s]\n",
                modelIdx, maxPhaseError, pass ? "PASS" : "FAIL");
    }

    printf ("  Result: %d passed, %d failed\n", passCount, failCount);
}

//==============================================================================
static void testLatencyReporting (double sampleRate)
{
    printf ("\n=== TEST: Latency Reporting ===\n");

    EarwareAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sampleRate, 512);
    proc.prepareToPlay (sampleRate, 512);

    // Trigger model load
    auto child = proc.apvts.state.getChildWithProperty ("id", "model");
    if (child.isValid())
        child.setProperty ("value", 1.0f, nullptr);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);

    const int latency = proc.getLatencySamples();
    const bool pass = latency > 0;
    printf ("  latency = %d samples (%.1f ms at %.0f Hz)  [%s]\n",
            latency, (float) latency * 1000.0f / (float) sampleRate, sampleRate,
            pass ? "PASS" : "FAIL");
}

//==============================================================================
int main()
{
    juce::initialiseJuce_GUI();

    directIIRTest();

    const double sampleRate = 44100.0;
    const int seconds = 2;

    {
        EarwareAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, 512);
        proc.prepareToPlay (sampleRate, 512);

        struct Case { bool bypassed; int model; int freq; const char* name; };
        const Case cases[] = {
            { true,   0, 1000, "dry_bypass_model0_1k"  },
            { false,  0, 1000, "active_model0_1k"      },
            { false,  1, 1000, "active_model1_1k"      },
            { false, 200, 1000, "active_model200_1k"   },
            { false,  0,  100, "active_model0_100hz"   },
        };

        for (const auto& c : cases)
        {
            auto out = render (proc, c.bypassed, c.model, sampleRate, c.freq, seconds);

            auto outPath = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile (juce::String ("earware_rt_") + c.name + ".wav");
            writeWav (outPath.getFullPathName().toRawUTF8(), out, out, sampleRate);

            printf ("%-24s bypass=%-5s model=%-3d freq=%-4d  peak=%7.2f dBFS  rms=%7.2f dBFS\n",
                    c.name, c.bypassed ? "TRUE" : "FALSE", c.model, c.freq,
                    peakDB (out), rmsDB (out));
        }
    }

    // Linear phase accuracy tests
    testMagnitudeAccuracy (sampleRate);
    testOutputLevelMatch (sampleRate);
    testDCGain (sampleRate);
    testIRSymmetry (sampleRate);
    testGroupDelay (sampleRate);
    testPhaseLinearity (sampleRate);
    testLatencyReporting (sampleRate);

    juce::shutdownJuce_GUI();
    return 0;
}

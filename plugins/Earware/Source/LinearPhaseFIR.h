#pragma once

#include <cmath>
#include <complex>
#include <vector>
#include "ParametricEQData.h"

struct LinearPhaseFIR
{
    static constexpr int irLength = 2048;

    static void compute (const EarwareEqPreset* preset, double sampleRate, float* irOut)
    {
        computeIDFT (preset, sampleRate, irOut);

        // Normalize to match IIR DC gain
        float tapSum = 0.0f;
        for (int i = 0; i < irLength; ++i)
            tapSum += irOut[i];

        const float dcGain = computeDCGain (preset);
        if (std::abs (tapSum) > 1e-12f)
        {
            const float scale = dcGain / tapSum;
            for (int i = 0; i < irLength; ++i)
                irOut[i] *= scale;
        }
    }

private:
    static float computeDCGain (const EarwareEqPreset* preset)
    {
        const float preampLin = juce::Decibels::decibelsToGain (preset->preampGain);
        float dcGain = preampLin;
        for (int f = 0; f < 10; ++f)
            dcGain *= biquadDCGain (preset->filters[f]);
        return dcGain;
    }

    static void computeIDFT (const EarwareEqPreset* preset, double sampleRate, float* irOut)
    {
        jassert (preset != nullptr);
        const float preampLin = juce::Decibels::decibelsToGain (preset->preampGain);
        const float nyquist = static_cast<float> (sampleRate * 0.5);

        std::vector<float> mag (static_cast<size_t> (irLength / 2 + 1));

        for (int k = 0; k <= irLength / 2; ++k)
        {
            const float freq = static_cast<float> (k) * nyquist / static_cast<float> (irLength / 2);
            std::complex<float> H (1.0f, 0.0f);

            for (int f = 0; f < 10; ++f)
                H *= biquadResponse (preset->filters[f], freq, sampleRate);

            H *= preampLin;
            mag[static_cast<size_t> (k)] = std::abs (H);
        }

        for (int n = 0; n < irLength; ++n)
        {
            double sum = 0.0;
            sum += mag[0];

            for (int k = 1; k < irLength / 2; ++k)
            {
                const double w = 2.0 * M_PI * static_cast<double> (k) * static_cast<double> (n)
                                 / static_cast<double> (irLength);
                sum += 2.0 * static_cast<double> (mag[k]) * std::cos (w);
            }

            sum += static_cast<double> (mag[irLength / 2]) * std::cos (M_PI * static_cast<double> (n));

            irOut[n] = static_cast<float> (sum / static_cast<double> (irLength));
        }

        const int half = irLength / 2;
        for (int i = 0; i < half; ++i)
            std::swap (irOut[i], irOut[i + half]);
    }

    static float biquadDCGain (const EarwareFilterStage& s)
    {
        if (std::abs (s.gain) < 1e-6f)
            return 1.0f;

        const float gLin = juce::Decibels::decibelsToGain (s.gain);

        switch (s.type)
        {
            case EarwareFilterLSC:
                return gLin;

            case EarwareFilterHSC:
                return 1.0f;

            case EarwareFilterPK:
            default:
                return 1.0f;
        }
    }

    static std::complex<float> biquadResponse (const EarwareFilterStage& s, float freq, double sampleRate)
    {
        if (std::abs (s.gain) < 1e-6f)
            return { 1.0f, 0.0f };

        if (freq < 1e-6f)
        {
            switch (s.type)
            {
                case EarwareFilterLSC: return { juce::Decibels::decibelsToGain (s.gain), 0.0f };
                case EarwareFilterHSC: return { 1.0f, 0.0f };
                case EarwareFilterPK:
                default:               return { 1.0f, 0.0f };
            }
        }

        const float nyquist = static_cast<float> (sampleRate * 0.5);
        if (freq > nyquist - 1.0f)
        {
            switch (s.type)
            {
                case EarwareFilterLSC: return { 1.0f, 0.0f };
                case EarwareFilterHSC: return { juce::Decibels::decibelsToGain (s.gain), 0.0f };
                case EarwareFilterPK:
                default:               return { 1.0f, 0.0f };
            }
        }

        const float A  = std::sqrt (juce::Decibels::decibelsToGain (s.gain));
        const float w0coeff = 6.2831853f * s.freq / static_cast<float> (sampleRate);
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

        const float w0eval = 6.2831853f * freq / static_cast<float> (sampleRate);
        const std::complex<float> z1 (std::cos (w0eval), -std::sin (w0eval));
        const std::complex<float> z2 = z1 * z1;

        const std::complex<float> num = b0 + b1 * z1 + b2 * z2;
        const std::complex<float> den = a0 + a1 * z1 + a2 * z2;

        return num / den;
    }
};

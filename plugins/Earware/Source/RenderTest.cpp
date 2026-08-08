#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
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

    juce::shutdownJuce_GUI();
    return 0;
}
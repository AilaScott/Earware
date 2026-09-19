#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ParametricEQData.h"
#include "LinearPhaseFIR.h"

class EarwareAudioProcessor : public juce::AudioProcessor
{
public:
    EarwareAudioProcessor();
    ~EarwareAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<int> currentModelIndex { 0 };
    std::atomic<int> pendingModelIndex { -1 };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::dsp::Convolution convolution;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassRamp;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> dryDelayBuffer;
    juce::AudioBuffer<float> irBuffer;

    double currentSampleRate = 44100.0;
    int loadedModelIndex = -1;
    int convLatency = 0;
    int dryDelayWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarwareAudioProcessor)
};

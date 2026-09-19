#include "PluginProcessor.h"
#include "PluginEditor.h"

EarwareAudioProcessor::EarwareAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    bypassRamp.setCurrentAndTargetValue (0.0f);
}

EarwareAudioProcessor::~EarwareAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout EarwareAudioProcessor::createParameterLayout()
{
    juce::StringArray modelChoices;
    for (int i = 0; i < earwareNumModels; ++i)
        modelChoices.add (earwareGetModelName (i));

    return
    {
        std::make_unique<juce::AudioParameterChoice> ("model", "Model", modelChoices, 0),
        std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false),
    };
}

void EarwareAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    convolution.prepare (spec);

    bypassRamp.reset (sampleRate, 0.005);

    const int numCh = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    dryBuffer.setSize (numCh, samplesPerBlock, false, false, true);
    dryDelayBuffer.setSize (numCh, 8192, false, false, true);
    irBuffer.setSize (1, LinearPhaseFIR::irLength, false, false, false);
    dryDelayWritePos = 0;

    loadedModelIndex = -1;

    int modelIdx = (int) apvts.getRawParameterValue ("model")->load();
    pendingModelIndex.store (modelIdx);
    currentModelIndex.store (modelIdx);
}

void EarwareAudioProcessor::releaseResources()
{
}

bool EarwareAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::mono()
     && layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void EarwareAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    int modelIdx = juce::jlimit (0, earwareNumModels - 1, juce::roundToInt (apvts.getRawParameterValue ("model")->load()));
    bool bypassed = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    currentModelIndex.store (modelIdx);

    if (modelIdx != loadedModelIndex && modelIdx >= 0 && modelIdx < earwareNumModels)
    {
        auto* preset = earwareGetPreset (modelIdx);
        if (preset != nullptr)
        {
            LinearPhaseFIR::compute (preset, currentSampleRate, irBuffer.getWritePointer (0));

            juce::AudioBuffer<float> irCopy (1, LinearPhaseFIR::irLength);
            irCopy.copyFrom (0, 0, irBuffer, 0, 0, LinearPhaseFIR::irLength);

            convolution.loadImpulseResponse (std::move (irCopy), currentSampleRate,
                                             juce::dsp::Convolution::Stereo::yes,
                                             juce::dsp::Convolution::Trim::no,
                                             juce::dsp::Convolution::Normalise::no);
            convLatency = LinearPhaseFIR::irLength / 2;
            setLatencySamples (convLatency);

            loadedModelIndex = modelIdx;
        }
    }

    const bool transitioning = bypassRamp.isSmoothing();
    const auto numSamples = buffer.getNumSamples();

    if (transitioning)
    {
        const int ringSize = dryDelayBuffer.getNumSamples();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* inPtr = buffer.getReadPointer (ch, 0);
            float* ringPtr = dryDelayBuffer.getWritePointer (ch, 0);

            for (int s = 0; s < numSamples; ++s)
                ringPtr[(dryDelayWritePos + s) % ringSize] = inPtr[s];

            const int readPos = (dryDelayWritePos + numSamples - convLatency + ringSize * 2) % ringSize;
            float* dryPtr = dryBuffer.getWritePointer (ch, 0);
            for (int s = 0; s < numSamples; ++s)
                dryPtr[s] = ringPtr[(readPos + s) % ringSize];
        }

        dryDelayWritePos = (dryDelayWritePos + numSamples) % ringSize;
    }

    bypassRamp.setTargetValue (bypassed ? 0.0f : 1.0f);

    if (! bypassed || transitioning)
    {
        auto wetBlock = juce::dsp::AudioBlock<float> (buffer);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (wetBlock));
    }

    if (transitioning)
    {
        auto* channelData = buffer.getArrayOfWritePointers();
        for (int s = 0; s < numSamples; ++s)
        {
            auto rampVal = bypassRamp.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                channelData[ch][s] = channelData[ch][s] * rampVal
                                   + dryBuffer.getReadPointer (ch)[s] * (1.0f - rampVal);
        }
    }
}

juce::AudioProcessorEditor* EarwareAudioProcessor::createEditor()
{
    return new EarwareAudioProcessorEditor (*this);
}

bool EarwareAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String EarwareAudioProcessor::getName() const
{
    return "Earware";
}

bool EarwareAudioProcessor::acceptsMidi() const { return false; }
bool EarwareAudioProcessor::producesMidi() const { return false; }
bool EarwareAudioProcessor::isMidiEffect() const { return false; }
double EarwareAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int EarwareAudioProcessor::getNumPrograms() { return 1; }
int EarwareAudioProcessor::getCurrentProgram() { return 0; }
void EarwareAudioProcessor::setCurrentProgram (int) {}
const juce::String EarwareAudioProcessor::getProgramName (int) { return {}; }
void EarwareAudioProcessor::changeProgramName (int, const juce::String&) {}

void EarwareAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EarwareAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr)
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EarwareAudioProcessor();
}

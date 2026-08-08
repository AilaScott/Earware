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

    preampGain.prepare (spec);
    preampGain.setRampDurationSeconds (0.01);

    for (auto& f : filters)
        f.prepare (spec);

    bypassRamp.reset (sampleRate, 0.005);

    dryBuffer.setSize (juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels()), samplesPerBlock, false, false, true);

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
            preampGain.setGainDecibels (preset->preampGain);

            for (int f = 0; f < 10; ++f)
            {
                auto& stage = preset->filters[f];
                using Coeffs = juce::dsp::IIR::Coefficients<float>;
                Coeffs::Ptr newCoeffs;
                const float gainLinear = juce::Decibels::decibelsToGain (stage.gain);

                switch (stage.type)
                {
                    case EarwareFilterPK:
                        newCoeffs = Coeffs::makePeakFilter (
                            currentSampleRate, stage.freq, stage.q, gainLinear);
                        break;
                    case EarwareFilterLSC:
                        newCoeffs = Coeffs::makeLowShelf (
                            currentSampleRate, stage.freq, stage.q, gainLinear);
                        break;
                    case EarwareFilterHSC:
                        newCoeffs = Coeffs::makeHighShelf (
                            currentSampleRate, stage.freq, stage.q, gainLinear);
                        break;
                }

                *filters[f].state = *newCoeffs;
            }

            loadedModelIndex = modelIdx;
        }
    }

    bypassRamp.setTargetValue (bypassed ? 0.0f : 1.0f);

    const bool transitioning = bypassRamp.isSmoothing();
    const auto numSamples = buffer.getNumSamples();

    if (transitioning)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    if (! bypassed || transitioning)
    {
        auto preampBlock = juce::dsp::AudioBlock<float> (buffer);
        preampGain.process (juce::dsp::ProcessContextReplacing<float> (preampBlock));

        auto filterBlock = juce::dsp::AudioBlock<float> (buffer);
        for (auto& f : filters)
            f.process (juce::dsp::ProcessContextReplacing<float> (filterBlock));
    }

    if (transitioning)
    {
        auto* channelData = buffer.getArrayOfWritePointers();
        for (int s = 0; s < numSamples; ++s)
        {
            auto rampVal = bypassRamp.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                channelData[ch][s] = channelData[ch][s] * rampVal + dryBuffer.getReadPointer (ch)[s] * (1.0f - rampVal);
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

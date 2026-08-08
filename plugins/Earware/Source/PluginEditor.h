#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class EarwareAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Timer
{
public:
    EarwareAudioProcessorEditor (EarwareAudioProcessor&);
    ~EarwareAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

private:
    EarwareAudioProcessor& audioProcessor;

    // CRITICAL: MEMBER DECLARATION ORDER
    // 1. RELAYS FIRST (destroyed last)
    juce::WebSliderRelay modelRelay { "model" };
    juce::WebToggleButtonRelay bypassRelay { "bypass" };

    // 2. WEBVIEW SECOND (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> modelAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    juce::String curveDataToJSON (int modelIndex);

    int lastPushedModelIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarwareAudioProcessorEditor)
};

#include "PluginEditor.h"
#include "BinaryData.h"

static constexpr int CURVE_POINTS = 256;

class EarwareWebBrowserComponent : public juce::WebBrowserComponent
{
public:
    using PageFinishedCallback = std::function<void()>;

    EarwareWebBrowserComponent (juce::WebBrowserComponent::Options options, PageFinishedCallback callback)
        : juce::WebBrowserComponent (std::move (options)),
          onPageFinished (std::move (callback))
    {
    }

    void pageFinishedLoading (const juce::String& url) override
    {
        juce::WebBrowserComponent::pageFinishedLoading (url);
        if (onPageFinished)
            onPageFinished();
    }

private:
    PageFinishedCallback onPageFinished;
};

EarwareAudioProcessorEditor::EarwareAudioProcessorEditor (EarwareAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    modelAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *audioProcessor.apvts.getParameter ("model"), modelRelay);

    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *audioProcessor.apvts.getParameter ("bypass"), bypassRelay);

    juce::WebBrowserComponent::Options options{};
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                  .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)))
                     .withNativeIntegrationEnabled()
                     .withKeepPageLoadedWhenBrowserIsHidden()
                     .withOptionsFrom (modelRelay)
                     .withOptionsFrom (bypassRelay);

    for (int i = 0; i < earwareNumModels; ++i)
        options = options.withInitialisationData ("models", juce::var (earwareGetModelName (i)));

    options = options.withEventListener ("earwareReady", [this] (juce::String) { lastPushedModelIndex = -1; });

    options = options.withResourceProvider ([this] (const auto& url) { return getResource (url); });

    webView = std::make_unique<EarwareWebBrowserComponent> (options,
        [this] { lastPushedModelIndex = -1; });

    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (600, 450);
    startTimerHz (30);
}

EarwareAudioProcessorEditor::~EarwareAudioProcessorEditor()
{
    stopTimer();
}

void EarwareAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void EarwareAudioProcessorEditor::resized()
{
    if (webView)
        webView->setBounds (getLocalBounds());
}

void EarwareAudioProcessorEditor::timerCallback()
{
    if (! webView)
        return;

    int modelIdx = juce::jlimit (0, earwareNumModels - 1, juce::roundToInt (audioProcessor.apvts.getRawParameterValue ("model")->load()));

    if (modelIdx == lastPushedModelIndex)
        return;

    lastPushedModelIndex = modelIdx;

    auto modelName = earwareGetModelName (modelIdx);
    if (modelName == nullptr)
        return;

    juce::String escapedName = modelName;
    escapedName = escapedName.replace ("'", "\\'");

    juce::String js = "if (window.updateModelInfo && window.updateCurve) { window.updateModelInfo('"
                    + escapedName + "', " + juce::String (modelIdx) + "); window.updateCurve("
                    + curveDataToJSON (modelIdx) + "); }";

    try
    {
        webView->evaluateJavascript (js);
    }
    catch (...)
    {
    }
}

juce::String EarwareAudioProcessorEditor::curveDataToJSON (int modelIndex)
{
    auto* preset = earwareGetPreset (modelIndex);
    if (preset == nullptr)
        return "[]";

    std::vector<float> freqs (CURVE_POINTS);
    std::vector<float> eqs (CURVE_POINTS);

    earwareComputeCurve (modelIndex, freqs.data(), eqs.data(), CURVE_POINTS);

    juce::String json = "[";
    for (int i = 0; i < CURVE_POINTS; ++i)
    {
        if (i > 0) json += ",";
        json += "[" + juce::String (freqs[i], 2) + "," + juce::String (eqs[i], 3) + "]";
    }
    json += "]";

    return json;
}

std::optional<juce::WebBrowserComponent::Resource> EarwareAudioProcessorEditor::getResource (const juce::String& url)
{
    auto resourcePath = url;
    const auto root = juce::WebBrowserComponent::getResourceProviderRoot();

    if (url.startsWith (root))
        resourcePath = url.fromFirstOccurrenceOf (root, false, false);

    if (resourcePath.isEmpty() || resourcePath == "/")
        resourcePath = "/index.html";

    auto path = resourcePath.substring (1);

    const char* resourceData = nullptr;
    int resourceSize = 0;
    juce::String mimeType;

    if (path == "index.html")
    {
        resourceData = BinaryData::index_html;
        resourceSize = BinaryData::index_htmlSize;
        mimeType = "text/html";
    }
    else if (path == "js/index.js")
    {
        resourceData = BinaryData::index_js;
        resourceSize = BinaryData::index_jsSize;
        mimeType = "text/javascript";
    }
    else if (path == "js/juce/index.js")
    {
        resourceData = BinaryData::index_js2;
        resourceSize = BinaryData::index_js2Size;
        mimeType = "text/javascript";
    }
    else if (path == "js/juce/check_native_interop.js")
    {
        resourceData = BinaryData::check_native_interop_js;
        resourceSize = BinaryData::check_native_interop_jsSize;
        mimeType = "text/javascript";
    }

    if (resourceData != nullptr && resourceSize > 0)
    {
        std::vector<std::byte> data (resourceSize);
        std::memcpy (data.data(), resourceData, resourceSize);
        return juce::WebBrowserComponent::Resource { std::move (data), mimeType };
    }

    juce::String fallbackHtml = R"(<!DOCTYPE html>
<html>
<head><title>Earware - Resource Not Found</title>
<style>
body { background: #ffde00; color: #000; font-family: sans-serif; padding: 40px; }
h1 { font-size: 22px; }
</style></head>
<body><h1>Earware</h1><p>Resource not found: )" + path + R"(</p></body>
</html>)";

    std::vector<std::byte> fallbackData ((size_t) fallbackHtml.length());
    std::memcpy (fallbackData.data(), fallbackHtml.toRawUTF8(), (size_t) fallbackHtml.length());
    return juce::WebBrowserComponent::Resource { std::move (fallbackData), "text/html" };
}

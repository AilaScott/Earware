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

   #if JUCE_WINDOWS
    // WebView2 (Chromium) is only available on Windows; forcing it elsewhere is
    // a no-op in JUCE 8 (Linux/macOS always use WebKit/WKWebView), so it must
    // not be set for those platforms.
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                  .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)))
                     .withKeepPageLoadedWhenBrowserIsHidden();
   #endif

    options = options.withNativeIntegrationEnabled()
                     .withOptionsFrom (modelRelay)
                     .withOptionsFrom (bypassRelay);

    // Test hook (see docs/BUILD_AND_CI_REFERENCE.md): set EARWARE_NO_INJECT_MODELS=1
    // to skip the injected model list, forcing the page down the models.json
    // fetch fallback — the path macOS needs when injection fails (#6).
    const bool injectModels =
        juce::SystemStats::getEnvironmentVariable ("EARWARE_NO_INJECT_MODELS", "").isEmpty();

    if (injectModels)
        for (int i = 0; i < earwareNumModels; ++i)
            options = options.withInitialisationData ("models", juce::var (earwareGetModelName (i)));

    options = options.withEventListener ("earwareReady", [this] (juce::String) { lastPushedModelIndex = -1; });

    options = options.withResourceProvider ([this] (const auto& url) { return getResource (url); });

    webView = std::make_unique<EarwareWebBrowserComponent> (options,
        [this]
        {
            lastPushedModelIndex = -1;
            probeTicksRemaining = probeDelayTicks;
        });

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

    if (probeTicksRemaining > 0 && --probeTicksRemaining == 0)
    {
        probeTicksRemaining = -1;
        runDiagnosticsProbe();
    }

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

// Runs ~1.5 s after the page finishes loading (see timerCallback). The probe
// uses evaluateJavascript directly, which works even when the message-handler
// bridge is dead, so it can still tell us (via the debug log) which part of the
// UI failed: injected initialisation data, the page-side model list, or both.
void EarwareAudioProcessorEditor::runDiagnosticsProbe()
{
    if (webView == nullptr)
        return;

    static constexpr const char* probeJs = R"JS(
(function () {
  try {
    var j = window.__JUCE__;
    var d = (j && j.initialisationData) || {};
    var g = window.__EARWARE_DIAG__ || null;
    return JSON.stringify({
      initModels: Array.isArray(d.models) ? d.models.length : -1,
      diag: g,
      backend: !!(j && j.backend),
      transport: !!(window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.__JUCE__)
                 || !!(window.chrome && window.chrome.webview),
      uiBound: typeof window.updateModelInfo === 'function',
      needError: !g || !(g.models > 0)
    });
  } catch (e) {
    return JSON.stringify({ needError: true, error: String(e) });
  }
})())JS";

    webView->evaluateJavascript (probeJs,
        [this] (const juce::WebBrowserComponent::EvaluationResult& result)
        {
            const auto* value = result.getResult();
            const auto json = value != nullptr ? value->toString() : juce::String();

            if (json.isEmpty())
            {
                juce::Logger::writeToLog ("Earware UI probe: no result");
                return;
            }

            juce::Logger::writeToLog ("Earware UI probe: " + json);

            if (json.contains ("\"needError\":true") && webView != nullptr)
                webView->evaluateJavascript (
                    "(function(){var s=document.getElementById('modelStatus');"
                    "if(s){s.textContent='Model list failed to load';}"
                    "return 'ok';})()");
        });
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
    else if (path == "models.json")
    {
        // Fallback transport for the model list: the injected initialisation
        // data does not reach the page on every platform (see #6), but the
        // resource provider demonstrably does — the page itself loads through it.
        juce::MemoryOutputStream json { 256 * 1024 };

        json << "[";
        for (int i = 0; i < earwareNumModels; ++i)
        {
            if (i > 0)
                json << ",";
            json << juce::JSON::toString (juce::var (earwareGetModelName (i)));
        }
        json << "]";

        std::vector<std::byte> data (json.getDataSize());
        std::memcpy (data.data(), json.getData(), json.getDataSize());
        return juce::WebBrowserComponent::Resource { std::move (data), "application/json" };
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

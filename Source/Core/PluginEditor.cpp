#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DS1BinaryData.h"

#include <unordered_map>

static const juce::String localDevServerAddress = "http://localhost:3000/";

static juce::ZipFile* getZipFile()
{
    static juce::MemoryInputStream stream {
        DS1BinaryData::bossds1gui_1_0_0_zip,
        (size_t) DS1BinaryData::bossds1gui_1_0_0_zipSize,
        false };
    static juce::ZipFile zip { &stream, false };
    return &zip;
}

const char* AudioPluginAudioProcessorEditor::getMimeForExtension (const juce::String& ext)
{
    static const std::unordered_map<std::string, const char*> mimeMap
    {
        { "html",  "text/html"                },
        { "htm",   "text/html"                },
        { "js",    "text/javascript"          },
        { "mjs",   "text/javascript"          },
        { "css",   "text/css"                 },
        { "json",  "application/json"         },
        { "map",   "application/json"         },
        { "png",   "image/png"                },
        { "jpg",   "image/jpeg"               },
        { "jpeg",  "image/jpeg"               },
        { "svg",   "image/svg+xml"            },
        { "ico",   "image/vnd.microsoft.icon" },
        { "woff2", "font/woff2"               },
        { "txt",   "text/plain"               },
    };

    const auto it = mimeMap.find (ext.toLowerCase().toStdString());
    return it != mimeMap.end() ? it->second : "application/octet-stream";
}

juce::String AudioPluginAudioProcessorEditor::getExtension (const juce::String& filename)
{
    return filename.fromLastOccurrenceOf (".", false, false);
}

std::optional<juce::WebBrowserComponent::Resource>
AudioPluginAudioProcessorEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String { "index.html" }
                                 : url.fromFirstOccurrenceOf ("/", false, false);

    auto* archive = getZipFile();
    if (auto* entry = archive->getEntry (path))
    {
        std::unique_ptr<juce::InputStream> stream (archive->createStreamForEntry (*entry));

        std::vector<std::byte> data ((size_t) stream->getTotalLength());
        stream->read (data.data(), data.size());

        return juce::WebBrowserComponent::Resource {
            std::move (data),
            juce::String { getMimeForExtension (getExtension (path)) }
        };
    }

    return std::nullopt;
}

bool AudioPluginAudioProcessorEditor::SinglePageBrowser::pageAboutToLoad (const juce::String& newURL)
{
    return newURL == localDevServerAddress
        || newURL == juce::WebBrowserComponent::getResourceProviderRoot();
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      webComponent (juce::WebBrowserComponent::Options{}
                        .withNativeIntegrationEnabled()
                        .withOptionsFrom (driveRelay)
                        .withOptionsFrom (levelRelay)
                        .withOptionsFrom (modelRelay)
                        .withResourceProvider (
                            [this] (const auto& url) { return getResource (url); },
                            juce::URL { localDevServerAddress }.getOrigin())),
      driveAttachment (*p.apvts.getParameter ("drive"), driveRelay, p.apvts.undoManager),
      levelAttachment (*p.apvts.getParameter ("level"), levelRelay, p.apvts.undoManager),
      modelAttachment (*p.apvts.getParameter ("model"), modelRelay, p.apvts.undoManager)
{
    addAndMakeVisible (webComponent);

  #if BOSS_DS1_DEV_UI
    webComponent.goToURL (localDevServerAddress);
  #else
    webComponent.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
  #endif

    setSize (600, 350);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void AudioPluginAudioProcessorEditor::resized()
{
    webComponent.setBounds (getLocalBounds());
}

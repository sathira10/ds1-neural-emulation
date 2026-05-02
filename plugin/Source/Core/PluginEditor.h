#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    static const char* getMimeForExtension (const juce::String& ext);
    static juce::String getExtension       (const juce::String& filename);

    AudioPluginAudioProcessor& processorRef;

    // Relays must be declared before webComponent — construction order matters
    juce::WebSliderRelay   driveRelay  { "drive" };
    juce::WebSliderRelay   levelRelay  { "level" };
    juce::WebComboBoxRelay modelRelay  { "model" };
    juce::WebSliderRelay   bypassRelay { "bypass" };

    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& newURL) override;
    };

    SinglePageBrowser webComponent;

    // Attachments declared after webComponent
    juce::WebSliderParameterAttachment   driveAttachment;
    juce::WebSliderParameterAttachment   levelAttachment;
    juce::WebComboBoxParameterAttachment modelAttachment;
    juce::WebSliderParameterAttachment   bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};

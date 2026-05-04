#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr double kGainSmoothingSeconds   = 0.02;
constexpr double kBypassCrossfadeSeconds = 0.015;
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "drive", "Drive",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f, "dB"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "level", "Level",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f, "dB"));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "model", "Model",
        juce::StringArray { "LSTM", "GRU" },
        0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "bypass", "Bypass", false));

    return layout;
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                   #if ! JucePlugin_IsMidiEffect
                    #if ! JucePlugin_IsSynth
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                    #endif
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                   #endif
                     ),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    driveParam  = apvts.getRawParameterValue ("drive");
    levelParam  = apvts.getRawParameterValue ("level");
    modelParam  = apvts.getRawParameterValue ("model");
    bypassParam = apvts.getRawParameterValue ("bypass");
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms()    { return 1; }
int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }
void AudioPluginAudioProcessor::setCurrentProgram (int) {}
const juce::String AudioPluginAudioProcessor::getProgramName (int) { return {}; }
void AudioPluginAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mlEngine.prepare (sampleRate, samplesPerBlock);
    monoScratch.setSize (1, samplesPerBlock, false, false, true);
    setLatencySamples (mlEngine.getLatencySamples());

    driveGainSmoothed.reset (sampleRate, kGainSmoothingSeconds);
    levelGainSmoothed.reset (sampleRate, kGainSmoothingSeconds);
    wetnessSmoothed  .reset (sampleRate, kBypassCrossfadeSeconds);

    driveGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (driveParam->load()));
    levelGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (levelParam->load()));
    wetnessSmoothed.setCurrentAndTargetValue (
        bypassParam->load() > 0.5f ? 0.0f : 1.0f);
}

void AudioPluginAudioProcessor::releaseResources() {}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples        = buffer.getNumSamples();
    const int numInputChannels  = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();

    for (int i = numInputChannels; i < numOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // 1. Snapshot parameters via cached atomics — no string lookup per block.
    const float driveGainTarget = juce::Decibels::decibelsToGain (driveParam->load());
    const float levelGainTarget = juce::Decibels::decibelsToGain (levelParam->load());
    const int   modelIdx        = static_cast<int> (modelParam->load());
    const bool  bypassed        = bypassParam->load() > 0.5f;

    driveGainSmoothed.setTargetValue (driveGainTarget);
    levelGainSmoothed.setTargetValue (levelGainTarget);
    wetnessSmoothed  .setTargetValue (bypassed ? 0.0f : 1.0f);

    // 2. Drive (smoothed ramp) applied to every input channel.
    {
        const float startG = driveGainSmoothed.getCurrentValue();
        driveGainSmoothed.skip (numSamples);
        const float endG   = driveGainSmoothed.getCurrentValue();
        for (int ch = 0; ch < numInputChannels; ++ch)
            buffer.applyGainRamp (ch, 0, numSamples, startG, endG);
    }

    // 3. Always update the active model so a switch made during bypass resets
    //    the incoming model's hidden state before the next un-bypassed block.
    mlEngine.setActiveModel (modelIdx);

    // 4. Run the model only when the wet path contributes to output — either
    //    actively un-bypassed, or still fading out after a bypass press.
    const bool needWet = wetnessSmoothed.isSmoothing() || ! bypassed;

    if (needWet)
    {
        auto* mono = monoScratch.getWritePointer (0);

        if (numInputChannels >= 2)
        {
            juce::FloatVectorOperations::copyWithMultiply (mono, buffer.getReadPointer (0), 0.5f, numSamples);
            juce::FloatVectorOperations::addWithMultiply  (mono, buffer.getReadPointer (1), 0.5f, numSamples);
        }
        else
        {
            juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0), numSamples);
        }

        // ML inference — passthrough if engine isn't valid (placeholder models).
        mlEngine.process (mono, numSamples);

        // Per-channel crossfade: dry = current buffer content, wet = mono model output.
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            auto* dst     = buffer.getWritePointer (ch);
            auto  smoother = wetnessSmoothed;
            for (int i = 0; i < numSamples; ++i)
            {
                const float w = smoother.getNextValue();
                dst[i] = (1.0f - w) * dst[i] + w * mono[i];
            }
        }
        wetnessSmoothed.skip (numSamples);
    }
    else
    {
        wetnessSmoothed.skip (numSamples);
    }

    // 5. Level (smoothed ramp) applied to all output channels.
    {
        const float startG = levelGainSmoothed.getCurrentValue();
        levelGainSmoothed.skip (numSamples);
        const float endG   = levelGainSmoothed.getCurrentValue();
        for (int ch = 0; ch < numOutputChannels; ++ch)
            buffer.applyGainRamp (ch, 0, numSamples, startG, endG);
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

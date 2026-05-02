#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    const float inputGain  = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("drive")->load() - 24.0f);
    const float outputGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("level")->load());
    const int   modelIdx   = static_cast<int> (apvts.getRawParameterValue ("model")->load());
    const bool  bypassed   = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    // Apply input gain to all input channels.
    for (int ch = 0; ch < numInputChannels; ++ch)
        buffer.applyGain (ch, 0, numSamples, inputGain);

    // Update active model unconditionally so a switch made during bypass takes
    // effect (with hidden state reset) before the next un-bypassed block.
    mlEngine.setActiveModel (modelIdx);

    if (! bypassed)
    {
        // Sum to mono (or copy if already mono). Model is mono in / mono out.
        auto* mono = monoScratch.getWritePointer (0);
        if (numInputChannels >= 2)
        {
            const auto* L = buffer.getReadPointer (0);
            const auto* R = buffer.getReadPointer (1);
            for (int i = 0; i < numSamples; ++i)
                mono[i] = 0.5f * (L[i] + R[i]);
        }
        else
        {
            juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0), numSamples);
        }

        // ML inference — if engine is not yet valid (placeholder models) audio passes through.
        mlEngine.process (mono, numSamples);

        // Splat mono output to all output channels.
        for (int ch = 0; ch < numOutputChannels; ++ch)
            juce::FloatVectorOperations::copy (buffer.getWritePointer (ch), mono, numSamples);
    }

    // Apply output gain.
    buffer.applyGain (outputGain);
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

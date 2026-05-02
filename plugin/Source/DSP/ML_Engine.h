#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <RTNeural/RTNeural.h>
#include <atomic>
#include <vector>

class ML_Engine
{
public:
    ML_Engine()  = default;
    ~ML_Engine() = default;

    // Load both models and set up resampler. Call from prepareToPlay.
    void prepare (double hostSampleRate, int maxBlockSize);

    // Zero RNN hidden state and resampler buffers. Call on transport jumps.
    void reset() noexcept;

    // Select active model. 0 = LSTM, 1 = GRU. Safe to call from the audio thread.
    void setActiveModel (int modelIndex) noexcept;

    // Mono in-place processing. numSamples is at hostSampleRate.
    void process (float* samples, int numSamples) noexcept;

    int  getLatencySamples() const noexcept { return latencySamples; }

    // False when the active model failed to load (audio passes through unmodified).
    bool isValid() const noexcept;

private:
    static constexpr int kModelSampleRate = 44100;
    static constexpr int kHiddenSize      = 32;

    using LSTMModel = RTNeural::ModelT<float, 1, 1,
        RTNeural::LSTMLayerT<float, 1, kHiddenSize>,
        RTNeural::DenseT   <float, kHiddenSize, 1>>;

    using GRUModel  = RTNeural::ModelT<float, 1, 1,
        RTNeural::GRULayerT<float, 1, kHiddenSize>,
        RTNeural::DenseT   <float, kHiddenSize, 1>>;

    bool loadLSTMFromJson (const char* data, int dataSize);
    bool loadGRUFromJson  (const char* data, int dataSize);
    void runInference     (float* samples, int numSamples) noexcept;

    LSTMModel lstmModel;
    GRUModel  gruModel;

    bool lstmValid = false, gruValid = false;
    bool lstmSkip  = false, gruSkip  = false;

    std::atomic<bool> useGRU { false };   // false = LSTM, true = GRU

    double hostSR          = 44100.0;
    int    maxBlock        = 0;
    bool   needsResample   = false;
    double downsampleRatio = 1.0;

    juce::Interpolators::WindowedSinc downsampler;
    juce::Interpolators::WindowedSinc upsampler;
    std::vector<float> modelRateScratch;

    int latencySamples = 0;
};

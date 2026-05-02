#include "ML_Engine.h"
#include "DS1ModelData.h"

namespace {

struct ModelMeta { bool skip = false; bool bias = true; };

// Shared parse + validation. Populates meta and j on success.
static bool parseModelJson (const char*        data,
                            int                dataSize,
                            const std::string& expectedUnitType,
                            int                expectedHiddenSize,
                            int                expectedSampleRate,
                            ModelMeta&         meta,
                            nlohmann::json&    j)
{
    if (dataSize <= 2)
    {
        juce::Logger::writeToLog ("ML_Engine: model file is a placeholder. "
                                  "Replace Resources/Models/ files with a GAM-trained model.");
        return false;
    }

    try
    {
        j = nlohmann::json::parse (data, data + dataSize);
    }
    catch (const nlohmann::json::exception& e)
    {
        juce::Logger::writeToLog (juce::String ("ML_Engine: JSON parse failed: ") + e.what());
        return false;
    }

    if (!j.contains ("model_data") || !j.contains ("state_dict"))
    {
        juce::Logger::writeToLog ("ML_Engine: missing model_data or state_dict key.");
        return false;
    }

    const auto& md       = j["model_data"];
    const auto  unitType = md.value ("unit_type",   std::string{});
    const int   skipVal  = md.value ("skip",         0);
    const int   sr       = md.value ("sample_rate",  expectedSampleRate);
    const int   hidSz    = md.value ("hidden_size",  expectedHiddenSize);

    if (md.value ("model",       std::string{}) != "SimpleRNN") { juce::Logger::writeToLog ("ML_Engine: only SimpleRNN supported.");              return false; }
    if (unitType != expectedUnitType)                            { juce::Logger::writeToLog (juce::String ("ML_Engine: unit_type must be ") + expectedUnitType.c_str() + "."); return false; }
    if (md.value ("num_layers",  0) != 1)                       { juce::Logger::writeToLog ("ML_Engine: num_layers > 1 not supported.");          return false; }
    if (md.value ("input_size",  0) != 1)                       { juce::Logger::writeToLog ("ML_Engine: input_size != 1 not supported.");         return false; }
    if (md.value ("output_size", 0) != 1)                       { juce::Logger::writeToLog ("ML_Engine: output_size != 1 not supported.");        return false; }
    if (skipVal != 0 && skipVal != 1)                           { juce::Logger::writeToLog ("ML_Engine: skip must be 0 or 1.");                   return false; }
    if (hidSz != expectedHiddenSize)
    {
        juce::Logger::writeToLog (juce::String ("ML_Engine: hidden_size ") + juce::String (hidSz) +
                                  " != " + juce::String (expectedHiddenSize) + " — rejected.");
        return false;
    }

    if (sr != expectedSampleRate)
        juce::Logger::writeToLog (juce::String ("ML_Engine: WARNING - model sample_rate != ") +
                                  juce::String (expectedSampleRate) + ". Proceeding; tone may differ.");

    meta.skip = (skipVal == 1);
    meta.bias = md.value ("bias_fl", true);
    return true;
}

} // namespace

void ML_Engine::prepare (double hostSampleRate, int maxBlockSize)
{
    hostSR   = hostSampleRate;
    maxBlock = maxBlockSize;

    lstmValid = loadLSTMFromJson (DS1ModelData::lstm_model_json, DS1ModelData::lstm_model_jsonSize);
    gruValid  = loadGRUFromJson  (DS1ModelData::gru_model_json,  DS1ModelData::gru_model_jsonSize);

    // Default to LSTM; fall back to GRU if LSTM failed to load.
    useGRU.store (!lstmValid && gruValid, std::memory_order_relaxed);

    downsampleRatio = hostSR / kModelSampleRate;
    needsResample   = std::abs (downsampleRatio - 1.0) > 1e-6;

    if (needsResample)
    {
        // +8 samples headroom for fractional-position drift across blocks.
        const int maxModelSamples = static_cast<int> (std::ceil (maxBlockSize / downsampleRatio)) + 8;
        modelRateScratch.resize (static_cast<size_t> (maxModelSamples), 0.0f);

        // Round-trip group delay: one filter length at host SR (downsampler) + one at model SR converted to host SR (upsampler).
        const int filterLatency = juce::Interpolators::WindowedSinc::getBaseLatency();
        latencySamples = static_cast<int> (std::round (
            filterLatency * (1.0 + hostSR / kModelSampleRate)));
    }
    else
    {
        modelRateScratch.clear();
        latencySamples = 0;
    }

    reset();
}

void ML_Engine::reset() noexcept
{
    if (lstmValid) lstmModel.reset();
    if (gruValid)  gruModel.reset();
    downsampler.reset();
    upsampler.reset();
}

void ML_Engine::setActiveModel (int modelIndex) noexcept
{
    const bool wantGRU = (modelIndex != 0);

    if (wantGRU != useGRU.load (std::memory_order_relaxed))
    {
        // Reset hidden state on the incoming model to avoid a transient on switch.
        if (wantGRU  && gruValid)  gruModel.reset();
        if (!wantGRU && lstmValid) lstmModel.reset();
        useGRU.store (wantGRU, std::memory_order_relaxed);
    }
}

bool ML_Engine::isValid() const noexcept
{
    return useGRU.load (std::memory_order_relaxed) ? gruValid : lstmValid;
}

void ML_Engine::process (float* samples, int numSamples) noexcept
{
    if (!isValid()) return;

    if (!needsResample)
    {
        runInference (samples, numSamples);
        return;
    }

    const int numModelSamples = static_cast<int> (std::round (numSamples / downsampleRatio));
    downsampler.process (downsampleRatio, samples, modelRateScratch.data(), numModelSamples);
    runInference (modelRateScratch.data(), numModelSamples);
    upsampler.process (1.0 / downsampleRatio, modelRateScratch.data(), samples, numSamples);
}

void ML_Engine::runInference (float* samples, int numSamples) noexcept
{
    if (useGRU.load (std::memory_order_relaxed))
    {
        const bool s = gruSkip;
        for (int i = 0; i < numSamples; ++i)
        {
            const float in  = samples[i];
            float       out = gruModel.forward (&in);
            if (s) out += in;
            samples[i] = out;
        }

        const auto range = juce::FloatVectorOperations::findMinAndMax (samples, numSamples);
        if (! std::isfinite (range.getStart()) || ! std::isfinite (range.getEnd()))
        {
            gruModel.reset();
            juce::FloatVectorOperations::clear (samples, numSamples);
        }
    }
    else
    {
        const bool s = lstmSkip;
        for (int i = 0; i < numSamples; ++i)
        {
            const float in  = samples[i];
            float       out = lstmModel.forward (&in);
            if (s) out += in;
            samples[i] = out;
        }

        const auto range = juce::FloatVectorOperations::findMinAndMax (samples, numSamples);
        if (! std::isfinite (range.getStart()) || ! std::isfinite (range.getEnd()))
        {
            lstmModel.reset();
            juce::FloatVectorOperations::clear (samples, numSamples);
        }
    }
}

bool ML_Engine::loadLSTMFromJson (const char* data, int dataSize)
{
    ModelMeta      meta;
    nlohmann::json j;

    if (!parseModelJson (data, dataSize, "LSTM", kHiddenSize, kModelSampleRate, meta, j))
        return false;

    try
    {
        const auto& sd = j["state_dict"];
        RTNeural::torch_helpers::loadLSTM<float>  (sd, "rec.", lstmModel.get<0>());
        RTNeural::torch_helpers::loadDense<float> (sd, "lin.", lstmModel.get<1>(), meta.bias);
        lstmModel.reset();
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog (juce::String ("ML_Engine: LSTM weight loading failed: ") + e.what());
        return false;
    }

    lstmSkip = meta.skip;
    return true;
}

bool ML_Engine::loadGRUFromJson (const char* data, int dataSize)
{
    ModelMeta      meta;
    nlohmann::json j;

    if (!parseModelJson (data, dataSize, "GRU", kHiddenSize, kModelSampleRate, meta, j))
        return false;

    try
    {
        const auto& sd = j["state_dict"];
        RTNeural::torch_helpers::loadGRU<float>  (sd, "rec.", gruModel.get<0>());
        RTNeural::torch_helpers::loadDense<float> (sd, "lin.", gruModel.get<1>(), meta.bias);
        gruModel.reset();
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog (juce::String ("ML_Engine: GRU weight loading failed: ") + e.what());
        return false;
    }

    gruSkip = meta.skip;
    return true;
}

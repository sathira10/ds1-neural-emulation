# Audio Plugin Architecture

The plugin is split into a small host-facing processor layer and a dedicated DSP/model layer. The processor owns DAW integration and parameter flow; `ML_Engine` owns model loading, sample-rate adaptation, recurrent inference, and model state.

## Main Components

- `AudioPluginAudioProcessor`
  - Handles the host audio callback in `processBlock()`.
  - Owns the plugin parameters through `AudioProcessorValueTreeState`.
  - Exposes `Drive`, `Level`, `Model`, and `Bypass` to the host for automation and session recall.
  - Allocates the mono scratch buffer during `prepareToPlay()`, not during playback.
  - Reports the latency calculated by `ML_Engine` when sample-rate conversion is active.

- `ML_Engine`
  - Loads the embedded LSTM and GRU JSON model files into RTNeural models.
  - Validates the expected exported model shape:
    - `SimpleRNN`
    - mono input and output
    - one recurrent layer
    - hidden size `24`
    - LSTM or GRU unit type
  - Keeps the active recurrent model alive across audio blocks so temporal memory is not reset at every buffer boundary.
  - Resets hidden state when switching between LSTM and GRU to avoid carrying state from one architecture into the other.
  - Checks model output for non-finite values and clears the block if inference becomes invalid.

- Web UI
  - The control surface is a React/Vite app bundled into the plugin binary.
  - The plugin serves the bundled UI locally through `WebBrowserComponent`.
  - WebView parameter relays keep the React controls synchronized with the same APVTS parameters used by the audio thread.

## Preparation Path

`prepareToPlay()` sets up everything needed by the real-time callback:

- `ML_Engine::prepare()` loads both embedded model files.
- The default active model is LSTM, with GRU used only as a fallback if the LSTM model fails to load.
- Host sample rate and maximum block size are cached.
- The model-rate scratch buffer is sized if sample-rate conversion is needed.
- Downsampler and upsampler state is reset.
- Gain and bypass smoothers are initialized from the current parameter values.
- Reported plugin latency is updated from the sample-rate conversion path.

## Audio Processing Path

For each host block:

1. Clear any extra output channels that do not have matching inputs.
2. Read cached parameter atomics for `Drive`, `Level`, `Model`, and `Bypass`.
3. Apply `Drive` as a smoothed dB-to-linear gain ramp on the input channels.
4. Update the selected model before inference; if the selected model changed, reset the incoming model state.
5. Build a mono wet path:
   - mono input is copied directly;
   - stereo input is averaged to mono because the models were trained on mono dry/wet pairs.
6. Run `ML_Engine::process()` on the mono wet buffer.
7. Crossfade dry and wet when bypass changes, avoiding a hard discontinuity.
8. Copy the mono wet result back to the output channels through the dry/wet crossfade.
9. Apply `Level` as a smoothed output gain ramp.

## 44.1 kHz Model Rate

- The trained neural models are fixed to `44,100 Hz`.
- This matters because recurrent models learn sample-to-sample timing at the training rate.
- Running the same weights directly at `48 kHz`, `88.2 kHz`, or `96 kHz` would change the effective time scale of the learned pedal response.
- The plugin therefore treats `44.1 kHz` as the model's internal clock, even when the DAW session uses another sample rate.

When the host is already running at `44.1 kHz`:

- No sample-rate conversion is used.
- The host block is passed directly into RTNeural inference.
- The plugin reports zero added latency from `ML_Engine`.

When the host is running at another sample rate:

- `ML_Engine` computes `hostSampleRate / 44100.0`.
- The wet path is converted from host rate to `44.1 kHz` using JUCE `WindowedSinc` interpolation.
- RTNeural inference runs at the model rate.
- The processed wet signal is converted back to the host rate before dry/wet mixing and output.
- The round-trip interpolation latency is reported to the host.

This conversion is not distortion oversampling. It exists only to keep the neural model operating at the same time scale used during training.

## Real-Time Constraints

- The audio callback does not load model files.
- The callback does not allocate model objects.
- Scratch storage is prepared ahead of time.
- Parameters are read from cached atomics rather than looked up by string inside the processing loop.
- Drive, Level, and Bypass changes are smoothed to avoid zipper noise and switching clicks.
- Recurrent state is preserved across blocks so behaviour is independent of the DAW buffer size.

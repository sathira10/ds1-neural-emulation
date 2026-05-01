import { useState, useEffect } from 'react';
import * as Juce from 'juce-framework-frontend';
import Knob from './components/Knob';
import ModelSelect from './components/ModelSelect';
import BypassControl from './components/BypassControl';
import LedIndicator from './components/LedIndicator';

export default function App() {
  const bypassState = Juce.getSliderState("bypass");
  const [bypassVal, setBypassVal] = useState(bypassState.getNormalisedValue());

  useEffect(() => {
    const id = bypassState.valueChangedEvent.addListener(() =>
      setBypassVal(bypassState.getNormalisedValue())
    );
    return () => bypassState.valueChangedEvent.removeListener(id);
  });

  const toggleBypass = () => {
    const newVal = bypassVal > 0.5 ? 0 : 1;
    bypassState.setNormalisedValue(newVal);
    setBypassVal(newVal);
  };

  return (
    <div className="plugin-container">
      <div className="plugin-image"></div>
      <div className="plugin-ui">

        <div className="top-bar">
          <div className="title-area">
            <span className="plugin-title">DS-1</span>
            <span className="plugin-subtitle">research preview</span>
          </div>
          <BypassControl value={bypassVal} toggle={toggleBypass} />
        </div>

        <div className="main-controls-area">
          <div className="status-indicator">
            <LedIndicator value={bypassVal} />
            <div className="check-label">CHECK</div>
          </div>

          <div className="controls-row">
            <Knob identifier="drive" label="DRIVE" />
            <ModelSelect identifier="model" label="MODEL" />
            <Knob identifier="level" label="LEVEL" />
          </div>
        </div>

        <div className="stomp-area">
          <div className="stomp-screw screw-left"></div>
          <div className="stomp-text">DISTORTION</div>
          <div className="stomp-screw screw-right"></div>
        </div>

      </div>
    </div>
  );
}

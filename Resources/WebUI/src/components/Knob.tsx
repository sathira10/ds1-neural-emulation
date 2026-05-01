import { useState, useEffect } from 'react';
import * as Juce from 'juce-framework-frontend';

interface Props { identifier: string }

export default function Knob({ identifier }: Props) {
  const state = Juce.getSliderState(identifier);
  const [value, setValue] = useState(state.getNormalisedValue());

  useEffect(() => {
    const id = state.valueChangedEvent.addListener(() =>
      setValue(state.getNormalisedValue())
    );
    return () => state.valueChangedEvent.removeListener(id);
  });

  const onChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const v = parseFloat(e.target.value);
    state.setNormalisedValue(v);
    setValue(v);
  };

  return (
    <div className="knob-wrap" data-paramindex={state.properties.parameterIndex}>
      <label>{state.properties.name}</label>
      <input
        type="range"
        min={0} max={1}
        step={1 / (state.properties.numSteps - 1)}
        value={value}
        onMouseDown={() => state.sliderDragStarted()}
        onChange={onChange}
        onMouseUp={() => state.sliderDragEnded()}
      />
      <span>{state.getScaledValue().toFixed(2)} {state.properties.label}</span>
    </div>
  );
}

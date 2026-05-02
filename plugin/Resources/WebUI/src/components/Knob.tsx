import { useState, useEffect } from 'react';
import * as Juce from 'juce-framework-frontend';

interface Props { 
  identifier: string;
  label?: string;
}

export default function Knob({ identifier, label }: Props) {
  const state = Juce.getSliderState(identifier);
  const [value, setValue] = useState(state.getNormalisedValue());

  useEffect(() => {
    const id = state.valueChangedEvent.addListener(() =>
      setValue(state.getNormalisedValue())
    );
    return () => state.valueChangedEvent.removeListener(id);
  });

  const handleMouseDown = (e: React.MouseEvent) => {
    state.sliderDragStarted();
    const startY = e.clientY;
    const startVal = value;

    const handleMouseMove = (moveEvent: MouseEvent) => {
      const deltaY = startY - moveEvent.clientY;
      let newVal = startVal + deltaY * 0.005; // Adjust sensitivity as needed
      newVal = Math.max(0, Math.min(1, newVal));
      state.setNormalisedValue(newVal);
      setValue(newVal);
    };

    const handleMouseUp = () => {
      state.sliderDragEnded();
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };

    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
  };

  // Convert 0..1 to -135deg..135deg
  const rotation = value * 270 - 135;

  const displayLabel = label || (state.properties.name ? state.properties.name.toUpperCase() : identifier.toUpperCase());

  return (
    <div className="knob-wrap" data-paramindex={state.properties.parameterIndex}>
      <div className="knob-container" onMouseDown={handleMouseDown}>
        <div className="knob-base">
          <div className="knob-rotatable" style={{ transform: `rotate(${rotation}deg)` }}>
            <div className="knob-top">
              <div className="knob-indicator" />
            </div>
          </div>
        </div>
      </div>
      <span className="knob-label">{displayLabel}</span>
    </div>
  );
}

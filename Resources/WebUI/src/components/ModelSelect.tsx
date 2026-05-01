import { useState, useEffect } from 'react';
import * as Juce from 'juce-framework-frontend';

interface Props { 
  identifier: string;
  label?: string;
}

export default function ModelSelect({ identifier, label }: Props) {
  const state = Juce.getComboBoxState(identifier);
  const [index, setIndex] = useState(state.getChoiceIndex());

  useEffect(() => {
    const id = state.valueChangedEvent.addListener(() =>
      setIndex(state.getChoiceIndex())
    );
    return () => state.valueChangedEvent.removeListener(id);
  });

  const toggle = () => {
    const newIndex = index === 0 ? 1 : 0;
    state.setChoiceIndex(newIndex);
    setIndex(newIndex);
  };

  const setChoice = (newIndex: number) => {
    state.setChoiceIndex(newIndex);
    setIndex(newIndex);
  };

  const choices = state.properties.choices;
  const choiceTop = choices && choices.length > 0 ? choices[0] : 'LSTM';
  const choiceBottom = choices && choices.length > 1 ? choices[1] : 'GRU';
  
  const displayLabel = label || (state.properties.name ? state.properties.name.toUpperCase() : identifier.toUpperCase());

  return (
    <div className="model-wrap" data-paramindex={state.properties.parameterIndex}>
      <span className="switch-label-top" onClick={() => setChoice(0)}>
        {choiceTop}
      </span>
      <div className="switch-track" onClick={toggle}>
        <div className={`switch-handle pos-${index}`} />
      </div>
      <span className="switch-label-bottom" onClick={() => setChoice(1)}>
        {choiceBottom}
      </span>
      <span className="model-label">{displayLabel}</span>
    </div>
  );
}

import { useState, useEffect } from 'react';
import * as Juce from 'juce-framework-frontend';

interface Props { identifier: string }

export default function ModelSelect({ identifier }: Props) {
  const state = Juce.getComboBoxState(identifier);
  const [index, setIndex] = useState(state.getChoiceIndex());

  useEffect(() => {
    const id = state.valueChangedEvent.addListener(() =>
      setIndex(state.getChoiceIndex())
    );
    return () => state.valueChangedEvent.removeListener(id);
  });

  const onChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
    const i = parseInt(e.target.value);
    state.setChoiceIndex(i);
    setIndex(i);
  };

  return (
    <div className="model-wrap" data-paramindex={state.properties.parameterIndex}>
      <label>{state.properties.name}</label>
      <select value={index} onChange={onChange}>
        {state.properties.choices.map((choice: string, i: number) => (
          <option key={i} value={i}>{choice}</option>
        ))}
      </select>
    </div>
  );
}

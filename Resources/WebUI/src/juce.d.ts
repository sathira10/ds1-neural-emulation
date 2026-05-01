declare module 'juce-framework-frontend' {
  interface Event {
    addListener(cb: () => void): number;
    removeListener(id: number): void;
  }
  interface SliderProperties {
    name: string;
    label: string;
    numSteps: number;
    parameterIndex: number;
  }
  interface SliderState {
    getNormalisedValue(): number;
    setNormalisedValue(v: number): void;
    getScaledValue(): number;
    sliderDragStarted(): void;
    sliderDragEnded(): void;
    properties: SliderProperties;
    valueChangedEvent: Event;
    propertiesChangedEvent: Event;
  }
  interface ComboBoxProperties {
    name: string;
    choices: string[];
    parameterIndex: number;
  }
  interface ComboBoxState {
    getChoiceIndex(): number;
    setChoiceIndex(i: number): void;
    properties: ComboBoxProperties;
    valueChangedEvent: Event;
    propertiesChangedEvent: Event;
  }

  export function getSliderState(name: string): SliderState;
  export function getComboBoxState(name: string): ComboBoxState;
  export function getNativeFunction(name: string): (...args: unknown[]) => Promise<unknown>;
  export function getBackendResourceAddress(path: string): string;
  export class ControlParameterIndexUpdater {
    constructor(annotationKey: string);
    handleMouseMove(event: MouseEvent): void;
  }
}

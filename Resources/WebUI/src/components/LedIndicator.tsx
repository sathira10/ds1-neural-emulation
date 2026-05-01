interface Props { 
  value: number;
}

export default function LedIndicator({ value }: Props) {
  // When bypass is pressed (value > 0.5), the effect is off, so the LED is off
  return <div className={`led-indicator ${value > 0.5 ? 'off' : ''}`}></div>;
}

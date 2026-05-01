interface Props { 
  value: number;
  toggle: () => void;
}

export default function BypassControl({ value, toggle }: Props) {
  return (
    <div className="bypass-wrap">
      <button 
        className={`bypass-button ${value > 0.5 ? 'active' : ''}`}
        onMouseDown={toggle}
      />
      <span className="bypass-label">BYPASS</span>
    </div>
  );
}

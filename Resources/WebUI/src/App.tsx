import Knob from './components/Knob';
import ModelSelect from './components/ModelSelect';

export default function App() {
  return (
    <div className="plugin-container">
      <h1 className="plugin-title">Boss DS-1</h1>
      <div className="controls">
        <Knob identifier="drive" />
        <Knob identifier="level" />
        <ModelSelect identifier="model" />
      </div>
    </div>
  );
}

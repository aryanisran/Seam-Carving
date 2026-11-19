import { useState } from 'react';
import { PRESETS, randomGrid } from '../presets';

/** Preset energy grid shared by the explanation players. */
export function useGridPreset(initial: string) {
  const [id, setId] = useState(initial);
  const [seed, setSeed] = useState(7);
  const preset = PRESETS.find((p) => p.id === id)!;
  const rows = id === 'random' ? randomGrid(5, 6, seed) : preset.rows();
  const picker = (
    <div className="picker">
      <label>
        <span className="sr">Example grid</span>
        <select value={id} onChange={(e) => setId(e.target.value)}>
          {PRESETS.map((p) => <option key={p.id} value={p.id}>{p.label}</option>)}
        </select>
      </label>
      {id === 'random' && <button className="btn" onClick={() => setSeed((s) => s + 1)}>New random</button>}
    </div>
  );
  return { rows, picker, key: `${id}:${seed}` };
}

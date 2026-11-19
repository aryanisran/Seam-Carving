import { useEffect, useMemo, useState } from 'react';
import { energyFromRows } from '../algo/energy';
import { DPEvent, dpVerticalSeam } from '../algo/dp';
import { CellGrid, CellState, ParentArrow, fmt } from '../components/CellGrid';
import { Panel, Pseudocode, StepPlayer } from '../components/StepPlayer';
import { useGridPreset } from '../components/GridPicker';
import { AlgoIntro } from '../components/AlgoIntro';

const CODE = [
  'for c in 0..w-1:  M[0][c] = e[0][c]',
  'for r in 1..h-1:',
  '  for c in 0..w-1:',
  '    best = min(M[r-1][c-1], M[r-1][c], M[r-1][c+1])   # ties: up, up-left, up-right',
  '    M[r][c] = e[r][c] + best;  parent[r][c] = column of best',
  'end = argmin_c M[h-1][c]                             # leftmost on ties',
  'seam[h-1] = end',
  'for r = h-2 down to 0:  seam[r] = parent[r+1][seam[r+1]]',
  'remove pixel (r, seam[r]) from every row r',
];

interface Frame {
  cost: Float64Array;
  choice: Int32Array;
  filled: number; // cells [0, filled) of M are computed
  cur?: [number, number];
  candidates?: number[];
  best?: number;
  seamRows: Map<number, number>;
  end?: number;
  lines: number[];
  text: string;
}

function buildFrames(rows: number[][]): { frames: Frame[]; seam: number[]; total: number } {
  const e = energyFromRows(rows);
  const { w, h } = e;
  const frames: Frame[] = [];
  const empty = new Map<number, number>();
  frames.push({
    cost: new Float64Array(w * h), choice: new Int32Array(w * h).fill(-1), filled: 0, seamRows: empty, lines: [],
    text: `Energy grid e is ${h} x ${w}. M[r][c] will hold the cheapest total energy of any connected seam from the top row down to pixel (r, c).`,
  });
  const res = dpVerticalSeam(e, (ev: DPEvent) => {
    if (ev.kind === 'init') {
      frames.push({ cost: ev.cost, choice: new Int32Array(w * h).fill(-1), filled: w, seamRows: empty, lines: [0],
        text: 'Base case: a seam that ends in the top row is just that pixel, so the first row of M is a copy of the first row of e.' });
    } else if (ev.kind === 'cell') {
      const { r, c, candidates, best } = ev;
      const parts = candidates.map((x) => `M[${r - 1}][${x}] = ${fmt(ev.cost[(r - 1) * w + x])}`).join(', ');
      const dir = best === c ? 'straight up' : best < c ? 'up-left' : 'up-right';
      frames.push({
        cost: ev.cost, choice: ev.choice, filled: r * w + c + 1, cur: [r, c], candidates, best, seamRows: empty, lines: [3, 4],
        text: `M[${r}][${c}] = e[${r}][${c}] + min(${parts}) = ${fmt(rows[r][c])} + ${fmt(ev.cost[(r - 1) * w + best])} = ${fmt(ev.cost[r * w + c])}. Best parent is ${dir} (column ${best}).`,
      });
    } else if (ev.kind === 'pickEnd') {
      frames.push({ cost: ev.cost, choice: ev.choice, filled: w * h, end: ev.c, seamRows: empty, lines: [5],
        text: `Table complete. The smallest value in the last row is M[${h - 1}][${ev.c}] = ${fmt(ev.cost[(h - 1) * w + ev.c])}: that is the cost of the best seam, and it ends at column ${ev.c}.` });
    } else {
      const m = new Map<number, number>();
      ev.seam.forEach((c, r) => { if (c !== undefined) m.set(r, c); });
      frames.push({ cost: ev.cost, choice: ev.choice, filled: w * h, cur: [ev.r, ev.c], seamRows: m, lines: ev.r === h - 1 ? [6] : [7],
        text: ev.r === h - 1
          ? `Backtrack: start at row ${h - 1}, column ${ev.c}.`
          : `Row ${ev.r}: follow parent[${ev.r + 1}][${ev.seam[ev.r + 1]}] = ${ev.c}, so the seam passes column ${ev.c}.` });
    }
  });
  const all = new Map(res.seam.map((c, r) => [r, c] as [number, number]));
  const last = frames[frames.length - 1];
  frames.push({ ...last, cur: undefined, seamRows: all, lines: [8],
    text: `Seam = columns [${res.seam.join(', ')}] with total energy ${fmt(res.energy)}. Removing one pixel per row makes the image 1 column narrower. Each seam costs O(w*h) time.` });
  return { frames, seam: res.seam, total: res.energy };
}

export function DPTab() {
  const { rows, picker, key } = useGridPreset('valley');
  const { frames } = useMemo(() => buildFrames(rows), [key]); // eslint-disable-line react-hooks/exhaustive-deps
  const [i, setI] = useState(0);
  useEffect(() => setI(0), [key]);
  const f = frames[Math.min(i, frames.length - 1)];
  const h = rows.length, w = rows[0].length;

  const filled = (r: number, c: number) => r * w + c < f.filled;
  const eStates = (r: number, c: number): CellState[] => {
    const s: CellState[] = [];
    if (f.seamRows.get(r) === c) s.push('seam');
    if (f.cur && f.cur[0] === r && f.cur[1] === c && f.candidates) s.push('current');
    return s;
  };
  const mStates = (r: number, c: number): CellState[] => {
    const s: CellState[] = [];
    if (!filled(r, c)) s.push('dim');
    if (f.seamRows.get(r) === c) s.push('seam');
    if (f.end === c && r === h - 1) s.push('chosen');
    if (f.cur && f.cur[0] === r && f.cur[1] === c) s.push('current');
    if (f.cur && f.candidates && r === f.cur[0] - 1 && f.candidates.includes(c)) s.push(c === f.best ? 'chosen' : 'compared');
    return s;
  };

  return (
    <div className="tab-body">
      <AlgoIntro
        title="Dynamic Programming"
        summary="Builds a table M where each cell is the cheapest seam ending at that pixel, reusing the answers of the row above. One pass down fills the table, one pass up recovers the seam. Always optimal."
        complexity={[['Time per seam', 'O(w h)'], ['Space', 'O(w h) table, O(w) if only costs are kept'], ['Optimal?', 'Yes']]}
      />
      <StepPlayer
        title="Dynamic Programming - filling the cost table M"
        total={frames.length} index={i} onIndex={setI}
        toolbar={picker}
        input={<code>e = {JSON.stringify(rows)}</code>}
        narration={f.text}
        marks={frames.map((fr, k) => (k === 0 || fr.lines.join() !== frames[k - 1].lines.join() ? k : -1)).filter((k) => k >= 0)}
      >
        <div className="row-panels">
          <Panel title="Energy e">
            <CellGrid rows={h} cols={w} value={(r, c) => fmt(rows[r][c])} states={eStates} caption="Energy grid" />
          </Panel>
          <Panel title="Cost table M (with parent arrows)">
            <CellGrid
              rows={h} cols={w}
              value={(r, c) => (filled(r, c) ? fmt(f.cost[r * w + c]) : '-')}
              states={mStates}
              corner={(r, c) => (r > 0 && filled(r, c) ? <ParentArrow dc={f.choice[r * w + c] - c} /> : null)}
              caption="Cumulative cost table"
            />
          </Panel>
        </div>
        <Panel title="Pseudocode (src/dp_seam.cpp)">
          <Pseudocode lines={CODE} active={f.lines} />
        </Panel>
        <Legend />
      </StepPlayer>

    </div>
  );
}

export function Legend() {
  return (
    <div className="legend" aria-label="Legend">
      <span><i className="sw is-current" /> current cell</span>
      <span><i className="sw is-compared" /> compared</span>
      <span><i className="sw is-chosen" /> chosen</span>
      <span><i className="sw is-seam" /> seam</span>
    </div>
  );
}

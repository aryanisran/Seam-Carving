import { useEffect, useMemo, useState } from 'react';
import { energyFromRows } from '../algo/energy';
import { GreedyEvent, greedyVerticalSeam } from '../algo/greedy';
import { dpVerticalSeam } from '../algo/dp';
import { CellGrid, CellState, fmt } from '../components/CellGrid';
import { Panel, Pseudocode, StepPlayer } from '../components/StepPlayer';
import { useGridPreset } from '../components/GridPicker';
import { AlgoIntro } from '../components/AlgoIntro';

const CODE = [
  'c = argmin_c e[0][c]                      # leftmost on ties',
  'seam[0] = c;  total = e[0][c]',
  'for r in 1..h-1:',
  '  c = cheapest of e[r][c-1], e[r][c], e[r][c+1]   # ties: down, left, right',
  '  seam[r] = c;  total += e[r][c]',
  '# never looks ahead, so total can be worse than the optimum',
];

interface Frame {
  path: number[];
  cur?: [number, number];
  candidates?: number[];
  row0?: boolean;
  optimal?: number[];
  lines: number[];
  sum: number;
  text: string;
}

function buildFrames(rows: number[][]) {
  const e = energyFromRows(rows);
  const frames: Frame[] = [];
  frames.push({ path: [], lines: [], sum: 0,
    text: 'Greedy builds the seam one row at a time, always taking the cheapest pixel it can reach right now.' });
  const res = greedyVerticalSeam(e, (ev: GreedyEvent) => {
    if (ev.kind === 'start') {
      frames.push({ path: [ev.c], row0: true, cur: [0, ev.c], lines: [0, 1], sum: ev.sum,
        text: `Scan the top row: the cheapest pixel is e[0][${ev.c}] = ${fmt(rows[0][ev.c])}. Start there.` });
    } else {
      const opts = ev.candidates.map((x) => `e[${ev.r}][${x}] = ${fmt(rows[ev.r][x])}`).join(', ');
      frames.push({ path: ev.seam, cur: [ev.r, ev.best], candidates: ev.candidates, lines: [3, 4], sum: ev.sum,
        text: `Row ${ev.r}: from column ${ev.from} the reachable pixels are ${opts}. Take column ${ev.best} (${fmt(rows[ev.r][ev.best])}). Running total ${fmt(ev.sum)}.` });
    }
  });
  const dp = dpVerticalSeam(e);
  const last = frames[frames.length - 1];
  const gap = res.energy - dp.energy;
  frames.push({ ...last, cur: undefined, candidates: undefined, optimal: dp.seam, lines: [5],
    text: gap > 1e-9
      ? `Greedy seam costs ${fmt(res.energy)}, but the optimal seam (dashed, found by DP) costs ${fmt(dp.energy)}. The early cheap choice led into expensive pixels: greedy is fast, O(h) per seam after the energy map, but not optimal.`
      : `Here greedy happens to find an optimal seam (cost ${fmt(res.energy)}, same as DP). That is luck, not a guarantee: try the "Greedy trap" grid.` });
  return { frames, greedy: res, dp };
}

export function GreedyTab() {
  const { rows, picker, key } = useGridPreset('trap');
  const { frames } = useMemo(() => buildFrames(rows), [key]); // eslint-disable-line react-hooks/exhaustive-deps
  const [i, setI] = useState(0);
  useEffect(() => setI(0), [key]);
  const f = frames[Math.min(i, frames.length - 1)];
  const h = rows.length, w = rows[0].length;

  const states = (r: number, c: number): CellState[] => {
    const s: CellState[] = [];
    if (f.path[r] === c) s.push('seam');
    if (f.row0 && r === 0) s.push(c === f.path[0] ? 'chosen' : 'compared');
    if (f.candidates && f.cur && r === f.cur[0] && f.candidates.includes(c)) s.push(c === f.cur[1] ? 'chosen' : 'compared');
    if (f.optimal && f.optimal[r] === c) s.push('best');
    return s;
  };

  return (
    <div className="tab-body">
      <AlgoIntro
        title="Greedy"
        summary="Starts at the cheapest pixel in the top row and walks down, each time picking the cheapest of the three pixels below. Very fast, but a locally cheap step can lead into an expensive region."
        complexity={[['Time per seam', 'O(w + h)'], ['Space', 'O(h)'], ['Optimal?', 'No']]}
      />
      <StepPlayer
        title="Greedy - one local decision per row"
        total={frames.length} index={i} onIndex={setI}
        toolbar={picker}
        input={<code>e = {JSON.stringify(rows)}</code>}
        narration={f.text}
      >
        <div className="row-panels">
          <Panel title="Energy e">
            <CellGrid rows={h} cols={w} value={(r, c) => fmt(rows[r][c])} states={states} caption="Energy grid" />
          </Panel>
          <Panel title="Running total">
            <div className="sum-box">
              {f.path.length === 0 ? <span className="muted">empty</span> : (
                <>
                  {f.path.map((c, r) => (
                    <span key={r} className="term">{r > 0 && <b className="op">+</b>}<span className="chip">{fmt(rows[r][c])}</span></span>
                  ))}
                  <b className="op">=</b><span className="chip strong">{fmt(f.sum)}</span>
                </>
              )}
            </div>
            {f.optimal && (
              <div className="sum-box">
                <span className="muted">optimal (DP):</span>
                {f.optimal.map((c, r) => (
                  <span key={r} className="term">{r > 0 && <b className="op">+</b>}<span className="chip">{fmt(rows[r][c])}</span></span>
                ))}
                <b className="op">=</b>
                <span className="chip strong">{fmt(f.optimal.reduce((s, c, r) => s + rows[r][c], 0))}</span>
              </div>
            )}
          </Panel>
        </div>
        <Panel title="Pseudocode (src/greedy_seam.cpp)">
          <Pseudocode lines={CODE} active={f.lines} />
        </Panel>
        <div className="legend">
          <span><i className="sw is-compared" /> reachable options</span>
          <span><i className="sw is-chosen" /> chosen</span>
          <span><i className="sw is-seam" /> greedy seam</span>
          <span><i className="sw is-best" /> optimal seam</span>
        </div>
      </StepPlayer>

    </div>
  );
}

import { useEffect, useMemo, useState } from 'react';
import { energyFromRows } from '../algo/energy';
import { FlowEvent, GridGraph, Solver, buildVerticalGraph, graphCutVerticalSeam, pixelNode } from '../algo/graphcut';
import { fmt } from '../components/CellGrid';
import { Panel, Pseudocode, StepPlayer } from '../components/StepPlayer';
import { useGridPreset } from '../components/GridPicker';
import { FlowNetwork, FlowView, nodeName } from '../components/FlowNetwork';
import { AlgoIntro } from '../components/AlgoIntro';

const CODE_COMMON_HEAD = [
  'S -> every pixel of column 0 (INF);  pixel (r, w-1) -> T  (cap e[r][w-1])',
  '(r,c) -> (r,c+1) cap e[r][c];  back edge INF;  diagonals (r,c) -> (r+-1,c-1) INF',
];
const CODE_DINIC = [
  'while BFS from S in the residual graph reaches T:   # label levels',
  '  push a blocking flow with DFS along level+1 edges',
];
const CODE_EK = [
  'for delta = 512, 256, ... :                          # capacity scaling',
  '  while BFS finds S->T paths with residual >= delta: push bottleneck',
];
const CODE_TAIL = [
  'S_side = BFS from S over edges with residual > 0',
  'seam[r] = rightmost column of row r inside S_side',
  'cut capacity = max flow = energy of the seam          # max-flow min-cut',
];

interface Frame extends FlowView {
  lines: number[];
  text: string;
  total: number;
  phase?: string;
}

function buildFrames(rows: number[][], solver: Solver) {
  const e = energyFromRows(rows);
  const { w, h } = e;
  const frames: Frame[] = [];
  const G0 = buildVerticalGraph(e);
  const zero = G0.g.flow.slice();
  frames.push({ flow: zero, lines: [0], total: 0, dimInf: true,
    text: `One node per pixel (${h * w}) plus a source S and a sink T. S connects to the first column and the last column connects to T. Finite edges carry the energy of the pixel on their left: cutting the edge right of (r, c) means removing pixel (r, c).` });
  frames.push({ flow: zero, lines: [1], total: 0,
    text: 'Each right edge has an infinite edge back, so a finite cut crosses every row exactly once. The dashed infinite diagonals stop the cut jumping more than one column between rows, so it forms a connected seam.' });

  const lineOf = (k: 'run' | 'push') => (k === 'run' ? 2 : 3);
  let ekPhase = '';
  const onEvent = (ev: FlowEvent, G: GridGraph) => {
    if (ev.kind === 'levels') {
      frames.push({ flow: ev.flow, level: ev.level, lines: [lineOf('run')], total: ev.total, phase: `phase ${ev.phase}`,
        text: `Dinic phase ${ev.phase}: BFS from S over edges with spare capacity labels each node with its distance (L0, L1, ...). T is at level ${ev.level[G.sink]}. Only edges going from level k to k+1 may be used in this phase.` });
    } else if (ev.kind === 'push') {
      frames.push({ flow: ev.flow, level: undefined, hot: new Set(ev.changed), lines: [lineOf('push')], total: ev.total, phase: frames.at(-1)?.phase,
        text: `DFS pushed ${fmt(round(ev.amount))} unit(s) of flow along ${ev.paths} path(s) (highlighted). Total flow is now ${fmt(round(ev.total))}. Red edges are saturated: no capacity left.` });
    } else if (ev.kind === 'delta') {
      const sk = ev.skipped.length ? `No path had residual >= ${ev.skipped.map((d) => fmt(d)).join(', ')}, so those phases did nothing. ` : '';
      if (ev.delta < 0) {
        frames.push({ flow: ev.flow, lines: [2], total: ev.total, phase: ekPhase,
          text: `${sk}No augmenting path is left at any delta: the flow of ${fmt(round(ev.total))} is maximum.` });
        return;
      }
      ekPhase = `delta ${fmt(ev.delta)}`;
      frames.push({ flow: ev.flow, lines: [2], total: ev.total, phase: ekPhase,
        text: `${sk}Capacity scaling: now only edges with residual >= ${fmt(ev.delta)} may be used. Big pushes first means far fewer augmenting paths.` });
    } else {
      const nodes = [nodeName(G, G.source), ...ev.path.map((id) => nodeName(G, G.g.to[id]))];
      frames.push({ flow: ev.flow, hot: new Set(ev.path), lines: [3], total: ev.total, phase: ekPhase,
        text: `BFS found the path ${nodes.join(' -> ')}. Its bottleneck is ${fmt(round(ev.amount))}, so push that much. Total flow ${fmt(round(ev.total))}.` });
    }
  };

  // G0 has the same node/edge numbering as the graph the solver builds, so it can name path nodes.
  const res = graphCutVerticalSeam(e, solver, (ev) => onEvent(ev, G0));
  const Gf = res.graph;
  const flow = Gf.g.flow.slice();
  frames.push({ flow, sourceSide: res.inSource, lines: [4], total: res.stats.maxFlow,
    text: `Max flow reached (${fmt(round(res.stats.maxFlow))}). BFS from S using only edges with spare capacity: the reachable pixels (shaded) form the source side of the minimum cut.` });
  const seam = new Map<number, number>();
  for (let r = 0; r < h; r++) {
    seam.set(r, res.seam[r]);
    frames.push({ flow, sourceSide: res.inSource, seam: new Map(seam), lines: [5], total: res.stats.maxFlow,
      text: `Row ${r}: the rightmost shaded pixel is column ${res.seam[r]} (e = ${fmt(rows[r][res.seam[r]])}), so it is the seam pixel for this row.` });
  }
  frames.push({ flow, sourceSide: res.inSource, seam, showCut: true, lines: [6], total: res.stats.maxFlow,
    text: `The cut edges (red) leave the shaded side. Their capacities add up to ${fmt(round(res.energy))} = max flow ${fmt(round(res.stats.maxFlow))}: by the max-flow min-cut theorem this seam is optimal, the same cost DP finds. ${res.stats.augmentingPaths} augmenting path(s), ${res.stats.bfsRuns} BFS run(s).` });
  return { frames, G: Gf, res, code: [...CODE_COMMON_HEAD, ...(solver === 'dinic' ? CODE_DINIC : CODE_EK), ...CODE_TAIL] };
}

const round = (v: number) => Math.round(v * 1000) / 1000;

export function GraphCutTab({ solver, setSolver }: { solver: Solver; setSolver: (s: Solver) => void }) {
  const { rows, picker, key } = useGridPreset('valley');
  const { frames, G, code } = useMemo(() => buildFrames(rows, solver), [key, solver]); // eslint-disable-line react-hooks/exhaustive-deps
  const [i, setI] = useState(0);
  useEffect(() => setI(0), [key, solver]);
  const f = frames[Math.min(i, frames.length - 1)];

  const toolbar = (
    <>
      {picker}
      <div className="speeds" role="group" aria-label="Max-flow solver">
        {(['dinic', 'edmonds_karp'] as Solver[]).map((s) => (
          <button key={s} className={'btn seg' + (s === solver ? ' on' : '')} onClick={() => setSolver(s)} aria-pressed={s === solver}>
            {s === 'dinic' ? 'Dinic' : 'Edmonds-Karp'}
          </button>
        ))}
      </div>
    </>
  );

  return (
    <div className="tab-body">
      <AlgoIntro
        title="Graph Cut (min-cut / max-flow)"
        summary="Turns seam finding into a minimum S-T cut. Infinite edges force the cut to be a connected seam with one pixel per row, so the cheapest cut is the cheapest seam. Max-flow finds it."
        complexity={[['Dinic', 'O(V^2 E)'], ['Edmonds-Karp', 'O(V E^2)'], ['Graph size', 'V = w h + 2, E = O(w h)'], ['Optimal?', 'Yes']]}
      />
      <StepPlayer
        title={`Graph Cut - ${solver === 'dinic' ? 'Dinic' : 'Edmonds-Karp (capacity scaling)'}`}
        total={frames.length} index={i} onIndex={setI}
        toolbar={toolbar}
        input={<code>e = {JSON.stringify(rows)}</code>}
        narration={f.text}
      >
        <Panel title="Flow network" aside={<span className="stat">flow <b>{fmt(round(f.total))}</b>{f.phase ? <> | {f.phase}</> : null}</span>}>
          <FlowNetwork G={G} energy={rows} view={f} />
        </Panel>
        <Panel title="Pseudocode (src/graph_cut_seam.cpp, src/maxflow.cpp)">
          <Pseudocode lines={code} active={f.lines} />
        </Panel>
        <div className="legend">
          <span><i className="sw line fin" /> finite edge flow/capacity</span>
          <span><i className="sw line inf" /> infinite edge</span>
          <span><i className="sw line hot" /> path / changed</span>
          <span><i className="sw line sat" /> saturated or cut</span>
          <span><i className="sw is-source" /> source side</span>
          <span><i className="sw is-seam" /> seam</span>
        </div>
      </StepPlayer>

    </div>
  );
}

export { pixelNode };

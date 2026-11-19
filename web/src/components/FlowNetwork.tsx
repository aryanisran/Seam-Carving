// SVG drawing of the seam-carving flow network for a small grid.
import { FLOW_EPS, GridGraph, pixelNode } from '../algo/graphcut';
import { fmt } from './CellGrid';

export interface FlowView {
  flow: number[];
  /** edge ids (either direction) to emphasise, e.g. the augmenting path */
  hot?: Set<number>;
  level?: Int32Array;
  sourceSide?: Uint8Array;
  seam?: Map<number, number>;
  showCut?: boolean;
  dimInf?: boolean;
}

const DX = 92, DY = 70, PADX = 78, PADY = 46, R = 16;

export function FlowNetwork({ G, energy, view }: { G: GridGraph; energy: number[][]; view: FlowView }) {
  const { w, h, g } = G;
  const pos = (node: number): [number, number] => {
    if (node === G.source) return [22, PADY + ((h - 1) * DY) / 2];
    if (node === G.sink) return [PADX + (w - 1) * DX + 64, PADY + ((h - 1) * DY) / 2];
    const k = node - 1;
    return [PADX + (k % w) * DX, PADY + Math.floor(k / w) * DY];
  };
  const W = PADX + (w - 1) * DX + 90;
  const H = PADY + (h - 1) * DY + 40;
  const inS = view.sourceSide;
  const isCut = (u: number, v: number) => !!inS && view.showCut && inS[u] === 1 && inS[v] === 0;

  return (
    <div className="svg-wrap">
      <svg viewBox={`0 0 ${W} ${H}`} width={W} height={H} className="flow" role="img" aria-label="Flow network">
        <defs>
          {['norm', 'hot', 'sat', 'faint'].map((k) => (
            <marker key={k} id={'ah-' + k} viewBox="0 0 10 10" refX="9" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
              <path d="M0 0 L10 5 L0 10 z" className={'ah ' + k} />
            </marker>
          ))}
        </defs>

        {G.edges.map((e) => {
          const [x1, y1] = pos(e.u);
          const [x2, y2] = pos(e.v);
          const len = Math.hypot(x2 - x1, y2 - y1);
          const ux = (x2 - x1) / len, uy = (y2 - y1) / len;
          const sx = x1 + ux * R, sy = y1 + uy * R, ex = x2 - ux * (R + 2), ey = y2 - uy * (R + 2);
          const finite = e.kind === 'right' || e.kind === 'sink';
          const hot = view.hot?.has(e.id) || view.hot?.has(e.id ^ 1);
          const sat = finite && g.cap[e.id] - view.flow[e.id] <= FLOW_EPS;
          const cut = finite && isCut(e.u, e.v);
          let cls = finite ? 'edge fin' : 'edge inf';
          let mk = finite ? 'norm' : 'faint';
          if (sat) { cls += ' sat'; mk = 'sat'; }
          if (view.showCut && !cut && finite) cls += ' dim';
          if (cut) { cls += ' cut'; mk = 'sat'; }
          if (hot) { cls += ' hot'; mk = 'hot'; }
          if (!finite && view.dimInf && !hot) cls += ' dim';
          const mx = (sx + ex) / 2, my = (sy + ey) / 2;
          return (
            <g key={e.id}>
              <line x1={sx} y1={sy} x2={ex} y2={ey} className={cls} markerEnd={`url(#ah-${mk})`} />
              {finite && (
                <text x={mx} y={e.kind === 'sink' ? my - 7 : my - 6} className={'elabel' + (sat ? ' sat' : '') + (hot ? ' hot' : '')} textAnchor="middle">
                  {fmt(round(view.flow[e.id]))}/{fmt(g.cap[e.id])}
                </text>
              )}
            </g>
          );
        })}

        {Array.from({ length: g.n }, (_, node) => {
          const [x, y] = pos(node);
          const isTerm = node === G.source || node === G.sink;
          const r = Math.floor((node - 1) / w), c = (node - 1) % w;
          const seam = !isTerm && view.seam?.get(r) === c;
          let cls = 'node';
          if (isTerm) cls += ' term';
          if (inS?.[node]) cls += ' s-side';
          if (seam) cls += ' seam';
          const lv = view.level?.[node];
          return (
            <g key={node}>
              <circle cx={x} cy={y} r={isTerm ? 18 : R} className={cls} />
              <text x={x} y={y + 4} textAnchor="middle" className="nlabel">
                {node === G.source ? 'S' : node === G.sink ? 'T' : fmt(energy[r][c])}
              </text>
              {lv !== undefined && lv >= 0 && (
                <text x={x + (isTerm ? 0 : 14)} y={y - (isTerm ? 24 : 18)} textAnchor="middle" className="level">L{lv}</text>
              )}
            </g>
          );
        })}
        {Array.from({ length: h }, (_, r) => (
          <text key={'r' + r} x={PADX - 30} y={PADY + r * DY - 18} className="rlabel">r{r}</text>
        ))}
      </svg>
    </div>
  );
}

const round = (v: number) => Math.round(v * 1000) / 1000;

export function nodeName(G: GridGraph, node: number) {
  if (node === G.source) return 'S';
  if (node === G.sink) return 'T';
  return `(${Math.floor((node - 1) / G.w)},${(node - 1) % G.w})`;
}

export { pixelNode };

// Graph-cut seam finder, ported from src/graph.cpp, src/graph_cut_seam.cpp and src/maxflow.cpp.
// Same graph construction, same Dinic and capacity-scaling Edmonds-Karp, same residual BFS.
import { EnergyMap, at } from './energy';
import { SeamResult } from './dp';

export const INF_CAP = 1e15;
export const FLOW_EPS = 1e-9;

/** Flow network with paired edges: edge id ^ 1 is its reverse (FlowGraph::add_edge). */
export class FlowGraph {
  to: number[] = [];
  cap: number[] = [];
  flow: number[] = [];
  adj: number[][];
  constructor(public n: number) {
    this.adj = Array.from({ length: n }, () => []);
  }
  addEdge(u: number, v: number, cap: number, revCap = 0): number {
    const id = this.to.length;
    this.to.push(v, u);
    this.cap.push(cap, revCap);
    this.flow.push(0, 0);
    this.adj[u].push(id);
    this.adj[v].push(id + 1);
    return id;
  }
  residual(id: number) {
    return this.cap[id] - this.flow[id];
  }
}

export type EdgeKind = 'source' | 'sink' | 'right' | 'diag';
export interface GridGraph {
  g: FlowGraph;
  w: number;
  h: number;
  source: number;
  sink: number;
  /** forward edges only (the ids add_edge returned), for drawing */
  edges: { id: number; u: number; v: number; kind: EdgeKind }[];
}

export const pixelNode = (r: number, c: number, w: number) => 1 + r * w + c;

export function buildVerticalGraph(e: EnergyMap): GridGraph {
  const { w, h } = e;
  const source = 0;
  const sink = h * w + 1;
  const g = new FlowGraph(h * w + 2);
  const edges: GridGraph['edges'] = [];
  const add = (u: number, v: number, cap: number, rev: number, kind: EdgeKind) =>
    edges.push({ id: g.addEdge(u, v, cap, rev), u, v, kind });

  for (let r = 0; r < h; r++) {
    add(source, pixelNode(r, 0, w), INF_CAP, 0, 'source');
    add(pixelNode(r, w - 1, w), sink, at(e, r, w - 1), 0, 'sink');
    for (let c = 0; c < w - 1; c++) add(pixelNode(r, c, w), pixelNode(r, c + 1, w), at(e, r, c), INF_CAP, 'right');
    for (let c = 1; c < w; c++) {
      const u = pixelNode(r, c, w);
      if (r > 0) add(u, pixelNode(r - 1, c - 1, w), INF_CAP, 0, 'diag');
      if (r < h - 1) add(u, pixelNode(r + 1, c - 1, w), INF_CAP, 0, 'diag');
    }
  }
  return { g, w, h, source, sink, edges };
}

export type Solver = 'dinic' | 'edmonds_karp';

export type FlowEvent =
  | { kind: 'levels'; phase: number; level: Int32Array; flow: number[]; total: number }
  | { kind: 'push'; phase: number; amount: number; changed: number[]; flow: number[]; total: number; paths: number }
  | { kind: 'delta'; delta: number; flow: number[]; total: number; skipped: number[] }
  | { kind: 'augment'; delta: number; path: number[]; amount: number; flow: number[]; total: number };

export interface FlowStats {
  maxFlow: number;
  augmentingPaths: number;
  bfsRuns: number;
}

// ---------------------------------------------------------------- Edmonds-Karp
// Capacity scaling (delta starts at 512) and "wavefront" augmentation exactly as in maxflow.cpp.
export function edmondsKarp(G: GridGraph, trace?: (ev: FlowEvent) => void): FlowStats {
  const { g, source, sink } = G;
  const stats: FlowStats = { maxFlow: 0, augmentingPaths: 0, bfsRuns: 0 };
  const pv = new Int32Array(g.n).fill(-1);
  const pe = new Int32Array(g.n).fill(-1);
  const q = new Int32Array(g.n);
  let delta = 512.0;
  let skipped: number[] = [];

  while (delta > FLOW_EPS / 2.0) {
    const pathsAtPhaseStart = stats.augmentingPaths;
    let announced = false;
    const announce = () => {
      if (trace && !announced) {
        trace({ kind: 'delta', delta, flow: g.flow.slice(), total: stats.maxFlow, skipped });
        skipped = [];
        announced = true;
      }
    };
    while (true) {
      let head = 0, tail = 0;
      q[tail++] = source;
      pv[source] = source;
      stats.bfsRuns++;
      const pathsBefore = stats.augmentingPaths;

      while (head < tail) {
        const u = q[head++];
        const edges = g.adj[u];
        for (let i = 0; i < edges.length; i++) {
          const id = edges[i];
          if (g.residual(id) < delta) continue;
          const v = g.to[id];
          if (v === sink) {
            pv[sink] = u;
            pe[sink] = id;
            let pushed = INF_CAP;
            for (let x = sink; x !== source; x = pv[x]) pushed = Math.min(pushed, g.residual(pe[x]));
            if (pushed >= delta) {
              const path: number[] = [];
              for (let x = sink; x !== source; x = pv[x]) {
                const fid = pe[x];
                g.flow[fid] += pushed;
                g.flow[fid ^ 1] -= pushed;
                path.push(fid);
              }
              stats.maxFlow += pushed;
              stats.augmentingPaths++;
              if (trace) {
                announce();
                trace({ kind: 'augment', delta, path: path.reverse(), amount: pushed, flow: g.flow.slice(), total: stats.maxFlow });
              }
            }
            pv[sink] = -1;
            continue;
          }
          if (pv[v] === -1) {
            pv[v] = u;
            pe[v] = id;
            q[tail++] = v;
          }
        }
      }
      for (let i = 0; i < tail; i++) pv[q[i]] = -1;
      if (stats.augmentingPaths === pathsBefore) break;
    }
    if (trace && stats.augmentingPaths === pathsAtPhaseStart) skipped.push(delta);

    if (delta > FLOW_EPS && delta / 2.0 <= FLOW_EPS) delta = FLOW_EPS;
    else if (delta <= FLOW_EPS) break;
    else delta /= 2.0;
  }
  if (trace && skipped.length) trace({ kind: 'delta', delta: -1, flow: g.flow.slice(), total: stats.maxFlow, skipped });
  return stats;
}

// ---------------------------------------------------------------- Dinic
function dinicBfs(g: FlowGraph, source: number, sink: number, level: Int32Array): boolean {
  level.fill(-1);
  level[source] = 0;
  const q = new Int32Array(g.n);
  let head = 0, tail = 0;
  q[tail++] = source;
  while (head < tail) {
    const u = q[head++];
    if (level[sink] !== -1 && level[u] >= level[sink]) break;
    for (const id of g.adj[u]) {
      const v = g.to[id];
      if (level[v] === -1 && g.residual(id) > FLOW_EPS) {
        level[v] = level[u] + 1;
        q[tail++] = v;
      }
    }
  }
  return level[sink] !== -1;
}

function dinicDfs(
  g: FlowGraph, u: number, sink: number, pushed: number,
  level: Int32Array, ptr: Int32Array, stats: FlowStats,
): number {
  if (pushed <= FLOW_EPS) return 0;
  if (u === sink) {
    stats.augmentingPaths++;
    return pushed;
  }
  let trTotal = 0;
  const edges = g.adj[u];
  for (; ptr[u] < edges.length; ptr[u]++) {
    const id = edges[ptr[u]];
    const v = g.to[id];
    if (level[v] !== level[u] + 1 || g.residual(id) <= FLOW_EPS) continue;
    const tr = dinicDfs(g, v, sink, Math.min(pushed - trTotal, g.residual(id)), level, ptr, stats);
    if (tr > FLOW_EPS) {
      g.flow[id] += tr;
      g.flow[id ^ 1] -= tr;
      trTotal += tr;
      if (pushed - trTotal <= FLOW_EPS) break;
    }
  }
  if (trTotal < pushed) level[u] = -1; // dead end: prune from this level graph
  return trTotal;
}

export function dinic(G: GridGraph, trace?: (ev: FlowEvent) => void): FlowStats {
  const { g, source, sink } = G;
  const stats: FlowStats = { maxFlow: 0, augmentingPaths: 0, bfsRuns: 0 };
  const level = new Int32Array(g.n);
  const ptr = new Int32Array(g.n);
  let phase = 0;
  while (dinicBfs(g, source, sink, level)) {
    stats.bfsRuns++;
    phase++;
    trace?.({ kind: 'levels', phase, level: level.slice(), flow: g.flow.slice(), total: stats.maxFlow });
    ptr.fill(0);
    while (true) {
      const before = trace ? g.flow.slice() : null;
      const pathsBefore = stats.augmentingPaths;
      const pushed = dinicDfs(g, source, sink, INF_CAP, level, ptr, stats);
      if (pushed <= FLOW_EPS) break;
      stats.maxFlow += pushed;
      if (trace && before) {
        const changed: number[] = [];
        for (let i = 0; i < g.flow.length; i += 2) if (g.flow[i] !== before[i] || g.flow[i + 1] !== before[i + 1]) changed.push(i);
        trace({ kind: 'push', phase, amount: pushed, changed, flow: g.flow.slice(), total: stats.maxFlow, paths: stats.augmentingPaths - pathsBefore });
      }
    }
  }
  return stats;
}

/** Vertices reachable from the source in the residual graph (MaxFlowSolver::compute_source_cut). */
export function sourceSide(g: FlowGraph, source: number): Uint8Array {
  const vis = new Uint8Array(g.n);
  const q = new Int32Array(g.n);
  let head = 0, tail = 0;
  q[tail++] = source;
  vis[source] = 1;
  while (head < tail) {
    const u = q[head++];
    for (const id of g.adj[u]) {
      const v = g.to[id];
      if (!vis[v] && g.residual(id) > FLOW_EPS) {
        vis[v] = 1;
        q[tail++] = v;
      }
    }
  }
  return vis;
}

export interface GraphCutResult extends SeamResult {
  stats: FlowStats;
  inSource: Uint8Array;
  graph: GridGraph;
}

export function graphCutVerticalSeam(e: EnergyMap, solver: Solver = 'dinic', trace?: (ev: FlowEvent) => void): GraphCutResult {
  const G = buildVerticalGraph(e);
  const stats = solver === 'dinic' ? dinic(G, trace) : edmondsKarp(G, trace);
  const inSource = sourceSide(G.g, G.source);
  const seam = new Array<number>(e.h).fill(0);
  let energy = 0;
  for (let r = 0; r < e.h; r++) {
    let right = 0;
    for (let c = e.w - 1; c >= 0; c--)
      if (inSource[pixelNode(r, c, e.w)]) { right = c; break; }
    seam[r] = right;
    energy += at(e, r, right);
  }
  return { seam, energy, stats, inSource, graph: G };
}

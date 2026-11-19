// Greedy seam finder, ported from src/greedy_seam.cpp: start at the cheapest pixel of
// the first row (leftmost on ties), then each row take the cheapest of the three pixels
// below (ties: straight down, then left, then right).
import { EnergyMap, at } from './energy';
import { SeamResult } from './dp';

export type GreedyEvent =
  | { kind: 'start'; c: number; sum: number }
  | { kind: 'step'; r: number; from: number; candidates: number[]; best: number; seam: number[]; sum: number };

export function greedyVerticalSeam(e: EnergyMap, trace?: (ev: GreedyEvent) => void): SeamResult {
  const { w, h } = e;
  let start = 0;
  for (let c = 1; c < w; c++) if (at(e, 0, c) < at(e, 0, start)) start = c;

  const seam = new Array<number>(h);
  let c = start;
  seam[0] = c;
  let sum = at(e, 0, c);
  trace?.({ kind: 'start', c, sum });

  for (let r = 1; r < h; r++) {
    const from = c;
    let bestC = c;
    let bestE = at(e, r, c);
    if (c > 0 && at(e, r, c - 1) < bestE) { bestC = c - 1; bestE = at(e, r, c - 1); }
    if (c < w - 1 && at(e, r, c + 1) < bestE) { bestC = c + 1; bestE = at(e, r, c + 1); }
    c = bestC;
    seam[r] = c;
    sum += bestE;
    if (trace) {
      const cand = [from - 1, from, from + 1].filter((x) => x >= 0 && x < w);
      trace({ kind: 'step', r, from, candidates: cand, best: c, seam: seam.slice(0, r + 1), sum });
    }
  }
  return { seam, energy: sum };
}

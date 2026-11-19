// Dynamic programming seam finder, ported from src/dp_seam.cpp (same tie-breaking:
// straight down first, then left, then right; leftmost minimum in the last row).
import { EnergyMap, at } from './energy';

export interface SeamResult {
  seam: number[];
  energy: number;
}

export type DPEvent =
  | { kind: 'init'; cost: Float64Array }
  | { kind: 'cell'; r: number; c: number; candidates: number[]; best: number; cost: Float64Array; choice: Int32Array }
  | { kind: 'pickEnd'; c: number; cost: Float64Array; choice: Int32Array }
  | { kind: 'back'; r: number; c: number; seam: number[]; cost: Float64Array; choice: Int32Array };

export function dpVerticalSeam(e: EnergyMap, trace?: (ev: DPEvent) => void): SeamResult {
  const { w, h } = e;
  const cost = new Float64Array(w * h);
  const choice = new Int32Array(w * h).fill(-1);
  for (let c = 0; c < w; c++) cost[c] = at(e, 0, c);
  trace?.({ kind: 'init', cost: cost.slice() });

  for (let r = 1; r < h; r++) {
    const prev = (r - 1) * w;
    for (let c = 0; c < w; c++) {
      let best = cost[prev + c];
      let bestC = c;
      if (c > 0 && cost[prev + c - 1] < best) { best = cost[prev + c - 1]; bestC = c - 1; }
      if (c < w - 1 && cost[prev + c + 1] < best) { best = cost[prev + c + 1]; bestC = c + 1; }
      cost[r * w + c] = at(e, r, c) + best;
      choice[r * w + c] = bestC;
      if (trace) {
        const cand = [c - 1, c, c + 1].filter((x) => x >= 0 && x < w);
        trace({ kind: 'cell', r, c, candidates: cand, best: bestC, cost: cost.slice(), choice: choice.slice() });
      }
    }
  }

  const last = (h - 1) * w;
  let bestC = 0;
  let bestCost = cost[last];
  for (let c = 1; c < w; c++) if (cost[last + c] < bestCost) { bestCost = cost[last + c]; bestC = c; }
  trace?.({ kind: 'pickEnd', c: bestC, cost: cost.slice(), choice: choice.slice() });

  const seam = new Array<number>(h);
  seam[h - 1] = bestC;
  if (trace) trace({ kind: 'back', r: h - 1, c: bestC, seam: seam.slice(), cost, choice });
  for (let r = h - 2; r >= 0; r--) {
    bestC = choice[(r + 1) * w + bestC];
    seam[r] = bestC;
    if (trace) trace({ kind: 'back', r, c: bestC, seam: seam.slice(), cost, choice });
  }
  return { seam, energy: bestCost };
}

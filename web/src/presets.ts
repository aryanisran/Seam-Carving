// Small energy grids for the step-by-step explanations.

/** Counterexample from Avik Das (avikdas.com, 2019): greedy pays 19, the optimal seam 8. */
export const GREEDY_TRAP = [
  [9, 9, 0, 9, 9],
  [9, 1, 9, 8, 9],
  [9, 9, 9, 9, 0],
  [9, 9, 9, 0, 9],
];

/** A hand-made grid with a cheap valley that wanders, good for DP and graph cut. */
export const VALLEY = [
  [7, 2, 8, 6, 9, 5],
  [6, 9, 3, 7, 4, 8],
  [8, 4, 9, 2, 7, 6],
  [5, 8, 6, 9, 3, 7],
  [9, 3, 7, 8, 5, 4],
];

export function randomGrid(h: number, w: number, seed: number): number[][] {
  let s = seed >>> 0 || 1;
  const rand = () => ((s = (s * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
  return Array.from({ length: h }, () => Array.from({ length: w }, () => 1 + Math.floor(rand() * 9)));
}

export interface Preset {
  id: string;
  label: string;
  rows: () => number[][];
}

export const PRESETS: Preset[] = [
  { id: 'valley', label: 'Winding valley (5 x 6)', rows: () => VALLEY },
  { id: 'trap', label: 'Greedy trap (4 x 5)', rows: () => GREEDY_TRAP },
  { id: 'random', label: 'Random (5 x 6)', rows: () => randomGrid(5, 6, Date.now()) },
];

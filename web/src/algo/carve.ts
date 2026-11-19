// Full carving loop, mirroring SeamCarver: all vertical seams first, then horizontal seams.
// Energy is recomputed on the current image before every seam.
import { RGBImage, removeHorizontalSeam, removeVerticalSeam } from './image';
import { EnergyMap, EnergyMetric, computeEnergy, transposeEnergy } from './energy';
import { SeamResult, dpVerticalSeam } from './dp';
import { greedyVerticalSeam } from './greedy';
import { FlowStats, Solver, graphCutVerticalSeam } from './graphcut';

export type Algorithm = 'dp' | 'greedy' | 'graph_cut';

export const ALGO_LABEL: Record<Algorithm, string> = {
  dp: 'Dynamic Programming',
  greedy: 'Greedy',
  graph_cut: 'Graph Cut',
};

export interface SeamRecord {
  vertical: boolean;
  seam: number[]; // vertical: seam[r] = column, horizontal: seam[c] = row
  energy: number;
  ms: number;
  wBefore: number;
  hBefore: number;
  flow?: FlowStats;
}

export interface CarveOptions {
  algorithm: Algorithm;
  metric: EnergyMetric;
  solver: Solver;
}

export function findVerticalSeam(e: EnergyMap, opt: CarveOptions): SeamResult & { flow?: FlowStats } {
  if (opt.algorithm === 'dp') return dpVerticalSeam(e);
  if (opt.algorithm === 'greedy') return greedyVerticalSeam(e);
  const r = graphCutVerticalSeam(e, opt.solver);
  return { seam: r.seam, energy: r.energy, flow: r.stats };
}

export function carveStep(img: RGBImage, vertical: boolean, opt: CarveOptions): { next: RGBImage; rec: SeamRecord } {
  const t0 = performance.now();
  let e = computeEnergy(img, opt.metric);
  if (!vertical) e = transposeEnergy(e);
  const res = findVerticalSeam(e, opt);
  const ms = performance.now() - t0;
  const next = vertical ? removeVerticalSeam(img, res.seam) : removeHorizontalSeam(img, res.seam);
  return { next, rec: { vertical, seam: res.seam, energy: res.energy, ms, wBefore: img.w, hBefore: img.h, flow: res.flow } };
}

export function carveSync(img: RGBImage, cols: number, rows: number, opt: CarveOptions) {
  const records: SeamRecord[] = [];
  const images: RGBImage[] = [img];
  let cur = img;
  for (let i = 0; i < cols + rows; i++) {
    const { next, rec } = carveStep(cur, i < cols, opt);
    records.push(rec);
    images.push(next);
    cur = next;
  }
  return { records, images };
}

/** Same as carveSync but yields to the browser between seams so the UI stays responsive. */
export async function carveAsync(
  img: RGBImage, cols: number, rows: number, opt: CarveOptions,
  onProgress: (done: number, total: number) => void, cancelled: () => boolean,
) {
  const records: SeamRecord[] = [];
  const images: RGBImage[] = [img];
  let cur = img;
  let last = performance.now();
  for (let i = 0; i < cols + rows; i++) {
    if (cancelled()) return null;
    const { next, rec } = carveStep(cur, i < cols, opt);
    records.push(rec);
    images.push(next);
    cur = next;
    if (performance.now() - last > 30) {
      onProgress(i + 1, cols + rows);
      await new Promise((res) => setTimeout(res, 0));
      last = performance.now();
    }
  }
  onProgress(cols + rows, cols + rows);
  return { records, images };
}

/**
 * Map every removed seam back to coordinates in the original image by tracking, for each
 * remaining pixel, which original pixel it came from.
 */
export function seamsInOriginal(w: number, h: number, records: SeamRecord[]): Int32Array[] {
  let cw = w, ch = h;
  let origin = new Int32Array(w * h).map((_, i) => i);
  const out: Int32Array[] = [];
  for (const rec of records) {
    if (rec.vertical) {
      const hit = new Int32Array(ch);
      const nxt = new Int32Array((cw - 1) * ch);
      for (let r = 0; r < ch; r++) {
        const s = rec.seam[r];
        hit[r] = origin[r * cw + s];
        let k = r * (cw - 1);
        for (let c = 0; c < cw; c++) if (c !== s) nxt[k++] = origin[r * cw + c];
      }
      out.push(hit);
      origin = nxt;
      cw--;
    } else {
      const hit = new Int32Array(cw);
      const nxt = new Int32Array(cw * (ch - 1));
      for (let c = 0; c < cw; c++) {
        const s = rec.seam[c];
        hit[c] = origin[s * cw + c];
        let k = 0;
        for (let r = 0; r < ch; r++) if (r !== s) nxt[k++ * cw + c] = origin[r * cw + c];
      }
      out.push(hit);
      origin = nxt;
      ch--;
    }
  }
  return out;
}

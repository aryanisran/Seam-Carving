// Energy map. Same formulas and operation order as src/energy.cpp so results are bit-identical.
import { RGBImage, px } from './image';

export type EnergyMetric = 'dual' | 'sobel';

export interface EnergyMap {
  w: number;
  h: number;
  data: Float64Array; // row-major
}

export const at = (e: EnergyMap, r: number, c: number) => e.data[r * e.w + c];

export function dualGradientAt(img: RGBImage, r: number, c: number): number {
  const d = img.data;
  const L = px(img, r, c - 1), R = px(img, r, c + 1), U = px(img, r - 1, c), D = px(img, r + 1, c);
  const rx = d[R] - d[L], gx = d[R + 1] - d[L + 1], bx = d[R + 2] - d[L + 2];
  const ry = d[D] - d[U], gy = d[D + 1] - d[U + 1], by = d[D + 2] - d[U + 2];
  const dx2 = rx * rx + gx * gx + bx * bx;
  const dy2 = ry * ry + gy * gy + by * by;
  return Math.sqrt(dx2 + dy2);
}

function lum(img: RGBImage, r: number, c: number): number {
  const i = px(img, r, c);
  return 0.299 * img.data[i] + 0.587 * img.data[i + 1] + 0.114 * img.data[i + 2];
}

export function sobelAt(img: RGBImage, r: number, c: number): number {
  const tl = lum(img, r - 1, c - 1), tc = lum(img, r - 1, c), tr = lum(img, r - 1, c + 1);
  const ml = lum(img, r, c - 1), mr = lum(img, r, c + 1);
  const bl = lum(img, r + 1, c - 1), bc = lum(img, r + 1, c), br = lum(img, r + 1, c + 1);
  const gx = -tl + tr - 2.0 * ml + 2.0 * mr - bl + br;
  const gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;
  return Math.sqrt(gx * gx + gy * gy);
}

export function computeEnergy(img: RGBImage, metric: EnergyMetric = 'dual'): EnergyMap {
  const data = new Float64Array(img.w * img.h);
  const f = metric === 'sobel' ? sobelAt : dualGradientAt;
  for (let r = 0; r < img.h; r++) for (let c = 0; c < img.w; c++) data[r * img.w + c] = f(img, r, c);
  return { w: img.w, h: img.h, data };
}

export function transposeEnergy(e: EnergyMap): EnergyMap {
  const data = new Float64Array(e.w * e.h);
  for (let r = 0; r < e.h; r++) for (let c = 0; c < e.w; c++) data[c * e.h + r] = e.data[r * e.w + c];
  return { w: e.h, h: e.w, data };
}

export function energyFromRows(rows: number[][]): EnergyMap {
  return { h: rows.length, w: rows[0].length, data: Float64Array.from(rows.flat()) };
}

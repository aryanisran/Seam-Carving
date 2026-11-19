import { RGBImage, toRGBA } from '../algo/image';
import { EnergyMap } from '../algo/energy';

export const VERT = [213, 94, 0]; // Okabe-Ito vermillion
export const HORZ = [86, 180, 233]; // Okabe-Ito sky blue
export const CANVAS_BG = '#141417';

/** Draw an RGB image at integer zoom (pixel-sharp) into a canvas of a fixed size. */
export function drawImage(cv: HTMLCanvasElement, img: RGBImage, zoom: number, boxW: number, boxH: number) {
  cv.width = boxW * zoom;
  cv.height = boxH * zoom;
  const ctx = cv.getContext('2d')!;
  ctx.fillStyle = CANVAS_BG;
  ctx.fillRect(0, 0, cv.width, cv.height);
  const off = document.createElement('canvas');
  off.width = img.w;
  off.height = img.h;
  off.getContext('2d')!.putImageData(new ImageData(toRGBA(img), img.w, img.h), 0, 0);
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(off, 0, 0, img.w * zoom, img.h * zoom);
  return ctx;
}

export function energyToImage(e: EnergyMap): RGBImage {
  let max = 1e-9;
  for (const v of e.data) max = Math.max(max, v);
  const data = new Uint8ClampedArray(e.w * e.h * 3);
  for (let i = 0; i < e.w * e.h; i++) {
    const t = Math.sqrt(e.data[i] / max); // sqrt so faint edges stay visible
    data[i * 3] = data[i * 3 + 1] = data[i * 3 + 2] = Math.round(t * 255);
  }
  return { w: e.w, h: e.h, data };
}

export function fillPixel(ctx: CanvasRenderingContext2D, r: number, c: number, zoom: number, rgb: number[], alpha = 1) {
  ctx.fillStyle = `rgba(${rgb[0]},${rgb[1]},${rgb[2]},${alpha})`;
  ctx.fillRect(c * zoom, r * zoom, zoom, zoom);
}

export function zoomFor(w: number, h: number, maxW: number, maxH = 360) {
  return Math.max(1, Math.min(8, Math.floor(Math.min(maxW / w, maxH / h))));
}

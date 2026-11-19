// RGB image helpers. Mirrors include/image.hpp: out-of-range reads clamp to the border.

export interface RGBImage {
  w: number;
  h: number;
  data: Uint8ClampedArray; // RGB, 3 bytes per pixel, row-major
}

export function makeImage(w: number, h: number, data?: Uint8ClampedArray): RGBImage {
  return { w, h, data: data ?? new Uint8ClampedArray(w * h * 3) };
}

export function fromRGBA(w: number, h: number, rgba: ArrayLike<number>): RGBImage {
  const out = makeImage(w, h);
  for (let i = 0, j = 0; i < w * h; i++, j += 4) {
    out.data[i * 3] = rgba[j];
    out.data[i * 3 + 1] = rgba[j + 1];
    out.data[i * 3 + 2] = rgba[j + 2];
  }
  return out;
}

export function toRGBA(img: RGBImage): Uint8ClampedArray {
  const out = new Uint8ClampedArray(img.w * img.h * 4);
  for (let i = 0; i < img.w * img.h; i++) {
    out[i * 4] = img.data[i * 3];
    out[i * 4 + 1] = img.data[i * 3 + 1];
    out[i * 4 + 2] = img.data[i * 3 + 2];
    out[i * 4 + 3] = 255;
  }
  return out;
}

/** Index of the red byte of pixel (r, c), clamped to the image like Image::get_pixel. */
export function px(img: RGBImage, r: number, c: number): number {
  const rr = r < 0 ? 0 : r >= img.h ? img.h - 1 : r;
  const cc = c < 0 ? 0 : c >= img.w ? img.w - 1 : c;
  return (rr * img.w + cc) * 3;
}

export function transpose(img: RGBImage): RGBImage {
  const out = makeImage(img.h, img.w);
  for (let r = 0; r < img.h; r++)
    for (let c = 0; c < img.w; c++) {
      const s = (r * img.w + c) * 3;
      const d = (c * img.h + r) * 3;
      out.data[d] = img.data[s];
      out.data[d + 1] = img.data[s + 1];
      out.data[d + 2] = img.data[s + 2];
    }
  return out;
}

/** Remove one pixel per row; seam[r] is the column removed in row r. */
export function removeVerticalSeam(img: RGBImage, seam: number[]): RGBImage {
  const out = makeImage(img.w - 1, img.h);
  for (let r = 0; r < img.h; r++) {
    const s = seam[r];
    const src = r * img.w * 3;
    const dst = r * (img.w - 1) * 3;
    out.data.set(img.data.subarray(src, src + s * 3), dst);
    out.data.set(img.data.subarray(src + (s + 1) * 3, src + img.w * 3), dst + s * 3);
  }
  return out;
}

/** Remove one pixel per column; seam[c] is the row removed in column c. */
export function removeHorizontalSeam(img: RGBImage, seam: number[]): RGBImage {
  return transpose(removeVerticalSeam(transpose(img), seam));
}

/** Area-average downscale used for the working image (browser side only). */
export function resizeToWidth(img: RGBImage, w: number): RGBImage {
  if (w >= img.w) return img;
  return resize(img, w, Math.max(2, Math.round((img.h * w) / img.w)));
}

export function resize(img: RGBImage, w: number, h: number): RGBImage {
  if (w === img.w && h === img.h) return img;
  const out = makeImage(w, h);
  const sx = img.w / w;
  const sy = img.h / h;
  for (let r = 0; r < h; r++)
    for (let c = 0; c < w; c++) {
      const r0 = Math.floor(r * sy), r1 = Math.max(r0 + 1, Math.floor((r + 1) * sy));
      const c0 = Math.floor(c * sx), c1 = Math.max(c0 + 1, Math.floor((c + 1) * sx));
      let R = 0, G = 0, B = 0, n = 0;
      for (let y = r0; y < r1; y++)
        for (let x = c0; x < c1; x++) {
          const i = (y * img.w + x) * 3;
          R += img.data[i]; G += img.data[i + 1]; B += img.data[i + 2]; n++;
        }
      const o = (r * w + c) * 3;
      out.data[o] = R / n; out.data[o + 1] = G / n; out.data[o + 2] = B / n;
    }
  return out;
}

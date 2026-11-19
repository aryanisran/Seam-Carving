// Runs the carving loop off the main thread so the page stays responsive (graph cut is slow).
import { CarveOptions, carveStep, SeamRecord } from './carve';
import { RGBImage } from './image';

export interface CarveRequest {
  id: number;
  img: RGBImage;
  cols: number;
  rows: number;
  opt: CarveOptions;
}
export type CarveReply =
  | { id: number; kind: 'progress'; done: number; total: number }
  | { id: number; kind: 'done'; records: SeamRecord[] }
  | { id: number; kind: 'error'; message: string };

self.onmessage = (ev: MessageEvent<CarveRequest>) => {
  const { id, img, cols, rows, opt } = ev.data;
  try {
    const records: SeamRecord[] = [];
    let cur = img;
    let last = 0;
    for (let i = 0; i < cols + rows; i++) {
      const { next, rec } = carveStep(cur, i < cols, opt);
      records.push(rec);
      cur = next;
      const now = performance.now();
      if (now - last > 50) {
        (self as unknown as Worker).postMessage({ id, kind: 'progress', done: i + 1, total: cols + rows } satisfies CarveReply);
        last = now;
      }
    }
    (self as unknown as Worker).postMessage({ id, kind: 'done', records } satisfies CarveReply);
  } catch (e) {
    (self as unknown as Worker).postMessage({ id, kind: 'error', message: String(e) } satisfies CarveReply);
  }
};

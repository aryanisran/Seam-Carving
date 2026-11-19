// Cached carving jobs, each run in its own Web Worker.
import { useEffect, useState } from 'react';
import { CarveOptions, SeamRecord } from './carve';
import { RGBImage, removeHorizontalSeam, removeVerticalSeam } from './image';
import type { CarveReply, CarveRequest } from './carve.worker';

export interface CarveResult {
  records: SeamRecord[];
  images: RGBImage[];
  wallMs: number;
}

interface Job {
  progress: number;
  result?: CarveResult;
  error?: string;
  listeners: Set<() => void>;
}

const jobs = new Map<string, Job>();
let nextId = 1;

function replayImages(img: RGBImage, records: SeamRecord[]): RGBImage[] {
  const out = [img];
  let cur = img;
  for (const r of records) {
    cur = r.vertical ? removeVerticalSeam(cur, r.seam) : removeHorizontalSeam(cur, r.seam);
    out.push(cur);
  }
  return out;
}

function start(key: string, img: RGBImage, cols: number, rows: number, opt: CarveOptions): Job {
  const job: Job = { progress: 0, listeners: new Set() };
  jobs.set(key, job);
  const notify = () => job.listeners.forEach((l) => l());
  const t0 = performance.now();
  const worker = new Worker(new URL('./carve.worker.ts', import.meta.url), { type: 'module' });
  const id = nextId++;
  worker.onmessage = (ev: MessageEvent<CarveReply>) => {
    const m = ev.data;
    if (m.id !== id) return;
    if (m.kind === 'progress') job.progress = m.done / m.total;
    else if (m.kind === 'done') {
      job.progress = 1;
      job.result = { records: m.records, images: replayImages(img, m.records), wallMs: performance.now() - t0 };
      worker.terminate();
    } else {
      job.error = m.message;
      worker.terminate();
    }
    notify();
  };
  worker.onerror = (e) => { job.error = e.message || 'Worker failed'; worker.terminate(); notify(); };
  worker.postMessage({ id, img, cols, rows, opt } satisfies CarveRequest);
  return job;
}

export function useCarve(imgKey: string, img: RGBImage | null, cols: number, rows: number, opt: CarveOptions) {
  const key = `${imgKey}|${cols}|${rows}|${opt.algorithm}|${opt.metric}|${opt.algorithm === 'graph_cut' ? opt.solver : ''}`;
  const [, bump] = useState(0);
  useEffect(() => {
    if (!img) return;
    const job = jobs.get(key) ?? start(key, img, cols, rows, opt);
    const l = () => bump((x) => x + 1);
    job.listeners.add(l);
    l();
    return () => { job.listeners.delete(l); };
  }, [key]); // eslint-disable-line react-hooks/exhaustive-deps
  const job = jobs.get(key);
  return { key, progress: job?.progress ?? 0, result: job?.result, error: job?.error };
}

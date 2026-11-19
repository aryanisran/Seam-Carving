// "Effect on the image": step through every seam an algorithm removes from the working image.
import { useEffect, useMemo, useRef, useState } from 'react';
import { ALGO_LABEL, Algorithm, seamsInOriginal } from '../algo/carve';
import { EnergyMetric, computeEnergy } from '../algo/energy';
import { Solver } from '../algo/graphcut';
import { CarveResult, useCarve } from '../algo/useCarve';
import { RGBImage } from '../algo/image';
import { CarveSettings } from '../App';
import { HORZ, VERT, drawImage, energyToImage, fillPixel, zoomFor } from './canvas';
import { Panel, StepPlayer } from './StepPlayer';
import { fmt } from './CellGrid';

interface Shown {
  result: CarveResult;
  image: RGBImage;
  cols: number;
  rows: number;
  metric: EnergyMetric;
}

export function ImageCarvePanel({ algorithm, settings, solver = 'dinic' }: { algorithm: Algorithm; settings: CarveSettings; solver?: Solver }) {
  const live = settings;
  const { result: liveResult, progress, error, key } = useCarve(live.imageKey, live.image, live.cols, live.rows, { algorithm, metric: live.metric, solver });
  // Keep showing the last finished result while a new one is carved, so the panel never
  // collapses to a progress bar and the page below it does not jump.
  const last = useRef<Shown | null>(null);
  if (liveResult && live.image) last.current = { result: liveResult, image: live.image, cols: live.cols, rows: live.rows, metric: live.metric };
  const busy = !liveResult && !error;
  const result = last.current?.result;
  const image = last.current?.image ?? live.image;
  const cols = last.current?.cols ?? live.cols;
  const rows = last.current?.rows ?? live.rows;
  const metric = last.current?.metric ?? live.metric;
  const n = result?.records.length ?? 0;
  const [k, setK] = useState(0);
  const [view, setView] = useState<'image' | 'energy'>('image');
  useEffect(() => setK(0), [key]);

  const curRef = useRef<HTMLCanvasElement>(null);
  const allRef = useRef<HTMLCanvasElement>(null);
  const zoom = image ? zoomFor(image.w, image.h, 420) : 1;
  const inOrig = useMemo(() => (result && image ? seamsInOriginal(image.w, image.h, result.records) : []), [result, image]);

  useEffect(() => {
    if (!result || !image || !curRef.current || !allRef.current) return;
    const step = Math.min(k, n);
    const img = result.images[step];
    const shown = view === 'energy' ? energyToImage(computeEnergy(img, metric)) : img;
    const ctx = drawImage(curRef.current, shown, zoom, image.w, image.h);
    if (step < n) {
      const rec = result.records[step];
      rec.seam.forEach((v, i) => (rec.vertical ? fillPixel(ctx, i, v, zoom, VERT) : fillPixel(ctx, v, i, zoom, HORZ)));
    }
    // all seams removed so far, drawn on the original image
    const actx = drawImage(allRef.current, image, zoom, image.w, image.h);
    for (let s = 0; s <= Math.min(step, n - 1); s++) {
      const rec = result.records[s];
      const alpha = s === step ? 1 : 0.75;
      for (const idx of inOrig[s]) fillPixel(actx, Math.floor(idx / image.w), idx % image.w, zoom, rec.vertical ? VERT : HORZ, alpha);
    }
  }, [result, k, n, view, zoom, image, metric, inOrig]);

  if (!image) return null;
  const title = `${ALGO_LABEL[algorithm]}${algorithm === 'graph_cut' ? ` (${solver === 'dinic' ? 'Dinic' : 'Edmonds-Karp'})` : ''} - effect on the image`;

  if (error) return <div className="panel error">Carving failed: {error}</div>;
  const progressBar = (
    <div className="progress-wrap">
      <div className="progress"><div style={{ width: `${Math.round(progress * 100)}%` }} /></div>
      <span className="muted">
        Removing {live.cols + live.rows} seams from the {live.image?.w} x {live.image?.h} working image... {Math.round(progress * 100)}%
      </span>
    </div>
  );
  if (!result) {
    return (
      <section className="player first-load">
        <header className="player-head"><h3 className="player-title">{title}</h3></header>
        {progressBar}
      </section>
    );
  }

  const step = Math.min(k, n);
  const rec = step < n ? result.records[step] : null;
  const img = result.images[step];
  const removedEnergy = result.records.slice(0, step).reduce((s, r) => s + r.energy, 0);
  const total = result.records.reduce((s, r) => s + r.energy, 0);
  const algoMs = result.records.reduce((s, r) => s + r.ms, 0);

  const narration = rec
    ? `Seam ${step + 1} of ${n}: ${rec.vertical ? 'vertical (red)' : 'horizontal (blue)'} seam with energy ${fmt(round1(rec.energy))}` +
      `${rec.flow ? `, max flow ${fmt(round1(rec.flow.maxFlow))} in ${rec.flow.augmentingPaths} augmenting paths` : ''}` +
      `, found in ${rec.ms.toFixed(1)} ms. Energy is recomputed on the current ${img.w} x ${img.h} image first; removing the seam gives ${rec.vertical ? img.w - 1 : img.w} x ${rec.vertical ? img.h : img.h - 1}.`
    : `Done: ${image.w} x ${image.h} -> ${img.w} x ${img.h}. ${n} seams with total energy ${fmt(round1(total))}, ${algoMs.toFixed(0)} ms of algorithm time in your browser.`;

  return (
    <StepPlayer
      title={title}
      total={n + 1} index={k} onIndex={setK}
      toolbar={
        <div className="speeds" role="group" aria-label="View">
          {(['image', 'energy'] as const).map((v) => (
            <button key={v} className={'btn seg' + (view === v ? ' on' : '')} onClick={() => setView(v)} aria-pressed={view === v}>
              {v === 'image' ? 'Image' : 'Energy'}
            </button>
          ))}
        </div>
      }
      input={<code>{image.w} x {image.h} -&gt; {image.w - cols} x {image.h - rows}: remove {cols} vertical + {rows} horizontal seams ({metric === 'dual' ? 'dual-gradient' : 'Sobel'} energy)</code>}
      narration={narration}
    >
      <div className="carve-area">
        <div className="row-panels">
          <Panel title={`Current image ${img.w} x ${img.h}`} aside={<span className="stat">seam {Math.min(step + 1, n)} / {n}</span>}>
            <div className="pix-box"><canvas ref={curRef} className="pix fit" /></div>
          </Panel>
          <Panel title="Removed seams on the original" aside={<span className="stat">energy removed <b>{fmt(round1(removedEnergy))}</b></span>}>
            <div className="pix-box"><canvas ref={allRef} className="pix fit" /></div>
          </Panel>
        </div>
        {busy && <div className="busy-overlay" role="status">{progressBar}</div>}
      </div>
    </StepPlayer>
  );
}

const round1 = (v: number) => Math.round(v * 10) / 10;

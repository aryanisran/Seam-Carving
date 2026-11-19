// "Effect on the image": step through every seam an algorithm removes from the working image.
import { useEffect, useMemo, useRef, useState } from 'react';
import { ALGO_LABEL, Algorithm, seamsInOriginal } from '../algo/carve';
import { computeEnergy } from '../algo/energy';
import { Solver } from '../algo/graphcut';
import { useCarve } from '../algo/useCarve';
import { CarveSettings } from '../App';
import { HORZ, VERT, drawImage, energyToImage, fillPixel, zoomFor } from './canvas';
import { Panel, StepPlayer } from './StepPlayer';
import { fmt } from './CellGrid';

export function ImageCarvePanel({ algorithm, settings, solver = 'dinic' }: { algorithm: Algorithm; settings: CarveSettings; solver?: Solver }) {
  const { image, imageKey, cols, rows, metric } = settings;
  const { result, progress, error, key } = useCarve(imageKey, image, cols, rows, { algorithm, metric, solver });
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
  if (!result) {
    return (
      <section className="player">
        <header className="player-head"><h3 className="player-title">{title}</h3></header>
        <div className="progress-wrap">
          <div className="progress"><div style={{ width: `${Math.round(progress * 100)}%` }} /></div>
          <span className="muted">Removing {cols + rows} seams from the {image.w} x {image.h} working image... {Math.round(progress * 100)}%</span>
        </div>
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
      <div className="row-panels">
        <Panel title={`Current image ${img.w} x ${img.h}`} aside={<span className="stat">seam {Math.min(step + 1, n)} / {n}</span>}>
          <canvas ref={curRef} className="pix" />
        </Panel>
        <Panel title="Removed seams on the original" aside={<span className="stat">energy removed <b>{fmt(round1(removedEnergy))}</b></span>}>
          <canvas ref={allRef} className="pix" />
        </Panel>
      </div>
    </StepPlayer>
  );
}

const round1 = (v: number) => Math.round(v * 10) / 10;

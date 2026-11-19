import { useEffect, useMemo, useRef, useState } from 'react';
import { RGBImage, fromRGBA, resizeToWidth, toRGBA } from './algo/image';
import { EnergyMetric } from './algo/energy';
import { CANVAS_BG } from './components/canvas';
import { Icon } from './components/Icon';
import { DPTab } from './tabs/DPTab';
import { GreedyTab } from './tabs/GreedyTab';
import { GraphCutTab } from './tabs/GraphCutTab';
import { ImageCarvePanel } from './components/ImageCarvePanel';
import { Algorithm } from './algo/carve';
import { Solver } from './algo/graphcut';
import towerUrl from '../../samples/synthetic_tower.png';
import photoUrl from '../../samples/image.png';

export interface CarveSettings {
  image: RGBImage | null;
  imageKey: string;
  cols: number;
  rows: number;
  metric: EnergyMetric;
}

const SAMPLES = [
  { id: 'photo', label: 'Castle photo (samples/image.png)', url: photoUrl },
  { id: 'tower', label: 'Synthetic tower (samples/synthetic_tower.png)', url: towerUrl },
];

const TABS = [
  { id: 'dp', label: 'Dynamic Programming' },
  { id: 'greedy', label: 'Greedy' },
  { id: 'graphcut', label: 'Graph Cut' },
];

const TAB_ALGO: Record<string, Algorithm> = { dp: 'dp', greedy: 'greedy', graphcut: 'graph_cut' };

const WW_MIN = 40, WW_MAX = 240, WW_DEFAULT = 100;

async function decode(src: string | Blob): Promise<RGBImage> {
  const blob = typeof src === 'string' ? await (await fetch(src)).blob() : src;
  const bmp = await createImageBitmap(blob);
  const cv = document.createElement('canvas');
  cv.width = bmp.width;
  cv.height = bmp.height;
  const ctx = cv.getContext('2d')!;
  ctx.drawImage(bmp, 0, 0);
  return fromRGBA(bmp.width, bmp.height, ctx.getImageData(0, 0, bmp.width, bmp.height).data);
}

function useDebounced<T>(value: T, ms: number): T {
  const [v, setV] = useState(value);
  useEffect(() => {
    const t = setTimeout(() => setV(value), ms);
    return () => clearTimeout(t);
  }, [value, ms]);
  return v;
}

function readTab() {
  const h = location.hash.replace('#', '');
  return TABS.some((t) => t.id === h) ? h : 'dp';
}

export default function App() {
  const [tab, setTab] = useState(readTab);
  const [sampleId, setSampleId] = useState('photo');
  const [upload, setUpload] = useState<{ name: string; img: RGBImage } | null>(null);
  const [source, setSource] = useState<RGBImage | null>(null);
  const [loadErr, setLoadErr] = useState('');
  const [width, setWidth] = useState(WW_DEFAULT);
  const [cols, setCols] = useState(20);
  const [rows, setRows] = useState(0);
  const [metric, setMetric] = useState<EnergyMetric>('dual');
  const [solver, setSolver] = useState<Solver>('dinic');

  useEffect(() => {
    const onHash = () => setTab(readTab());
    window.addEventListener('hashchange', onHash);
    return () => window.removeEventListener('hashchange', onHash);
  }, []);

  useEffect(() => {
    if (upload) { setSource(upload.img); return; }
    let live = true;
    decode(SAMPLES.find((s) => s.id === sampleId)!.url)
      .then((img) => live && setSource(img))
      .catch((e) => setLoadErr(String(e)));
    return () => { live = false; };
  }, [sampleId, upload]);

  const maxW = source ? Math.min(WW_MAX, source.w) : WW_MAX;
  const minW = Math.min(WW_MIN, maxW);
  const draftW = Math.max(minW, Math.min(maxW, width));
  const preview = useMemo(() => (source ? resizeToWidth(source, draftW) : null), [source, draftW]);

  // Expensive work (carving) only starts once the sliders stop moving.
  const committedW = useDebounced(draftW, 350);
  const working = useMemo(() => (source ? resizeToWidth(source, committedW) : null), [source, committedW]);
  const srcName = upload ? upload.name : sampleId;
  const imageKey = `${srcName}@${committedW}`;
  const maxCols = working ? working.w - 2 : 0;
  const maxRows = working ? working.h - 2 : 0;
  const c = Math.min(cols, maxCols), r = Math.min(rows, maxRows);
  const settings: CarveSettings = {
    image: working, imageKey,
    cols: useDebounced(c, 350), rows: useDebounced(r, 350), metric,
  };
  const pending = settings.cols !== c || settings.rows !== r || committedW !== draftW;

  const pickTab = (id: string) => { setTab(id); history.replaceState(null, '', '#' + id); };

  return (
    <div className="app">
      <header className="top">
        <div>
          <h1>Seam Carving, step by step</h1>
          <p className="muted">Dynamic Programming, Greedy and Graph Cut (min-cut / max-flow), each explained one step at a time and applied to a real image.</p>
        </div>
      </header>

      <section className="settings" aria-label="Image and size">
        <div className="set-col">
          <label className="field">
            <span>Image</span>
            <select value={upload ? 'upload' : sampleId} onChange={(e) => { if (e.target.value !== 'upload') { setUpload(null); setSampleId(e.target.value); } }}>
              {SAMPLES.map((s) => <option key={s.id} value={s.id}>{s.label}</option>)}
              {upload && <option value="upload">Uploaded: {upload.name}</option>}
            </select>
          </label>
          <label className="btn upload">
            <Icon name="upload" /> Upload your own
            <input type="file" accept="image/png,image/jpeg,image/bmp,image/webp" onChange={async (e) => {
              const f = e.target.files?.[0];
              if (!f) return;
              try { setUpload({ name: f.name, img: await decode(f) }); setLoadErr(''); } catch (err) { setLoadErr('Could not read that image: ' + String(err)); }
            }} />
          </label>
          <div className="field">
            <span>Energy function</span>
            <div className="speeds" role="group" aria-label="Energy function">
              {(['dual', 'sobel'] as EnergyMetric[]).map((m) => (
                <button key={m} className={'btn seg' + (metric === m ? ' on' : '')} onClick={() => setMetric(m)} aria-pressed={metric === m}>
                  {m === 'dual' ? 'Dual gradient' : 'Sobel'}
                </button>
              ))}
            </div>
          </div>
          {loadErr && <p className="error">{loadErr}</p>}
        </div>

        <div className="set-col">
          <Slider label="Working width" value={draftW} min={minW} max={maxW} onChange={setWidth}
            note={source && preview ? `${preview.w} x ${preview.h} px (original ${source.w} x ${source.h}). Smaller = faster graph cut.` : ''} />
          <Slider label="Target width" value={preview ? preview.w - Math.min(cols, preview.w - 2) : 0} min={2} max={preview?.w ?? 2}
            onChange={(v) => setCols(Math.max(0, (preview?.w ?? 0) - v))} note={`remove ${c} vertical seams`} />
          <Slider label="Target height" value={preview ? preview.h - Math.min(rows, preview.h - 2) : 0} min={2} max={preview?.h ?? 2}
            onChange={(v) => setRows(Math.max(0, (preview?.h ?? 0) - v))} note={`remove ${r} horizontal seams`} />
          <p className="muted small" style={{ visibility: pending ? "visible" : "hidden", margin: 0 }} aria-live="polite">Updating when you stop dragging...</p>
        </div>

        <div className="set-preview">
          <span className="muted small">Live preview of the working image</span>
          {preview && <Preview img={preview} targetW={preview.w - c} targetH={preview.h - r} />}
        </div>
      </section>

      {working && <div className="effect"><ImageCarvePanel key={tab} algorithm={TAB_ALGO[tab]} settings={settings} solver={solver} /></div>}

      <nav className="tabs" role="tablist" aria-label="Algorithms">
        {TABS.map((t) => (
          <button key={t.id} role="tab" aria-selected={tab === t.id} className={'tab' + (tab === t.id ? ' on' : '')} onClick={() => pickTab(t.id)}>
            {t.label}
          </button>
        ))}
      </nav>

      <main role="tabpanel">
        {!working ? <p className="muted">Loading image...</p> : (
          <>
            {tab === 'dp' && <DPTab />}
            {tab === 'greedy' && <GreedyTab />}
            {tab === 'graphcut' && <GraphCutTab solver={solver} setSolver={setSolver} />}
          </>
        )}
      </main>

      <footer className="foot muted small">
        Algorithms ported line for line from the team's C++ code (src/energy.cpp, src/dp_seam.cpp, src/greedy_seam.cpp, src/graph_cut_seam.cpp, src/maxflow.cpp)
        and verified seam-for-seam and pixel-for-pixel against it in CI.
      </footer>
    </div>
  );
}

function Slider({ label, value, min, max, onChange, note }: { label: string; value: number; min: number; max: number; onChange: (v: number) => void; note?: string }) {
  return (
    <label className="field slider">
      <span className="slider-head"><span>{label}</span><b>{value}</b></span>
      <input type="range" min={min} max={max} value={value} onChange={(e) => onChange(Number(e.target.value))} />
      {note && <span className="muted small">{note}</span>}
    </label>
  );
}

// The preview always occupies the same box, so dragging the working width never moves the layout.
const PREVIEW_W = 240, PREVIEW_H = 165;

function Preview({ img, targetW, targetH }: { img: RGBImage; targetW: number; targetH: number }) {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const cv = ref.current;
    if (!cv) return;
    const dpr = window.devicePixelRatio || 1;
    cv.width = Math.round(PREVIEW_W * dpr);
    cv.height = Math.round(PREVIEW_H * dpr);
    const ctx = cv.getContext('2d')!;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.fillStyle = CANVAS_BG;
    ctx.fillRect(0, 0, PREVIEW_W, PREVIEW_H);
    const s = Math.min(PREVIEW_W / img.w, PREVIEW_H / img.h);
    const dw = img.w * s, dh = img.h * s;
    const x = (PREVIEW_W - dw) / 2, y = (PREVIEW_H - dh) / 2;
    const off = document.createElement('canvas');
    off.width = img.w;
    off.height = img.h;
    off.getContext('2d')!.putImageData(new ImageData(toRGBA(img), img.w, img.h), 0, 0);
    ctx.imageSmoothingEnabled = false; // show the real (blocky) working resolution
    ctx.drawImage(off, x, y, dw, dh);
    ctx.strokeStyle = '#E69F00';
    ctx.lineWidth = 2;
    ctx.setLineDash([5, 4]);
    ctx.strokeRect(x + 1, y + 1, targetW * s - 2, targetH * s - 2);
  }, [img, targetW, targetH]);
  return (
    <>
      <canvas ref={ref} className="preview" style={{ width: PREVIEW_W, height: PREVIEW_H }} />
      <span className="muted small">Dashed box = target size {targetW} x {targetH}</span>
    </>
  );
}

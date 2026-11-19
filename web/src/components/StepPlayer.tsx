// Step-by-step player: title bar, transport controls, speed, ticked timeline, narration.
// Keyboard (when the player has focus): Space play/pause, Left/Right step, Home/End.
import { ReactNode, useEffect, useRef, useState } from 'react';
import { Icon } from './Icon';

const SPEEDS = [0.5, 1, 2, 4];
const BASE_MS = 900;

function loadSpeed(): number {
  try {
    const v = Number(localStorage.getItem('player-speed'));
    return SPEEDS.includes(v) ? v : 1;
  } catch {
    return 1;
  }
}

export interface StepPlayerProps {
  title: string;
  total: number;
  index: number;
  onIndex: (i: number) => void;
  input?: ReactNode;
  narration?: ReactNode;
  toolbar?: ReactNode;
  /** indices that deserve a tick on the timeline (default: evenly spaced) */
  marks?: number[];
  children: ReactNode;
}

export function StepPlayer({ title, total, index, onIndex, input, narration, toolbar, marks, children }: StepPlayerProps) {
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(loadSpeed);
  const last = Math.max(0, total - 1);
  const idx = Math.min(index, last);
  const idxRef = useRef(idx);
  idxRef.current = idx;

  useEffect(() => {
    if (!playing) return;
    const t = setInterval(() => {
      const next = idxRef.current + 1;
      if (next > last) setPlaying(false);
      else onIndex(next);
    }, BASE_MS / speed);
    return () => clearInterval(t);
  }, [playing, speed, last, onIndex]);

  const go = (i: number) => onIndex(Math.max(0, Math.min(last, i)));
  const toggle = () => {
    if (!playing && idx >= last) onIndex(0);
    setPlaying(!playing);
  };
  const changeSpeed = (s: number) => {
    setSpeed(s);
    try { localStorage.setItem('player-speed', String(s)); } catch { /* storage unavailable */ }
  };

  const onKey = (e: React.KeyboardEvent) => {
    if ((e.target as HTMLElement).closest('select, input[type="text"], input[type="file"]')) return;
    const keys: Record<string, () => void> = {
      ' ': toggle,
      ArrowRight: () => { setPlaying(false); go(idx + 1); },
      ArrowLeft: () => { setPlaying(false); go(idx - 1); },
      Home: () => { setPlaying(false); go(0); },
      End: () => { setPlaying(false); go(last); },
    };
    if (keys[e.key]) { e.preventDefault(); keys[e.key](); }
  };

  const tickList = marks ?? evenTicks(total);

  return (
    <section className="player" tabIndex={0} onKeyDown={onKey} aria-label={title}>
      <header className="player-head">
        <h3 className="player-title" title={title}>{title}</h3>
        <span className="player-step">Step {total ? idx + 1 : 0} / {total}</span>
      </header>

      <div className="player-controls">
        <div className="transport">
          <button className="btn icon" onClick={() => { setPlaying(false); go(0); }} disabled={idx === 0} aria-label="First step (Home)" title="First step (Home)"><Icon name="first" /></button>
          <button className="btn icon" onClick={() => { setPlaying(false); go(idx - 1); }} disabled={idx === 0} aria-label="Previous step (Left arrow)" title="Previous (Left arrow)"><Icon name="prev" /></button>
          <button className="btn icon primary" onClick={toggle} aria-label={playing ? 'Pause (Space)' : 'Play (Space)'} title={playing ? 'Pause (Space)' : 'Play (Space)'}><Icon name={playing ? 'pause' : 'play'} /></button>
          <button className="btn icon" onClick={() => { setPlaying(false); go(idx + 1); }} disabled={idx >= last} aria-label="Next step (Right arrow)" title="Next (Right arrow)"><Icon name="next" /></button>
          <button className="btn icon" onClick={() => { setPlaying(false); go(last); }} disabled={idx >= last} aria-label="Last step (End)" title="Last step (End)"><Icon name="last" /></button>
        </div>
        <div className="speeds" role="group" aria-label="Playback speed">
          {SPEEDS.map((s) => (
            <button key={s} className={'btn seg' + (s === speed ? ' on' : '')} onClick={() => changeSpeed(s)} aria-pressed={s === speed}>{s}x</button>
          ))}
        </div>
        {toolbar && <div className="player-toolbar">{toolbar}</div>}
      </div>

      <div className="timeline">
        <input
          type="range" min={0} max={last} step={1} value={idx}
          onChange={(e) => { setPlaying(false); onIndex(Number(e.target.value)); }}
          aria-label="Timeline"
        />
        <div className="ticks" aria-hidden="true">
          {tickList.map((t) => (
            <span key={t} style={{ left: `calc(${last ? (t / last) * 100 : 0}% + ${8 - (last ? (t / last) * 16 : 0)}px)` }} />
          ))}
        </div>
      </div>

      {input && <div className="player-input"><span className="tag">INPUT</span>{input}</div>}

      <div className="player-body">{children}</div>

      {narration && <div className="narration" aria-live="polite">{narration}</div>}
    </section>
  );
}

function evenTicks(total: number): number[] {
  if (total <= 1) return [];
  const n = Math.min(total, 16);
  return Array.from({ length: n }, (_, i) => Math.round((i * (total - 1)) / (n - 1)));
}

export function Panel({ title, children, aside, className }: { title: string; children: ReactNode; aside?: ReactNode; className?: string }) {
  return (
    <div className={'panel ' + (className ?? '')}>
      <div className="panel-head">
        <span className="panel-title">{title}</span>
        {aside}
      </div>
      {children}
    </div>
  );
}

export function Pseudocode({ lines, active }: { lines: string[]; active: number[] }) {
  return (
    <ol className="code">
      {lines.map((l, i) => (
        <li key={i} className={active.includes(i) ? 'active' : ''}>
          <span className="ln">{i + 1}</span>
          <code>{l}</code>
        </li>
      ))}
    </ol>
  );
}

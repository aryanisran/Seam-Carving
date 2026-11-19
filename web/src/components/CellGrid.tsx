// Labelled grid of value cells (like the memo table in the reference design).
import { ReactNode } from 'react';

export type CellState = 'current' | 'compared' | 'chosen' | 'seam' | 'written' | 'dim' | 'source' | 'path' | 'best';

export interface CellGridProps {
  rows: number;
  cols: number;
  value: (r: number, c: number) => ReactNode;
  states?: (r: number, c: number) => CellState[];
  corner?: (r: number, c: number) => ReactNode;
  bg?: (r: number, c: number) => string | undefined;
  caption?: string;
  rowLabel?: string;
}

export function CellGrid({ rows, cols, value, states, corner, bg, caption, rowLabel = 'r' }: CellGridProps) {
  return (
    <div className="grid-wrap">
      <table className="cells" aria-label={caption}>
        <thead>
          <tr>
            <th className="idx corner">{rowLabel} \ c</th>
            {Array.from({ length: cols }, (_, c) => <th key={c} className="idx">{c}</th>)}
          </tr>
        </thead>
        <tbody>
          {Array.from({ length: rows }, (_, r) => (
            <tr key={r}>
              <th className="idx">{r}</th>
              {Array.from({ length: cols }, (_, c) => {
                const st = states?.(r, c) ?? [];
                const b = bg?.(r, c);
                return (
                  <td key={c} className={'cell ' + st.map((s) => 'is-' + s).join(' ')} style={b ? { background: b } : undefined}>
                    <span className="v">{value(r, c)}</span>
                    {corner && <span className="corner-mark">{corner(r, c)}</span>}
                  </td>
                );
              })}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

/** Tiny arrow pointing to the parent cell in the previous row: dc = -1 (up-left), 0 (up), 1 (up-right). */
export function ParentArrow({ dc }: { dc: number }) {
  const rot = dc === 0 ? 0 : dc < 0 ? -45 : 45;
  return (
    <svg width="10" height="10" viewBox="0 0 10 10" aria-label={dc === 0 ? 'from above' : dc < 0 ? 'from up-left' : 'from up-right'}>
      <g transform={`rotate(${rot} 5 5)`}>
        <path d="M5 1 L8 5 H6 V9 H4 V5 H2 Z" fill="currentColor" />
      </g>
    </svg>
  );
}

export const fmt = (v: number) => (Number.isInteger(v) ? String(v) : v.toFixed(1));

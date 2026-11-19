// Small inline SVG icons (no icon fonts, no unicode glyphs).
const PATHS: Record<string, string> = {
  play: 'M7 5l12 7-12 7z',
  pause: 'M7 5h4v14H7zM13 5h4v14h-4z',
  prev: 'M17 5v14l-10-7z M6 5h2v14H6z',
  next: 'M7 5v14l10-7z M16 5h2v14h-2z',
  first: 'M5 5h2v14H5zM19 5v14l-10-7z',
  last: 'M17 5h2v14h-2zM5 5v14l10-7z',
  upload: 'M12 4l5 5h-3v6h-4V9H7zM5 17h14v3H5z',
};

export function Icon({ name, size = 16 }: { name: keyof typeof PATHS | string; size?: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
      <path d={PATHS[name]} />
    </svg>
  );
}

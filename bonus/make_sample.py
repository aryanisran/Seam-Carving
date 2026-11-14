"""Generate a small synthetic test image (sky, clouds, tower, grass, person).

Used by CI and as a default sample in the app, so the repo needs no binary
photos. Deterministic (fixed seed). Real photos can also be dropped into samples/.

    python bonus/make_sample.py            -> samples/synthetic_tower.png
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image


def synthetic_tower(width: int = 200, height: int = 130, seed: int = 7) -> np.ndarray:
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:height, 0:width]
    img = np.zeros((height, width, 3), dtype=np.float64)

    # Sky: vertical blue gradient
    t = yy / height
    img[..., 0] = 40 + 60 * t
    img[..., 1] = 110 + 70 * t
    img[..., 2] = 200 + 40 * t

    # Clouds: soft white ellipses
    for cx, cy, rx, ry in [(35, 22, 26, 7), (70, 38, 20, 5), (160, 15, 22, 6)]:
        d = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2
        a = np.clip(1.2 - d, 0, 1)[..., None]
        img = img * (1 - a) + 245 * a

    # Grass: bottom 25%
    horizon = int(height * 0.75)
    img[horizon:] = [70, 140, 50]

    # Tower with battlements and windows
    x0, x1, top = int(width * 0.62), int(width * 0.80), int(height * 0.22)
    img[top:horizon + 2, x0:x1] = [190, 160, 110]
    for bx in range(x0, x1, 6):
        img[top - 5:top, bx:bx + 3] = [190, 160, 110]
    for wy in range(top + 10, horizon - 8, 16):
        img[wy:wy + 6, x0 + 6:x0 + 9] = [40, 35, 30]
        img[wy:wy + 6, x1 - 9:x1 - 6] = [40, 35, 30]

    # Small person on the left
    px = int(width * 0.10)
    img[horizon - 7:horizon + 1, px:px + 2] = [120, 20, 30]

    img += rng.normal(0, 3, img.shape)  # mild texture so energy is never exactly zero
    return np.clip(img, 0, 255).astype(np.uint8)


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "samples" / "synthetic_tower.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(synthetic_tower()).save(out)
    print(f"Wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

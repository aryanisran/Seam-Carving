"""Step-by-step seam visualisation for the graph-cut seam carver (Bonus 7a).

Reads the original image plus the ``seams.json`` written by
``bin/seam_carve --export-seams`` and replays every seam removal in order.

Outputs (written to ``--out``):
    steps.gif       animation; each frame shows the next seam in colour just
                    before it is removed (vertical = red, horizontal = blue)
    overlay.png     every removed seam drawn on the ORIGINAL image
    comparison.png  original | seam carving | scaling | cropping
    energy.png      dual-gradient energy map of the original image

Coordinate convention (same as the C++ exporter):
    vertical seam   indices[r] = column removed in row r
    horizontal seam indices[c] = row removed in column c
Indices refer to the image *at that step* (already shrunk by earlier seams),
so to draw all seams on the original image we carry an "origin" grid that
stores each surviving pixel's original (row, col) and shrink it in lockstep.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

VERTICAL_COLOR = np.array([255, 0, 0], dtype=np.uint8)
HORIZONTAL_COLOR = np.array([0, 120, 255], dtype=np.uint8)
CANVAS_COLOR = np.array([32, 32, 32], dtype=np.uint8)


# --------------------------------------------------------------------------- #
# Data loading
# --------------------------------------------------------------------------- #
@dataclass
class SeamRecord:
    """One carved seam, as exported by the C++ CLI."""

    step: int
    vertical: bool
    indices: np.ndarray
    width_before: int
    height_before: int
    energy: float = 0.0
    elapsed_ms: float = 0.0
    bfs_runs: int = 0
    augmenting_paths: int = 0


def parse_seams(data: dict) -> list[SeamRecord]:
    records = []
    for s in data["seams"]:
        records.append(
            SeamRecord(
                step=int(s["step"]),
                vertical=s["orientation"] == "vertical",
                indices=np.asarray(s["indices"], dtype=np.int64),
                width_before=int(s["width_before"]),
                height_before=int(s["height_before"]),
                energy=float(s.get("energy_sum", 0.0)),
                elapsed_ms=float(s.get("elapsed_ms", 0.0)),
                bfs_runs=int(s.get("bfs_runs", 0)),
                augmenting_paths=int(s.get("augmenting_paths", 0)),
            )
        )
    return records


def load_seams(path: str | Path) -> list[SeamRecord]:
    return parse_seams(json.loads(Path(path).read_text()))


def load_image(path: str | Path) -> np.ndarray:
    """Load any image as an (H, W, 3) uint8 RGB array."""
    with Image.open(path) as im:
        return np.asarray(im.convert("RGB"), dtype=np.uint8).copy()


# --------------------------------------------------------------------------- #
# Seam geometry
# --------------------------------------------------------------------------- #
def validate_seam(seam: np.ndarray, vertical: bool, height: int, width: int) -> None:
    """Raise ValueError unless `seam` is a valid connected seam for an HxW image."""
    length, limit = (height, width) if vertical else (width, height)
    kind = "vertical" if vertical else "horizontal"
    if seam.ndim != 1 or len(seam) != length:
        raise ValueError(f"{kind} seam has length {len(seam)}, expected {length}")
    if len(seam) and (seam.min() < 0 or seam.max() >= limit):
        raise ValueError(f"{kind} seam index out of range [0, {limit - 1}]")
    if len(seam) > 1 and np.abs(np.diff(seam)).max() > 1:
        raise ValueError(f"{kind} seam is not 8-connected (jump > 1 between neighbours)")


def seam_coords(seam: np.ndarray, vertical: bool) -> tuple[np.ndarray, np.ndarray]:
    """Return (rows, cols) of every pixel on the seam."""
    along = np.arange(len(seam))
    return (along, seam) if vertical else (seam, along)


def remove_seam(arr: np.ndarray, seam: np.ndarray, vertical: bool) -> np.ndarray:
    """Remove one seam from an (H, W, ...) array. Returns a new array.

    Horizontal seams are handled by swapping the first two axes, removing a
    vertical seam, and swapping back - the same transpose trick the C++
    graph-cut code uses.
    """
    if not vertical:
        return np.swapaxes(remove_seam(np.swapaxes(arr, 0, 1), seam, True), 0, 1)
    h, w = arr.shape[:2]
    keep = np.ones((h, w), dtype=bool)
    keep[np.arange(h), seam] = False
    return arr[keep].reshape(h, w - 1, *arr.shape[2:])


# --------------------------------------------------------------------------- #
# Replay
# --------------------------------------------------------------------------- #
@dataclass
class SeamStep:
    record: SeamRecord
    image_before: np.ndarray  # image just before this seam is removed
    rows: np.ndarray  # seam pixel coordinates in image_before
    cols: np.ndarray
    orig_rows: np.ndarray  # the same pixels in ORIGINAL image coordinates
    orig_cols: np.ndarray


def replay(image: np.ndarray, records: list[SeamRecord]) -> tuple[list[SeamStep], np.ndarray]:
    """Apply every seam in order. Returns (per-step info, final carved image)."""
    img = image
    h, w = img.shape[:2]
    origin = np.stack(np.meshgrid(np.arange(h), np.arange(w), indexing="ij"), axis=-1)

    steps: list[SeamStep] = []
    for rec in records:
        h, w = img.shape[:2]
        if (rec.height_before, rec.width_before) != (h, w):
            raise ValueError(
                f"step {rec.step}: seams.json expects a {rec.width_before}x{rec.height_before} "
                f"image but the replay is at {w}x{h} (wrong input image?)"
            )
        validate_seam(rec.indices, rec.vertical, h, w)
        rows, cols = seam_coords(rec.indices, rec.vertical)
        o = origin[rows, cols]
        steps.append(SeamStep(rec, img, rows, cols, o[:, 0], o[:, 1]))
        img = remove_seam(img, rec.indices, rec.vertical)
        origin = remove_seam(origin, rec.indices, rec.vertical)
    return steps, img


# --------------------------------------------------------------------------- #
# Rendering
# --------------------------------------------------------------------------- #
def seam_color(vertical: bool) -> np.ndarray:
    return VERTICAL_COLOR if vertical else HORIZONTAL_COLOR


def auto_scale(width: int, target: int = 480, max_scale: int = 6) -> int:
    """Integer upscale factor so small images are visible (nearest neighbour)."""
    return int(max(1, min(max_scale, target // max(width, 1))))


def upscale(arr: np.ndarray, scale: int) -> np.ndarray:
    if scale <= 1:
        return arr
    return np.repeat(np.repeat(arr, scale, axis=0), scale, axis=1)


def on_canvas(arr: np.ndarray, height: int, width: int) -> np.ndarray:
    """Place `arr` in the top-left of a fixed-size canvas (GIF frames must match)."""
    canvas = np.empty((height, width, 3), dtype=np.uint8)
    canvas[:] = CANVAS_COLOR
    canvas[: arr.shape[0], : arr.shape[1]] = arr
    return canvas


def render_step(step: SeamStep) -> np.ndarray:
    """The image before this step with its seam highlighted."""
    out = step.image_before.copy()
    out[step.rows, step.cols] = seam_color(step.record.vertical)
    return out


def overlay_all_seams(image: np.ndarray, steps: list[SeamStep]) -> np.ndarray:
    """Draw every removed seam on the original image."""
    out = image.copy()
    for s in steps:
        out[s.orig_rows, s.orig_cols] = seam_color(s.record.vertical)
    return out


def make_gif(
    image: np.ndarray,
    steps: list[SeamStep],
    final: np.ndarray,
    path: str | Path,
    scale: int = 1,
    frame_ms: int = 80,
    max_frames: int = 200,
) -> int:
    """Write the step-by-step animation. Returns the number of frames written."""
    h, w = image.shape[:2]
    stride = max(1, int(np.ceil(len(steps) / max_frames)))
    chosen = steps[::stride]

    frames = [Image.fromarray(upscale(on_canvas(image, h, w), scale))]
    durations = [800]
    for s in chosen:
        frames.append(Image.fromarray(upscale(on_canvas(render_step(s), h, w), scale)))
        durations.append(frame_ms)
    frames.append(Image.fromarray(upscale(on_canvas(final, h, w), scale)))
    durations.append(1500)

    # `path` may also be a file-like object such as io.BytesIO
    frames[0].save(path, format="GIF", save_all=True, append_images=frames[1:], duration=durations, loop=0)
    return len(frames)


def scaled(image: np.ndarray, width: int, height: int) -> np.ndarray:
    """Plain (content-unaware) resize, for comparison."""
    return np.asarray(Image.fromarray(image).resize((width, height), Image.LANCZOS))


def cropped(image: np.ndarray, width: int, height: int) -> np.ndarray:
    """Centre crop, for comparison."""
    h, w = image.shape[:2]
    top, left = (h - height) // 2, (w - width) // 2
    return image[top : top + height, left : left + width]


def comparison_figure(image: np.ndarray, final: np.ndarray, scale: int = 1) -> np.ndarray:
    """Side-by-side: original | seam carving | scaling | cropping."""
    h, w = image.shape[:2]
    th, tw = final.shape[:2]
    panels = [
        ("Original", image),
        ("Seam carving", final),
        ("Scaling", scaled(image, tw, th)),
        ("Cropping", cropped(image, tw, th)),
    ]
    label_h, gap = 22, 10
    ph, pw = h * scale, w * scale
    fig = Image.new("RGB", (len(panels) * pw + (len(panels) + 1) * gap, ph + label_h + 2 * gap), (255, 255, 255))
    draw = ImageDraw.Draw(fig)
    for i, (label, arr) in enumerate(panels):
        x = gap + i * (pw + gap)
        draw.text((x, gap // 2), f"{label} ({arr.shape[1]}x{arr.shape[0]})", fill=(0, 0, 0))
        fig.paste(Image.fromarray(upscale(arr, scale)), (x, label_h + gap))
    return np.asarray(fig)


def energy_map(image: np.ndarray) -> np.ndarray:
    """Dual-gradient energy, matching the C++ `DualGradient` metric (clamped borders)."""
    p = np.pad(image.astype(np.float64), ((1, 1), (1, 1), (0, 0)), mode="edge")
    dx = p[1:-1, 2:] - p[1:-1, :-2]
    dy = p[2:, 1:-1] - p[:-2, 1:-1]
    return np.sqrt((dx**2).sum(axis=-1) + (dy**2).sum(axis=-1))


def energy_image(image: np.ndarray) -> np.ndarray:
    """Energy map as a viewable greyscale RGB image (sqrt-stretched)."""
    e = np.sqrt(energy_map(image))
    peak = e.max()
    g = (255 * e / peak).astype(np.uint8) if peak > 0 else np.zeros(e.shape, np.uint8)
    return np.repeat(g[..., None], 3, axis=-1)


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #
def render_all(image: np.ndarray, records: list[SeamRecord], out_dir: str | Path, scale: int | None = None) -> dict:
    """Write every visualisation into out_dir. Returns a small summary dict."""
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    scale = scale or auto_scale(image.shape[1])

    steps, final = replay(image, records)
    n_frames = make_gif(image, steps, final, out / "steps.gif", scale=scale)
    Image.fromarray(upscale(overlay_all_seams(image, steps), scale)).save(out / "overlay.png")
    Image.fromarray(comparison_figure(image, final, scale)).save(out / "comparison.png")
    Image.fromarray(upscale(energy_image(image), scale)).save(out / "energy.png")
    Image.fromarray(final).save(out / "carved.png")

    return {
        "seams": len(steps),
        "vertical": sum(s.record.vertical for s in steps),
        "horizontal": sum(not s.record.vertical for s in steps),
        "original_size": (image.shape[1], image.shape[0]),
        "final_size": (final.shape[1], final.shape[0]),
        "gif_frames": n_frames,
        "scale": scale,
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", required=True, help="original input image given to seam_carve")
    ap.add_argument("--seams", required=True, help="seams.json from seam_carve --export-seams")
    ap.add_argument("--out", default="bonus/out", help="output directory (default: bonus/out)")
    ap.add_argument("--scale", type=int, default=None, help="integer upscale factor (default: auto)")
    args = ap.parse_args(argv)

    summary = render_all(load_image(args.image), load_seams(args.seams), args.out, args.scale)
    ow, oh = summary["original_size"]
    fw, fh = summary["final_size"]
    print(
        f"Replayed {summary['seams']} seams ({summary['vertical']} vertical, {summary['horizontal']} horizontal): "
        f"{ow}x{oh} -> {fw}x{fh}. GIF has {summary['gif_frames']} frames (x{summary['scale']})."
    )
    print(f"Wrote steps.gif, overlay.png, comparison.png, energy.png, carved.png to {args.out}/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

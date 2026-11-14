"""Thin Python wrapper around the C++ graph-cut CLI (``bin/seam_carve``).

The app and tests call the compiled binary instead of re-implementing seam
carving in Python, so every result shown is produced by the real graph-cut code.

Other algorithms (DP, Greedy) can be plugged in later: anything that writes the
same seams.json format can be replayed and visualised by ``visualize.py``.
"""
from __future__ import annotations

import os
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

from visualize import SeamRecord, load_image, load_seams

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BINARY = REPO_ROOT / "bin" / "seam_carve"

SOLVERS = ("dinic", "edmonds_karp")
ENERGIES = ("dual", "sobel")


def find_binary() -> Path:
    """Locate the compiled CLI (override with the SEAM_CARVE_BIN env var)."""
    path = Path(os.environ.get("SEAM_CARVE_BIN", DEFAULT_BINARY))
    if not path.is_file():
        raise FileNotFoundError(f"seam_carve binary not found at {path}. Build it first with `make all`.")
    return path


@dataclass
class CarveResult:
    carved: np.ndarray
    records: list[SeamRecord]
    wall_ms: float
    log: str


def run_graph_cut(
    image: np.ndarray,
    remove_cols: int,
    remove_rows: int,
    solver: str = "dinic",
    energy: str = "dual",
) -> CarveResult:
    """Carve `image` with the C++ graph-cut implementation and return the seams."""
    h, w = image.shape[:2]
    if solver not in SOLVERS:
        raise ValueError(f"solver must be one of {SOLVERS}")
    if energy not in ENERGIES:
        raise ValueError(f"energy must be one of {ENERGIES}")
    if not (0 <= remove_cols < w and 0 <= remove_rows < h):
        raise ValueError(f"cannot remove {remove_cols} cols / {remove_rows} rows from a {w}x{h} image")

    binary = find_binary()
    with tempfile.TemporaryDirectory(prefix="seam_") as tmp:
        tmp = Path(tmp)
        src, dst, seams = tmp / "input.png", tmp / "carved.png", tmp / "seams.json"
        Image.fromarray(image).save(src)  # PNG is lossless, so C++ sees identical pixels
        cmd = [
            str(binary), "-i", str(src), "-o", str(dst),
            "--remove-cols", str(remove_cols), "--remove-rows", str(remove_rows),
            "--solver", solver, "--energy", energy,
            "--export-seams", str(seams),
        ]
        t0 = time.perf_counter()
        proc = subprocess.run(cmd, capture_output=True, text=True)
        wall_ms = (time.perf_counter() - t0) * 1000
        if proc.returncode != 0:
            raise RuntimeError(f"seam_carve failed ({proc.returncode}):\n{proc.stderr or proc.stdout}")
        return CarveResult(load_image(dst), load_seams(seams), wall_ms, proc.stdout)

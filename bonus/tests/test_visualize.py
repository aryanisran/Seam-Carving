"""Tests for the bonus visualiser and the C++ CLI integration.

Run from the repo root:  python -m pytest bonus/tests -v
The integration tests are skipped automatically if bin/seam_carve is not built.
"""
from __future__ import annotations

import io
import json

import numpy as np
import pytest
from PIL import Image

from carver import find_binary, run_graph_cut
from make_sample import synthetic_tower
from visualize import (
    SeamRecord,
    energy_map,
    make_gif,
    overlay_all_seams,
    parse_seams,
    remove_seam,
    render_all,
    replay,
    validate_seam,
)


# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #
def unique_image(h: int, w: int) -> np.ndarray:
    """Image whose pixels are all distinct, so we can track where each one went."""
    idx = np.arange(h * w).reshape(h, w)
    return np.stack([idx % 256, (idx // 256) % 256, idx // 65536], axis=-1).astype(np.uint8)


def random_seam(rng: np.random.Generator, length: int, limit: int) -> np.ndarray:
    """Random 8-connected seam (random walk clipped to the image)."""
    seam = [int(rng.integers(limit))]
    for _ in range(length - 1):
        seam.append(int(np.clip(seam[-1] + rng.integers(-1, 2), 0, limit - 1)))
    return np.array(seam)


def random_records(rng, h, w, n_vertical, n_horizontal) -> list[SeamRecord]:
    """Vertical seams first, then horizontal - the same order as the C++ CLI."""
    records = []
    for i in range(n_vertical):
        records.append(SeamRecord(i + 1, True, random_seam(rng, h, w), w, h))
        w -= 1
    for j in range(n_horizontal):
        records.append(SeamRecord(n_vertical + j + 1, False, random_seam(rng, w, h), w, h))
        h -= 1
    return records


# --------------------------------------------------------------------------- #
# Seam geometry
# --------------------------------------------------------------------------- #
def test_remove_vertical_seam():
    a = np.arange(12).reshape(3, 4)
    out = remove_seam(a, np.array([0, 1, 2]), vertical=True)
    assert out.tolist() == [[1, 2, 3], [4, 6, 7], [8, 9, 11]]


def test_remove_horizontal_seam():
    a = np.arange(12).reshape(3, 4)
    # remove row 0 in col 0, row 1 in col 1, row 2 in col 2, row 2 in col 3
    out = remove_seam(a, np.array([0, 1, 2, 2]), vertical=False)
    assert out.tolist() == [[4, 1, 2, 3], [8, 9, 6, 7]]


def test_remove_seam_keeps_channels():
    img = unique_image(5, 6)
    out = remove_seam(img, np.array([5, 4, 3, 4, 5]), vertical=True)
    assert out.shape == (5, 5, 3)
    assert out.dtype == np.uint8


@pytest.mark.parametrize(
    "seam, msg",
    [
        (np.array([0, 1]), "length"),  # too short
        (np.array([0, 1, 4]), "out of range"),  # col 4 in a 4-wide image
        (np.array([-1, 0, 0]), "out of range"),
        (np.array([0, 2, 2]), "8-connected"),  # jump of 2
    ],
)
def test_validate_rejects_bad_seams(seam, msg):
    with pytest.raises(ValueError, match=msg):
        validate_seam(seam, vertical=True, height=3, width=4)


def test_validate_accepts_edge_seams():
    validate_seam(np.array([0, 0, 0]), True, 3, 4)
    validate_seam(np.array([3, 3, 3]), True, 3, 4)
    validate_seam(np.array([0, 1, 2, 2]), False, 3, 4)


# --------------------------------------------------------------------------- #
# Replay / original-coordinate tracking
# --------------------------------------------------------------------------- #
@pytest.mark.parametrize("seed", range(10))
def test_replay_maps_seams_back_to_original_pixels(seed):
    """Every seam pixel's recorded original position must hold that exact pixel."""
    rng = np.random.default_rng(seed)
    h, w = int(rng.integers(5, 15)), int(rng.integers(5, 15))
    img = unique_image(h, w)
    records = random_records(rng, h, w, n_vertical=w // 2, n_horizontal=h // 2)

    steps, final = replay(img, records)

    assert final.shape == (h - h // 2, w - w // 2, 3)
    removed = set()
    for s in steps:
        assert np.array_equal(s.image_before[s.rows, s.cols], img[s.orig_rows, s.orig_cols])
        removed.update(zip(s.orig_rows.tolist(), s.orig_cols.tolist()))
    # Each original pixel is removed at most once, and removed + kept = everything
    assert len(removed) == sum(len(s.rows) for s in steps)
    assert len(removed) + final.shape[0] * final.shape[1] == h * w


def test_replay_rejects_mismatched_image():
    img = unique_image(4, 4)
    rec = SeamRecord(1, True, np.array([0, 0, 0, 0, 0]), width_before=4, height_before=5)
    with pytest.raises(ValueError, match="wrong input image"):
        replay(img, [rec])


def test_overlay_marks_every_removed_pixel():
    rng = np.random.default_rng(0)
    img = np.zeros((8, 10, 3), dtype=np.uint8)
    steps, _ = replay(img, random_records(rng, 8, 10, 4, 3))
    overlay = overlay_all_seams(img, steps)
    assert np.count_nonzero(overlay.any(axis=-1)) == sum(len(s.rows) for s in steps)


def test_parse_seams_json():
    data = json.loads(
        '{"seams": [{"step": 1, "orientation": "horizontal", "width_before": 3, "height_before": 2,'
        ' "energy_sum": 1.5, "max_flow": 1.5, "elapsed_ms": 0.1, "augmenting_paths": 2,'
        ' "bfs_runs": 3, "indices": [0, 1, 1]}]}'
    )
    (rec,) = parse_seams(data)
    assert not rec.vertical and rec.indices.tolist() == [0, 1, 1] and rec.bfs_runs == 3


# --------------------------------------------------------------------------- #
# Rendering
# --------------------------------------------------------------------------- #
def test_gif_frames_all_same_size():
    rng = np.random.default_rng(1)
    img = unique_image(12, 16)
    steps, final = replay(img, random_records(rng, 12, 16, 5, 4))
    buf = io.BytesIO()
    n = make_gif(img, steps, final, buf, scale=2)
    gif = Image.open(io.BytesIO(buf.getvalue()))
    assert gif.n_frames == n == len(steps) + 2  # original + one per seam + final
    assert gif.size == (32, 24)


def test_energy_map_matches_hand_computation():
    img = np.zeros((3, 3, 3), dtype=np.uint8)
    img[1, 2] = [30, 40, 0]  # right neighbour of the centre
    e = energy_map(img)
    assert e[1, 1] == pytest.approx(50.0)  # sqrt(30^2 + 40^2)
    assert e[0, 0] == 0.0


def test_render_all_writes_outputs(tmp_path):
    rng = np.random.default_rng(2)
    img = synthetic_tower(40, 30)
    summary = render_all(img, random_records(rng, 30, 40, 6, 3), tmp_path)
    assert summary["final_size"] == (34, 27)
    for name in ["steps.gif", "overlay.png", "comparison.png", "energy.png", "carved.png"]:
        assert (tmp_path / name).stat().st_size > 0


# --------------------------------------------------------------------------- #
# Integration with the C++ graph-cut binary
# --------------------------------------------------------------------------- #
def binary_available() -> bool:
    try:
        find_binary()
        return True
    except FileNotFoundError:
        return False


needs_binary = pytest.mark.skipif(not binary_available(), reason="bin/seam_carve not built (run `make all`)")


@needs_binary
@pytest.mark.parametrize("solver", ["dinic", "edmonds_karp"])
def test_python_replay_reproduces_cpp_output_exactly(solver):
    """Replaying the exported seams in Python must give the C++ output pixel-for-pixel."""
    img = synthetic_tower(48, 32)
    result = run_graph_cut(img, remove_cols=8, remove_rows=4, solver=solver)

    assert len(result.records) == 12
    assert [r.vertical for r in result.records] == [True] * 8 + [False] * 4
    _, final = replay(img, result.records)
    assert final.shape == (28, 40, 3)
    assert np.array_equal(final, result.carved)


@needs_binary
def test_seam_energy_equals_sum_of_energy_along_seam():
    """Max-flow min-cut: the exported cut value equals the seam's total energy."""
    img = synthetic_tower(40, 28)
    result = run_graph_cut(img, remove_cols=5, remove_rows=0)
    steps, _ = replay(img, result.records)
    for s in steps:
        expected = energy_map(s.image_before)[s.rows, s.cols].sum()
        assert s.record.energy == pytest.approx(expected, rel=1e-4)


@needs_binary
def test_dinic_and_edmonds_karp_find_equal_cost_seams():
    img = synthetic_tower(36, 24)
    d = run_graph_cut(img, 4, 0, solver="dinic")
    ek = run_graph_cut(img, 4, 0, solver="edmonds_karp")
    # First seam: same image, so the optimal cost must match (paths may differ on ties).
    assert d.records[0].energy == pytest.approx(ek.records[0].energy, rel=1e-5)


@needs_binary
def test_run_graph_cut_rejects_impossible_sizes():
    img = synthetic_tower(20, 10)
    with pytest.raises(ValueError):
        run_graph_cut(img, remove_cols=20, remove_rows=0)

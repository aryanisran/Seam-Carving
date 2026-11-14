"""Interactive seam-carving app (Bonus 7b).

Run from the repo root (inside WSL, after `make all`):
    streamlit run bonus/app.py

Workflow: pick/upload an image -> choose target width/height, solver and energy
-> "Carve". The C++ graph-cut binary does the work; this app only displays it.
After carving, the step slider scrubs through every intermediate size instantly
(no recomputation), so the user can interactively explore any width/height
between the original and the target.
"""
from __future__ import annotations

import hashlib
import io
import sys
from pathlib import Path

import numpy as np
import streamlit as st
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))

from carver import ENERGIES, REPO_ROOT, SOLVERS, find_binary, run_graph_cut  # noqa: E402
from visualize import (  # noqa: E402
    auto_scale,
    comparison_figure,
    energy_image,
    make_gif,
    overlay_all_seams,
    render_step,
    replay,
    upscale,
)

SAMPLES_DIR = REPO_ROOT / "samples"
SOLVER_LABELS = {"dinic": "Dinic", "edmonds_karp": "Edmonds-Karp"}
ENERGY_LABELS = {"dual": "Dual gradient", "sobel": "Sobel"}

st.set_page_config(page_title="Seam Carving - Graph Cut", layout="wide")


# --------------------------------------------------------------------------- #
# Cached helpers
# --------------------------------------------------------------------------- #
def shrink_to_width(image: np.ndarray, max_width: int) -> np.ndarray:
    h, w = image.shape[:2]
    if w <= max_width:
        return image
    new_h = max(2, round(h * max_width / w))
    return np.asarray(Image.fromarray(image).resize((max_width, new_h), Image.LANCZOS))


@st.cache_data(show_spinner=False)
def carve_cached(image_bytes: bytes, shape: tuple, cols: int, rows: int, solver: str, energy: str):
    image = np.frombuffer(image_bytes, dtype=np.uint8).reshape(shape)
    return run_graph_cut(image, cols, rows, solver, energy)


@st.cache_data(show_spinner=False)
def gif_bytes(image_bytes: bytes, shape: tuple, _result, key: tuple, scale: int) -> bytes:
    # `_result` is not hashed by Streamlit; `key` (the carve parameters) identifies it.
    image = np.frombuffer(image_bytes, dtype=np.uint8).reshape(shape)
    steps, final = replay(image, _result.records)
    buf = io.BytesIO()
    make_gif(image, steps, final, buf, scale=scale)
    return buf.getvalue()


def show(arr: np.ndarray, caption: str, scale: int) -> None:
    st.image(upscale(arr, scale), caption=caption)


# --------------------------------------------------------------------------- #
# Sidebar: input + settings
# --------------------------------------------------------------------------- #
st.title("Seam Carving with Graph Cut (Min-Cut / Max-Flow)")

try:
    find_binary()
except FileNotFoundError as exc:
    st.error(f"{exc}\n\nRun `make all` in the repo root, then refresh.")
    st.stop()

with st.sidebar:
    st.header("1. Image")
    uploaded = st.file_uploader("Upload an image", type=["png", "jpg", "jpeg", "bmp"])
    samples = sorted(p for p in SAMPLES_DIR.glob("*") if p.suffix.lower() in {".png", ".jpg", ".jpeg", ".bmp"})
    sample = None
    if uploaded is None and samples:
        sample = st.selectbox("...or pick a sample", samples, format_func=lambda p: p.name)

    max_width = st.slider(
        "Working width (px)", 60, 400, 160, step=10,
        help="Images are downscaled to this width first. Graph cut builds one graph node per pixel "
             "and runs max-flow for every seam, so large images are slow.",
    )

    st.header("2. Algorithm")
    solver = st.radio("Max-flow solver", SOLVERS, format_func=SOLVER_LABELS.get)
    energy = st.radio("Energy function", ENERGIES, format_func=ENERGY_LABELS.get)

if uploaded is not None:
    source = Image.open(uploaded)
    source_name = uploaded.name
elif sample is not None:
    source = Image.open(sample)
    source_name = sample.name
else:
    st.info("Upload an image or add one to the `samples/` folder to get started.")
    st.stop()

image = shrink_to_width(np.asarray(source.convert("RGB"), dtype=np.uint8), max_width)
H, W = image.shape[:2]
scale = auto_scale(W, target=360)

with st.sidebar:
    st.header("3. Target size")
    st.caption(f"Working image: {W} x {H}")
    target_w = st.slider("Target width", 2, W, max(2, round(W * 0.75)))
    target_h = st.slider("Target height", 2, H, H)
    carve_clicked = st.button("Carve", type="primary", width="stretch")

remove_cols, remove_rows = W - target_w, H - target_h
params = (hashlib.sha1(image.tobytes()).hexdigest(), remove_cols, remove_rows, solver, energy)

if carve_clicked:
    with st.spinner(f"Removing {remove_cols} vertical + {remove_rows} horizontal seams with {SOLVER_LABELS[solver]}..."):
        try:
            st.session_state["result"] = carve_cached(image.tobytes(), image.shape, remove_cols, remove_rows, solver, energy)
            st.session_state["params"] = params
        except Exception as exc:  # show CLI errors in the UI instead of crashing
            st.error(str(exc))
            st.stop()

result = st.session_state.get("result")
if result is None or st.session_state.get("params") != params:
    c1, c2 = st.columns(2)
    with c1:
        show(image, f"{source_name} ({W} x {H})", scale)
    with c2:
        show(energy_image(image), "Energy map (bright = important, avoided by seams)", scale)
    st.info("Choose a target size in the sidebar and press **Carve**.")
    st.stop()

steps, final = replay(image, result.records)
n = len(steps)

# --------------------------------------------------------------------------- #
# Results
# --------------------------------------------------------------------------- #
total_cpp_ms = sum(s.record.elapsed_ms for s in steps)
m1, m2, m3, m4 = st.columns(4)
m1.metric("Size", f"{final.shape[1]} x {final.shape[0]}", f"-{remove_cols} cols, -{remove_rows} rows", delta_color="off")
m2.metric("Seams removed", n)
m3.metric("Max-flow time (total)", f"{total_cpp_ms:.0f} ms")
m4.metric("Avg per seam", f"{total_cpp_ms / n:.1f} ms" if n else "-")

tab_steps, tab_compare, tab_overlay, tab_energy, tab_stats = st.tabs(
    ["Step by step", "Compare", "All seams", "Energy map", "Per-seam stats"]
)

with tab_steps:
    if n == 0:
        st.write("No seams were removed.")
    else:
        k = st.slider("Seams removed so far", 0, n, n, help="Scrub to see the image at any intermediate size.")
        if k < n:
            frame = render_step(steps[k])
            rec = steps[k].record
            caption = (
                f"Step {k + 1}/{n}: next {'vertical' if rec.vertical else 'horizontal'} seam "
                f"(energy {rec.energy:.1f}, {rec.elapsed_ms:.1f} ms) - size {frame.shape[1]} x {frame.shape[0]}"
            )
        else:
            frame, caption = final, f"Final result - {final.shape[1]} x {final.shape[0]}"
        show(frame, caption, scale)

        st.caption("Red = vertical seam, blue = horizontal seam.")
        if st.toggle("Show animation"):
            data = gif_bytes(image.tobytes(), image.shape, result, params, scale)
            st.image(data, caption="All steps")
            st.download_button("Download GIF", data, file_name="seam_steps.gif", mime="image/gif")

with tab_compare:
    st.image(comparison_figure(image, final, scale), caption="Seam carving keeps important content; scaling squashes it, cropping cuts it off.")

with tab_overlay:
    show(overlay_all_seams(image, steps), f"All {n} removed seams on the original image", scale)

with tab_energy:
    c1, c2 = st.columns(2)
    with c1:
        show(energy_image(image), "Energy of the original", scale)
    with c2:
        show(energy_image(final), "Energy after carving", scale)

with tab_stats:
    rows = [
        {
            "step": s.record.step,
            "orientation": "vertical" if s.record.vertical else "horizontal",
            "size before": f"{s.record.width_before}x{s.record.height_before}",
            "seam energy (= max flow)": round(s.record.energy, 2),
            "time (ms)": round(s.record.elapsed_ms, 2),
            "BFS runs": s.record.bfs_runs,
            "augmenting paths": s.record.augmenting_paths,
        }
        for s in steps
    ]
    if rows:
        st.line_chart({"time per seam (ms)": [r["time (ms)"] for r in rows]})
        st.dataframe(rows, width="stretch")
    st.caption(f"Total wall-clock time including process start-up: {result.wall_ms:.0f} ms")

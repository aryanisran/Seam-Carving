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

import base64
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
    replay,
    upscale,
)

SAMPLES_DIR = REPO_ROOT / "samples"
SOLVER_LABELS = {"dinic": "Dinic", "edmonds_karp": "Edmonds-Karp"}
ENERGY_LABELS = {"dual": "Dual gradient", "sobel": "Sobel"}
PREVIEW_ZOOM = 2  # fixed, so the preview size reflects the working width
WORKING_MIN, WORKING_MAX, WORKING_DEFAULT = 60, 400, 160

st.set_page_config(page_title="Seam Carving - Graph Cut", layout="wide")


# --------------------------------------------------------------------------- #
# Live working-width slider (custom component)
# --------------------------------------------------------------------------- #
# A normal st.slider only reports its value when released. This component runs
# the slider and the preview in the browser, so the image is resized *while*
# dragging; the final width is sent back to Python only on release.
LIVE_WIDTH_CSS = """
.ww-root { font-family: var(--st-font, sans-serif); color: var(--st-text-color); }
.ww-label { display: flex; justify-content: space-between; font-size: 0.9rem; }
.ww-range { width: 100%; max-width: 820px; accent-color: var(--st-primary-color); }
.ww-meta { font-size: 0.8rem; opacity: 0.75; margin: 2px 0 8px; }
.ww-view { display: block; max-width: 100%; image-rendering: pixelated; }
"""

LIVE_WIDTH_JS = """
export default function(component) {
  const { data, setStateValue, parentElement } = component;

  let root = parentElement.querySelector('.ww-root');
  const fresh = !root;
  if (fresh) {
    root = document.createElement('div');
    root.className = 'ww-root';
    root.innerHTML = `
      <div class="ww-label"><span>Working width (drag to resize)</span><b class="ww-val"></b></div>
      <input type="range" class="ww-range">
      <div class="ww-meta"></div>
      <canvas class="ww-view"></canvas>`;
    parentElement.appendChild(root);
  }
  const range = root.querySelector('.ww-range');
  const val = root.querySelector('.ww-val');
  const meta = root.querySelector('.ww-meta');
  const view = root.querySelector('.ww-view');

  range.min = data.min;
  range.max = data.max;
  range.step = 1;
  // The browser owns the value after the first render, so a rerun never
  // snaps the slider back while the user is dragging.
  if (fresh || root.dataset.src !== data.src) range.value = data.value;
  root.dataset.src = data.src;

  const img = new Image();
  const work = document.createElement('canvas');
  function draw() {
    if (!img.complete || !img.naturalWidth) return;
    const w = Number(range.value);
    const h = Math.max(2, Math.round(data.orig_h * w / data.orig_w));  // same rounding as Python
    work.width = w; work.height = h;
    const wctx = work.getContext('2d');
    wctx.imageSmoothingQuality = 'high';
    wctx.drawImage(img, 0, 0, w, h);                 // downscale = what carving will use
    view.width = w * data.zoom; view.height = h * data.zoom;
    const vctx = view.getContext('2d');
    vctx.imageSmoothingEnabled = false;              // show real pixels, blocky when small
    vctx.drawImage(work, 0, 0, view.width, view.height);
    val.textContent = `${w} px`;
    meta.textContent = `${w} x ${h} px = ${(w * h).toLocaleString()} graph nodes per max-flow`
      + ` (original ${data.orig_w} x ${data.orig_h})`;
  }
  img.onload = draw;
  img.src = data.src;
  range.oninput = draw;
  range.onchange = () => setStateValue('width', Number(range.value));
}
"""

live_width_slider = st.components.v2.component(
    "live_working_width", css=LIVE_WIDTH_CSS, js=LIVE_WIDTH_JS
)


def png_data_url(arr: np.ndarray) -> str:
    buf = io.BytesIO()
    Image.fromarray(arr).save(buf, format="PNG")
    return "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode("ascii")


def working_width_picker(original: np.ndarray) -> int:
    """Render the live slider + preview and return the chosen working width."""
    oh, ow = original.shape[:2]
    lo, hi = min(WORKING_MIN, ow), min(WORKING_MAX, ow)
    current = int(np.clip(st.session_state.get("working_width", WORKING_DEFAULT), lo, hi))
    result = live_width_slider(
        key="working_width_picker",
        data={
            "src": png_data_url(shrink_to_width(original, hi)),  # largest size the slider can reach
            "min": lo, "max": hi, "value": current,
            "orig_w": ow, "orig_h": oh, "zoom": PREVIEW_ZOOM,
        },
        on_width_change=lambda: None,
    )
    chosen = getattr(result, "width", None)
    if chosen is not None:
        current = int(np.clip(chosen, lo, hi))
    st.session_state["working_width"] = current
    return current


# --------------------------------------------------------------------------- #
# Live "seams removed" stepper (custom component)
# --------------------------------------------------------------------------- #
# Python sends the working image and the seam list once. The browser replays
# the removals itself (the same logic as visualize.remove_seam), so the image
# updates *while* the slider is dragged. Snapshots every CHECKPOINT seams keep
# scrubbing fast even for long carves.
SEAM_STEPPER_JS = """
const CHECKPOINT = 10;
const VERTICAL = [255, 0, 0];
const HORIZONTAL = [0, 120, 255];
const CANVAS_BG = 'rgb(32, 32, 32)';

function removeVertical(img, seam) {          // seam[r] = column removed in row r
  const { w, h, px } = img;
  const out = new Uint8ClampedArray((w - 1) * h * 4);
  let o = 0;
  for (let r = 0; r < h; r++) {
    for (let c = 0; c < w; c++) {
      if (c === seam[r]) continue;
      const i = (r * w + c) * 4;
      out[o++] = px[i]; out[o++] = px[i + 1]; out[o++] = px[i + 2]; out[o++] = 255;
    }
  }
  return { w: w - 1, h, px: out };
}

function removeHorizontal(img, seam) {        // seam[c] = row removed in column c
  const { w, h, px } = img;
  const out = new Uint8ClampedArray(w * (h - 1) * 4);
  for (let c = 0; c < w; c++) {
    let dr = 0;
    for (let r = 0; r < h; r++) {
      if (r === seam[c]) continue;
      const i = (r * w + c) * 4, j = (dr * w + c) * 4;
      out[j] = px[i]; out[j + 1] = px[i + 1]; out[j + 2] = px[i + 2]; out[j + 3] = 255;
      dr++;
    }
  }
  return { w, h: h - 1, px: out };
}

function removeSeam(img, s) {
  return s.v ? removeVertical(img, s.idx) : removeHorizontal(img, s.idx);
}

export default function(component) {
  const { data, parentElement } = component;

  let root = parentElement.querySelector('.ss-root');
  if (!root) {
    root = document.createElement('div');
    root.className = 'ss-root';
    root.innerHTML = `
      <div class="ss-label"><span>Seams removed so far (drag to scrub)</span><b class="ss-val"></b></div>
      <input type="range" class="ss-range" min="0" step="1">
      <canvas class="ss-view"></canvas>
      <div class="ss-caption"></div>`;
    parentElement.appendChild(root);
  }
  const range = root.querySelector('.ss-range');
  const val = root.querySelector('.ss-val');
  const view = root.querySelector('.ss-view');
  const caption = root.querySelector('.ss-caption');
  const seams = data.seams;
  const n = seams.length;

  function imageAt(k) {
    const st = root._state;
    const base = Math.floor(k / CHECKPOINT);
    let cur = st.checkpoints[base];
    for (let i = base * CHECKPOINT; i < k; i++) cur = removeSeam(cur, seams[i]);
    return cur;
  }

  function draw() {
    const st = root._state;
    if (!st || !st.checkpoints) return;
    const k = Number(range.value);
    const img = imageAt(k);
    const px = new Uint8ClampedArray(img.px);
    if (k < n) {                               // highlight the seam removed next
      const s = seams[k];
      const color = s.v ? VERTICAL : HORIZONTAL;
      for (let a = 0; a < s.idx.length; a++) {
        const r = s.v ? a : s.idx[a];
        const c = s.v ? s.idx[a] : a;
        const i = (r * img.w + c) * 4;
        px[i] = color[0]; px[i + 1] = color[1]; px[i + 2] = color[2];
      }
    }
    const off = document.createElement('canvas');
    off.width = img.w; off.height = img.h;
    off.getContext('2d').putImageData(new ImageData(px, img.w, img.h), 0, 0);

    const z = data.zoom;
    view.width = st.w0 * z; view.height = st.h0 * z;   // fixed canvas = original size
    const ctx = view.getContext('2d');
    ctx.fillStyle = CANVAS_BG;
    ctx.fillRect(0, 0, view.width, view.height);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(off, 0, 0, img.w * z, img.h * z);

    val.textContent = `${k} / ${n}`;
    if (k < n) {
      const s = seams[k];
      caption.textContent = `Step ${k + 1}/${n}: next ${s.v ? 'vertical' : 'horizontal'} seam `
        + `(energy ${s.e.toFixed(1)}, ${s.ms.toFixed(1)} ms) - size ${img.w} x ${img.h}`;
    } else {
      caption.textContent = `Final result - ${img.w} x ${img.h}`;
    }
  }

  range.oninput = draw;

  // Only rebuild when a new carve result arrives; plain reruns keep the position.
  if (root.dataset.key !== data.key) {
    root.dataset.key = data.key;
    root._state = null;
    range.max = n;
    range.value = n;
    const im = new Image();
    im.onload = () => {
      if (root.dataset.key !== data.key) return;   // a newer result replaced this one
      const c = document.createElement('canvas');
      c.width = im.naturalWidth; c.height = im.naturalHeight;
      const cctx = c.getContext('2d');
      cctx.drawImage(im, 0, 0);
      let cur = { w: c.width, h: c.height, px: cctx.getImageData(0, 0, c.width, c.height).data };
      const checkpoints = [cur];
      for (let i = 0; i < n; i++) {
        cur = removeSeam(cur, seams[i]);
        if ((i + 1) % CHECKPOINT === 0) checkpoints.push(cur);
      }
      root._state = { checkpoints, w0: c.width, h0: c.height };
      draw();
    };
    im.src = data.src;
  } else {
    draw();
  }
}
"""

SEAM_STEPPER_CSS = """
.ss-root { font-family: var(--st-font, sans-serif); color: var(--st-text-color); }
.ss-label { display: flex; justify-content: space-between; font-size: 0.9rem; }
.ss-range { width: 100%; max-width: 820px; accent-color: var(--st-primary-color); }
.ss-view { display: block; max-width: 100%; margin-top: 6px; image-rendering: pixelated; }
.ss-caption { font-size: 0.85rem; opacity: 0.75; margin-top: 4px; }
"""

seam_stepper = st.components.v2.component("live_seam_stepper", css=SEAM_STEPPER_CSS, js=SEAM_STEPPER_JS)


def live_seam_stepper(image: np.ndarray, steps: list, key: tuple, zoom: int) -> None:
    """Render the live step slider for one carve result."""
    seams = [
        {
            "v": bool(s.record.vertical),
            "idx": s.record.indices.tolist(),
            "e": float(s.record.energy),
            "ms": float(s.record.elapsed_ms),
        }
        for s in steps
    ]
    seam_stepper(
        key="live_seam_stepper",
        data={"key": str(key), "src": png_data_url(image), "zoom": zoom, "seams": seams},
    )


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

    st.caption(
        "Set the working width above the image. Graph cut builds one graph node per pixel "
        "and runs max-flow for every seam, so large images are slow."
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

original = np.asarray(source.convert("RGB"), dtype=np.uint8)
with st.expander("Working image size", expanded=st.session_state.get("result") is None):
    max_width = working_width_picker(original)
image = shrink_to_width(original, max_width)
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
    # The live preview of the working image is shown by the picker above.
    st.info(f"Working image: **{W} x {H}** px. Choose a target size in the sidebar and press **Carve**.")
    with st.expander("Energy map of the working image"):
        show(energy_image(image), "Energy map (bright = important, avoided by seams)", PREVIEW_ZOOM)
    st.stop()

steps, final = replay(image, result.records)
n = len(steps)

# --------------------------------------------------------------------------- #
# Results
# --------------------------------------------------------------------------- #
total_cpp_ms = sum(s.record.elapsed_ms for s in steps)
m1, m2, m3, m4 = st.columns(4)
# No metric deltas: Streamlit draws an arrow icon next to them.
m1.metric("Size", f"{final.shape[1]} x {final.shape[0]}")
m2.metric("Seams removed", n)
m3.metric("Max-flow time (total)", f"{total_cpp_ms:.0f} ms")
m4.metric("Avg per seam", f"{total_cpp_ms / n:.1f} ms" if n else "-")
st.caption(f"From {W} x {H}: removed {remove_cols} columns and {remove_rows} rows.")

tab_steps, tab_compare, tab_overlay, tab_energy, tab_stats = st.tabs(
    ["Step by step", "Compare", "All seams", "Energy map", "Per-seam stats"]
)

with tab_steps:
    if n == 0:
        st.write("No seams were removed.")
    else:
        live_seam_stepper(image, steps, params, scale)
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

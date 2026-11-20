# Contributor & Developer Guide

Welcome to the **Content-Aware Image Resizing (Seam Carving)** repository!

This guide covers the repository architecture, shared C++ contracts, how to extend algorithms or visualizers, and how to run the C++, Web, and Python test suites.

---

## 1. High-Level Architecture & Shared Contracts

All seam-finding algorithms operate on shared image containers and energy representations, and return standardized seam coordinates.

```
                           ┌────────────────────────┐
                           │    seam_carving::      │
                           │         Image          │
                           └───────────┬────────────┘
                                       │
                                       ▼
                           ┌────────────────────────┐
                           │    seam_carving::      │
                           │       EnergyMap        │
                           └───────────┬────────────┘
                                       │
         ┌─────────────────────────────┼─────────────────────────────┐
         ▼                             ▼                             ▼
┌──────────────────┐         ┌──────────────────┐         ┌───────────────────┐
│   DPSeamFinder   │         │ GreedySeamFinder │         │GraphCutSeamFinder │
│(Dynamic Program) │         │ (Greedy Search)  │         │ (Min-Cut/Max-Flow)│
└────────┬─────────┘         └────────┬─────────┘         └─────────┬─────────┘
         │                            │                             │
         └────────────────────────────┼─────────────────────────────┘
                                      │ returns SeamResult
                                      ▼
                           ┌────────────────────────┐
                           │      SeamCarver        │
                           └───────────┬────────────┘
                                       │
                    ┌──────────────────┴──────────────────┐
                    ▼                                     ▼
        Image Pixel Removal                       Export Seams JSON
      (img.remove_seam(...))                     (--export-seams)
                                                          │
                                                          ▼
                                              ┌───────────────────────┐
                                              │ bonus/visualize.py    │
                                              │ & React Visualizer    │
                                              └───────────────────────┘
```

### Shared Types & Headers

1. **`seam_carving::Image`** ([`include/image.hpp`](include/image.hpp)):
   - `int width() const`, `int height() const`, `int channels() const`
   - `Pixel get_pixel(int row, int col) const`, `void set_pixel(int row, int col, const Pixel& p)`
   - `Image transposed() const`: Returns a transposed copy ($W' = H, H' = W$).
   - `void remove_vertical_seam(const std::vector<int>& seam)`: Removes 1 pixel per row according to column indices `seam[r]`.
   - `void remove_horizontal_seam(const std::vector<int>& seam)`: Removes 1 pixel per column according to row indices `seam[c]`.

2. **`seam_carving::EnergyMap`** ([`include/energy.hpp`](include/energy.hpp)):
   - Computed via `compute_energy(const Image& img, EnergyMetric metric)`.
   - Supports `EnergyMetric::DualGradient` (default) and `EnergyMetric::Sobel`.
   - Accessors: `energy.height()`, `energy.width()`, `energy.at(row, col)`.
   - Transpose: `energy.transposed()`.

3. **`seam_carving::SeamResult`** ([`include/graph_cut_seam.hpp`](include/graph_cut_seam.hpp)):
   Every seam finder returns a `SeamResult`:
   ```cpp
   namespace seam_carving {
   struct SeamResult {
       std::vector<int> seam; // seam[r] = col for vertical; seam[c] = row for horizontal
       double energy_sum = 0.0; // Sum of energy values along the seam
       bool valid = false;      // True if seam satisfies bounds and 8-connectivity
   };
   }
   ```

4. **Seam Validity Rules (8-Connectivity)**:
   - For a vertical seam in an $H \times W$ image:
     - `seam.size() == H`
     - $0 \le \text{seam}[r] < W$ for all $0 \le r < H$
     - $|\text{seam}[r] - \text{seam}[r+1]| \le 1$ for all $0 \le r < H - 1$
   - Helper function: `GraphCutSeamFinder::validate_vertical_seam(seam, height, width)` in [`include/graph_cut_seam.hpp`](include/graph_cut_seam.hpp).

---

## 2. Repository Layout & File Conventions

To keep changes modular and avoid merge conflicts, each component lives in its own directory:

| Area | Files | Purpose |
| :--- | :--- | :--- |
| **Core Engine** | [`include/image.hpp`](include/image.hpp), [`src/image.cpp`](src/image.cpp), [`include/energy.hpp`](include/energy.hpp), [`src/energy.cpp`](src/energy.cpp) | Image loading/saving (`stb_image`) and energy map computation |
| **Graph Cut** | [`include/graph.hpp`](include/graph.hpp), [`src/graph.cpp`](src/graph.cpp), [`include/maxflow.hpp`](include/maxflow.hpp), [`src/maxflow.cpp`](src/maxflow.cpp), [`include/graph_cut_seam.hpp`](include/graph_cut_seam.hpp), [`src/graph_cut_seam.cpp`](src/graph_cut_seam.cpp) | Dinic / Edmonds-Karp max-flow solvers and min-cut seam extraction |
| **DP & Greedy** | [`include/dp_seam.hpp`](include/dp_seam.hpp), [`src/dp_seam.cpp`](src/dp_seam.cpp), [`include/greedy_seam.hpp`](include/greedy_seam.hpp), [`src/greedy_seam.cpp`](src/greedy_seam.cpp) | Dynamic Programming and Greedy seam finders |
| **Carver & CLI** | [`include/seam_carver.hpp`](include/seam_carver.hpp), [`src/seam_carver.cpp`](src/seam_carver.cpp), [`include/benchmark.hpp`](include/benchmark.hpp), [`src/benchmark.cpp`](src/benchmark.cpp), [`src/main.cpp`](src/main.cpp) | Carving orchestration, JSON/CSV export, and `bin/seam_carve` CLI |
| **C++ Tests & Benchmarks** | [`tests/test_graph_cut.cpp`](tests/test_graph_cut.cpp), [`tests/test_dp.cpp`](tests/test_dp.cpp), [`tests/test_greedy.cpp`](tests/test_greedy.cpp), [`tests/compare_greedy_dp.cpp`](tests/compare_greedy_dp.cpp), [`tests/benchmark_algorithms.cpp`](tests/benchmark_algorithms.cpp) | Unit tests, DP vs. Greedy comparison, and OpenMP benchmarks |
| **Python Bonus Suite** | [`bonus/carver.py`](bonus/carver.py), [`bonus/visualize.py`](bonus/visualize.py), [`bonus/make_sample.py`](bonus/make_sample.py), [`bonus/tests/`](bonus/tests/) | Replay visualizer (GIF, overlay, comparison panels) and pytest suite |
| **Web Visualizer** | [`web/src/`](web/src/), [`web/tests/algo.test.ts`](web/tests/algo.test.ts) | Interactive React + TypeScript visualizer and C++-parity tests |
| **CI/CD** | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) | Automated C++, Web, and Python tests, demo artifacts, Pages deploy, and Releases |

---

## 3. Extending Algorithms & the CLI

When adding a new seam-finding algorithm or solver:

1. **Implement the Seam Finder**: Add a dedicated header/source pair in `include/` and `src/` returning `SeamResult` and passing `validate_vertical_seam`.
2. **Wire into `SeamCarver`**: Register it in `AlgorithmType` in [`include/seam_carver.hpp`](include/seam_carver.hpp) and dispatch to it in [`src/seam_carver.cpp`](src/seam_carver.cpp).
3. **Expose via CLI**: Add the corresponding `--algorithm` option in [`src/main.cpp`](src/main.cpp).
4. **Update [`Makefile`](Makefile)**: Add the new `.cpp` file to `CORE_SRCS`, add a test binary target, and include it in the `test` target.

---

## 4. Bonus & Web Visualizers

1. **Python Replay Visualizer (`bonus/`)**:
   Any algorithm wired into `SeamCarver` can export seams via `--export-seams <path>.json` and render offline visualizations (`steps.gif`, `overlay.png`, `comparison.png`, `energy.png`):
   ```bash
   python bonus/visualize.py --image samples/synthetic_tower.png --seams seams.json --out out/
   ```

2. **Web Visualizer (`web/`)**:
   The React + Vite app runs TypeScript ports of the algorithms (`web/src/algo/`) in a Web Worker so it can be hosted as a static site:
   ```bash
   cd web
   npm ci
   npm run dev          # local dev server
   npm test             # checks TypeScript algorithms seam-for-seam against bin/seam_carve
   npm run build        # type-checks (tsc --noEmit) and builds web/dist/
   ```
   > **Important:** Any changes to algorithm behavior in C++ must be mirrored in `web/src/algo/*.ts`, and `web/tests/algo.test.ts` must continue to pass seam-for-seam and pixel-for-pixel against `bin/seam_carve`.

---

## 5. Testing, Benchmarks & Quality Standards

- **Language Standard**: C++17 (`-std=c++17`).
- **Compiler Flags**: `-Wall -Wextra -Wpedantic -O3` — keep compilation clean with zero warnings.
- **Dependencies**: Header-only `stb_image.h` / `stb_image_write.h` in [`include/third_party/`](include/third_party/). Do not add heavy external C++ dependencies.

### Verification Commands

- **C++ Build & Unit Tests** (via Linux/WSL or bash with `g++` and `make`):
  ```bash
  make clean && make all && make test
  ```
- **C++ Comparison & OpenMP Benchmarks**:
  ```bash
  make compare
  make benchmark
  ```
- **Web Visualizer Tests & Build** (requires `bin/seam_carve` to be built first):
  ```bash
  cd web && npm test && npm run build
  ```
- **Python Bonus Tests**:
  ```bash
  pip install -r bonus/requirements.txt
  python -m pytest bonus/tests -v
  ```

---

## 6. CI/CD & GitHub Pages Deployment

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) runs automatically on pushes, pull requests, and `workflow_dispatch`:

- **`build-and-test`**: Builds `bin/seam_carve`, runs `make test`, runs `web/` Vitest parity tests + `npm run build`, runs `pytest bonus/tests`, and uploads sample demo artifacts.
- **`deploy-web`**: On `push` or `workflow_dispatch` to `main` (when `vars.ENABLE_PAGES == 'true'`), configures GitHub Pages via `actions/configure-pages@v5`, builds `web/` with `BASE_PATH: ${{ steps.pages.outputs.base_path }}/`, and deploys `web/dist` to GitHub Pages.
  - One-time repo setup: **Settings → Pages → Source: `GitHub Actions`**, and **Settings → Secrets and variables → Actions → Variables → `ENABLE_PAGES` = `true`**.
- **`release`**: Pushing a tag matching `v*` publishes a GitHub Release with `bin/seam_carve` and demo outputs.

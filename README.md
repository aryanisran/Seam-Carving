# Content-Aware Image Resizing (Seam Carving)
A C++17 implementation of Dynamic Programming, Greedy, and Graph Cut (Min-Cut / Max-Flow) Solvers.

This repository provides a modular, high-performance content-aware image resizing suite implementing three core seam-finding paradigms:
1. **Dynamic Programming (DP)**: Globally optimal seam selection via cumulative energy memoization ($O(W \cdot H)$).
2. **Greedy Algorithm**: Fast, local row-by-row minimum-energy decision heuristic ($O(H)$).
3. **Graph Cut (Min-Cut / Max-Flow)**: Seam finding reformulated as a minimum $s-t$ cut and solved via Dinic's Algorithm ($O(V^2 E)$) and Edmonds-Karp ($O(V E^2)$).

Targeted to build and run seamlessly under **WSL (Windows Subsystem for Linux)** with the **GNU toolchain** (`g++`, `make`). Uses embedded `stb_image` and `stb_image_write` for self-contained image loading and saving without external package dependencies.
---

## Table of Contents
1. [Algorithm Implementation Matrix](#algorithm-implementation-matrix)
2. [Theoretical Formulations](#theoretical-formulations)
   - [2.1 Dynamic Programming](#21-dynamic-programming)
   - [2.2 Greedy Algorithm](#22-greedy-algorithm)
   - [2.3 Graph Cut (Min-Cut / Max-Flow)](#23-graph-cut-min-cut--max-flow)
3. [Project Structure](#project-structure)
4. [Building & Testing](#building--testing)
5. [CLI Usage & Options](#cli-usage--options)
6. [Benchmarking & Complexity Analysis](#benchmarking--complexity-analysis)
   - [Theoretical Complexity Comparison](#theoretical-complexity-comparison)
   - [Empirical Benchmark Run](#empirical-benchmark-run-8060-textured-image)
   - [Performance & Memory Optimizations](#performance--memory-optimizations)
7. [Bonus Features](#bonus-features)
   - [7a. Step-by-Step Visualizer](#7a-step-by-step-visualizer)
   - [7b. Interactive Web Application](#7b-interactive-web-application)
   - [7c. CI/CD & Automated Releases](#7c-cicd--automated-releases)
   - [Standardized Seam Export JSON Format](#standardized-seam-export-json-format)
   - [Programmatic C++ API](#programmatic-c-api)
8. [AI Usage Declaration](#ai-usage-declaration)

---

## Algorithm Implementation Matrix

| Paradigm | Component | Source Location | Description |
|---|---|---|---|
| **Dynamic Programming** | `DPSeamFinder` | [`include/dp_seam.hpp`](include/dp_seam.hpp)<br>[`src/dp_seam.cpp`](src/dp_seam.cpp) | Bottom-up 2D cumulative energy table $M(i, j)$, backtracking, transposition for horizontal seams. |
| **Greedy Search** | `GreedySeamFinder` | [`include/greedy_seam.hpp`](include/greedy_seam.hpp)<br>[`src/greedy_seam.cpp`](src/greedy_seam.cpp) | Local step heuristic selecting the minimum-energy neighbor at each row. |
| **Graph Cut (Dinic)** | `GraphCutSeamFinder`<br>`DinicSolver` | [`include/graph_cut_seam.hpp`](include/graph_cut_seam.hpp)<br>[`src/maxflow.cpp`](src/maxflow.cpp) | Layered level graph BFS + blocking flow DFS on $s-t$ flow network. |
| **Graph Cut (Edmonds-Karp)** | `GraphCutSeamFinder`<br>`EdmondsKarpSolver` | [`include/graph_cut_seam.hpp`](include/graph_cut_seam.hpp)<br>[`src/maxflow.cpp`](src/maxflow.cpp) | Shortest augmenting path BFS on residual network. |
| **Visualizer & Replay** | Python Replay Engine | [`bonus/visualize.py`](bonus/visualize.py) | Seam validation, animated GIF replay, overlay diagrams, comparison panels. |
| **Interactive App** | Streamlit Web UI | [`bonus/app.py`](bonus/app.py) | Interactive image upload, parameter tuning, live carving, and scrubber. |
| **CI/CD Pipeline** | GitHub Actions | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) | Automated build, C++ test suite, Python test suite, smoke test, and release tagging. |

---

## Theoretical Formulations

Seam carving resizes an image by iteratively finding and removing connected paths of pixels (seams) having the lowest cumulative disruption (energy).

### 2.1 Dynamic Programming

TODO

---

### 2.2 Greedy Algorithm

TODO

---

### 2.3 Graph Cut (Min-Cut / Max-Flow)

Seam finding is reformulated as finding a **minimum $s-t$ cut** in a directed flow network $G = (V, E)$.

#### 1. Graph Topology (Vertical Seams)
For an image of size $H \times W$:
- **Vertices**:
  - Source node $S = 0$
  - Sink node $T = H \times W + 1$
  - Pixel vertex: $v_{r, c} = 1 + r \times W + c$ for $0 \le r < H$, $0 \le c < W$.
  - Total vertices: $|V| = H \cdot W + 2$.

- **Edges & Capacities**:
  1. **Source Boundary**: $S \to (r, 0)$ with capacity $\infty$ for all rows $0 \le r < H$.
  2. **Sink Boundary**: $(r, W - 1) \to T$ with capacity equal to $e(r, W - 1)$ for all rows $0 \le r < H$.
  3. **Horizontal Forward Edges**: $(r, c) \to (r, c + 1)$ with capacity $C((r, c), (r, c+1)) = e(r, c)$ for $0 \le c < W - 1$.
  4. **Horizontal Backward Edges**: $(r, c + 1) \to (r, c)$ with capacity $\infty$.
  5. **Diagonal Constraint Edges**:
     - $(r, c) \to (r - 1, c - 1)$ with capacity $\infty$ (for $r > 0, c > 0$).
     - $(r, c) \to (r + 1, c - 1)$ with capacity $\infty$ (for $r < H - 1, c > 0$).

```
        Source (S)
       /    |    \  (inf capacity)
      v     v     v
   (0,0)  (1,0)  (2,0) ...
     |  \  ^  /   |
  e  |   inf inf  | e
     v  /  v  \   v
   (0,1)  (1,1)  (2,1) ...
     |     |      |
  e  |     | e    | e
     v     v      v
   (0,2)  (1,2)  (2,2) ...
      \     |    /
       v    v   v   (capacity e(r, W-1))
         Sink (T)
```

#### 2. Proof of Correctness
- **Monotonicity (Exactly 1 pixel per row)**: The backward edge $(r, c + 1) \to (r, c)$ has infinite capacity. Any finite cut cannot leave $(r, c + 1) \in S_{cut}$ and $(r, c) \in T_{cut}$. Thus, in each row $r$, there is a unique transition column $c_r$ where all pixels $\le c_r$ belong to $S_{cut}$ and all pixels $> c_r$ belong to $T_{cut}$.
- **8-Connected Continuity ($|c_r - c_{r\pm 1}| \le 1$)**: The diagonal edges $(r, c) \to (r - 1, c - 1)$ and $(r, c) \to (r + 1, c - 1)$ have infinite capacity. If row $r$ cuts at $c_r$, then $(r, c_r) \in S_{cut}$. To avoid cutting an infinite edge, $(r - 1, c_r - 1)$ and $(r + 1, c_r - 1)$ must also belong to $S_{cut}$. This guarantees $c_{r\pm 1} \ge c_r - 1$, i.e., $c_r - c_{r\pm 1} \le 1$. By adjacent row symmetry, $|c_r - c_{r\pm 1}| \le 1$.
- **Min-Cut = Minimum Seam Energy**: The capacity of this cut is $\sum_{r=0}^{H-1} e(r, c_r)$. By the Max-Flow Min-Cut Theorem, the maximum flow from $S$ to $T$ equals the minimum cut capacity, yielding the globally optimal seam.

#### 3. Supported Max-Flow Solvers
1. **Edmonds-Karp (Without Dinic)**:
   - Uses Breadth-First Search (BFS) to find the shortest augmenting path in the residual graph.
   - Theoretical Complexity: $O(V E^2)$.
   - Serves as the classic textbook baseline.
2. **Dinic's Algorithm (With Dinic)**:
   - Constructs a layered level graph via BFS, then uses Depth-First Search (DFS) with current-arc pointer optimization to push blocking flows.
   - Theoretical Complexity: $O(V^2 E)$.
   - Delivers substantial practical speedups on grid networks.

---

## Project Structure

```
├── Makefile                    # GNU Makefile (g++ -O3 -std=c++17)
├── CONTRIBUTING.md             # Developer onboarding guide & API contracts
├── README.md                   # Project documentation & architecture overview
├── .gitignore                  # Excludes binaries, build objects, and outputs
├── .github/workflows/
│   └── ci.yml                  # GitHub Actions CI/CD (build, test, smoke test, releases)
├── bonus/                      # Bonus features
│   ├── app.py                  # Streamlit interactive web application
│   ├── carver.py               # Python wrapper executing bin/seam_carve
│   ├── make_sample.py          # Synthetic test image generator
│   ├── README.md               # Bonus module documentation & instructions
│   ├── requirements.txt        # Python dependencies (streamlit, pillow, numpy, pytest)
│   ├── visualize.py            # Step-by-step seam replay, GIF, overlay, comparison
│   └── tests/                  # Pytest suite (29 tests)
│       ├── conftest.py
│       └── test_visualize.py   # Seam validation, replay, pixel-exact match tests
├── samples/
│   └── synthetic_tower.png     # Benchmark test image
├── include/
│   ├── image.hpp               # Image container, pixel access, transpose, seam removal
│   ├── energy.hpp              # Dual-gradient & Sobel filter gradient calculations
│   ├── graph.hpp               # Adjacency list representation with residual capacities
│   ├── maxflow.hpp             # Abstract MaxFlowSolver, Dinic, and Edmonds-Karp solvers
│   ├── graph_cut_seam.hpp      # s-t network construction & min-cut seam extraction
│   ├── dp_seam.hpp             # DP Seam Finder header
│   ├── greedy_seam.hpp         # Greedy Seam Finder header
│   ├── seam_carver.hpp         # Seam carving orchestrator and JSON exporter
│   ├── benchmark.hpp           # Solver benchmarking & comparison harness
│   └── third_party/
│       ├── stb_image.h         # Header-only image loader
│       └── stb_image_write.h   # Header-only image writer
├── src/
│   ├── image.cpp
│   ├── energy.cpp
│   ├── graph.cpp
│   ├── maxflow.cpp
│   ├── graph_cut_seam.cpp
│   ├── dp_seam.cpp             # DP Seam Finder implementation
│   ├── greedy_seam.cpp         # Greedy Seam Finder implementation
│   ├── seam_carver.cpp
│   ├── benchmark.cpp
│   └── main.cpp                # CLI application
└── tests/
    ├── test_graph_cut.cpp      # Graph Cut unit test suite
    ├── test_dp.cpp             # DP unit test suite
    └── test_greedy.cpp         # Greedy unit test suite
```

---

## Building & Testing

### 1. C++ Core (WSL / Linux)

#### Prerequisites
In WSL (e.g., Ubuntu):
```bash
sudo apt update
sudo apt install build-essential
```

#### Build & Test Commands
```bash
# Build the main CLI executable (bin/seam_carve)
make all

# Build and execute unit tests
make test

# Clean build artifacts
make clean
```

The C++ unit tests verify:
- Known global minimum cut on synthetic grids.
- Boundary edge conditions (leftmost and rightmost columns).
- Prevention of jump discontinuities ($|c_r - c_{r\pm 1}| > 1$).
- Horizontal seam extraction via transposition.
- Energy equivalence and non-zero flow across both Dinic and Edmonds-Karp.

### 2. Python Bonus Suite & Visualizer

#### Prerequisites
```bash
# Setup virtual environment
python3 -m venv .venv
source .venv/bin/activate        # On Windows: .venv\Scripts\activate

# Install dependencies
pip install -r bonus/requirements.txt
```

#### Running Python Tests
```bash
# Run all 29 pytest tests
python -m pytest bonus/tests -v
```

The Python test suite verifies:
- JSON parsing and seam record schema validation.
- Boundary bounds checking and 8-connectivity validation.
- Transposition handling for horizontal seam replay.
- **Pixel-exact equivalence** checking that Python seam replay matches the output of `bin/seam_carve` byte-for-byte.

---

## CLI Usage & Options

The compiled CLI binary is located at `bin/seam_carve`.

```
Usage: ./bin/seam_carve -i <input> [options]

Options:
  -i, --input <path>        Input image file (JPG, PNG, BMP) [Required]
  -o, --output <path>       Output image file (JPG, PNG, BMP)
  --remove-cols <n>         Number of vertical seams to remove (default: 0)
  --remove-rows <n>         Number of horizontal seams to remove (default: 0)
  --width <w>               Target width (overrides --remove-cols)
  --height <h>              Target height (overrides --remove-rows)
  --solver <name>           Max-flow solver: 'dinic' (default) or 'edmonds_karp'
  --energy <name>           Energy metric: 'dual' (default) or 'sobel'
  --compare                 Run both Dinic and Edmonds-Karp and compare
  --export-csv <path>       Export benchmark comparison table to CSV
  --export-seams <path>     Export carved seam coordinates to JSON
  -h, --help                Show this help message
```

### Example Commands

#### 1. Resize an image using Dinic's Algorithm
```bash
./bin/seam_carve -i input.jpg -o output_carved.jpg --remove-cols 50 --remove-rows 20 --solver dinic
```

#### 2. Resize using Edmonds-Karp (Without Dinic)
```bash
./bin/seam_carve -i input.jpg -o output_ek.jpg --remove-cols 10 --solver edmonds_karp
```

#### 3. Run Benchmark Comparison (Dinic vs. Edmonds-Karp)
```bash
./bin/seam_carve -i input.jpg --remove-cols 5 --remove-rows 5 --compare --export-csv benchmark.csv
```

#### 4. Export Seam Data for Visualizer
```bash
./bin/seam_carve -i input.jpg -o out.jpg --remove-cols 30 --export-seams seams.json
```

---

## Benchmarking & Complexity Analysis

### Theoretical Complexity Comparison
| Metric | Greedy Algorithm | Dynamic Programming | Edmonds-Karp (Graph Cut) | Dinic's Algorithm (Graph Cut) |
|---|---|---|---|---|
| **Optimality Guarantee** | None (Local Minima) | **Global Minimum** | **Global Minimum** | **Global Minimum** |
| **Augmentation / Search Strategy** | Local 3-pixel minimum per row | Cumulative 2D energy table + backtrack | Single shortest path BFS | Layered graph BFS + blocking flow DFS |
| **Time Complexity (per seam)** | $O(W \cdot H)$ energy + $O(H)$ seam | $O(W \cdot H)$ | $O(V E^2)$ where $\|V\| \approx WH$ | $O(V^2 E)$ where $\|V\| \approx WH$ |
| **Space Complexity** | $O(1)$ extra space | $O(W \cdot H)$ table ($O(W)$ if value only) | $O(V + E)$ residual graph | $O(V + E)$ residual graph + levels |

### Empirical Benchmark Run (80x60 Textured Image)
Testing 3 vertical seams followed by 2 horizontal seams under identical graph conditions:

| Seam # | Dir | Size | Dinic Energy | EK Energy | Match? | Dinic (ms) | EK (ms) | Dinic BFS | EK BFS | Speedup |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | V | 80x60 | 3061.68 | 3061.68 | **YES** | 3.07 | 6.88 | 31 | 153 | **2.24x** |
| 2 | V | 79x60 | 3158.94 | 3158.94 | **YES** | 3.90 | 6.96 | 38 | 172 | **1.78x** |
| 3 | V | 78x60 | 3252.63 | 3252.63 | **YES** | 3.60 | 7.75 | 38 | 184 | **2.15x** |
| 4 | H | 77x60 | 5941.79 | 5941.79 | **YES** | 4.34 | 13.10 | 43 | 320 | **3.02x** |
| 5 | H | 77x59 | 5928.85 | 5928.85 | **YES** | 4.28 | 12.95 | 42 | 329 | **3.03x** |

- **Cumulative Dinic Time**: `19.19 ms`
- **Cumulative Edmonds-Karp Time**: `47.63 ms`
- **Overall Speedup**: **`2.48x`**
- **Optimality Verification**: Dinic and Edmonds-Karp produce **100% identical cut energies** across every single carved seam.

## Bonus Features

| Bonus | What | Implementation |
|---|---|---|
| **7a. Step-by-Step Visualisation** | Animated GIF highlighting each seam before removal; all seams mapped to original coordinates; 4-way comparison (seam carving vs. scaling vs. cropping); energy map render | [`bonus/visualize.py`](bonus/visualize.py) |
| **7b. User Interaction** | Streamlit web application: upload image, adjust target dimensions, choose solver/energy, carve, and scrub through intermediate steps | [`bonus/app.py`](bonus/app.py) |
| **7c. CI/CD & Releases** | Automated GitHub Actions workflow: C++ build & tests, Python test suite, Streamlit smoke test, demo artifacts, and tag-triggered GitHub Releases | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) |

> **Design Note:** The bonus layer never reimplements seam finding. Every seam originates from the C++ engine via `--export-seams` JSON. The Python suite simply replays, validates, and renders the result. A test verifies that the Python replay matches the C++ output **pixel for pixel**.

---

### 7a. Step-by-Step Visualizer

`bonus/visualize.py` replays the JSON seam records against the original image to produce comprehensive analysis artifacts:

- **`steps.gif`**: Animated progression with each horizontal/vertical seam rendered in red before removal.
- **`overlay.png`**: Original image with all removed seams drawn simultaneously. Reconstructed by maintaining an $O(HW)$ coordinate tracking grid across seam steps.
- **`comparison.png`**: Side-by-side comparison figure displaying:
  1. *Original Image*
  2. *Seam Carving* (content-aware)
  3. *Scaling* (Lanczos downsampling)
  4. *Cropping* (center crop)
- **`energy.png`**: Visual representation of the dual-gradient energy landscape.

#### Usage:
```bash
# 1. Carve image and export seam metadata in C++
./bin/seam_carve -i samples/synthetic_tower.png -o carved.png --remove-cols 40 --remove-rows 15 --export-seams seams.json

# 2. Render all visualisations to bonus/out/
python bonus/visualize.py --image samples/synthetic_tower.png --seams seams.json --out bonus/out
```

---

### 7b. Interactive Web Application

An interactive web application built with [Streamlit](https://streamlit.io/) provides hands-on exploration of seam carving:

- **Live Carving**: Choose from built-in samples or upload custom images.
- **Configurable Parameters**: Adjust columns/rows to remove, algorithm solver, and gradient metric.
- **Interactive Scrubber**: After carving, scrub smoothly through every intermediate image size without re-running flow computations, replaying saved seam records on demand.
- **Execution Diagnostics**: Displays wall-clock runtime, max-flow values, and step breakdown logs.

#### Running the App:
```bash
streamlit run bonus/app.py
```
*Open http://localhost:8501 in your browser.*

---

### 7c. CI/CD & Automated Releases

The repository includes a production-grade GitHub Actions pipeline ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)):

1. **C++ CI**: Compiles using `g++ -O3 -Wall -Wextra -Wpedantic` (`make all`) and runs the unit test suite (`make test`).
2. **Python CI**: Sets up Python 3.11, installs dependencies, and runs all 29 `pytest` test cases.
3. **App Smoke Test**: Boots the Streamlit app headlessly and validates responsiveness via health-check probes.
4. **End-to-End Demo**: Carves a synthetic test image, generates all visualization outputs, and uploads them as downloadable workflow artifacts.
5. **Continuous Deployment (CD)**: Pushing a git tag (e.g. `v1.0`) automatically publishes a GitHub Release containing prebuilt binaries, demonstration GIFs, and benchmark figures.

---

### Standardized Seam Export JSON Format

The C++ binary and Python visualization layer communicate via a standardized, decoupled JSON interface (`--export-seams`):

```json
{
  "seams": [
    {
      "step": 1,
      "orientation": "vertical",
      "width_before": 800,
      "height_before": 600,
      "energy_sum": 451.25,
      "max_flow": 451.25,
      "elapsed_ms": 12.4,
      "augmenting_paths": 28,
      "bfs_runs": 8,
      "indices": [120, 121, 121, 122]
    }
  ]
}
```
- For vertical seams: `indices[r]` is the column index removed at row `r`.
- For horizontal seams: `indices[c]` is the row index removed at column `c`.

> **Pluggable Architecture:** Any alternative algorithm (such as Dynamic Programming or Greedy Search) can produce this identical JSON output schema and immediately gain animated GIF export, comparison panels, and Streamlit interactive scrubbers with zero code modifications.

---

### Programmatic C++ API

The C++ codebase is modularly designed and can be integrated directly into other applications:

```cpp
#include "image.hpp"
#include "energy.hpp"
#include "graph_cut_seam.hpp"
#include "maxflow.hpp"

// 1. Load image
seam_carving::Image img;
img.load_from_file("input.jpg");

// 2. Compute energy map
seam_carving::EnergyMap energy = seam_carving::compute_energy(img, seam_carving::EnergyMetric::DualGradient);

// 3. Find vertical seam using Dinic solver
seam_carving::DinicSolver solver;
seam_carving::SeamResult result = seam_carving::GraphCutSeamFinder::find_vertical_seam(energy, solver);

// result.seam contains vector of column indices [c_0, c_1, ..., c_{H-1}]
// result.energy_sum contains the cut capacity
img.remove_vertical_seam(result.seam);
```

---

## AI Usage Declaration

Charity (`t-charity`) used **Claude** (Anthropic), via **Claude Code** (model: Claude Opus 5.5), to help with the bonus features (Item 7).

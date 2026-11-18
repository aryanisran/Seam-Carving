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
   - [High-Resolution Scaling Benchmark](#high-resolution-scaling-benchmark-427x290-image)
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
   - Uses Breadth-First Search (BFS) to find shortest augmenting paths in the residual network.
   - Enhanced with **Capacity Scaling ($\Delta$-scaling)** and **Multi-Path Wavefront Augmentation** to prevent fractional micro-flow bottlenecks on continuous gradient images.
   - Theoretical Complexity: $O(V E^2)$ classic, accelerated to $O(E^2 \log C_{max})$ via capacity scaling.
   - Serves as the classic textbook baseline, optimized for practical grid graphs.
2. **Dinic's Algorithm (With Dinic)**:
   - Constructs a layered level graph via BFS, then uses Depth-First Search (DFS) with current-arc pointer optimization to push multi-branch blocking flows.
   - Enhanced with dead-end level graph pruning and zero-allocation static queue buffers.
   - Theoretical Complexity: $O(V^2 E)$.
   - Delivers substantial practical speedups on dense grid networks.

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
Testing 3 vertical seams followed by 2 horizontal seams under identical graph conditions (`bin/test_graph_cut`):

| Seam # | Dir | Size | Dinic Energy | EK Energy | Match? | Dinic (ms) | EK (ms) | Dinic BFS | EK BFS | Speedup |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | V | 80x60 | 3061.68 | 3061.68 | **YES** | 2.22 | 1.23 | 31 | 63 | **0.55x** |
| 2 | V | 79x60 | 3158.94 | 3158.94 | **YES** | 2.68 | 1.67 | 38 | 72 | **0.62x** |
| 3 | V | 78x60 | 3252.63 | 3252.63 | **YES** | 2.51 | 1.60 | 38 | 71 | **0.64x** |
| 4 | H | 77x60 | 5941.79 | 5941.79 | **YES** | 3.19 | 4.30 | 43 | 113 | **1.35x** |
| 5 | H | 77x59 | 5928.85 | 5928.85 | **YES** | 3.01 | 4.05 | 42 | 111 | **1.34x** |

- **Cumulative Dinic Time**: `13.63 ms`
- **Cumulative Edmonds-Karp Time**: `12.85 ms`
- **Overall Speedup**: **`0.94x`** (Edmonds-Karp with capacity scaling slightly edges out Dinic on small dimensions due to lower overhead)
- **Optimality Verification**: Dinic and Edmonds-Karp produce **100% identical cut energies and seam coordinates** across every single carved seam.

---

### High-Resolution Scaling Benchmark (427x290 Image)
Carving 20 vertical and 20 horizontal seams (40 seams total) on `samples/image.png` ($|V| \approx 124,000$, $|E| \approx 370,000$):

| Solver / Configuration | Per-Seam Avg (ms) | 40 Seams Total (ms) | Wall Clock Time | Speedup vs Baseline EK | Correctness |
|---|---|---|---|---|---|
| **Baseline Edmonds-Karp** | ~47,200 ms | 1,888,660 ms | **31.5 min** | 1.0x (baseline) | Verified |
| **Optimized Edmonds-Karp** | **~1,200 – 1,700 ms** | **48,000 – 68,332 ms** | **~48 – 68 s** | **~28x – 39x** | **100% Identical** |
| **Baseline Dinic** | ~1,210 ms | ~48,400 ms | **~48.4 s** | **39x** | Verified |
| **Optimized Dinic** | **~595 ms** | **~23,800 ms** | **~24 s** | **78x** | **100% Identical** |

---

### Performance & Memory Optimizations

Directly profiling baseline max-flow algorithms on large grid networks revealed critical bottlenecks:
1. Classic Edmonds-Karp executed **~9,380 full-graph BFS passes per seam**, with each path augmenting fractional flow ($\sim 0.15$ units) across 124,000 vertices.
2. Every BFS pass wiped a 124,000-element `parent_v` array via `std::fill`, writing over **25 GB of redundant data** into RAM per seam.
3. Graph construction created redundant directed edge pairs and triggered hundreds of thousands of heap reallocations.

To address this without compromising mathematical correctness, the engine was optimized across four tiers:

#### Tier 1: Graph Topology & Edge Pruning ([`include/graph.hpp`](include/graph.hpp), [`src/graph_cut_seam.cpp`](src/graph_cut_seam.cpp))
- **Merged Bidirectional Edge Pairs**: The standard graph formulation requires a forward capacity $e(r, c)$ and an infinite backward capacity $\infty$. Previously, `add_edge(u, v, energy)` and `add_edge(v, u, INF)` were invoked separately, allocating 4 edge structs per horizontal pair. We enhanced `FlowGraph::add_edge(u, v, cap, rev_cap = 0.0)` to merge them into a single complementary forward-reverse edge pair:
  $$\text{Forward } (u \to v): \text{cap} = e(r, c), \quad \text{Reverse } (v \to u): \text{cap} = \infty$$
  This eliminated **~250,000 redundant directed edge structures** (~40% of the entire graph), dramatically reducing cache footprint and memory bandwidth during traversals.
- **Fixed-Degree Pre-Reservation**: Pre-reserved vertex degree in `FlowGraph::reset` ($H$ edges for $S$ and $T$, 6 edges per pixel node), completely eliminating dynamic vector reallocations during graph construction.

#### Tier 2: Edmonds-Karp Algorithmic Accelerations ([`src/maxflow.cpp`](src/maxflow.cpp))
- **Capacity Scaling ($\Delta$-scaling)**: Replaced unconstrained shortest-path BFS with Capacity Scaling. In each phase, the search only traverses edges with residual capacity $\ge \Delta$, where $\Delta$ starts at 512.0 and halves down to `FLOW_EPS`. This guarantees that every augmenting path pushes a significant chunk of flow, preventing micro-flow fragmentation and cutting augmenting paths from **~9,380 down to ~1,500**.
- **Multi-Path Wavefront Augmentation Sweep**: Because rows in a grid network are largely independent horizontal channels, BFS does not immediately discard its queue upon reaching the sink. It augments the discovered path and continues expanding the existing queue to augment other disjoint rows reaching the sink in the *same* wavefront pass. This reduced BFS runs from **~9,400 down to ~150–250 per seam**.
- **$O(\text{visited})$ Queue Cleanup**: Eliminated full $O(V)$ `std::fill` calls. Only nodes actually visited and enqueued in $q[0..\text{tail}-1]$ are cleared after augmentation, reducing array cleanup overhead from $O(V)$ to $O(\text{visited})$.
- **Direct Pointer Traversal**: Inner BFS loops operate directly on contiguous memory via raw pointers (`Edge*`, `int*`), bypassing vector indirection.

#### Tier 3: Dinic Solver Optimizations ([`src/maxflow.cpp`](src/maxflow.cpp))
- **True Multi-Branch Blocking Flow DFS**: Fixed the blocking-flow DFS so it pushes flow across *all* outgoing level-graph arcs out of vertex $u$ up to requested capacity, rather than terminating after finding a single path.
- **Dead-End Level Pruning**: If a node $u$ exhausts its outgoing capacity without satisfying requested flow, `level[u]` is set to $-1$. This prevents future DFS traversals from re-entering blocked nodes within the same phase.
- **Zero-Allocation BFS**: Dynamic `std::queue<int>` instances were replaced with a flat, reusable thread-local array, with early termination as soon as BFS finishes the `level[sink]` layer.

#### Tier 4: Fast Min-Cut Extraction ([`src/maxflow.cpp`](src/maxflow.cpp))
- Replaced `std::queue` with flat vector traversal in `MaxFlowSolver::compute_source_cut`, making the post-flow source-component reachability traversal virtually instantaneous (~0.1 ms).

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

In compliance with **Item 9.a.v** of the CSD3130 Assignment 2 Brief, the following table and prompt records detail how AI tools were utilized during the design, implementation, testing, and documentation phases of this project:

### Summary of AI Tools & Scope

| Contributor | AI Tool & Model | Platform | Scope of Assistance |
|---|---|---|---|
| **Aryan bin Mohamed Isran** (`aryanisran`) | **Google Gemini** | Google Antigravity CLI | **Graph Cut Optimization & Benchmarking**: <br>• Theoretical profiling of $O(V E^2)$ runtime bottlenecks on continuous gradient grid networks.<br>• Algorithmic design of Capacity Scaling ($\Delta$-scaling) and Multi-Path Wavefront Augmentation for Edmonds-Karp.<br>• Implementation of multi-branch blocking-flow DFS and dead-end level graph pruning in Dinic.<br>• Graph topology optimization (merging forward/backward edges, fixed-degree pre-allocation).<br>• Zero-allocation $O(\text{visited})$ array resets and raw pointer traversal in tight loops.<br>• Empirical benchmarking on 80×60 and 427×290 images, and rubric compliance verification. |
| **Charity** (`t-charity`) | **Claude Opus 5.5** (Anthropic) | Claude Code | **Bonus Features (Item 7)**: <br>• Step-by-step seam replay and visualization engine (`bonus/visualize.py`).<br>• Streamlit interactive web application (`bonus/app.py`).<br>• Automated CI/CD pipeline and release workflow (`.github/workflows/ci.yml`).<br>• Python unit test suite for visualization replay (`bonus/tests/test_visualize.py`). |

---

### Main Prompts Log

Below are the primary prompts used during the design and optimization of the Graph Cut max-flow engine:

1. **Initial Bottleneck Discovery & Performance Query**:
   > *"Carved 20 vertical and 20 horizontal seams using Edmonds-Karp in 1.88866e+06 ms. New size: 407x270, we need to optimise the graph cut"*
   - **AI Response & Contribution**: Analyzed why continuous gradient capacities caused 9,300+ augmenting paths pushing fractional flow ($\sim 0.15$ units), identified that standard BFS was visiting 97% of all 124,000 vertices on every single path, identified the 25 GB memory write overhead of `std::fill(parent_v)`, and designed the 4-tier optimization roadmap. Designed and implemented Capacity Scaling ($\Delta$-scaling) to reduce augmenting path count from ~9,380 down to ~1,500, and Multi-Path Wavefront Augmentation to reduce BFS runs from ~9,400 down to ~150–250 per seam, accelerating 40 seams to ~48–68 seconds (~1.2 s per seam).

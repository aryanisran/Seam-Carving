# Content-Aware Image Resizing (Seam Carving)
A C++17 implementation of Dynamic Programming, Greedy, and Graph Cut (Min-Cut / Max-Flow) Solvers.

This repository provides a modular, high-performance content-aware image resizing suite implementing three core seam-finding paradigms:
1. **Dynamic Programming (DP)**: Globally optimal seam selection via cumulative energy memoization ($O(W \cdot H)$).
2. **Greedy Algorithm**: Fast, local row-by-row minimum-energy decision heuristic ($O(W + H)$ per seam, not optimal).
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
   - [Dynamic Programming Complexity](#dynamic-programming-complexity)
   - [Theoretical Complexity Comparison](#theoretical-complexity-comparison)
   - [Empirical Benchmark Run](#empirical-benchmark-run-80x60-textured-image)
   - [High-Resolution Scaling Benchmark](#high-resolution-scaling-benchmark-427x290-image)
   - [Algorithm Benchmark: Greedy vs DP vs Graph Cut](#algorithm-benchmark-greedy-vs-dp-vs-graph-cut)
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
| **Interactive Visualizer** | React + TypeScript web app | [`web/`](web/) | One tab per algorithm (Dynamic Programming, Greedy, Graph Cut): step-by-step player of the algorithm's internals plus seam-by-seam effect on a real image. |
| **CI/CD Pipeline** | GitHub Actions | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) | Automated build, C++ tests, web tests against the C++ binary, Python tests, GitHub Pages deploy, and release tagging. |

---

## Theoretical Formulations

Seam carving resizes an image by iteratively finding and removing connected paths of pixels (seams) having the lowest cumulative disruption (energy).

### 2.1 Dynamic Programming

#### Question 2(a)(i): Exponential number of vertical seams

Consider an image with height $m$ and width $n>1$. A vertical seam contains
one pixel in every row, and consecutive pixels may move by at most one column:

\[
|c_{r+1}-c_r|\leq 1.
\]

To obtain a lower bound, restrict attention to the first two columns, numbered
0 and 1. Start every seam at column 0 in the first row. At each of the next
$m-1$ rows, choose either to remain in the current column or to move to the
other column. Both choices are valid because the columns are adjacent, so the
change is always 0 or 1. Each binary sequence of $m-1$ choices gives a
distinct valid seam. Therefore, there are at least

\[
2^{m-1}
\]

valid vertical seams. Since $2^{m-1}$ is exponential in $m$, the number of
possible seams grows at least exponentially with image height. This is a lower
bound: allowing all $n$ columns can only increase the number of seams.

#### Question 2(a)(ii): Dynamic Programming formulation

Let $e(r,c)$ be the energy of the pixel in row $r$, column $c$, where
$0\le r<m$ and $0\le c<n$. Define the DP subproblem

\[
M(r,c)=\text{minimum energy of any valid seam from row 0 through }(r,c).
\]

The first row is the base case:

\[
M(0,c)=e(0,c).
\]

For every later row, a seam ending at $(r,c)$ must come from one of the
three valid predecessors in the preceding row. Predecessors outside the image
are omitted:

\[
M(r,c)=e(r,c)+
\min\{M(r-1,c-1),M(r-1,c),M(r-1,c+1)\}.
\]

The minimum-energy seam ends at the least entry in the final row:

\[
\min_{0\le c<n}M(m-1,c).
\]

The recurrence has optimal substructure. If an optimal seam ending at
$(r,c)$ used a non-optimal prefix ending at its predecessor, replacing that
prefix with a lower-energy prefix would produce an even lower-energy complete
seam, which is a contradiction. It also has overlapping subproblems because
the same minimum-energy prefix ending at a pixel can be used by several
possible paths to later rows. DP computes each pixel's subproblem once instead
of enumerating the exponentially many complete seams.

To recover the coordinates, store the predecessor selected for each DP cell.
After selecting the minimum endpoint in the final row, follow predecessor
pointers backwards until row 0, then reverse or place the recovered positions
in row order. The implementation stores a predecessor table in
`src/dp_seam.cpp`; the resulting seam has exactly one column index per row and
satisfies the adjacency constraint by construction.

For an $m\times n$ energy map, there are $mn$ states and only constant work
per state, so seam finding takes $O(mn)$ time. The current implementation
stores both cumulative costs and predecessors, using $O(mn)$ space.

### Question 2(b): Divide and Conquer and Sobel convolution

Sobel filtering estimates horizontal and vertical image gradients by applying
two fixed $3\times3$ kernels to each pixel. Each output uses a constant
number of neighboring pixel values, so computing the complete energy map for
an $m\times n$ image takes $O(mn)$ time and $O(mn)$ output storage.

A straightforward Divide and Conquer strategy could divide the image into
regions, recursively filter each region, and combine the results. However,
the Sobel kernel has constant size, and every pixel still must be visited.
Handling the boundary halo between subregions adds bookkeeping but does not
reduce the asymptotic work below $O(mn)$. Ordinary Divide and Conquer
therefore does not provide an asymptotic improvement for this fixed-size
convolution.

Other techniques may improve constants or become useful in different
settings, but they should not be confused with ordinary Divide and Conquer.
For example, a separable implementation can replace a $3\times3$ filter
with two one-dimensional passes, reducing arithmetic per pixel. FFT-based
convolution can be advantageous for very large kernels or batch processing,
although its overhead is unnecessary for a Sobel kernel. Parallel execution
can also process independent pixels or image regions, subject to boundary
handling, but it does not change the sequential DP recurrence used later for
seam selection.

---

### 2.2 Greedy Algorithm

Implementation: [`include/greedy_seam.hpp`](include/greedy_seam.hpp),
[`src/greedy_seam.cpp`](src/greedy_seam.cpp). Tests: [`tests/test_greedy.cpp`](tests/test_greedy.cpp).
Seams and visual quality vs DP: [`tests/compare_greedy_dp.cpp`](tests/compare_greedy_dp.cpp)
(`make compare`, results in [`results/greedy_vs_dp/`](results/greedy_vs_dp)). Running time:
[`tests/benchmark_algorithms.cpp`](tests/benchmark_algorithms.cpp) (`make benchmark`, results in
[`results/benchmark/`](results/benchmark)).

#### Algorithm

As an alternative to DP, the greedy finder chooses the seam with a **local, row-by-row
decision** and never reconsiders it:

1. Start in row 0 at the minimum-energy pixel:
\[
c_0=\arg\min_{0\le c<n} e(0,c).
\]
2. For every later row, move to the cheapest of the (at most) three pixels that keep the seam
connected:
\[
c_r=\arg\min_{c\in\{c_{r-1}-1,\;c_{r-1},\;c_{r-1}+1\}\cap[0,n)} e(r,c).
\]

Each step changes the column by at most one, so every greedy seam is valid (one pixel per row,
8-connected) by construction. Ties go straight down, then left, then right, which is the same
order the DP implementation uses to choose a predecessor. Horizontal seams run the same
procedure on the transposed energy map, as DP and Graph Cut do.

For an $m\times n$ energy map, choosing the start costs $O(n)$, and every later row costs three
comparisons, so one greedy seam takes $O(m+n)$ time. DP takes $O(mn)$. Greedy needs only the
$O(m)$ seam itself and no table. Every seam still requires a fresh energy map, which costs
$O(mn)$, so one complete carving iteration is $O(mn)$ for both algorithms.

#### Why greedy is not optimal

Greedy uses only the next row, so a cheap pixel now can lead the seam into an expensive region
later. The DP recurrence avoids this by taking the minimum over complete prefixes. Greedy
lacks the greedy-choice property: the cheapest next pixel does not have to lie on the cheapest
seam. The smallest counter-example (`test_greedy_is_not_optimal_counter_example`):

```
          col0 col1 col2
row 0  [   1    2    9 ]     Greedy: 1 → 1 → 99 → 1 = 102   (trapped above the wall)
row 1  [   1    2    9 ]     DP:     1 → 2 →  2 → 1 =   6   (optimal)
row 2  [  99   99    2 ]
row 3  [   1    1    1 ]
```

DP pays 2 instead of 1 in row 1 so that it can reach the cheap pixel in row 2. If the wall cost
is $M$, greedy pays about $M$ while the optimum stays at 6, so greedy's error has no upper bound.

#### Comparison with Dynamic Programming

Both algorithms receive the **same energy map** at every step; only the seam finder differs.
Test image: `samples/synthetic_tower.png` (200×130), carved to 140×117 (60 vertical + 13
horizontal seams).

**Seams.** All removed seams drawn on the original (left: greedy in red, right: DP in green):

![Greedy vs DP seams](results/greedy_vs_dp/synthetic_tower_seams_greedy_vs_dp.png)

First seam chosen by each algorithm (red = greedy, green = DP):

![First seam](results/greedy_vs_dp/synthetic_tower_first_seam.png)

DP's seams stay in the flat sky and grass. Greedy's seams start wherever the top row happens to
be cheapest and then drift. Many of them cut through the tower and the clouds.

**Visual quality.** Carved results (left: greedy, right: DP):

![Greedy vs DP carved](results/greedy_vs_dp/synthetic_tower_carved_greedy_vs_dp.png)

DP keeps the tower, its windows, the clouds and the person intact. With greedy, the tower is
narrower, its windows are uneven, and the clouds are squashed.

| Tower image, 73 seams | Total seam energy removed | Relative to DP |
|---|---:|---:|
| Dynamic Programming | 92,595 | 1.00 |
| **Greedy** | **133,962** | **1.45** |

Measured on identical energy maps at each of the 60 vertical steps, greedy's seam costs
**1.67× the DP optimum on average** (worst 1.82×), and it never found the optimal seam
(0/60 steps). On 300 small random maps in the unit tests it was optimal on 197, and up to
4.75× worse.

**Real photograph.** The same comparison on `samples/image.png`, the Broadway Tower photo
from the assignment brief (427×290 → 299×261, i.e. 128 vertical + 29 horizontal seams):

![Greedy vs DP seams on the photo](results/greedy_vs_dp/image_seams_greedy_vs_dp.png)

![Greedy vs DP carved photo](results/greedy_vs_dp/image_carved_greedy_vs_dp.png)

DP (right) removes seams almost only from the sky and grass, and the tower keeps its full width
and shape. Greedy (left) sends many seams straight through the tower, so it comes out visibly
thinner, with narrower turrets and windows.

| Photo, 157 seams | Total seam energy removed | Relative to DP |
|---|---:|---:|
| Dynamic Programming | 449,760 | 1.00 |
| **Greedy** | **716,572** | **1.59** |

On identical energy maps, greedy's seam costs **1.60× the optimum on average and 2.87× at
worst**, and it never matched the optimal seam (0/128 steps). The result therefore holds on a
real photograph, not only on the synthetic test image.

**Running time.** Running time is measured for all three algorithms in one place, the
[Algorithm Benchmark](#algorithm-benchmark-greedy-vs-dp-vs-graph-cut), so every
timing comes from the same run. For Greedy against DP:
- **Seam search:** Greedy is **8× faster** than DP at 40×30 and **1,037× faster** at 1920×1440. The
  gap grows with image size, as $O(m+n)$ against $O(mn)$ predicts.
- **Whole resize** of the tower image: **19.8 ms with Greedy and 29.9 ms with DP**, only about
  1.5× faster. The energy map is recomputed for every seam whichever algorithm is used, and it costs
  more than either search (19.1 ms of Greedy's 19.8 ms).

**Conclusion.** Greedy does **not** produce results comparable to DP. Its seam search is much
faster, but the whole resize is only about 1.5× faster. In return it removes 45 % more energy on the
synthetic image and 59 % more on the real photo, and the tower and clouds are visibly damaged. DP gives much better quality for a small extra cost.

#### Alternative greedy designs considered

- **Multi-start greedy:** run the same walk from every pixel of row 0 and keep the cheapest seam.
  This removes the "bad starting pixel" problem, but it still never looks ahead (it scores 14,
  not 6, on the counter-example above). It also costs $O(mn)$, the same as DP, so it gives up
  greedy's only advantage while staying non-optimal.
- **$k$-row look-ahead:** choose each step by the cheapest of the $3^k$ paths over the next $k$
  rows. Quality improves as $k$ grows, but the cost grows exponentially. Avoiding that blow-up
  by reusing results for shared prefixes is exactly what the DP recurrence does.
- **Starting from the best end instead of row 0:** run the walk upward from the cheapest pixel in
  the last row. This only moves the problem: the seam can still be trapped near the top.

We implemented the classic single-start greedy because it is the variant the assignment
describes (a local, row-by-row choice without considering the whole image), and it is the
clearest contrast with DP.

#### References

1. S. Avidan and A. Shamir, "Seam Carving for Content-Aware Image Resizing," *ACM SIGGRAPH*, 2007.
   [doi:10.1145/1275808.1276390](https://dl.acm.org/doi/10.1145/1275808.1276390)
2. M. Rubinstein, A. Shamir and S. Avidan, "Improved Seam Carving for Video Retargeting,"
   *ACM SIGGRAPH*, 2008. [doi:10.1145/1360612.1360615](https://dl.acm.org/doi/10.1145/1360612.1360615)
3. T. H. Cormen, C. E. Leiserson, R. L. Rivest and C. Stein, *Introduction to Algorithms*, 3rd ed.,
   MIT Press, 2009: Ch. 15 (dynamic programming, Problem 15-8 "Image compression by seam
   carving") and Ch. 16 (greedy algorithms and the greedy-choice property).
4. A. Das, "Real-world dynamic programming: seam carving," 2019.
   [avikdas.com](https://avikdas.com/2019/05/14/real-world-dynamic-programming-seam-carving.html)

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
│   ├── carver.py               # Python wrapper executing bin/seam_carve
│   ├── make_sample.py          # Synthetic test image generator
│   ├── README.md               # Bonus module documentation & instructions
│   ├── requirements.txt        # Python dependencies (pillow, numpy, pytest)
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
    ├── test_greedy.cpp         # Greedy unit test suite
    ├── compare_greedy_dp.cpp   # Greedy vs DP seams & visual quality (make compare -> results/greedy_vs_dp/)
    └── benchmark_algorithms.cpp # Greedy vs DP vs Graph Cut benchmark (make benchmark -> results/benchmark/)
```

The bonus web visualizer lives in `web/` (React + TypeScript, built with Vite):

```
web/
  src/algo/        energy.ts, dp.ts, greedy.ts, graphcut.ts  - line-for-line ports of the C++ algorithms
                   carve.ts, carve.worker.ts, useCarve.ts     - carving loop (runs in a Web Worker)
  src/components/  StepPlayer (transport, timeline, narration), CellGrid, FlowNetwork (SVG), ImageCarvePanel
  src/tabs/        DPTab, GreedyTab, GraphCutTab
  tests/           algo.test.ts - hand-checked examples + seam-for-seam / pixel-for-pixel parity with bin/seam_carve
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
- Exhaustive brute-force verification of DP on small deterministic and random energy maps.
- RGB pixel-order preservation during vertical and horizontal seam removal.
- Repeated DP resizing, exact target dimensions, and invalid resize requests.
- Greedy seams are always valid, report the correct energy, and are never better than the DP
  optimum (300 random maps, vertical and horizontal, checked against brute force).
- The greedy counter-example (greedy = 102 vs DP = 6), tie-breaking, image borders, and
  degenerate 1×H / W×1 / empty maps.
- End-to-end greedy resizing through `SeamCarver` (`--algorithm greedy`).

The DP-specific tests are included in `tests/test_dp.cpp` and the Greedy tests in
`tests/test_greedy.cpp`; both are executed by the `make test` target.

```bash
# Greedy vs DP comparison (seams and visual quality) -> results/greedy_vs_dp/
make compare

# Benchmark of all three algorithms (Greedy, DP, Graph Cut) -> results/benchmark/
make benchmark
```

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

### 3. Web Visualizer (React)

Needs Node.js 20+.

```bash
cd web
npm install          # once
npm run dev          # open the printed http://localhost:5173 link
npm test             # TypeScript algorithms vs bin/seam_carve (run `make all` first)
npm run build        # static site in web/dist/
```

On Windows without a Linux `bin/seam_carve`, the parity tests can call the WSL build:
`SEAM_CARVE_CMD="wsl -d Ubuntu -- ./bin/seam_carve" npm test`.

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
  --algorithm <name>        Seam-finding algorithm: 'graph_cut', 'dp', or 'greedy'
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

#### 2. Resize using Dynamic Programming

```bash
./bin/seam_carve -i input.jpg -o output_dp.png \
  --remove-cols 30 --remove-rows 15 --algorithm dp \
  --export-seams seams_dp.json
```

`--remove-cols` removes vertical seams and reduces the width. `--remove-rows`
removes horizontal seams and reduces the height. Alternatively, `--width` and
`--height` specify positive target dimensions that cannot exceed the original
dimensions. The image is resized by repeatedly recomputing energy, finding one
seam, and removing it.

#### 3. Resize using Edmonds-Karp (Without Dinic)
```bash
./bin/seam_carve -i input.jpg -o output_ek.jpg --remove-cols 10 --solver edmonds_karp
```

#### 4. Run Benchmark Comparison (Dinic vs. Edmonds-Karp)
```bash
./bin/seam_carve -i input.jpg --remove-cols 5 --remove-rows 5 --compare --export-csv benchmark.csv
```

#### 5. Export Seam Data for Visualizer
```bash
./bin/seam_carve -i input.jpg -o out.jpg --remove-cols 30 --export-seams seams.json
```

#### 6. Resize using the Greedy Algorithm
```bash
./bin/seam_carve -i input.jpg -o output_greedy.png \
  --remove-cols 30 --remove-rows 15 --algorithm greedy \
  --export-seams seams_greedy.json
```

---

## Benchmarking & Complexity Analysis

### Dynamic Programming Complexity

For an image of height $H$ and width $W$, the current implementation has
the following costs:

| Operation | Time | Space | Notes |
|---|---:|---:|---|
| Energy map | $O(HW)$ | $O(HW)$ | Each pixel evaluates a constant-size dual-gradient or Sobel neighborhood. |
| Vertical seam search | $O(HW)$ | $O(HW)$ | One DP state and constant predecessor comparisons per pixel. |
| Horizontal seam search | $O(HW)$ | $O(HW)$ | Transposes the energy map, then runs the same vertical DP. |
| Backtracking | $O(H)$ | $O(H)$ output | One predecessor is followed per row; horizontal backtracking is $O(W)$. |
| Seam removal | $O(HW)$ | $O(HW)$ temporary image storage | The current image implementation copies the remaining pixels. |
| One complete seam iteration | $O(HW)$ | $O(HW)$ | Energy, DP, backtracking, and removal are all bounded by the image area. |

The implementation currently stores an $H\times W$ cumulative-cost table and
an $H\times W$ predecessor table, so its DP working memory is $O(HW)$.
The cumulative-cost table only needs the previous row while the recurrence is
being evaluated, so rolling arrays could reduce that table to $O(W)$. The
predecessor table is still $O(HW)$ if direct backtracking is required; thus
rolling the costs reduces memory constants but does not change the overall
asymptotic space bound for this implementation.

When $K$ seams are removed, the total work is the sum of the costs for the
successively smaller images. A simple upper bound is $O(KHW)$, while the
actual work is lower because either $H$ or $W$ decreases after every
iteration. The current implementation removes all requested vertical seams
before all requested horizontal seams.

The DP recurrence has a row-to-row dependency: row $r$ cannot be finalized
until row $r-1$ is known. This limits parallelism inside one seam search.
Energy calculations and independent benchmark runs can be parallelized, but
multithreading the small constant-size predecessor comparisons is unlikely to
justify its synchronization overhead for ordinary image sizes. The sequential
DP implementation is therefore appropriate for the assignment baseline.

### Theoretical Complexity Comparison
| Metric | Greedy Algorithm | Dynamic Programming | Edmonds-Karp (Graph Cut) | Dinic's Algorithm (Graph Cut) |
|---|---|---|---|---|
| **Optimality Guarantee** | None (Local Minima) | **Global Minimum** | **Global Minimum** | **Global Minimum** |
| **Augmentation / Search Strategy** | Local 3-pixel minimum per row | Cumulative 2D energy table + backtrack | Single shortest path BFS | Layered graph BFS + blocking flow DFS |
| **Time Complexity (per seam)** | $O(W \cdot H)$ energy + $O(W + H)$ seam | $O(W \cdot H)$ | $O(V E^2)$ where $\|V\| \approx WH$ | $O(V^2 E)$ where $\|V\| \approx WH$ |
| **Space Complexity** | $O(H)$ (the seam only, no table) | $O(W \cdot H)$ table ($O(W)$ if value only) | $O(V + E)$ residual graph | $O(V + E)$ residual graph + levels |

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

### Algorithm Benchmark: Greedy vs DP vs Graph Cut

[`tests/benchmark_algorithms.cpp`](tests/benchmark_algorithms.cpp) runs all three seam finders
(Greedy, Dynamic Programming, and Graph Cut with both max-flow solvers) on **identical energy maps**,
so only the seam finder differs. Run it with `make benchmark`; the results are in
[`results/benchmark/`](results/benchmark). Timings are medians of 5 runs in WSL (one run once a seam
takes over 200 ms). An algorithm is dropped at larger sizes once one seam takes over 3 s.

**Time to find one vertical seam** (procedural tower scene, same content at every size):

| Image size | Energy map | Greedy | DP | Graph Cut (Dinic) | Graph Cut (EK) |
|---|---:|---:|---:|---:|---:|
| 40×30 | 0.013 ms | 0.0003 ms | 0.003 ms | 1.31 ms | 1.44 ms |
| 80×60 | 0.060 ms | 0.0006 ms | 0.011 ms | 14.0 ms | 16.0 ms |
| 160×120 | 0.212 ms | 0.001 ms | 0.058 ms | 171 ms | 163 ms |
| 320×240 | 0.926 ms | 0.002 ms | 0.484 ms | 3,523 ms | 2,230 ms |
| 640×480 | 5.06 ms | 0.005 ms | 2.07 ms | — | 15,992 ms |
| 1280×960 | 22.0 ms | 0.011 ms | 8.73 ms | — | — |
| 1920×1440 | 52.8 ms | 0.020 ms | 20.7 ms | — | — |

"—" = skipped because the previous size already took over 3 s per seam.

**Optimality.** At every size, DP and both Graph Cut solvers find a seam with **exactly the same
energy** (e.g. 1,909.3 at 320×240). This confirms the max-flow min-cut equivalence in practice.
Greedy's seam costs 1.01–1.19× the optimum on these maps.

**Growth.** Fitting time ∝ pixels<sup>α</sup> to the table:
- Greedy: α ≈ 0.53 (×59 time for ×2,304 pixels, 40×30 → 1920×1440). This is close to the 0.5 of
  $O(W+H)$, which grows like √pixels.
- DP: α ≈ 1.15 (×7,330). This matches $O(WH)$; the extra 0.15 is likely cache misses once its
  12 B/pixel tables outgrow the cache.
- Energy map: α ≈ 1.07, also $O(WH)$.
- Graph Cut: α ≈ 2.0 for Dinic (×252 for ×16 pixels, 80×60 → 320×240) and α ≈ 1.7 for Edmonds–Karp
  (×999 for ×64, 80×60 → 640×480). Both are far below their cubic worst case, because grid graphs
  have short augmenting paths, but still much faster-growing than DP. Graph Cut is
  **~4,600–7,300× slower than DP** at 320×240, and ~7,700× slower at 640×480.

**Working memory for one seam** (excluding the shared energy map):

| Image size | Greedy (the seam) | DP (cost + predecessor tables) | Graph Cut (flow graph) |
|---|---:|---:|---:|
| 320×240 | 0.9 KB | 900 KB | 12,559 KB |
| 640×480 | 1.9 KB | 3,600 KB | 50,318 KB |
| 1920×1440 | 5.6 KB | 32,400 KB | — |

The flow graph needs about **14× the memory of DP**: around 167 B/pixel for the edges and their
residual twins, against DP's 12 B/pixel. Greedy needs almost none.

**Full resize** of `samples/synthetic_tower.png`, 200×130 → 140×117 (60 vertical + 13 horizontal
seams, energy map recomputed before every seam):

| Algorithm | Total energy removed | Seam search | Energy maps | Total time |
|---|---:|---:|---:|---:|
| Greedy | 133,962 (1.45× DP) | 0.8 ms | 19.1 ms | **19.8 ms** |
| Dynamic Programming | 92,595 | 8.3 ms | 21.7 ms | **29.9 ms** |
| Graph Cut (Dinic) | 92,538 | 18,197 ms | 21.8 ms | **18,219 ms** |
| Graph Cut (Edmonds–Karp) | 92,538 | 16,392 ms | 19.2 ms | **16,412 ms** |

![Greedy | DP | Graph Cut](results/benchmark/synthetic_tower_carved_greedy_dp_graphcut.png)

*Left to right: Greedy, DP, Graph Cut (Dinic).* DP and Graph Cut produce visually identical
results, and Greedy squashes the clouds and distorts the tower. Graph Cut's total is 0.06 % *lower*
than DP's even though every one of its seams is optimal. The reason is ties: when several seams
have the same minimum energy, DP and Graph Cut may pick different ones. From then on they carve
slightly different images, and removing one seam at a time is not optimal for the resize as a
whole, for either algorithm.

**Conclusion.**
- **DP** gives the best trade-off: it is optimal, takes milliseconds even at 2.8 MP, and costs about
  as much as the energy map it depends on.
- **Greedy** saves only about a third of the end-to-end time (19.8 vs 29.9 ms), because the energy
  map dominates, but removes 45 % more energy.
- **Graph Cut** finds the same optimal seams as DP, but is about 600× slower end to end and uses 14×
  the memory. Its value is generality, e.g. 3-D graphs for video seam carving, not speed on still
  images.

#### Measured improvements (Item 5d)

The benchmark also measures prototypes of the Item 5d improvements. They live only in
`tests/benchmark_algorithms.cpp` and do not change `DPSeamFinder` or `compute_energy`. Each one is
first checked to give **exactly the same result** as the production code (same seam, bit-identical
energy map), then timed.

**1. Do we need the entire DP table?** No. Two rolling rows of costs plus a 1-byte parent offset
(−1, 0, +1) per pixel give the same seam as `DPSeamFinder`'s full cost and predecessor tables:

| Image size | Full tables (DP) | Rolling rows + 1-byte parents | Memory saved | Time (full → compact) | Same seam |
|---|---:|---:|---:|---:|:---:|
| 640×480 | 3,600 KB | 310 KB | 11.6× | 2.03 → 2.03 ms | ✔ |
| 1280×960 | 14,400 KB | 1,220 KB | 11.8× | 9.00 → 8.34 ms | ✔ |
| 1920×1440 | 32,400 KB | 2,730 KB | 11.9× | 19.6 → 19.6 ms | ✔ |

About 12× less memory, at no cost in time. The space is still $O(WH)$, because backtracking needs one
parent per pixel, but the constant drops from 12 bytes to about 1. If only the minimum energy were
needed, and not the seam, two rows ($O(W)$) would be enough.

**2. Does multithreading help?** It helps both, but the DP scales less well. Each row of the DP
depends on the previous row, so the threads must synchronise after every row; the energy map has
only one synchronisation point. Speed-up over the serial version (OpenMP, results identical to the
serial code in every case):

| Threads | Energy map 320×240 | Row-parallel DP 320×240 | Energy map 1920×1440 | Row-parallel DP 1920×1440 |
|---:|---:|---:|---:|---:|
| 2 | 1.90× | 1.80× | 1.53× | 1.83× |
| 4 | 1.98× | 2.45× | 2.65× | 2.52× |
| 8 | **3.99×** | **2.60×** | **3.70×** | **3.56×** |

On the small image, the DP levels off at 2.6× on 8 threads while the energy map reaches 4×: each
320-pixel row is too little work to hide the barrier. On the large image, each 1,920-pixel row is
enough work, and the DP reaches 3.6×. Multithreading therefore pays off most for the energy map,
which is also the largest cost per seam.

**3. Incremental energy update.** After a vertical seam is removed, only the pixels next to it have
new neighbours. The prototype shifts the previous energy map past the seam and recomputes at most
4 pixels per row. Over 192 seams removed from a 640×480 scene with DP, the energy maps took
**131.9 ms instead of 671.9 ms (5.1× faster)**, and they were identical to a full recompute at every
step. This removes the largest per-seam cost for every algorithm, and for Greedy it makes the whole
resize $O(k(W+H))$.

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
| **7a. Step-by-Step Visualisation** | Web visualizer: one tab per algorithm with a step player (play/pause, step, speed, timeline) that animates the algorithm's internals on a small grid (DP table filling and backtracking, greedy choices, max-flow augmenting paths and the min cut), plus a seam-by-seam player on a real image with every removed seam drawn on the original. Offline renders (GIF, overlay, comparison) via Python. | [`web/`](web/), [`bonus/visualize.py`](bonus/visualize.py) |
| **7b. User Interaction** | Upload an image or pick a sample, drag the working width and target width/height (live preview), switch energy function and max-flow solver; every algorithm re-carves in the browser (Web Worker) and can be scrubbed seam by seam, on the image or on its energy map. | [`web/`](web/) |
| **7c. CI/CD** | GitHub Actions: C++ build and tests, web tests that compare the TypeScript algorithms with the C++ binary, Python tests, demo artifacts, automatic GitHub Pages deployment of the visualizer on every push to `main`, and tag-triggered GitHub Releases | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) |

> **Design Note:** The web visualizer runs the algorithms in the browser so it can be hosted as a static site and show their internal steps. To guarantee it shows exactly what the team's C++ does, `web/src/algo/*.ts` are line-for-line ports (same graph construction, same Dinic and capacity-scaling Edmonds-Karp, same tie-breaking), and `web/tests/algo.test.ts` checks in CI that they remove **the same seams and produce the same pixels** as `bin/seam_carve` for DP, Greedy and Graph Cut (both solvers, both energy functions).

---

### 7a. Step-by-Step Visualizer

**Web (`web/`).** Each tab has two players:

1. **How it works** - a small energy grid (presets: a winding valley, the greedy counterexample from Avik Das, or random) with the algorithm's pseudocode, the active line highlighted, and a narration of every step:
   - *Dynamic Programming*: each cell of `M` with its three candidate parents, the chosen parent arrow, then the minimum of the last row and backtracking.
   - *Greedy*: the reachable pixels in each row, the running total, and at the end the optimal (DP) seam for comparison.
   - *Graph Cut*: the flow network (S, T, finite edges labelled flow/capacity, infinite back and diagonal edges), Dinic level graphs and blocking flows or Edmonds-Karp capacity-scaling phases and augmenting paths, then the source side of the min cut, the seam pixel of each row, and cut capacity = max flow.
2. **Effect on the image** - every seam the algorithm removes from the working image, shown on the current image (or its energy map) and accumulated on the original. Vertical seams are red, horizontal seams blue.

Colours follow the Okabe-Ito colour-blind-safe palette and every state also has its own outline style. Keyboard: Space play/pause, Left/Right step, Home/End.

**Python (`bonus/visualize.py`).** Replays the C++ `--export-seams` JSON against the original image to produce static files:

- **`steps.gif`**: animated progression with each vertical seam (red) or horizontal seam (blue) highlighted before removal.
- **`overlay.png`**: original image with all removed seams drawn simultaneously, using an $O(HW)$ coordinate-tracking grid.
- **`comparison.png`**: original vs seam carving vs scaling vs cropping.
- **`energy.png`**: the dual-gradient energy map.

```bash
./bin/seam_carve -i samples/synthetic_tower.png -o carved.png --remove-cols 40 --remove-rows 15 --export-seams seams.json
python bonus/visualize.py --image samples/synthetic_tower.png --seams seams.json --out bonus/out
```

---

### 7b. Interactive Web Application

- **Image**: two samples or upload your own (PNG, JPG, BMP, WebP); decoded and processed entirely in the browser.
- **Size**: working width (live preview while dragging), target width and height (dashed box on the preview). Carving restarts automatically when the sliders stop moving.
- **Algorithm options**: dual-gradient or Sobel energy; Dinic or Edmonds-Karp for graph cut.
- **Responsiveness**: carving runs in a Web Worker with a progress bar, and results are cached per setting.

```bash
cd web && npm install && npm run dev     # http://localhost:5173
```

---

### 7c. CI/CD & Automated Releases

The GitHub Actions pipeline ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) runs on every push and pull request:

1. **C++ CI**: `make all` with `-Wall -Wextra -Wpedantic`, then `make test` (Graph Cut, DP and Greedy unit tests).
2. **Web CI**: `npm ci`, `npm test` (TypeScript algorithms vs `bin/seam_carve`: identical seams and pixels), `npm run build` (type-check and production build).
3. **Python CI**: the `pytest` suite for the offline visualiser, including its pixel-exact replay check.
4. **End-to-End Demo**: carves the synthetic sample, renders all visualisations and uploads them as workflow artifacts.
5. **Continuous Deployment**: every push to `main` deploys the visualizer to GitHub Pages; pushing a tag such as `v1.0` publishes a GitHub Release with the Linux binary, demo GIF and images, and the Dinic vs Edmonds-Karp benchmark CSV.

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

> **Pluggable Architecture:** Any alternative algorithm (such as Dynamic Programming or Greedy Search) can produce this identical JSON output schema and immediately gains the Python GIF export, overlays and comparison panels with zero code modifications.

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
| **Charity** (`t-charity`) | **Claude Opus 5.5** (Anthropic) | Claude Code | **Bonus Features (Item 7)**: <br>- Planning the bonus design (seam JSON export replayed by a separate visual layer).<br>- Python step-by-step visualiser and its tests (`bonus/visualize.py`, `bonus/tests/`).<br>- React + TypeScript web visualizer with one tab per algorithm and TypeScript ports of the C++ algorithms (`web/`), plus its parity tests against the C++ binary.<br>- CI/CD pipeline, GitHub Pages deployment and release workflow (`.github/workflows/ci.yml`).<br>- Browser testing with Playwright, and web research on visualizer design via a research subagent. |
| **Chan Rou Hui** (`hui0w0`) | **Claude Opus 5.5** (Anthropic) | Claude Code | **Greedy Algorithm (Item 3)**: <br>• Implementation of `GreedySeamFinder` and its integration into `SeamCarver` / `--algorithm greedy`.<br>• Greedy unit tests (`tests/test_greedy.cpp`): counter-example, edge cases, brute-force and DP cross-checks.<br>• Greedy vs DP comparison tool (`tests/compare_greedy_dp.cpp`) and the results in `results/greedy_vs_dp/`.<br>• Three-way benchmark of Greedy, DP and Graph Cut (`tests/benchmark_algorithms.cpp`, `results/benchmark/`), including prototype measurements of the Item 5d improvements.<br>• Write-up of README §2.2 and the Algorithm Benchmark section, review against the assignment brief and rubric, and extra verification (AddressSanitizer/UBSan builds, 20,000-map stress test). |

---

### Main Prompts Log

Below are the primary prompts used during the design and optimization of the Graph Cut max-flow engine:

1. **Initial Bottleneck Discovery & Performance Query**:
   > *"Carved 20 vertical and 20 horizontal seams using Edmonds-Karp in 1.88866e+06 ms. New size: 407x270, we need to optimise the graph cut"*
   - **AI Response & Contribution**: Analyzed why continuous gradient capacities caused 9,300+ augmenting paths pushing fractional flow ($\sim 0.15$ units), identified that standard BFS was visiting 97% of all 124,000 vertices on every single path, identified the 25 GB memory write overhead of `std::fill(parent_v)`, and designed the 4-tier optimization roadmap. Designed and implemented Capacity Scaling ($\Delta$-scaling) to reduce augmenting path count from ~9,380 down to ~1,500, and Multi-Path Wavefront Augmentation to reduce BFS runs from ~9,400 down to ~150–250 per seam, accelerating 40 seams to ~48–68 seconds (~1.2 s per seam).

Below are the primary prompts used for the Greedy Algorithm (Item 3):

1. **Reviewing my Greedy implementation after the DP integration**:
   > *"My teammate has added the DP tests and updated the documentation. Could you review my Greedy Algorithm implementation again?"*
   - **AI Response & Contribution**: Reviewed the Greedy work against the newly merged DP code and documentation. It completed the README §2.2 section that was still marked `TODO`, and corrected the stated Greedy complexity to $O(W+H)$ time and $O(H)$ space. It also made `GreedySeamFinder` record `elapsed_ms` in the same way as `DPSeamFinder`, and aligned the structure of `tests/test_greedy.cpp` with `tests/test_dp.cpp`.

Below are the primary prompts used for the Bonus Features (Item 7):

1. **Planning the bonus**:
   > *"can you take a look at the branch? the repo is called graph-cut. can you plan it out how you're going to do the bonus part?"*
   - **AI Response & Contribution**: Read the assignment brief and the C++ code, and proposed exporting each removed seam from the C++ program as JSON and replaying it in a separate visualisation layer, so the bonus never changes the seam-finding code.
2. **First implementation**:
   > *"okay i created a bonus branch. can you start the bonus code there?"*
   - **AI Response & Contribution**: Wrote `bonus/visualize.py` (seam validation, replay with original-coordinate tracking, GIF, overlay, comparison, energy renders), its pytest suite including the pixel-exact check against `bin/seam_carve`, and the first GitHub Actions workflow.
3. **Live interaction**:
   > *"as i slide the working width slider to the right/left, the image adjusts at the same time"*
   - **AI Response & Contribution**: Made the sliders update the preview continuously while dragging instead of only on release.
4. **Browser testing**:
   > *"are you able to run playwright and test it for me"*
   - **AI Response & Contribution**: Drove the app in a headless browser with real mouse drags, checked every tab and the absence of JavaScript errors, and reviewed screenshots.
5. **React visualizer for all algorithms**:
   > *"i want something like this [screenshot of a step-by-step algorithm visualizer], for all the algorithms. instead of streamlit and python, lets use react, create a tab for each algorithm showing the step by step algorithm on how it works for each algorithm and how it affects the image. use a subagent to webresearch on relevant website design skills and also look into the algorithms itself for better context"*
   - **AI Response & Contribution**: A research subagent summarised design patterns from existing algorithm visualizers (VisuAlgo, algorithm-visualizer.org, USFCA), the colour-blind-safe Okabe-Ito palette, and the original papers (Avidan & Shamir 2007; Rubinstein, Shamir & Avidan 2008). Claude then ported the team's C++ energy, DP, Greedy and Graph Cut (Dinic and capacity-scaling Edmonds-Karp) code line for line to TypeScript, built the React app in `web/`, added tests that compare it with `bin/seam_carve` seam for seam and pixel for pixel, and added the GitHub Pages deployment to CI.

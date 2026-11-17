# Contributor & Developer Onboarding Guide

Welcome to the **Content-Aware Image Resizing (Seam Carving)** repository!

This guide provides everything developers need to implement **Dynamic Programming (DP)** and the **Greedy Algorithm** and integrate them seamlessly into the existing C++ engine, test suite, and bonus visualization tools.

---

## 1. High-Level Architecture & Shared Contracts

The repository is designed to be modular. All algorithms operate on shared image containers and energy representations, and produce standardized seam coordinates.

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
                                              │ & Streamlit Web App   │
                                              └───────────────────────┘
```

### Shared Types & Headers

1. **`seam_carving::Image`** ([`include/image.hpp`](include/image.hpp)):
   - `int width() const`, `int height() const`, `int channels() const`
   - `Pixel get_pixel(int row, int col) const`, `void set_pixel(int row, int col, const Pixel& p)`
   - `Image transposed() const`: Returns a transposed copy ($W' = H, H' = W$).
   - `void remove_vertical_seam(const std::vector<int>& seam)`: Removes 1 pixel per row according to column indices $seam[r]$.
   - `void remove_horizontal_seam(const std::vector<int>& seam)`: Removes 1 pixel per col according to row indices $seam[c]$.

2. **`seam_carving::EnergyMap`** ([`include/energy.hpp`](include/energy.hpp)):
   - Computed via `compute_energy(const Image& img, EnergyMetric metric)`.
   - Supports `EnergyMetric::DualGradient` (default) and `EnergyMetric::Sobel`.
   - Accessors: `energy.height()`, `energy.width()`, `energy.at(row, col)`.
   - Transpose: `energy.transposed()`.

3. **`seam_carving::SeamResult`** ([`include/graph_cut_seam.hpp`](include/graph_cut_seam.hpp)):
   Every seam finder must return a `SeamResult`:
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
     - $0 \le seam[r] < W$ for all $0 \le r < H$
     - $|seam[r] - seam[r+1]| \le 1$ for all $0 \le r < H - 1$
   - Helper function provided: `GraphCutSeamFinder::validate_vertical_seam(seam, height, width)` in [`include/graph_cut_seam.hpp`](include/graph_cut_seam.hpp).

---

## 2. File Conventions & Ownership

To prevent merge conflicts and keep code modular, each algorithm should have its dedicated files.

## 3. Integration into CLI & Build System

When your algorithm is ready, integrate it via the following steps:

### A. Update `include/seam_carver.hpp` and `src/seam_carver.cpp`
1. Add an `AlgorithmType` enum in `include/seam_carver.hpp`:
   ```cpp
   enum class AlgorithmType { GraphCut, DynamicProgramming, Greedy };
   ```
2. In `SeamCarver::carve_vertical_seams`:
   ```cpp
   SeamResult res;
   if (algorithm_ == AlgorithmType::DynamicProgramming) {
       res = DPSeamFinder::find_vertical_seam(energy);
   } else if (algorithm_ == AlgorithmType::Greedy) {
       res = GreedySeamFinder::find_vertical_seam(energy);
   } else {
       res = GraphCutSeamFinder::find_vertical_seam(energy, *solver);
   }
   ```

### B. Add CLI Flag in `src/main.cpp`
Accept `--algorithm <graph_cut|dp|greedy>`:
```cpp
// In argument parsing loop:
else if (arg == "--algorithm" && i + 1 < argc) {
    std::string a = argv[++i];
    if (a == "dp") algo = AlgorithmType::DynamicProgramming;
    else if (a == "greedy") algo = AlgorithmType::Greedy;
    else if (a == "graph_cut") algo = AlgorithmType::GraphCut;
}
```

### C. Update `Makefile`
Add your new `.cpp` files to `CORE_SRCS` and declare test binaries:
```makefile
CORE_SRCS := $(SRC_DIR)/image.cpp \
             $(SRC_DIR)/energy.cpp \
             $(SRC_DIR)/graph.cpp \
             $(SRC_DIR)/maxflow.cpp \
             $(SRC_DIR)/graph_cut_seam.cpp \
             $(SRC_DIR)/dp_seam.cpp \
             $(SRC_DIR)/greedy_seam.cpp \
             $(SRC_DIR)/seam_carver.cpp \
             $(SRC_DIR)/benchmark.cpp

# Add test targets:
TEST_DP := $(BIN_DIR)/test_dp
TEST_GREEDY := $(BIN_DIR)/test_greedy

test: $(TEST_TARGET) $(TEST_DP) $(TEST_GREEDY)
	./$(TEST_TARGET)
	./$(TEST_DP)
	./$(TEST_GREEDY)
```

---

## 4. Free Bonus Visualizations & Streamlit UI

By outputting seams via `SeamCarver::export_seams_json` (the `--export-seams <path>.json` flag), your algorithm automatically gains access to all bonus tools without writing any Python or GUI code:

1. **Step-by-Step Animated GIF**:
   ```bash
   python bonus/visualize.py --image samples/synthetic_tower.png --seams seams_dp.json --out out_dp/
   ```
   Generates `steps.gif`, `overlay.png` (all removed seams overlaid), `comparison.png`, and `energy.png`.

2. **Interactive Streamlit Web App**:
   Once `--algorithm` is hooked into `main.cpp` and `bonus/carver.py`, users can select your algorithm in `streamlit run bonus/app.py` and scrub through intermediate resizing steps interactively.

---

## 5. Testing & Quality Standards

- **Language Standard**: C++17 (`-std=c++17`).
- **Compiler Flags**: `-Wall -Wextra -Wpedantic -O3`. Ensure clean compilation with zero warnings.
- **Dependencies**: Header-only `stb_image.h` / `stb_image_write.h` (already included in `include/third_party/`). Do not add external heavy library dependencies.
- **Verification Command**:
  ```bash
  wsl make clean && wsl make all && wsl make test
  ```

# Bonus Features (Assignment 2, Item 7)

| Bonus | What | Where |
|---|---|---|
| **7a** Step-by-step visualisation | Animated GIF with every seam highlighted before removal; all seams drawn on the original image; original vs seam carving vs scaling vs cropping; energy map | `bonus/visualize.py` |
| **7b** User interaction | React web visualizer in `web/` (see the main README): one tab per algorithm with step-by-step players, live sliders for working and target size | `web/` |
| **7c** CI/CD | GitHub Actions: build, C++ tests, web tests against the C++ binary, Python tests, demo run with downloadable outputs, GitHub Pages deploy on main; tagged pushes (`v*`) publish a GitHub Release | `.github/workflows/ci.yml` |

This Python part never re-implements seam finding either: every seam comes from the C++ binary through its `--export-seams` JSON, and Python only replays and draws it. A test checks that the Python replay reproduces the C++ output **pixel for pixel**.

## Setup (WSL / Ubuntu)

```bash
sudo apt install python3-venv python3-pip   # once
make all                                     # build bin/seam_carve
python3 -m venv ~/.venvs/seam
source ~/.venvs/seam/bin/activate
pip install -r bonus/requirements.txt
```

## Usage

```bash
# Visualise any carving run
./bin/seam_carve -i samples/synthetic_tower.png -o out.png --remove-cols 40 --remove-rows 15 --export-seams seams.json
python bonus/visualize.py --image samples/synthetic_tower.png --seams seams.json --out bonus/out

# Tests (integration tests are skipped if bin/seam_carve is not built)
python -m pytest bonus/tests -v

# Regenerate the synthetic sample image
python bonus/make_sample.py
```

## Design notes

- **Original-coordinate tracking.** Seam indices in `seams.json` are relative to the image *at that step*, which has already been shrunk by earlier seams. To draw every seam on the original image, `replay()` keeps an `origin` grid holding each surviving pixel's original `(row, col)`, and removes the same seam from it as from the image. Cost: O(HW) per seam, the same as removing the seam itself.
- **Horizontal seams** reuse the vertical removal on the transposed (`swapaxes`) array, the same trick the C++ graph cut uses.
- **Interactive speed.** Graph cut runs one max-flow per seam on a graph with H*W nodes, so the app first downscales images to a working width (default 160 px). After one carve, the step slider shows any intermediate size instantly, because it replays saved seams instead of recomputing them.
- **Validation.** Every seam is checked while replaying (length, bounds, 8-connectivity, and that the image size matches `width_before`/`height_before`), so a bad export fails loudly instead of drawing garbage.

## Adding DP / Greedy to the app

Any algorithm can be visualised if it writes the same JSON:

```json
{"seams": [{"step": 1, "orientation": "vertical", "width_before": 200, "height_before": 130,
            "energy_sum": 451.2, "elapsed_ms": 1.3, "indices": [120, 121, 121, ...]}]}
```

`indices[r]` is the column removed in row `r` (vertical), or `indices[c]` is the row removed in column `c` (horizontal).

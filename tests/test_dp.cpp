#include "dp_seam.hpp"
#include "energy.hpp"
#include "graph_cut_seam.hpp"
#include "image.hpp"
#include "maxflow.hpp"
#include "seam_carver.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace seam_carving;

namespace {

// Fixtures 1-4 below are identical to tests/test_graph_cut.cpp's fixtures, deliberately:
// Dynamic Programming and Graph Cut are both globally-optimal algorithms, so running them
// on the same energy map must produce the same minimum energy. This cross-checks DP's
// correctness against an independently-implemented algorithm, not just against itself.

void test_known_optimal_vertical_seam() {
    // 4x4 energy grid where optimal diagonal seam is (0,1) -> (1,2) -> (2,2) -> (3,3)
    // Energies along seam: 1 + 2 + 1 + 1 = 5
    EnergyMap energy(4, 4, {
        10.0,  1.0, 10.0, 10.0,
        10.0, 10.0,  2.0, 10.0,
        10.0, 10.0,  1.0, 10.0,
        10.0, 10.0, 10.0,  1.0
    });

    SeamResult res = DPSeamFinder::find_vertical_seam(energy);

    assert(res.valid);
    assert(std::abs(res.energy_sum - 5.0) < 1e-6);
    assert(res.seam == std::vector<int>({1, 2, 2, 3}));
    std::cout << "[PASS] test_known_optimal_vertical_seam\n";
}

void test_rightmost_column_seam() {
    EnergyMap energy(3, 3, {
        50.0, 50.0, 2.0,
        50.0, 50.0, 3.0,
        50.0, 50.0, 1.0
    });

    SeamResult res = DPSeamFinder::find_vertical_seam(energy);

    assert(res.valid);
    assert(std::abs(res.energy_sum - 6.0) < 1e-6);
    assert(res.seam == std::vector<int>({2, 2, 2}));
    std::cout << "[PASS] test_rightmost_column_seam\n";
}

void test_leftmost_column_seam() {
    EnergyMap energy(3, 3, {
        1.0, 50.0, 50.0,
        2.0, 50.0, 50.0,
        3.0, 50.0, 50.0
    });

    SeamResult res = DPSeamFinder::find_vertical_seam(energy);

    assert(res.valid);
    assert(std::abs(res.energy_sum - 6.0) < 1e-6);
    assert(res.seam == std::vector<int>({0, 0, 0}));
    std::cout << "[PASS] test_leftmost_column_seam\n";
}

void test_connectivity_constraint_prevents_jump() {
    // A greedy/unconstrained choice might try col 0 -> col 3, which violates
    // |c_r - c_{r+1}| <= 1. The DP recurrence only ever looks at c-1, c, c+1, so this
    // cannot happen; the optimal 8-connected seam must step diagonally instead.
    EnergyMap energy(4, 4, {
         1.0, 100.0, 100.0, 100.0,
       100.0,   5.0, 100.0, 100.0,
       100.0, 100.0,   5.0, 100.0,
       100.0, 100.0, 100.0,   1.0
    });

    SeamResult res = DPSeamFinder::find_vertical_seam(energy);

    assert(res.valid);
    assert(std::abs(res.energy_sum - 12.0) < 1e-6);
    assert(res.seam == std::vector<int>({0, 1, 2, 3}));
    std::cout << "[PASS] test_connectivity_constraint_prevents_jump\n";
}

void test_horizontal_seam() {
    // 4 cols x 3 rows; optimal horizontal seam at row 1 across all 4 columns
    EnergyMap energy(4, 3, {
        50.0, 50.0, 50.0, 50.0,
         2.0,  1.0,  3.0,  2.0,
        50.0, 50.0, 50.0, 50.0
    });

    SeamResult res = DPSeamFinder::find_horizontal_seam(energy);

    assert(res.valid);
    assert(std::abs(res.energy_sum - 8.0) < 1e-6);
    assert(res.seam == std::vector<int>({1, 1, 1, 1}));
    std::cout << "[PASS] test_horizontal_seam\n";
}

void test_single_column_and_single_row() {
    // Degenerate width-1 image: the only possible vertical seam is column 0 every row.
    EnergyMap col_energy(1, 5, {3.0, 1.0, 4.0, 1.0, 5.0});
    SeamResult col_res = DPSeamFinder::find_vertical_seam(col_energy);
    assert(col_res.valid);
    assert(col_res.seam == std::vector<int>({0, 0, 0, 0, 0}));
    assert(std::abs(col_res.energy_sum - 14.0) < 1e-6);

    // Degenerate height-1 image: the seam is just the single minimum-energy pixel.
    EnergyMap row_energy(5, 1, {3.0, 1.0, 4.0, 1.0, 5.0});
    SeamResult row_res = DPSeamFinder::find_vertical_seam(row_energy);
    assert(row_res.valid);
    assert(row_res.seam == std::vector<int>({1}));
    assert(std::abs(row_res.energy_sum - 1.0) < 1e-6);

    std::cout << "[PASS] test_single_column_and_single_row\n";
}

void test_matches_graph_cut_on_textured_image() {
    // DP and Graph Cut are both globally-optimal, so on the same energy map they must
    // agree on the minimum seam ENERGY (this deterministic texture has no ties, so the
    // column choices match too, but energy equality is the real correctness claim).
    int w = 25, h = 20;
    Image img(w, h, 3);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            uint8_t rr = static_cast<uint8_t>((r * 7 + c * 13) % 256);
            uint8_t g = static_cast<uint8_t>((r * 11 + c * 5) % 256);
            uint8_t b = static_cast<uint8_t>((r * 3 + c * 17) % 256);
            img.set_pixel(r, c, Pixel{rr, g, b});
        }
    }

    EnergyMap energy = compute_energy(img, EnergyMetric::DualGradient);

    DinicSolver dinic;
    SeamResult dp_res = DPSeamFinder::find_vertical_seam(energy);
    SeamResult gc_res = GraphCutSeamFinder::find_vertical_seam(energy, dinic);

    assert(dp_res.valid && gc_res.valid);
    assert(std::abs(dp_res.energy_sum - gc_res.energy_sum) < 1e-6);
    std::cout << "[PASS] test_matches_graph_cut_on_textured_image (DP energy="
              << dp_res.energy_sum << ", Graph Cut energy=" << gc_res.energy_sum << ")\n";
}

void test_dp_carving_shrinks_image() {
    int w = 20, h = 16;
    Image img(w, h, 3);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            uint8_t val = static_cast<uint8_t>((c < 5) ? 200 : 30);
            img.set_pixel(r, c, Pixel{val, val, val});
        }
    }

    SeamCarver carver(SolverType::Dinic, EnergyMetric::DualGradient, AlgorithmType::DynamicProgramming);
    carver.resize(img, 16, 13);
    assert(img.width() == 16);
    assert(img.height() == 13);

    std::cout << "[PASS] test_dp_carving_shrinks_image\n";
}

} // namespace

int main() {
    test_known_optimal_vertical_seam();
    test_rightmost_column_seam();
    test_leftmost_column_seam();
    test_connectivity_constraint_prevents_jump();
    test_horizontal_seam();
    test_single_column_and_single_row();
    test_matches_graph_cut_on_textured_image();
    test_dp_carving_shrinks_image();

    std::cout << "\nAll Dynamic Programming unit tests PASSED!\n";
    return 0;
}

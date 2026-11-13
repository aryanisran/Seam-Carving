#include "benchmark.hpp"
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

void test_known_optimal_vertical_seam() {
    // 4x4 energy grid where optimal diagonal seam is (0,1) -> (1,2) -> (2,2) -> (3,3)
    // Energies along seam: 1 + 2 + 1 + 1 = 5
    EnergyMap energy(4, 4, {
        10.0,  1.0, 10.0, 10.0,
        10.0, 10.0,  2.0, 10.0,
        10.0, 10.0,  1.0, 10.0,
        10.0, 10.0, 10.0,  1.0
    });

    DinicSolver dinic;
    EdmondsKarpSolver ek;

    SeamResult res_d = GraphCutSeamFinder::find_vertical_seam(energy, dinic);
    SeamResult res_ek = GraphCutSeamFinder::find_vertical_seam(energy, ek);

    assert(res_d.valid);
    assert(res_ek.valid);
    assert(std::abs(res_d.energy_sum - 5.0) < 1e-6);
    assert(std::abs(res_ek.energy_sum - 5.0) < 1e-6);
    assert(std::abs(res_d.stats.max_flow - 5.0) < 1e-6);
    assert(std::abs(res_ek.stats.max_flow - 5.0) < 1e-6);
    assert(res_d.seam == std::vector<int>({1, 2, 2, 3}));
    assert(res_ek.seam == std::vector<int>({1, 2, 2, 3}));
    std::cout << "[PASS] test_known_optimal_vertical_seam\n";
}

void test_rightmost_column_seam() {
    // Optimal seam lies entirely on the rightmost column (col = 2)
    EnergyMap energy(3, 3, {
        50.0, 50.0, 2.0,
        50.0, 50.0, 3.0,
        50.0, 50.0, 1.0
    });

    DinicSolver dinic;
    EdmondsKarpSolver ek;

    SeamResult res_d = GraphCutSeamFinder::find_vertical_seam(energy, dinic);
    SeamResult res_ek = GraphCutSeamFinder::find_vertical_seam(energy, ek);

    assert(res_d.valid && res_ek.valid);
    assert(std::abs(res_d.energy_sum - 6.0) < 1e-6);
    assert(std::abs(res_ek.energy_sum - 6.0) < 1e-6);
    assert(res_d.seam == std::vector<int>({2, 2, 2}));
    assert(res_ek.seam == std::vector<int>({2, 2, 2}));
    std::cout << "[PASS] test_rightmost_column_seam\n";
}

void test_leftmost_column_seam() {
    // Optimal seam lies entirely on the leftmost column (col = 0)
    EnergyMap energy(3, 3, {
        1.0, 50.0, 50.0,
        2.0, 50.0, 50.0,
        3.0, 50.0, 50.0
    });

    DinicSolver dinic;
    EdmondsKarpSolver ek;

    SeamResult res_d = GraphCutSeamFinder::find_vertical_seam(energy, dinic);
    SeamResult res_ek = GraphCutSeamFinder::find_vertical_seam(energy, ek);

    assert(res_d.valid && res_ek.valid);
    assert(std::abs(res_d.energy_sum - 6.0) < 1e-6);
    assert(std::abs(res_ek.energy_sum - 6.0) < 1e-6);
    assert(res_d.seam == std::vector<int>({0, 0, 0}));
    assert(res_ek.seam == std::vector<int>({0, 0, 0}));
    std::cout << "[PASS] test_leftmost_column_seam\n";
}

void test_connectivity_constraint_prevents_jump() {
    // Greedy or unconstrained cut might try col 0 -> col 3, which violates |c_r - c_{r+1}| <= 1.
    // Optimal valid 8-connected seam must go (0,0) -> (1,1) -> (2,2) -> (3,3) or similar.
    EnergyMap energy(4, 4, {
         1.0, 100.0, 100.0, 100.0,
       100.0,   5.0, 100.0, 100.0,
       100.0, 100.0,   5.0, 100.0,
       100.0, 100.0, 100.0,   1.0
    });

    DinicSolver dinic;
    EdmondsKarpSolver ek;

    SeamResult res_d = GraphCutSeamFinder::find_vertical_seam(energy, dinic);
    SeamResult res_ek = GraphCutSeamFinder::find_vertical_seam(energy, ek);

    assert(res_d.valid && res_ek.valid);
    assert(std::abs(res_d.energy_sum - 12.0) < 1e-6);
    assert(std::abs(res_ek.energy_sum - 12.0) < 1e-6);
    assert(res_d.seam == std::vector<int>({0, 1, 2, 3}));
    assert(res_ek.seam == std::vector<int>({0, 1, 2, 3}));
    std::cout << "[PASS] test_connectivity_constraint_prevents_jump\n";
}

void test_horizontal_seam() {
    // 4 cols x 3 rows; optimal horizontal seam at row 1 across all 4 columns
    EnergyMap energy(4, 3, {
        50.0, 50.0, 50.0, 50.0,
         2.0,  1.0,  3.0,  2.0,
        50.0, 50.0, 50.0, 50.0
    });

    DinicSolver dinic;
    EdmondsKarpSolver ek;

    SeamResult res_d = GraphCutSeamFinder::find_horizontal_seam(energy, dinic);
    SeamResult res_ek = GraphCutSeamFinder::find_horizontal_seam(energy, ek);

    assert(res_d.valid && res_ek.valid);
    assert(std::abs(res_d.energy_sum - 8.0) < 1e-6);
    assert(std::abs(res_ek.energy_sum - 8.0) < 1e-6);
    assert(res_d.seam == std::vector<int>({1, 1, 1, 1}));
    assert(res_ek.seam == std::vector<int>({1, 1, 1, 1}));
    std::cout << "[PASS] test_horizontal_seam\n";
}

void test_synthetic_image_carving_and_comparison() {
    // Create a 20x16 synthetic image with a high-contrast stripe and low-gradient background
    int w = 20, h = 16;
    Image img(w, h, 3);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            uint8_t val = static_cast<uint8_t>((c < 5) ? 200 : 30);
            img.set_pixel(r, c, Pixel{val, val, val});
        }
    }

    ComparisonSummary summary = BenchmarkRunner::compare_solvers(img, 4, 3, EnergyMetric::DualGradient);
    assert(summary.rows.size() == 7);
    assert(summary.all_energies_matched);

    SeamCarver carver(SolverType::Dinic, EnergyMetric::DualGradient);
    carver.resize(img, 16, 13);
    assert(img.width() == 16);
    assert(img.height() == 13);

    std::cout << "[PASS] test_synthetic_image_carving_and_comparison\n";
}

void test_textured_image_nonzero_flow_benchmark() {
    // 80x60 textured image where every pixel has non-zero gradient energy
    int w = 80, h = 60;
    Image img(w, h, 3);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            uint8_t r_val = static_cast<uint8_t>((r * 7 + c * 13) % 256);
            uint8_t g_val = static_cast<uint8_t>((r * 11 + c * 5) % 256);
            uint8_t b_val = static_cast<uint8_t>((r * 3 + c * 17) % 256);
            img.set_pixel(r, c, Pixel{r_val, g_val, b_val});
        }
    }

    ComparisonSummary summary = BenchmarkRunner::compare_solvers(img, 3, 2, EnergyMetric::DualGradient);
    assert(summary.rows.size() == 5);
    assert(summary.all_energies_matched);
    for (const auto& row : summary.rows) {
        assert(row.dinic_energy > 0.0);
        assert(row.ek_energy > 0.0);
    }
    BenchmarkRunner::print_table(summary);
    std::cout << "[PASS] test_textured_image_nonzero_flow_benchmark\n";
}

} // namespace

int main() {
    test_known_optimal_vertical_seam();
    test_rightmost_column_seam();
    test_leftmost_column_seam();
    test_connectivity_constraint_prevents_jump();
    test_horizontal_seam();
    test_synthetic_image_carving_and_comparison();
    test_textured_image_nonzero_flow_benchmark();

    std::cout << "\nAll Graph Cut unit tests PASSED!\n";
    return 0;
}

#include "dp_seam.hpp"
#include "energy.hpp"
#include "graph_cut_seam.hpp"
#include "image.hpp"
#include "maxflow.hpp"
#include "seam_carver.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using namespace seam_carving;

namespace {

struct BruteForceResult {
    double energy = 0.0;
    std::vector<int> seam;
    bool found = false;
};

bool is_valid_seam(const std::vector<int>& seam, int length, int coordinate_limit) {
    if (static_cast<int>(seam.size()) != length) return false;
    for (int i = 0; i < length; ++i) {
        if (seam[static_cast<std::size_t>(i)] < 0 ||
            seam[static_cast<std::size_t>(i)] >= coordinate_limit) {
            return false;
        }
        if (i > 0 && std::abs(seam[static_cast<std::size_t>(i)] -
                              seam[static_cast<std::size_t>(i - 1)]) > 1) {
            return false;
        }
    }
    return true;
}

// Test-only exhaustive reference. It deliberately does not call the production
// DP recurrence; every valid path is enumerated recursively instead.
void enumerate_vertical_seams(const EnergyMap& energy, int row, int column,
                              std::vector<int>& current, BruteForceResult& best) {
    int height = energy.height();
    if (row == height) {
        double total = 0.0;
        for (int r = 0; r < height; ++r) {
            total += energy.at(r, current[static_cast<std::size_t>(r)]);
        }
        if (!best.found || total < best.energy) {
            best.energy = total;
            best.seam = current;
            best.found = true;
        }
        return;
    }

    current[static_cast<std::size_t>(row)] = column;
    if (row == height - 1) {
        enumerate_vertical_seams(energy, height, column, current, best);
        return;
    }

    for (int next = std::max(0, column - 1);
         next <= std::min(energy.width() - 1, column + 1); ++next) {
        enumerate_vertical_seams(energy, row + 1, next, current, best);
    }
}

BruteForceResult brute_force_vertical(const EnergyMap& energy) {
    BruteForceResult result;
    if (energy.width() <= 0 || energy.height() <= 0) return result;

    std::vector<int> current(static_cast<std::size_t>(energy.height()), 0);
    for (int start = 0; start < energy.width(); ++start) {
        enumerate_vertical_seams(energy, 0, start, current, result);
    }
    return result;
}

BruteForceResult brute_force_horizontal(const EnergyMap& energy) {
    EnergyMap transposed(energy.height(), energy.width());
    for (int row = 0; row < energy.height(); ++row) {
        for (int column = 0; column < energy.width(); ++column) {
            transposed.set(column, row, energy.at(row, column));
        }
    }
    return brute_force_vertical(transposed);
}

void assert_matches_reference(const SeamResult& actual, const BruteForceResult& expected,
                              int seam_length, int coordinate_limit) {
    assert(expected.found);
    assert(actual.valid);
    assert(is_valid_seam(actual.seam, seam_length, coordinate_limit));
    assert(std::abs(actual.energy_sum - expected.energy) < 1e-9);
}

void test_brute_force_reference_on_deterministic_matrices() {
    std::vector<EnergyMap> matrices = {
        EnergyMap(1, 1, {7.0}),
        EnergyMap(1, 5, {3.0, 1.0, 4.0, 1.0, 5.0}),
        EnergyMap(5, 1, {3.0, 1.0, 4.0, 1.0, 5.0}),
        EnergyMap(3, 3, {
            8.0, 2.0, 9.0,
            7.0, 6.0, 3.0,
            4.0, 5.0, 1.0
        }),
        EnergyMap(4, 4, {
            1.0, 1.0, 1.0, 1.0,
            1.0, 1.0, 1.0, 1.0,
            1.0, 1.0, 1.0, 1.0,
            1.0, 1.0, 1.0, 1.0
        }),
        EnergyMap(4, 3, {
            50.0, 50.0, 50.0, 50.0,
             2.0,  1.0,  3.0,  2.0,
            50.0, 50.0, 50.0, 50.0
        })
    };

    for (const EnergyMap& energy : matrices) {
        SeamResult vertical = DPSeamFinder::find_vertical_seam(energy);
        BruteForceResult vertical_reference = brute_force_vertical(energy);
        assert_matches_reference(vertical, vertical_reference, energy.height(), energy.width());

        SeamResult horizontal = DPSeamFinder::find_horizontal_seam(energy);
        BruteForceResult horizontal_reference = brute_force_horizontal(energy);
        assert_matches_reference(horizontal, horizontal_reference, energy.width(), energy.height());
    }

    std::cout << "[PASS] test_brute_force_reference_on_deterministic_matrices\n";
}

void test_brute_force_reference_on_random_matrices() {
    std::mt19937 generator(3131);
    std::uniform_int_distribution<int> dimension(1, 5);
    std::uniform_int_distribution<int> value(0, 20);

    for (int case_index = 0; case_index < 100; ++case_index) {
        int width = dimension(generator);
        int height = dimension(generator);
        std::vector<double> values(static_cast<std::size_t>(width) * height);
        for (double& entry : values) entry = static_cast<double>(value(generator));
        EnergyMap energy(width, height, values);

        SeamResult vertical = DPSeamFinder::find_vertical_seam(energy);
        assert_matches_reference(vertical, brute_force_vertical(energy), height, width);

        SeamResult horizontal = DPSeamFinder::find_horizontal_seam(energy);
        assert_matches_reference(horizontal, brute_force_horizontal(energy), width, height);
    }

    std::cout << "[PASS] test_brute_force_reference_on_random_matrices\n";
}

Image make_unique_rgb_image(int width, int height) {
    Image image(width, height, 3);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            image.set_pixel(row, column, Pixel{
                static_cast<uint8_t>(10 + row * 20 + column),
                static_cast<uint8_t>(80 + row * 20 + column),
                static_cast<uint8_t>(160 + row * 20 + column)
            });
        }
    }
    return image;
}

void assert_same_pixels(const Image& actual, const Image& expected) {
    assert(actual.width() == expected.width());
    assert(actual.height() == expected.height());
    assert(actual.channels() == expected.channels());
    assert(actual.data() == expected.data());
}

void test_vertical_seam_removal_preserves_pixel_order() {
    Image actual = make_unique_rgb_image(4, 3);
    const std::vector<int> seam = {1, 2, 1};

    Image expected(3, 3, 3);
    for (int row = 0; row < 3; ++row) {
        int destination_column = 0;
        for (int column = 0; column < 4; ++column) {
            if (column == seam[static_cast<std::size_t>(row)]) continue;
            expected.set_pixel(row, destination_column++, actual.get_pixel(row, column));
        }
    }

    actual.remove_vertical_seam(seam);
    assert_same_pixels(actual, expected);
    std::cout << "[PASS] test_vertical_seam_removal_preserves_pixel_order\n";
}

void test_horizontal_seam_removal_preserves_pixel_order() {
    Image actual = make_unique_rgb_image(4, 3);
    const std::vector<int> seam = {1, 0, 2, 1};

    Image expected(4, 2, 3);
    for (int column = 0; column < 4; ++column) {
        int destination_row = 0;
        for (int row = 0; row < 3; ++row) {
            if (row == seam[static_cast<std::size_t>(column)]) continue;
            expected.set_pixel(destination_row++, column, actual.get_pixel(row, column));
        }
    }

    actual.remove_horizontal_seam(seam);
    assert_same_pixels(actual, expected);
    std::cout << "[PASS] test_horizontal_seam_removal_preserves_pixel_order\n";
}

void test_repeated_dp_resize_preserves_dimensions_and_rgb_storage() {
    Image image = make_unique_rgb_image(6, 5);
    SeamCarver carver(SolverType::Dinic, EnergyMetric::DualGradient,
                      AlgorithmType::DynamicProgramming);
    carver.resize(image, 3, 2);

    assert(image.width() == 3);
    assert(image.height() == 2);
    assert(image.channels() == 3);
    assert(image.data().size() == static_cast<std::size_t>(3 * 2 * 3));
    std::cout << "[PASS] test_repeated_dp_resize_preserves_dimensions_and_rgb_storage\n";
}

template <typename Function>
void assert_throws_invalid_argument(Function&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void test_invalid_resize_requests_are_rejected() {
    Image image = make_unique_rgb_image(3, 3);

    assert_throws_invalid_argument([&] { image.remove_vertical_seam({0, 1}); });
    assert_throws_invalid_argument([&] { image.remove_horizontal_seam({0, 1}); });

    SeamCarver carver(SolverType::Dinic, EnergyMetric::DualGradient,
                      AlgorithmType::DynamicProgramming);
    assert_throws_invalid_argument([&] { carver.resize(image, 0, 2); });
    assert_throws_invalid_argument([&] { carver.resize(image, 2, 0); });
    assert_throws_invalid_argument([&] { carver.resize(image, 4, 2); });
    assert_throws_invalid_argument([&] { carver.resize(image, 2, 4); });
    assert_throws_invalid_argument([&] { carver.carve_vertical_seams(image, 3); });
    assert_throws_invalid_argument([&] { carver.carve_horizontal_seams(image, 3); });

    std::cout << "[PASS] test_invalid_resize_requests_are_rejected\n";
}

void test_small_image_removal_boundaries() {
    Image one_column = make_unique_rgb_image(1, 3);
    Image one_row = make_unique_rgb_image(3, 1);
    SeamCarver carver(SolverType::Dinic, EnergyMetric::DualGradient,
                      AlgorithmType::DynamicProgramming);

    assert_throws_invalid_argument([&] { carver.carve_vertical_seams(one_column, 1); });
    assert_throws_invalid_argument([&] { carver.carve_horizontal_seams(one_row, 1); });

    std::cout << "[PASS] test_small_image_removal_boundaries\n";
}

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
    test_brute_force_reference_on_deterministic_matrices();
    test_brute_force_reference_on_random_matrices();
    test_known_optimal_vertical_seam();
    test_rightmost_column_seam();
    test_leftmost_column_seam();
    test_connectivity_constraint_prevents_jump();
    test_horizontal_seam();
    test_single_column_and_single_row();
    test_matches_graph_cut_on_textured_image();
    test_dp_carving_shrinks_image();
    test_vertical_seam_removal_preserves_pixel_order();
    test_horizontal_seam_removal_preserves_pixel_order();
    test_repeated_dp_resize_preserves_dimensions_and_rgb_storage();
    test_invalid_resize_requests_are_rejected();
    test_small_image_removal_boundaries();

    std::cout << "\nAll Dynamic Programming unit tests PASSED!\n";
    return 0;
}

#include "dp_seam.hpp"
#include "energy.hpp"
#include "graph_cut_seam.hpp"
#include "greedy_seam.hpp"
#include "image.hpp"
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

bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

EnergyMap random_energy(std::mt19937& rng, int width, int height) {
    std::uniform_real_distribution<double> d(0.5, 100.0);
    EnergyMap e(width, height);
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c) e.set(r, c, d(rng));
    return e;
}

EnergyMap transpose(const EnergyMap& e) {
    EnergyMap t(e.height(), e.width());
    for (int r = 0; r < e.height(); ++r)
        for (int c = 0; c < e.width(); ++c) t.set(c, r, e.at(r, c));
    return t;
}

// Test-only exhaustive reference. Every valid 8-connected vertical seam is enumerated
// recursively (exponentially many, see Question 2(a)(i)), so this is only used on tiny maps.
double brute_force_min(const EnergyMap& e, int r, int c) {
    double here = e.at(r, c);
    if (r == e.height() - 1) return here;
    double best = 1e300;
    for (int dc = -1; dc <= 1; ++dc) {
        int nc = c + dc;
        if (nc >= 0 && nc < e.width()) best = std::min(best, brute_force_min(e, r + 1, nc));
    }
    return here + best;
}

double brute_force_min(const EnergyMap& e) {
    double best = 1e300;
    for (int c = 0; c < e.width(); ++c) best = std::min(best, brute_force_min(e, 0, c));
    return best;
}

// Energy of the pixels a vertical seam actually visits.
double vertical_seam_energy(const EnergyMap& e, const std::vector<int>& seam) {
    double sum = 0.0;
    for (int r = 0; r < e.height(); ++r) sum += e.at(r, seam[static_cast<std::size_t>(r)]);
    return sum;
}

void test_greedy_is_not_optimal_counter_example() {
    // 3x4 energy grid where greedy takes the cheap 1s on the left and is then trapped above
    // a wall of 99s: 1 + 1 + 99 + 1 = 102. DP pays 2 instead of 1 in row 1 to go around it:
    // 1 + 2 + 2 + 1 = 6.
    EnergyMap e(3, 4, {
         1,  2,  9,
         1,  2,  9,
        99, 99,  2,
         1,  1,  1,
    });
    SeamResult greedy = GreedySeamFinder::find_vertical_seam(e);
    SeamResult dp = DPSeamFinder::find_vertical_seam(e);

    assert(greedy.valid && dp.valid);
    assert(greedy.seam == std::vector<int>({0, 0, 0, 0}));
    assert(near(greedy.energy_sum, 102.0));
    assert(near(dp.energy_sum, 6.0));
    assert(near(brute_force_min(e), 6.0));
    std::cout << "[PASS] test_greedy_is_not_optimal_counter_example (greedy=102, DP=6)\n";
}

void test_greedy_follows_obvious_valley() {
    // 8x8 energy grid with a zero-energy diagonal (0,0) -> (7,7). Here the locally cheapest
    // step is also globally optimal, so greedy finds the same seam energy as DP.
    EnergyMap e(8, 8);
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) e.set(r, c, c == r ? 0.0 : 50.0);
    SeamResult greedy = GreedySeamFinder::find_vertical_seam(e);

    assert(greedy.valid);
    for (int r = 0; r < 8; ++r) assert(greedy.seam[static_cast<std::size_t>(r)] == r);
    assert(near(greedy.energy_sum, 0.0));
    assert(near(DPSeamFinder::find_vertical_seam(e).energy_sum, 0.0));
    std::cout << "[PASS] test_greedy_follows_obvious_valley\n";
}

void test_greedy_tie_breaking_and_borders() {
    // Uniform 5x6 energy grid: every choice ties, so greedy starts at col 0 and goes straight down.
    EnergyMap flat(5, 6);
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 5; ++c) flat.set(r, c, 7.0);
    assert(GreedySeamFinder::find_vertical_seam(flat).seam == std::vector<int>(6, 0));

    // Optimal seam lies entirely on the rightmost column (col = 3); greedy must never index col 4.
    EnergyMap right(4, 5);
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 4; ++c) right.set(r, c, c == 3 ? 1.0 : 10.0);
    SeamResult greedy = GreedySeamFinder::find_vertical_seam(right);
    assert(greedy.valid);
    assert(greedy.seam == std::vector<int>(5, 3));
    std::cout << "[PASS] test_greedy_tie_breaking_and_borders\n";
}

void test_greedy_degenerate_sizes() {
    // Degenerate width-1 image: the only possible vertical seam is column 0 every row.
    EnergyMap one_column(1, 5, {3, 1, 4, 1, 5});
    SeamResult greedy = GreedySeamFinder::find_vertical_seam(one_column);
    assert(greedy.seam == std::vector<int>(5, 0));
    assert(near(greedy.energy_sum, 14.0));

    // Degenerate height-1 image: the seam is just the single minimum-energy pixel.
    EnergyMap one_row(5, 1, {3, 1, 4, 0.5, 5});
    assert(GreedySeamFinder::find_vertical_seam(one_row).seam == std::vector<int>({3}));

    // Empty map gives an invalid result; a start column outside [0, W) throws.
    EnergyMap empty;
    assert(!GreedySeamFinder::find_vertical_seam(empty).valid);
    assert(!GreedySeamFinder::find_horizontal_seam(empty).valid);

    bool threw = false;
    try {
        GreedySeamFinder::walk_from(one_row, 5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
    std::cout << "[PASS] test_greedy_degenerate_sizes\n";
}

void test_greedy_random_maps_vs_dp_and_brute_force() {
    // 300 random energy maps (2-7 cols x 2-8 rows), vertical and horizontal. The greedy seam
    // must be valid, its reported energy must match the pixels it visits, and it can never
    // beat the optimum. DP is checked against brute force too, so the optimum is trusted.
    std::mt19937 rng(7);
    int greedy_optimal = 0;
    double worst_ratio = 1.0;
    const int trials = 300;
    for (int t = 0; t < trials; ++t) {
        int width = 2 + static_cast<int>(rng() % 6);
        int height = 2 + static_cast<int>(rng() % 7);
        EnergyMap e = random_energy(rng, width, height);

        // Vertical seam
        SeamResult greedy = GreedySeamFinder::find_vertical_seam(e);
        double optimum = brute_force_min(e);
        assert(greedy.valid);
        assert(near(vertical_seam_energy(e, greedy.seam), greedy.energy_sum));
        assert(near(DPSeamFinder::find_vertical_seam(e).energy_sum, optimum));
        assert(optimum <= greedy.energy_sum + 1e-9);
        if (near(greedy.energy_sum, optimum)) ++greedy_optimal;
        worst_ratio = std::max(worst_ratio, greedy.energy_sum / optimum);

        // Horizontal seam: one row index per column, checked on the transpose.
        SeamResult horizontal = GreedySeamFinder::find_horizontal_seam(e);
        EnergyMap t_map = transpose(e);
        assert(horizontal.valid);
        assert(static_cast<int>(horizontal.seam.size()) == width);
        assert(GraphCutSeamFinder::validate_vertical_seam(horizontal.seam, width, height));
        assert(near(vertical_seam_energy(t_map, horizontal.seam), horizontal.energy_sum));
        assert(brute_force_min(t_map) <= horizontal.energy_sum + 1e-9);
    }
    std::cout << "[PASS] test_greedy_random_maps_vs_dp_and_brute_force (greedy optimal on "
              << greedy_optimal << "/" << trials << ", worst " << worst_ratio << "x the optimum)\n";
}

// Test-only reference greedy written directly from the assignment text, kept independent of
// GreedySeamFinder: cheapest pixel in row 0 (leftmost on ties), then per row the cheapest of
// straight down / left / right (in that order on ties).
std::vector<int> reference_greedy(const EnergyMap& e) {
    std::vector<int> seam(static_cast<std::size_t>(e.height()));
    int c = 0;
    for (int col = 1; col < e.width(); ++col)
        if (e.at(0, col) < e.at(0, c)) c = col;
    seam[0] = c;
    for (int r = 1; r < e.height(); ++r) {
        int best = c;
        if (c - 1 >= 0 && e.at(r, c - 1) < e.at(r, best)) best = c - 1;
        if (c + 1 < e.width() && e.at(r, c + 1) < e.at(r, best)) best = c + 1;
        c = best;
        seam[static_cast<std::size_t>(r)] = c;
    }
    return seam;
}

void test_greedy_matches_reference_with_ties() {
    // 2,000 random maps (1-40 cols x 1-40 rows) with integer energies 0-3, so ties are very
    // common. The greedy seam must match the reference walk exactly, pixel for pixel.
    std::mt19937 rng(2024);
    for (int t = 0; t < 2000; ++t) {
        int width = 1 + static_cast<int>(rng() % 40);
        int height = 1 + static_cast<int>(rng() % 40);
        EnergyMap e(width, height);
        for (int r = 0; r < height; ++r)
            for (int c = 0; c < width; ++c) e.set(r, c, static_cast<double>(rng() % 4));
        assert(GreedySeamFinder::find_vertical_seam(e).seam == reference_greedy(e));
    }
    std::cout << "[PASS] test_greedy_matches_reference_with_ties\n";
}

void test_greedy_records_timing() {
    // 50x40 random energy grid; greedy must fill stats.elapsed_ms like DPSeamFinder does.
    std::mt19937 rng(3);
    SeamResult greedy = GreedySeamFinder::find_vertical_seam(random_energy(rng, 50, 40));
    assert(greedy.valid);
    assert(greedy.stats.elapsed_ms >= 0.0); // exported as "elapsed_ms" by --export-seams
    std::cout << "[PASS] test_greedy_records_timing\n";
}

void test_seam_carver_with_greedy() {
    // Create a 40x30 synthetic image and resize it through SeamCarver, the same path the CLI
    // takes for --algorithm greedy.
    Image img(40, 30);
    for (int r = 0; r < 30; ++r)
        for (int c = 0; c < 40; ++c)
            img.set_pixel(r, c, Pixel{static_cast<uint8_t>((r * 7 + c * 13) % 256),
                                      static_cast<uint8_t>((r * c) % 256),
                                      static_cast<uint8_t>((c * 5) % 256)});
    SeamCarver carver(SolverType::Dinic, EnergyMetric::DualGradient, AlgorithmType::Greedy);
    std::vector<CarveRecord> records;
    carver.resize(img, 32, 25, &records);

    assert(img.width() == 32 && img.height() == 25);
    assert(records.size() == 13); // 8 vertical + 5 horizontal seams
    for (const CarveRecord& rec : records) {
        int length = rec.is_vertical ? rec.height_before : rec.width_before;
        int limit = rec.is_vertical ? rec.width_before : rec.height_before;
        assert(GraphCutSeamFinder::validate_vertical_seam(rec.seam, length, limit));
    }
    std::cout << "[PASS] test_seam_carver_with_greedy (40x30 -> 32x25)\n";
}

} // namespace

int main() {
    test_greedy_is_not_optimal_counter_example();
    test_greedy_follows_obvious_valley();
    test_greedy_tie_breaking_and_borders();
    test_greedy_degenerate_sizes();
    test_greedy_random_maps_vs_dp_and_brute_force();
    test_greedy_matches_reference_with_ties();
    test_greedy_records_timing();
    test_seam_carver_with_greedy();

    std::cout << "\nAll Greedy unit tests PASSED!\n";
    return 0;
}

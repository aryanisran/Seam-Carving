#include "greedy_seam.hpp"

#include <chrono>
#include <stdexcept>

namespace seam_carving {

std::vector<int> GreedySeamFinder::walk_from(const EnergyMap& energy, int start_col,
                                             double* total_energy) {
    const int h = energy.height();
    const int w = energy.width();
    if (h <= 0 || w <= 0) {
        throw std::invalid_argument("GreedySeamFinder: energy map is empty");
    }
    if (start_col < 0 || start_col >= w) {
        throw std::out_of_range("GreedySeamFinder: start column out of range");
    }

    std::vector<int> seam(static_cast<std::size_t>(h));
    int c = start_col;
    seam[0] = c;
    double sum = energy.at(0, c);

    for (int r = 1; r < h; ++r) {
        // Local decision: cheapest of the three 8-connected pixels below.
        // Ties go straight down, then left, then right (same order as DPSeamFinder).
        int best_c = c;
        double best_e = energy.at(r, c);
        if (c > 0 && energy.at(r, c - 1) < best_e) {
            best_c = c - 1;
            best_e = energy.at(r, c - 1);
        }
        if (c < w - 1 && energy.at(r, c + 1) < best_e) {
            best_c = c + 1;
            best_e = energy.at(r, c + 1);
        }
        c = best_c;
        seam[r] = c;
        sum += best_e;
    }

    if (total_energy) *total_energy = sum;
    return seam;
}

SeamResult GreedySeamFinder::find_vertical_seam(const EnergyMap& energy) {
    SeamResult result;
    const int h = energy.height();
    const int w = energy.width();
    if (h <= 0 || w <= 0) return result;

    auto t0 = std::chrono::high_resolution_clock::now();

    // Start at the cheapest pixel in the first row (leftmost on ties).
    int start = 0;
    for (int c = 1; c < w; ++c) {
        if (energy.at(0, c) < energy.at(0, start)) start = c;
    }

    // Walk downward, making one local decision per row.
    result.seam = walk_from(energy, start, &result.energy_sum);

    auto t1 = std::chrono::high_resolution_clock::now();
    // Same timing field DPSeamFinder fills; exported as "elapsed_ms" by --export-seams.
    result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    result.valid = GraphCutSeamFinder::validate_vertical_seam(result.seam, h, w);
    return result;
}

SeamResult GreedySeamFinder::find_horizontal_seam(const EnergyMap& energy) {
    const int h = energy.height();
    const int w = energy.width();
    if (h <= 0 || w <= 0) return SeamResult{};

    // Transpose: new_width = h, new_height = w (identical convention to
    // GraphCutSeamFinder::find_horizontal_seam and DPSeamFinder::find_horizontal_seam).
    EnergyMap transposed(h, w);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            transposed.set(c, r, energy.at(r, c));
        }
    }
    return find_vertical_seam(transposed);
}

} // namespace seam_carving

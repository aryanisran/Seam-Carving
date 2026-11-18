#include "dp_seam.hpp"

#include <chrono>

namespace seam_carving {

SeamResult DPSeamFinder::find_vertical_seam(const EnergyMap& energy) {
    SeamResult result;
    int h = energy.height();
    int w = energy.width();
    if (h <= 0 || w <= 0) return result;

    auto t0 = std::chrono::high_resolution_clock::now();

    // cost[r*w + c]   = minimum cumulative energy of a seam ending at pixel (r, c)
    // choice[r*w + c] = column chosen in row r-1 that achieves that minimum
    //                   (left as -1 for row 0, which has no predecessor)
    std::vector<double> cost(static_cast<std::size_t>(h) * w);
    std::vector<int> choice(static_cast<std::size_t>(h) * w, -1);

    for (int c = 0; c < w; ++c) {
        cost[static_cast<std::size_t>(c)] = energy.at(0, c);
    }

    for (int r = 1; r < h; ++r) {
        const std::size_t prev_row = static_cast<std::size_t>(r - 1) * w;
        const std::size_t cur_row = static_cast<std::size_t>(r) * w;
        for (int c = 0; c < w; ++c) {
            double best = cost[prev_row + static_cast<std::size_t>(c)];
            int best_c = c;

            if (c > 0 && cost[prev_row + static_cast<std::size_t>(c - 1)] < best) {
                best = cost[prev_row + static_cast<std::size_t>(c - 1)];
                best_c = c - 1;
            }
            if (c < w - 1 && cost[prev_row + static_cast<std::size_t>(c + 1)] < best) {
                best = cost[prev_row + static_cast<std::size_t>(c + 1)];
                best_c = c + 1;
            }

            cost[cur_row + static_cast<std::size_t>(c)] = energy.at(r, c) + best;
            choice[cur_row + static_cast<std::size_t>(c)] = best_c;
        }
    }

    // The optimal seam ends wherever the last row's cumulative cost is smallest.
    const std::size_t last_row = static_cast<std::size_t>(h - 1) * w;
    int best_c = 0;
    double best_cost = cost[last_row];
    for (int c = 1; c < w; ++c) {
        if (cost[last_row + static_cast<std::size_t>(c)] < best_cost) {
            best_cost = cost[last_row + static_cast<std::size_t>(c)];
            best_c = c;
        }
    }

    // Backtrack from the last row up to row 0 to recover the seam path.
    result.seam.resize(static_cast<std::size_t>(h));
    result.seam[static_cast<std::size_t>(h - 1)] = best_c;
    for (int r = h - 2; r >= 0; --r) {
        best_c = choice[static_cast<std::size_t>(r + 1) * w + static_cast<std::size_t>(best_c)];
        result.seam[static_cast<std::size_t>(r)] = best_c;
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    result.energy_sum = best_cost;
    result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    result.valid = GraphCutSeamFinder::validate_vertical_seam(result.seam, h, w);
    return result;
}

SeamResult DPSeamFinder::find_horizontal_seam(const EnergyMap& energy) {
    int h = energy.height();
    int w = energy.width();
    if (h <= 0 || w <= 0) return SeamResult{};

    // Transpose: new_width = h, new_height = w (identical convention to
    // GraphCutSeamFinder::find_horizontal_seam).
    EnergyMap transposed(h, w);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            transposed.set(c, r, energy.at(r, c));
        }
    }

    return find_vertical_seam(transposed);
}

} // namespace seam_carving

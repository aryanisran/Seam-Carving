#ifndef GREEDY_SEAM_HPP
#define GREEDY_SEAM_HPP

#include "energy.hpp"
#include "graph_cut_seam.hpp" // reuses the shared SeamResult type (seam, energy_sum, stats, valid)
#include <vector>

namespace seam_carving {

// Finds seams using a Greedy Algorithm (Assignment 2, Item 3). Instead of solving the whole
// problem like DP, it makes one local, row-by-row choice and never revisits it. It is compared
// against the DP baseline in tests/compare_greedy_dp.cpp.
class GreedySeamFinder {
public:
    // Finds a vertical seam greedily. Starts at the minimum-energy pixel of row 0, then in every
    // following row moves to the cheapest of the (up to) three 8-connected pixels below:
    //
    //     c(0) = argmin_c e(0, c)
    //     c(r) = argmin over c in {c(r-1)-1, c(r-1), c(r-1)+1} of e(r, c)   for r > 0
    //
    // (neighbours outside [0, W) are simply excluded). Ties go straight down, then left, then
    // right -- the same order DPSeamFinder uses when picking a predecessor.
    //
    // Time:  O(W + H) -- W comparisons to pick the start, then 3 per row.
    // Space: O(H)     -- only the seam itself; no table is kept.
    //
    // Not optimal: a cheap pixel now can lead the seam into an expensive region later
    // (see test_greedy_is_not_optimal_counter_example in tests/test_greedy.cpp).
    static SeamResult find_vertical_seam(const EnergyMap& energy);

    // Finds a horizontal seam by transposing the energy map and delegating to
    // find_vertical_seam -- the same convention DPSeamFinder and GraphCutSeamFinder use.
    static SeamResult find_horizontal_seam(const EnergyMap& energy);

    // Walks greedily from a fixed start column in row 0 and returns the seam; the seam's total
    // energy is written to total_energy if it is not null. Throws std::out_of_range if
    // start_col is outside [0, W).
    static std::vector<int> walk_from(const EnergyMap& energy, int start_col,
                                      double* total_energy = nullptr);
};

} // namespace seam_carving

#endif // GREEDY_SEAM_HPP

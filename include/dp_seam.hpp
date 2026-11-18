#ifndef DP_SEAM_HPP
#define DP_SEAM_HPP

#include "energy.hpp"
#include "graph_cut_seam.hpp" // reuses the shared SeamResult type (seam, energy_sum, stats, valid)

namespace seam_carving {

// Finds minimum-energy seams using bottom-up Dynamic Programming (Assignment 2, Item 1).
// This is the DP baseline that the Greedy and Graph Cut implementations are compared
// against in the Analysis section (Item 5c).
class DPSeamFinder {
public:
    // Finds the globally minimum-energy vertical seam.
    //
    // Builds a cumulative-cost table M with the same shape as the energy map, where
    // M(r, c) is the minimum total energy of any 8-connected seam running from row 0
    // down to pixel (r, c):
    //
    //     M(0, c) = e(0, c)
    //     M(r, c) = e(r, c) + min( M(r-1, c-1), M(r-1, c), M(r-1, c+1) )   for r > 0
    //
    // (neighbours outside [0, W) are simply excluded from the min). The smallest entry
    // in the last row is the optimal seam's total energy; the seam itself is recovered
    // by backtracking the argmin recorded at each cell.
    //
    // Time:  O(H * W) -- each of the H*W cells does O(1) work.
    // Space: O(H * W) -- a predecessor table is required to backtrack the actual seam
    //        path. The cost table M only ever needs the previous row at any point in
    //        time and could be rolled down to O(W) if only the minimum energy (not the
    //        seam itself) were needed -- see the "do we need the entire DP table?"
    //        discussion in the report (Item 5d). The predecessor table is O(H * W)
    //        either way, so this implementation keeps both tables in full for clarity.
    static SeamResult find_vertical_seam(const EnergyMap& energy);

    // Finds the optimal horizontal seam by transposing the energy map and delegating to
    // find_vertical_seam -- the same convention GraphCutSeamFinder uses, so DP, Greedy
    // and Graph Cut all handle horizontal seams identically.
    static SeamResult find_horizontal_seam(const EnergyMap& energy);
};

} // namespace seam_carving

#endif // DP_SEAM_HPP

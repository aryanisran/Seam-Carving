#ifndef GRAPH_CUT_SEAM_HPP
#define GRAPH_CUT_SEAM_HPP

#include "energy.hpp"
#include "graph.hpp"
#include "maxflow.hpp"
#include <vector>

namespace seam_carving {

struct SeamResult {
    std::vector<int> seam; // seam[row] = col (for vertical) or seam[col] = row (for horizontal)
    double energy_sum = 0.0;
    MaxFlowStats stats;
    bool valid = false;
};

class GraphCutSeamFinder {
public:
    // Builds the s-t flow network for vertical seam finding on the given energy map.
    // Source node is 0, Sink node is H * W + 1, pixel (r, c) is 1 + r * W + c.
    static void build_vertical_graph(const EnergyMap& energy, FlowGraph& graph, int& source, int& sink);

    // Finds the optimal vertical seam using the specified MaxFlowSolver.
    static SeamResult find_vertical_seam(const EnergyMap& energy, MaxFlowSolver& solver);

    // Finds the optimal horizontal seam by transposing the energy map and delegating to vertical seam search.
    static SeamResult find_horizontal_seam(const EnergyMap& energy, MaxFlowSolver& solver);

    // Verifies that seam length == height, 0 <= seam[r] < width, and |seam[r] - seam[r+1]| <= 1.
    static bool validate_vertical_seam(const std::vector<int>& seam, int height, int width);
};

} // namespace seam_carving

#endif // GRAPH_CUT_SEAM_HPP

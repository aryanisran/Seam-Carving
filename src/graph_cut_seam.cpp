#include "graph_cut_seam.hpp"

#include <cmath>
#include <stdexcept>

namespace seam_carving {

namespace {
inline int pixel_node(int r, int c, int width) {
    return 1 + r * width + c;
}
} // namespace

void GraphCutSeamFinder::build_vertical_graph(const EnergyMap& energy, FlowGraph& graph, int& source, int& sink) {
    int h = energy.height();
    int w = energy.width();
    if (h <= 0 || w <= 0) {
        throw std::invalid_argument("EnergyMap cannot be empty");
    }

    source = 0;
    sink = h * w + 1;
    graph.reset(h * w + 2);

    graph.reserve_vertex(source, static_cast<std::size_t>(h));
    graph.reserve_vertex(sink, static_cast<std::size_t>(h));
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            graph.reserve_vertex(pixel_node(r, c, w), 6);
        }
    }

    for (int r = 0; r < h; ++r) {
        // Connect source to first column with infinite capacity
        graph.add_edge(source, pixel_node(r, 0, w), INF_CAP);

        // Connect last column to sink with capacity equal to removing pixel (r, w-1)
        graph.add_edge(pixel_node(r, w - 1, w), sink, energy.at(r, w - 1));

        for (int c = 0; c < w - 1; ++c) {
            int u = pixel_node(r, c, w);
            int v = pixel_node(r, c + 1, w);
            // Forward edge capacity = cost of removing pixel (r, c)
            // Backward infinite-capacity edge ensures monotonicity (1 pixel per row)
            graph.add_edge(u, v, energy.at(r, c), INF_CAP);
        }

        // Diagonal infinite-capacity edges enforce 8-connected seam constraint |c_r - c_{r+-1}| <= 1
        for (int c = 1; c < w; ++c) {
            int u = pixel_node(r, c, w);
            if (r > 0) {
                graph.add_edge(u, pixel_node(r - 1, c - 1, w), INF_CAP);
            }
            if (r < h - 1) {
                graph.add_edge(u, pixel_node(r + 1, c - 1, w), INF_CAP);
            }
        }
    }
}

SeamResult GraphCutSeamFinder::find_vertical_seam(const EnergyMap& energy, MaxFlowSolver& solver) {
    SeamResult result;
    int h = energy.height();
    int w = energy.width();
    if (h <= 0 || w <= 0) return result;

    FlowGraph graph;
    int source = 0;
    int sink = 0;
    build_vertical_graph(energy, graph, source, sink);

    result.stats = solver.solve(graph, source, sink);

    std::vector<bool> in_source = MaxFlowSolver::compute_source_cut(graph, source);

    result.seam.resize(static_cast<std::size_t>(h), 0);
    result.energy_sum = 0.0;

    for (int r = 0; r < h; ++r) {
        int rightmost_c = 0;
        for (int c = w - 1; c >= 0; --c) {
            if (in_source[pixel_node(r, c, w)]) {
                rightmost_c = c;
                break;
            }
        }
        result.seam[r] = rightmost_c;
        result.energy_sum += energy.at(r, rightmost_c);
    }

    result.valid = validate_vertical_seam(result.seam, h, w);
    return result;
}

SeamResult GraphCutSeamFinder::find_horizontal_seam(const EnergyMap& energy, MaxFlowSolver& solver) {
    int h = energy.height();
    int w = energy.width();
    if (h <= 0 || w <= 0) return SeamResult{};

    // Transpose energy map: new_width = h, new_height = w
    EnergyMap transposed(h, w);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            transposed.set(c, r, energy.at(r, c));
        }
    }

    return find_vertical_seam(transposed, solver);
}

bool GraphCutSeamFinder::validate_vertical_seam(const std::vector<int>& seam, int height, int width) {
    if (static_cast<int>(seam.size()) != height) return false;
    for (int r = 0; r < height; ++r) {
        if (seam[r] < 0 || seam[r] >= width) return false;
        if (r > 0 && std::abs(seam[r] - seam[r - 1]) > 1) return false;
    }
    return true;
}

} // namespace seam_carving

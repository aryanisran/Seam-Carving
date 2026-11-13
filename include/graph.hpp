#ifndef GRAPH_CUT_GRAPH_HPP
#define GRAPH_CUT_GRAPH_HPP

#include <cstddef>
#include <vector>

namespace seam_carving {

constexpr double INF_CAP = 1e15;
constexpr double FLOW_EPS = 1e-9;

struct Edge {
    int to;
    int rev;
    double capacity;
    double flow;

    double residual() const { return capacity - flow; }
};

class FlowGraph {
public:
    explicit FlowGraph(int num_vertices = 0);

    void reset(int num_vertices);
    void reset_flows();

    void add_edge(int u, int v, double cap);

    int num_vertices() const { return static_cast<int>(adj_.size()); }
    std::size_t num_edges() const { return edge_count_; }

    std::vector<Edge>& adj(int u) { return adj_[u]; }
    const std::vector<Edge>& adj(int u) const { return adj_[u]; }

private:
    std::vector<std::vector<Edge>> adj_;
    std::size_t edge_count_ = 0;
};

} // namespace seam_carving

#endif // GRAPH_CUT_GRAPH_HPP

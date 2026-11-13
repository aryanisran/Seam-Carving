#include "graph.hpp"

namespace seam_carving {

FlowGraph::FlowGraph(int num_vertices)
    : adj_(static_cast<std::size_t>(num_vertices)) {}

void FlowGraph::reset(int num_vertices) {
    adj_.assign(static_cast<std::size_t>(num_vertices), {});
    edge_count_ = 0;
}

void FlowGraph::reset_flows() {
    for (auto& edges : adj_) {
        for (auto& e : edges) {
            e.flow = 0.0;
        }
    }
}

void FlowGraph::add_edge(int u, int v, double cap) {
    Edge fwd{v, static_cast<int>(adj_[v].size()), cap, 0.0};
    Edge bwd{u, static_cast<int>(adj_[u].size()), 0.0, 0.0};
    adj_[u].push_back(fwd);
    adj_[v].push_back(bwd);
    ++edge_count_;
}

} // namespace seam_carving

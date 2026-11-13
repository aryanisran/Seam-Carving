#ifndef GRAPH_CUT_MAXFLOW_HPP
#define GRAPH_CUT_MAXFLOW_HPP

#include "graph.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace seam_carving {

enum class SolverType {
    Dinic,
    EdmondsKarp
};

struct MaxFlowStats {
    double max_flow = 0.0;
    std::size_t augmenting_paths = 0;
    std::size_t bfs_runs = 0;
    double elapsed_ms = 0.0;
};

class MaxFlowSolver {
public:
    virtual ~MaxFlowSolver() = default;

    virtual MaxFlowStats solve(FlowGraph& graph, int source, int sink) = 0;
    virtual std::string name() const = 0;

    // Finds the source side S of the minimum s-t cut via BFS in the residual graph
    static std::vector<bool> compute_source_cut(const FlowGraph& graph, int source);
};

class EdmondsKarpSolver : public MaxFlowSolver {
public:
    MaxFlowStats solve(FlowGraph& graph, int source, int sink) override;
    std::string name() const override { return "Edmonds-Karp"; }
};

class DinicSolver : public MaxFlowSolver {
public:
    MaxFlowStats solve(FlowGraph& graph, int source, int sink) override;
    std::string name() const override { return "Dinic"; }

private:
    bool bfs(const FlowGraph& graph, int source, int sink, std::vector<int>& level) const;
    double dfs(FlowGraph& graph, int u, int sink, double pushed,
               std::vector<int>& level, std::vector<std::size_t>& ptr,
               std::size_t& augmenting_paths) const;
};

std::unique_ptr<MaxFlowSolver> create_solver(SolverType type);

} // namespace seam_carving

#endif // GRAPH_CUT_MAXFLOW_HPP

#include "maxflow.hpp"

#include <algorithm>
#include <chrono>
#include <queue>

namespace seam_carving {

std::vector<bool> MaxFlowSolver::compute_source_cut(const FlowGraph& graph, int source) {
    int n = graph.num_vertices();
    std::vector<bool> visited(static_cast<std::size_t>(n), false);
    if (source < 0 || source >= n) return visited;

    std::queue<int> q;
    q.push(source);
    visited[source] = true;

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (const Edge& e : graph.adj(u)) {
            if (!visited[e.to] && e.residual() > FLOW_EPS) {
                visited[e.to] = true;
                q.push(e.to);
            }
        }
    }
    return visited;
}

MaxFlowStats EdmondsKarpSolver::solve(FlowGraph& graph, int source, int sink) {
    auto start_time = std::chrono::high_resolution_clock::now();

    MaxFlowStats stats;
    int n = graph.num_vertices();
    std::vector<int> parent_v(static_cast<std::size_t>(n), -1);
    std::vector<int> parent_e(static_cast<std::size_t>(n), -1);
    std::vector<int> q(static_cast<std::size_t>(n));

    while (true) {
        std::fill(parent_v.begin(), parent_v.end(), -1);
        int head = 0;
        int tail = 0;
        q[tail++] = source;
        parent_v[source] = source;
        ++stats.bfs_runs;

        bool found = false;
        while (head < tail && !found) {
            int u = q[head++];
            const auto& edges = graph.adj(u);
            for (int i = 0; i < static_cast<int>(edges.size()); ++i) {
                const Edge& e = edges[i];
                if (parent_v[e.to] == -1 && e.residual() > FLOW_EPS) {
                    parent_v[e.to] = u;
                    parent_e[e.to] = i;
                    q[tail++] = e.to;
                    if (e.to == sink) {
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found) break;

        double pushed = INF_CAP;
        for (int v = sink; v != source; v = parent_v[v]) {
            int u = parent_v[v];
            int idx = parent_e[v];
            pushed = std::min(pushed, graph.adj(u)[idx].residual());
        }

        for (int v = sink; v != source; v = parent_v[v]) {
            int u = parent_v[v];
            int idx = parent_e[v];
            Edge& fwd = graph.adj(u)[idx];
            Edge& bwd = graph.adj(v)[fwd.rev];
            fwd.flow += pushed;
            bwd.flow -= pushed;
        }

        stats.max_flow += pushed;
        ++stats.augmenting_paths;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return stats;
}

bool DinicSolver::bfs(const FlowGraph& graph, int source, int sink, std::vector<int>& level) const {
    std::fill(level.begin(), level.end(), -1);
    level[source] = 0;
    std::queue<int> q;
    q.push(source);

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        if (u == sink) continue;
        for (const Edge& e : graph.adj(u)) {
            if (level[e.to] == -1 && e.residual() > FLOW_EPS) {
                level[e.to] = level[u] + 1;
                q.push(e.to);
            }
        }
    }
    return level[sink] != -1;
}

double DinicSolver::dfs(FlowGraph& graph, int u, int sink, double pushed,
                        std::vector<int>& level, std::vector<std::size_t>& ptr,
                        std::size_t& augmenting_paths) const {
    if (pushed <= FLOW_EPS) return 0.0;
    if (u == sink) {
        ++augmenting_paths;
        return pushed;
    }

    auto& edges = graph.adj(u);
    for (std::size_t& cid = ptr[u]; cid < edges.size(); ++cid) {
        Edge& e = edges[cid];
        if (level[e.to] != level[u] + 1 || e.residual() <= FLOW_EPS) {
            continue;
        }
        double tr = dfs(graph, e.to, sink, std::min(pushed, e.residual()), level, ptr, augmenting_paths);
        if (tr > FLOW_EPS) {
            e.flow += tr;
            graph.adj(e.to)[e.rev].flow -= tr;
            return tr;
        }
    }
    return 0.0;
}

MaxFlowStats DinicSolver::solve(FlowGraph& graph, int source, int sink) {
    auto start_time = std::chrono::high_resolution_clock::now();

    MaxFlowStats stats;
    int n = graph.num_vertices();
    std::vector<int> level(static_cast<std::size_t>(n));
    std::vector<std::size_t> ptr(static_cast<std::size_t>(n));

    while (bfs(graph, source, sink, level)) {
        ++stats.bfs_runs;
        std::fill(ptr.begin(), ptr.end(), 0);
        while (true) {
            double pushed = dfs(graph, source, sink, INF_CAP, level, ptr, stats.augmenting_paths);
            if (pushed <= FLOW_EPS) break;
            stats.max_flow += pushed;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return stats;
}

std::unique_ptr<MaxFlowSolver> create_solver(SolverType type) {
    switch (type) {
        case SolverType::Dinic:
            return std::make_unique<DinicSolver>();
        case SolverType::EdmondsKarp:
            return std::make_unique<EdmondsKarpSolver>();
    }
    return std::make_unique<DinicSolver>();
}

} // namespace seam_carving

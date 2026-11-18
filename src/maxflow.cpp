#include "maxflow.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <queue>

namespace seam_carving {

std::vector<bool> MaxFlowSolver::compute_source_cut(const FlowGraph& graph, int source) {
    int n = graph.num_vertices();
    std::vector<bool> visited(static_cast<std::size_t>(n), false);
    if (source < 0 || source >= n) return visited;

    std::vector<int> q(static_cast<std::size_t>(n));
    int head = 0;
    int tail = 0;
    q[tail++] = source;
    visited[source] = true;

    while (head < tail) {
        int u = q[head++];
        for (const Edge& e : graph.adj(u)) {
            if (!visited[e.to] && e.residual() > FLOW_EPS) {
                visited[e.to] = true;
                q[tail++] = e.to;
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

    int* pv = parent_v.data();
    int* pe = parent_e.data();
    int* q_arr = q.data();

    double delta = 512.0;

    while (delta > FLOW_EPS / 2.0) {
        while (true) {
            int head = 0;
            int tail = 0;
            q_arr[tail++] = source;
            pv[source] = source;
            ++stats.bfs_runs;

            std::size_t paths_before = stats.augmenting_paths;

            while (head < tail) {
                int u = q_arr[head++];
                auto& edges = graph.adj(u);
                Edge* edge_data = edges.data();
                int edge_count = static_cast<int>(edges.size());

                for (int i = 0; i < edge_count; ++i) {
                    Edge& e = edge_data[i];
                    if (e.residual() < delta) continue;

                    if (e.to == sink) {
                        pv[sink] = u;
                        pe[sink] = i;

                        double pushed = INF_CAP;
                        for (int v = sink; v != source; v = pv[v]) {
                            int pu = pv[v];
                            int pidx = pe[v];
                            pushed = std::min(pushed, graph.adj(pu)[pidx].residual());
                        }

                        if (pushed >= delta) {
                            for (int v = sink; v != source; v = pv[v]) {
                                int pu = pv[v];
                                int pidx = pe[v];
                                Edge& fwd = graph.adj(pu)[pidx];
                                Edge& bwd = graph.adj(v)[fwd.rev];
                                fwd.flow += pushed;
                                bwd.flow -= pushed;
                            }
                            stats.max_flow += pushed;
                            ++stats.augmenting_paths;
                        }
                        pv[sink] = -1;
                        continue;
                    }

                    if (pv[e.to] == -1) {
                        pv[e.to] = u;
                        pe[e.to] = i;
                        q_arr[tail++] = e.to;
                    }
                }
            }

            for (int i = 0; i < tail; ++i) {
                pv[q_arr[i]] = -1;
            }

            if (stats.augmenting_paths == paths_before) {
                break;
            }
        }

        if (delta > FLOW_EPS && delta / 2.0 <= FLOW_EPS) {
            delta = FLOW_EPS;
        } else if (delta <= FLOW_EPS) {
            break;
        } else {
            delta /= 2.0;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return stats;
}

bool DinicSolver::bfs(const FlowGraph& graph, int source, int sink, std::vector<int>& level) const {
    int n = graph.num_vertices();
    std::fill(level.begin(), level.end(), -1);
    level[source] = 0;
    static thread_local std::vector<int> q;
    if (static_cast<int>(q.size()) < n) {
        q.resize(static_cast<std::size_t>(n));
    }
    int head = 0;
    int tail = 0;
    q[tail++] = source;

    while (head < tail) {
        int u = q[head++];
        if (level[sink] != -1 && level[u] >= level[sink]) {
            break;
        }
        for (const Edge& e : graph.adj(u)) {
            if (level[e.to] == -1 && e.residual() > FLOW_EPS) {
                level[e.to] = level[u] + 1;
                q[tail++] = e.to;
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

    double tr_total = 0.0;
    auto& edges = graph.adj(u);
    for (std::size_t& cid = ptr[u]; cid < edges.size(); ++cid) {
        Edge& e = edges[cid];
        if (level[e.to] != level[u] + 1 || e.residual() <= FLOW_EPS) {
            continue;
        }
        double tr = dfs(graph, e.to, sink, std::min(pushed - tr_total, e.residual()),
                        level, ptr, augmenting_paths);
        if (tr > FLOW_EPS) {
            e.flow += tr;
            graph.adj(e.to)[e.rev].flow -= tr;
            tr_total += tr;
            if (pushed - tr_total <= FLOW_EPS) {
                break;
            }
        }
    }
    if (tr_total < pushed) {
        level[u] = -1;
    }
    return tr_total;
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

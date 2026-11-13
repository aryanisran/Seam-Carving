#ifndef GRAPH_CUT_BENCHMARK_HPP
#define GRAPH_CUT_BENCHMARK_HPP

#include "energy.hpp"
#include "image.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace seam_carving {

struct ComparisonRow {
    int seam_index = 0;
    bool is_vertical = true;
    int width = 0;
    int height = 0;
    double dinic_energy = 0.0;
    double ek_energy = 0.0;
    bool energy_match = false;
    double dinic_ms = 0.0;
    double ek_ms = 0.0;
    std::size_t dinic_augmentations = 0;
    std::size_t ek_augmentations = 0;
    std::size_t dinic_bfs = 0;
    std::size_t ek_bfs = 0;
    double speedup = 0.0;
};

struct ComparisonSummary {
    std::vector<ComparisonRow> rows;
    double total_dinic_ms = 0.0;
    double total_ek_ms = 0.0;
    double overall_speedup = 0.0;
    bool all_energies_matched = true;
};

class BenchmarkRunner {
public:
    // Runs both Dinic and Edmonds-Karp on the exact same sequence of image states
    // (carving with Dinic's seam at each step so both see identical graphs).
    static ComparisonSummary compare_solvers(
        Image img,
        int remove_cols,
        int remove_rows,
        EnergyMetric metric = EnergyMetric::DualGradient);

    static void print_table(const ComparisonSummary& summary);
    static bool export_csv(const std::string& path, const ComparisonSummary& summary);
};

} // namespace seam_carving

#endif // GRAPH_CUT_BENCHMARK_HPP

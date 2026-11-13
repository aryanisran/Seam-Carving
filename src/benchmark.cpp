#include "benchmark.hpp"
#include "graph_cut_seam.hpp"
#include "maxflow.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace seam_carving {

ComparisonSummary BenchmarkRunner::compare_solvers(
    Image img,
    int remove_cols,
    int remove_rows,
    EnergyMetric metric) {
    ComparisonSummary summary;
    DinicSolver dinic;
    EdmondsKarpSolver ek;

    int seam_idx = 0;

    for (int i = 0; i < remove_cols; ++i) {
        if (img.width() <= 1) break;
        ++seam_idx;

        EnergyMap energy = compute_energy(img, metric);
        SeamResult res_d = GraphCutSeamFinder::find_vertical_seam(energy, dinic);
        SeamResult res_ek = GraphCutSeamFinder::find_vertical_seam(energy, ek);

        ComparisonRow row;
        row.seam_index = seam_idx;
        row.is_vertical = true;
        row.width = img.width();
        row.height = img.height();
        row.dinic_energy = res_d.energy_sum;
        row.ek_energy = res_ek.energy_sum;
        row.energy_match = (std::abs(res_d.energy_sum - res_ek.energy_sum) < 1e-4) && res_d.valid && res_ek.valid;
        row.dinic_ms = res_d.stats.elapsed_ms;
        row.ek_ms = res_ek.stats.elapsed_ms;
        row.dinic_augmentations = res_d.stats.augmenting_paths;
        row.ek_augmentations = res_ek.stats.augmenting_paths;
        row.dinic_bfs = res_d.stats.bfs_runs;
        row.ek_bfs = res_ek.stats.bfs_runs;
        row.speedup = row.ek_ms / std::max(row.dinic_ms, 1e-6);

        summary.total_dinic_ms += row.dinic_ms;
        summary.total_ek_ms += row.ek_ms;
        if (!row.energy_match) summary.all_energies_matched = false;
        summary.rows.push_back(row);

        img.remove_vertical_seam(res_d.seam);
    }

    for (int i = 0; i < remove_rows; ++i) {
        if (img.height() <= 1) break;
        ++seam_idx;

        EnergyMap energy = compute_energy(img, metric);
        SeamResult res_d = GraphCutSeamFinder::find_horizontal_seam(energy, dinic);
        SeamResult res_ek = GraphCutSeamFinder::find_horizontal_seam(energy, ek);

        ComparisonRow row;
        row.seam_index = seam_idx;
        row.is_vertical = false;
        row.width = img.width();
        row.height = img.height();
        row.dinic_energy = res_d.energy_sum;
        row.ek_energy = res_ek.energy_sum;
        row.energy_match = (std::abs(res_d.energy_sum - res_ek.energy_sum) < 1e-4) && res_d.valid && res_ek.valid;
        row.dinic_ms = res_d.stats.elapsed_ms;
        row.ek_ms = res_ek.stats.elapsed_ms;
        row.dinic_augmentations = res_d.stats.augmenting_paths;
        row.ek_augmentations = res_ek.stats.augmenting_paths;
        row.dinic_bfs = res_d.stats.bfs_runs;
        row.ek_bfs = res_ek.stats.bfs_runs;
        row.speedup = row.ek_ms / std::max(row.dinic_ms, 1e-6);

        summary.total_dinic_ms += row.dinic_ms;
        summary.total_ek_ms += row.ek_ms;
        if (!row.energy_match) summary.all_energies_matched = false;
        summary.rows.push_back(row);

        img.remove_horizontal_seam(res_d.seam);
    }

    summary.overall_speedup = summary.total_ek_ms / std::max(summary.total_dinic_ms, 1e-6);
    return summary;
}

void BenchmarkRunner::print_table(const ComparisonSummary& summary) {
    std::cout << "\n=== Graph Cut Solver Comparison (Dinic vs. Edmonds-Karp) ===\n";
    std::cout << std::left
              << std::setw(6) << "Seam"
              << std::setw(6) << "Dir"
              << std::setw(11) << "Size"
              << std::setw(12) << "Dinic E"
              << std::setw(12) << "EK E"
              << std::setw(7) << "Match"
              << std::setw(12) << "Dinic(ms)"
              << std::setw(12) << "EK(ms)"
              << std::setw(10) << "DinicBFS"
              << std::setw(10) << "EK_BFS"
              << std::setw(9) << "Speedup"
              << "\n";
    std::cout << std::string(107, '-') << "\n";

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : summary.rows) {
        std::string sz = std::to_string(r.width) + "x" + std::to_string(r.height);
        std::cout << std::left
                  << std::setw(6) << r.seam_index
                  << std::setw(6) << (r.is_vertical ? "V" : "H")
                  << std::setw(11) << sz
                  << std::setw(12) << r.dinic_energy
                  << std::setw(12) << r.ek_energy
                  << std::setw(7) << (r.energy_match ? "YES" : "NO")
                  << std::setw(12) << r.dinic_ms
                  << std::setw(12) << r.ek_ms
                  << std::setw(10) << r.dinic_bfs
                  << std::setw(10) << r.ek_bfs
                  << std::setw(8) << r.speedup << "x"
                  << "\n";
    }

    std::cout << std::string(107, '-') << "\n";
    std::cout << "Total Dinic Time:        " << summary.total_dinic_ms << " ms\n";
    std::cout << "Total Edmonds-Karp Time: " << summary.total_ek_ms << " ms\n";
    std::cout << "Overall Speedup:         " << summary.overall_speedup << "x\n";
    std::cout << "All Energies Matched:    " << (summary.all_energies_matched ? "YES" : "NO") << "\n\n";
}

bool BenchmarkRunner::export_csv(const std::string& path, const ComparisonSummary& summary) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "seam_index,orientation,width,height,dinic_energy,ek_energy,energy_match,"
           "dinic_ms,ek_ms,dinic_augmentations,ek_augmentations,dinic_bfs,ek_bfs,speedup\n";
    for (const auto& r : summary.rows) {
        out << r.seam_index << ","
            << (r.is_vertical ? "vertical" : "horizontal") << ","
            << r.width << ","
            << r.height << ","
            << r.dinic_energy << ","
            << r.ek_energy << ","
            << (r.energy_match ? "1" : "0") << ","
            << r.dinic_ms << ","
            << r.ek_ms << ","
            << r.dinic_augmentations << ","
            << r.ek_augmentations << ","
            << r.dinic_bfs << ","
            << r.ek_bfs << ","
            << r.speedup << "\n";
    }
    return true;
}

} // namespace seam_carving

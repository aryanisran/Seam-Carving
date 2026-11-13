#ifndef GRAPH_CUT_SEAM_CARVER_HPP
#define GRAPH_CUT_SEAM_CARVER_HPP

#include "energy.hpp"
#include "graph_cut_seam.hpp"
#include "image.hpp"
#include "maxflow.hpp"
#include <string>
#include <vector>

namespace seam_carving {

struct CarveRecord {
    int step = 0;
    bool is_vertical = true;
    int width_before = 0;
    int height_before = 0;
    std::vector<int> seam;
    double seam_energy = 0.0;
    MaxFlowStats stats;
};

class SeamCarver {
public:
    SeamCarver(SolverType solver_type = SolverType::Dinic,
               EnergyMetric metric = EnergyMetric::DualGradient);

    void set_solver(SolverType solver_type) { solver_type_ = solver_type; }
    void set_energy_metric(EnergyMetric metric) { metric_ = metric; }

    void carve_vertical_seams(Image& img, int count, std::vector<CarveRecord>* records = nullptr);
    void carve_horizontal_seams(Image& img, int count, std::vector<CarveRecord>* records = nullptr);
    void resize(Image& img, int target_width, int target_height, std::vector<CarveRecord>* records = nullptr);

    // Exports seam coordinates and telemetry to JSON for teammate visualization integration
    static bool export_seams_json(const std::string& path, const std::vector<CarveRecord>& records);

private:
    SolverType solver_type_;
    EnergyMetric metric_;
};

} // namespace seam_carving

#endif // GRAPH_CUT_SEAM_CARVER_HPP

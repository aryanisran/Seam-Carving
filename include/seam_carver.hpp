#ifndef GRAPH_CUT_SEAM_CARVER_HPP
#define GRAPH_CUT_SEAM_CARVER_HPP

#include "dp_seam.hpp"
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

// Which seam-finding algorithm SeamCarver should use. GraphCut is the default so all
// existing call sites (Team B's code, the benchmark harness) keep behaving exactly as
// before without passing this argument.
enum class AlgorithmType {
    GraphCut,
    DynamicProgramming,
    Greedy // reserved for Team C's GreedySeamFinder; selecting this currently throws.
};

class SeamCarver {
public:
    SeamCarver(SolverType solver_type = SolverType::Dinic,
               EnergyMetric metric = EnergyMetric::DualGradient,
               AlgorithmType algorithm = AlgorithmType::GraphCut);

    void set_solver(SolverType solver_type) { solver_type_ = solver_type; }
    void set_energy_metric(EnergyMetric metric) { metric_ = metric; }
    void set_algorithm(AlgorithmType algorithm) { algorithm_ = algorithm; }
    AlgorithmType algorithm() const { return algorithm_; }

    void carve_vertical_seams(Image& img, int count, std::vector<CarveRecord>* records = nullptr);
    void carve_horizontal_seams(Image& img, int count, std::vector<CarveRecord>* records = nullptr);
    void resize(Image& img, int target_width, int target_height, std::vector<CarveRecord>* records = nullptr);

    // Exports seam coordinates and telemetry to JSON for teammate visualization integration
    static bool export_seams_json(const std::string& path, const std::vector<CarveRecord>& records);

private:
    SolverType solver_type_;
    EnergyMetric metric_;
    AlgorithmType algorithm_;
};

} // namespace seam_carving

#endif // GRAPH_CUT_SEAM_CARVER_HPP

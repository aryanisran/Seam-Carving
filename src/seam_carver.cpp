#include "seam_carver.hpp"

#include <fstream>
#include <memory>
#include <stdexcept>

namespace seam_carving {

SeamCarver::SeamCarver(SolverType solver_type, EnergyMetric metric, AlgorithmType algorithm)
    : solver_type_(solver_type), metric_(metric), algorithm_(algorithm) {}

void SeamCarver::carve_vertical_seams(Image& img, int count, std::vector<CarveRecord>* records) {
    if (count < 0) {
        throw std::invalid_argument("Seam count cannot be negative");
    }
    if (count >= img.width()) {
        throw std::invalid_argument("Cannot remove >= image width vertical seams");
    }

    // Graph Cut needs a max-flow solver instance; DP and Greedy do not.
    std::unique_ptr<MaxFlowSolver> solver;
    if (algorithm_ == AlgorithmType::GraphCut) {
        solver = create_solver(solver_type_);
    }

    int start_step = records ? static_cast<int>(records->size()) + 1 : 1;

    for (int i = 0; i < count; ++i) {
        EnergyMap energy = compute_energy(img, metric_);

        SeamResult res;
        switch (algorithm_) {
            case AlgorithmType::DynamicProgramming:
                res = DPSeamFinder::find_vertical_seam(energy);
                break;
            case AlgorithmType::Greedy:
                throw std::runtime_error("GreedySeamFinder is not implemented yet (Team C)");
            case AlgorithmType::GraphCut:
            default:
                res = GraphCutSeamFinder::find_vertical_seam(energy, *solver);
                break;
        }

        if (!res.valid) {
            throw std::runtime_error("Seam finder produced an invalid vertical seam");
        }

        if (records) {
            CarveRecord rec;
            rec.step = start_step + i;
            rec.is_vertical = true;
            rec.width_before = img.width();
            rec.height_before = img.height();
            rec.seam = res.seam;
            rec.seam_energy = res.energy_sum;
            rec.stats = res.stats;
            records->push_back(std::move(rec));
        }

        img.remove_vertical_seam(res.seam);
    }
}

void SeamCarver::carve_horizontal_seams(Image& img, int count, std::vector<CarveRecord>* records) {
    if (count < 0) {
        throw std::invalid_argument("Seam count cannot be negative");
    }
    if (count >= img.height()) {
        throw std::invalid_argument("Cannot remove >= image height horizontal seams");
    }

    std::unique_ptr<MaxFlowSolver> solver;
    if (algorithm_ == AlgorithmType::GraphCut) {
        solver = create_solver(solver_type_);
    }

    int start_step = records ? static_cast<int>(records->size()) + 1 : 1;

    for (int i = 0; i < count; ++i) {
        EnergyMap energy = compute_energy(img, metric_);

        SeamResult res;
        switch (algorithm_) {
            case AlgorithmType::DynamicProgramming:
                res = DPSeamFinder::find_horizontal_seam(energy);
                break;
            case AlgorithmType::Greedy:
                throw std::runtime_error("GreedySeamFinder is not implemented yet (Team C)");
            case AlgorithmType::GraphCut:
            default:
                res = GraphCutSeamFinder::find_horizontal_seam(energy, *solver);
                break;
        }

        if (!res.valid) {
            throw std::runtime_error("Seam finder produced an invalid horizontal seam");
        }

        if (records) {
            CarveRecord rec;
            rec.step = start_step + i;
            rec.is_vertical = false;
            rec.width_before = img.width();
            rec.height_before = img.height();
            rec.seam = res.seam;
            rec.seam_energy = res.energy_sum;
            rec.stats = res.stats;
            records->push_back(std::move(rec));
        }

        img.remove_horizontal_seam(res.seam);
    }
}

void SeamCarver::resize(Image& img, int target_width, int target_height, std::vector<CarveRecord>* records) {
    if (target_width <= 0 || target_height <= 0) {
        throw std::invalid_argument("Target dimensions must be positive");
    }
    if (target_width > img.width() || target_height > img.height()) {
        throw std::invalid_argument("Target dimensions cannot exceed current image dimensions");
    }

    int remove_cols = img.width() - target_width;
    int remove_rows = img.height() - target_height;

    if (remove_cols > 0) {
        carve_vertical_seams(img, remove_cols, records);
    }
    if (remove_rows > 0) {
        carve_horizontal_seams(img, remove_rows, records);
    }
}

bool SeamCarver::export_seams_json(const std::string& path, const std::vector<CarveRecord>& records) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "{\n  \"seams\": [\n";
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        out << "    {\n";
        out << "      \"step\": " << r.step << ",\n";
        out << "      \"orientation\": \"" << (r.is_vertical ? "vertical" : "horizontal") << "\",\n";
        out << "      \"width_before\": " << r.width_before << ",\n";
        out << "      \"height_before\": " << r.height_before << ",\n";
        out << "      \"energy_sum\": " << r.seam_energy << ",\n";
        out << "      \"max_flow\": " << r.stats.max_flow << ",\n";
        out << "      \"elapsed_ms\": " << r.stats.elapsed_ms << ",\n";
        out << "      \"augmenting_paths\": " << r.stats.augmenting_paths << ",\n";
        out << "      \"bfs_runs\": " << r.stats.bfs_runs << ",\n";
        out << "      \"indices\": [";
        for (std::size_t j = 0; j < r.seam.size(); ++j) {
            out << r.seam[j];
            if (j + 1 < r.seam.size()) out << ", ";
        }
        out << "]\n";
        out << "    }";
        if (i + 1 < records.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return true;
}

} // namespace seam_carving

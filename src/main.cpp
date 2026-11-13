#include "benchmark.hpp"
#include "energy.hpp"
#include "image.hpp"
#include "maxflow.hpp"
#include "seam_carver.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace seam_carving;

namespace {

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " -i <input> [options]\n\n"
              << "Options:\n"
              << "  -i, --input <path>        Input image file (JPG, PNG, BMP)\n"
              << "  -o, --output <path>       Output image file\n"
              << "  --remove-cols <n>         Number of vertical seams to remove (default: 0)\n"
              << "  --remove-rows <n>         Number of horizontal seams to remove (default: 0)\n"
              << "  --width <w>               Target width (overrides --remove-cols)\n"
              << "  --height <h>              Target height (overrides --remove-rows)\n"
              << "  --solver <name>           Max-flow solver: 'dinic' (default) or 'edmonds_karp'\n"
              << "  --energy <name>           Energy metric: 'dual' (default) or 'sobel'\n"
              << "  --compare                 Run both Dinic and Edmonds-Karp and compare\n"
              << "  --export-csv <path>       Export --compare metrics to CSV\n"
              << "  --export-seams <path>     Export carved seam coordinates to JSON\n"
              << "  -h, --help                Show this help message\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string input_path;
    std::string output_path;
    std::string csv_path;
    std::string seams_json_path;
    int remove_cols = 0;
    int remove_rows = 0;
    int target_width = -1;
    int target_height = -1;
    SolverType solver_type = SolverType::Dinic;
    EnergyMetric metric = EnergyMetric::DualGradient;
    bool compare_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            input_path = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--remove-cols" && i + 1 < argc) {
            remove_cols = std::atoi(argv[++i]);
        } else if (arg == "--remove-rows" && i + 1 < argc) {
            remove_rows = std::atoi(argv[++i]);
        } else if (arg == "--width" && i + 1 < argc) {
            target_width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            target_height = std::atoi(argv[++i]);
        } else if (arg == "--solver" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "dinic") {
                solver_type = SolverType::Dinic;
            } else if (s == "edmonds_karp" || s == "ek") {
                solver_type = SolverType::EdmondsKarp;
            } else {
                std::cerr << "Unknown solver: " << s << "\n";
                return 1;
            }
        } else if (arg == "--energy" && i + 1 < argc) {
            std::string e = argv[++i];
            if (e == "dual") {
                metric = EnergyMetric::DualGradient;
            } else if (e == "sobel") {
                metric = EnergyMetric::Sobel;
            } else {
                std::cerr << "Unknown energy metric: " << e << "\n";
                return 1;
            }
        } else if (arg == "--compare") {
            compare_mode = true;
        } else if (arg == "--export-csv" && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (arg == "--export-seams" && i + 1 < argc) {
            seams_json_path = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_path.empty()) {
        std::cerr << "Error: --input is required.\n";
        print_usage(argv[0]);
        return 1;
    }

    Image img;
    if (!img.load_from_file(input_path)) {
        std::cerr << "Error: Failed to load image '" << input_path << "'.\n";
        return 1;
    }

    std::cout << "Loaded image '" << input_path << "' (" << img.width() << "x" << img.height() << ")\n";

    if (target_width > 0) {
        remove_cols = img.width() - target_width;
    }
    if (target_height > 0) {
        remove_rows = img.height() - target_height;
    }

    if (remove_cols < 0 || remove_rows < 0) {
        std::cerr << "Error: Cannot add seams (remove_cols/remove_rows must be >= 0).\n";
        return 1;
    }
    if (remove_cols >= img.width() || remove_rows >= img.height()) {
        std::cerr << "Error: Cannot remove all columns or rows.\n";
        return 1;
    }

    if (compare_mode) {
        ComparisonSummary summary = BenchmarkRunner::compare_solvers(img, remove_cols, remove_rows, metric);
        BenchmarkRunner::print_table(summary);
        if (!csv_path.empty()) {
            if (BenchmarkRunner::export_csv(csv_path, summary)) {
                std::cout << "Exported comparison CSV to '" << csv_path << "'.\n";
            } else {
                std::cerr << "Failed to export CSV to '" << csv_path << "'.\n";
            }
        }
    }

    if (!output_path.empty() || !seams_json_path.empty()) {
        SeamCarver carver(solver_type, metric);
        std::vector<CarveRecord> records;

        auto t0 = std::chrono::high_resolution_clock::now();
        carver.carve_vertical_seams(img, remove_cols, &records);
        carver.carve_horizontal_seams(img, remove_rows, &records);
        auto t1 = std::chrono::high_resolution_clock::now();

        double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "Carved " << remove_cols << " vertical and " << remove_rows
                  << " horizontal seams using "
                  << (solver_type == SolverType::Dinic ? "Dinic" : "Edmonds-Karp")
                  << " in " << total_ms << " ms. New size: "
                  << img.width() << "x" << img.height() << "\n";

        if (!output_path.empty()) {
            if (img.save_to_file(output_path)) {
                std::cout << "Saved output image to '" << output_path << "'.\n";
            } else {
                std::cerr << "Failed to save output image to '" << output_path << "'.\n";
                return 1;
            }
        }

        if (!seams_json_path.empty()) {
            if (SeamCarver::export_seams_json(seams_json_path, records)) {
                std::cout << "Exported seam records to '" << seams_json_path << "'.\n";
            } else {
                std::cerr << "Failed to export seam records to '" << seams_json_path << "'.\n";
            }
        }
    }

    return 0;
}

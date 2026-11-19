#include "dp_seam.hpp"
#include "energy.hpp"
#include "graph.hpp"
#include "graph_cut_seam.hpp"
#include "greedy_seam.hpp"
#include "image.hpp"
#include "maxflow.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace seam_carving;

namespace {

// Runs fn reps times and returns the median wall-clock time in milliseconds.
double time_ms(const std::function<void()>& fn, int reps = 1) {
    std::vector<double> v;
    for (int i = 0; i < reps; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        fn();
        auto t1 = std::chrono::steady_clock::now();
        v.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// Builds a procedural "tower" scene (sky, cloud, tower, grass) at any size, so every image size
// in the scaling benchmark has the same content.
Image make_scene(int w, int h) {
    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0.0, 3.0);
    Image img(w, h);
    const int horizon = h * 3 / 4, tx0 = w * 62 / 100, tx1 = w * 80 / 100, ttop = h * 22 / 100;
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            double t = double(r) / h, R = 40 + 60 * t, G = 110 + 70 * t, B = 200 + 40 * t;
            double dx = (c - 0.2 * w) / (0.13 * w), dy = (r - 0.17 * h) / (0.055 * h);
            double a = std::clamp(1.2 - (dx * dx + dy * dy), 0.0, 1.0); // cloud opacity
            R = R * (1 - a) + 245 * a; G = G * (1 - a) + 245 * a; B = B * (1 - a) + 245 * a;
            if (r >= horizon) { R = 70; G = 140; B = 50; }
            if (c >= tx0 && c < tx1 && r >= ttop && r < horizon) { R = 190; G = 160; B = 110; }
            auto clip = [](double v) { return static_cast<uint8_t>(std::clamp(v, 0.0, 255.0)); };
            img.set_pixel(r, c, Pixel{clip(R + noise(rng)), clip(G + noise(rng)), clip(B + noise(rng))});
        }
    }
    return img;
}

// One seam finder under test. memory_bytes estimates the working memory it needs for one
// vertical seam on an H x W energy map (the energy map itself is shared and not counted).
struct Algorithm {
    std::string name;
    std::function<SeamResult(const EnergyMap&, bool vertical)> find;
    std::function<std::size_t(int h, int w)> memory_bytes;
};

std::vector<Algorithm> algorithms() {
    static DinicSolver dinic;
    static EdmondsKarpSolver edmonds_karp;
    // Same topology as GraphCutSeamFinder::build_vertical_graph: source/sink edges, one paired
    // forward/backward edge per horizontal neighbour, two diagonal edges per pixel. Every
    // add_edge stores an Edge plus its residual twin.
    auto graph_bytes = [](int h, int w) {
        std::size_t hs = static_cast<std::size_t>(h), ws = static_cast<std::size_t>(w);
        std::size_t edges = 2 * hs + hs * (ws - 1) + 2 * (hs - 1) * (ws - 1);
        return 2 * edges * sizeof(Edge) + (hs * ws + 2) * sizeof(std::vector<Edge>);
    };
    return {
        {"Greedy",
         [](const EnergyMap& e, bool v) {
             return v ? GreedySeamFinder::find_vertical_seam(e) : GreedySeamFinder::find_horizontal_seam(e);
         },
         [](int h, int) { return static_cast<std::size_t>(h) * sizeof(int); }},
        {"DP",
         [](const EnergyMap& e, bool v) {
             return v ? DPSeamFinder::find_vertical_seam(e) : DPSeamFinder::find_horizontal_seam(e);
         },
         // Cost table (double) + predecessor table (int), both H x W.
         [](int h, int w) { return static_cast<std::size_t>(h) * w * (sizeof(double) + sizeof(int)); }},
        {"GraphCut-Dinic",
         [](const EnergyMap& e, bool v) {
             return v ? GraphCutSeamFinder::find_vertical_seam(e, dinic)
                      : GraphCutSeamFinder::find_horizontal_seam(e, dinic);
         },
         graph_bytes},
        {"GraphCut-EK",
         [](const EnergyMap& e, bool v) {
             return v ? GraphCutSeamFinder::find_vertical_seam(e, edmonds_karp)
                      : GraphCutSeamFinder::find_horizontal_seam(e, edmonds_karp);
         },
         graph_bytes},
    };
}

// Times one vertical seam per algorithm on the identical energy map at seven image sizes, and
// records seam energy (to show which algorithms are optimal) and estimated working memory.
// An algorithm is skipped at larger sizes once one seam takes longer than budget_ms.
void scaling(const std::string& out, double budget_ms) {
    std::ofstream csv(out + "/scaling.csv");
    csv << "width,height,algorithm,ms_per_seam,seam_energy,energy_vs_dp,memory_bytes\n";
    std::cout << "== Scaling: time to find one vertical seam on the same energy map ==\n"
              << std::left << std::setw(11) << "size" << std::setw(16) << "algorithm" << std::right
              << std::setw(13) << "ms/seam" << std::setw(14) << "seam energy" << std::setw(10) << "vs DP"
              << std::setw(13) << "memory KB" << "\n";
    std::vector<Algorithm> algos = algorithms();
    std::vector<bool> skipped(algos.size(), false);
    for (int w : {40, 80, 160, 320, 640, 1280, 1920}) {
        int h = w * 3 / 4;
        Image img = make_scene(w, h);
        EnergyMap e;
        double energy_ms = time_ms([&] { e = compute_energy(img); }, 5);
        double dp_energy = DPSeamFinder::find_vertical_seam(e).energy_sum;
        std::string size = std::to_string(w) + "x" + std::to_string(h);
        csv << w << "," << h << ",EnergyMap," << energy_ms << ",,," << static_cast<std::size_t>(w) * h * sizeof(double)
            << "\n";
        std::cout << std::left << std::setw(11) << size << std::setw(16) << "(energy map)" << std::right
                  << std::fixed << std::setprecision(3) << std::setw(13) << energy_ms << "\n";
        for (std::size_t i = 0; i < algos.size(); ++i) {
            if (skipped[i]) continue;
            SeamResult s;
            // Max-flow gets slow quickly: one run is enough once a seam takes over 200 ms.
            double ms = time_ms([&] { s = algos[i].find(e, true); });
            if (ms < 200.0) ms = time_ms([&] { s = algos[i].find(e, true); }, 5);
            if (!s.valid) { std::cerr << algos[i].name << " produced an invalid seam\n"; std::exit(1); }
            double ratio = s.energy_sum / dp_energy;
            std::size_t mem = algos[i].memory_bytes(h, w);
            csv << w << "," << h << "," << algos[i].name << "," << ms << "," << s.energy_sum << "," << ratio << ","
                << mem << "\n";
            std::cout << std::left << std::setw(11) << size << std::setw(16) << algos[i].name << std::right
                      << std::setprecision(3) << std::setw(13) << ms << std::setprecision(1) << std::setw(14)
                      << s.energy_sum << std::setprecision(3) << std::setw(10) << ratio << std::setprecision(1)
                      << std::setw(13) << mem / 1024.0 << "\n";
            if (ms > budget_ms) {
                skipped[i] = true;
                std::cout << "   (" << algos[i].name << " took over " << budget_ms
                          << " ms per seam: skipped at larger sizes)\n";
            }
        }
    }
}

// Carves the same image with every algorithm (energy map recomputed before each seam, as in
// SeamCarver), records total energy removed and time split into seam search and energy maps,
// and saves the carved results side by side in the order Greedy | DP | Graph Cut (Dinic).
void full_carve(const Image& original, const std::string& label, int cols, int rows, const std::string& out) {
    std::cout << "\n== Full carve: " << label << " " << original.width() << "x" << original.height() << " -> "
              << original.width() - cols << "x" << original.height() - rows << " (" << cols << " vertical + "
              << rows << " horizontal seams) ==\n";
    std::ofstream csv(out + "/full_carve_" + label + ".csv");
    csv << "algorithm,seams,total_energy_removed,seam_search_ms,energy_map_ms,total_ms\n";
    std::vector<Image> carved;
    for (const Algorithm& a : algorithms()) {
        Image img = original;
        double total_energy = 0, seam_ms = 0, energy_ms = 0;
        for (int k = 0; k < cols + rows; ++k) {
            bool vertical = k < cols;
            EnergyMap e;
            energy_ms += time_ms([&] { e = compute_energy(img); });
            SeamResult s;
            seam_ms += time_ms([&] { s = a.find(e, vertical); });
            total_energy += s.energy_sum;
            if (vertical) img.remove_vertical_seam(s.seam);
            else img.remove_horizontal_seam(s.seam);
        }
        csv << a.name << "," << cols + rows << "," << total_energy << "," << seam_ms << "," << energy_ms << ","
            << seam_ms + energy_ms << "\n";
        std::cout << std::left << std::setw(16) << a.name << std::right << "energy removed " << std::fixed
                  << std::setprecision(1) << std::setw(10) << total_energy << "   seam search " << std::setprecision(2)
                  << std::setw(10) << seam_ms << " ms   energy maps " << std::setw(7) << energy_ms
                  << " ms   total " << std::setw(10) << seam_ms + energy_ms << " ms\n";
        if (a.name != "GraphCut-EK") carved.push_back(img); // EK gives the same seams as Dinic
    }
    int gap = 8, width = -gap, height = 0;
    for (const Image& im : carved) { width += im.width() + gap; height = std::max(height, im.height()); }
    Image montage(width, height);
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c) montage.set_pixel(r, c, Pixel{255, 255, 255});
    int x = 0;
    for (const Image& im : carved) {
        for (int r = 0; r < im.height(); ++r)
            for (int c = 0; c < im.width(); ++c) montage.set_pixel(r, x + c, im.get_pixel(r, c));
        x += im.width() + gap;
    }
    montage.save_to_file(out + "/" + label + "_carved_greedy_dp_graphcut.png");
}

// The functions below are benchmark-only prototypes of the improvements discussed in Item 5d.
// They do not replace DPSeamFinder or compute_energy; each one is checked to give exactly the
// same result as the production code before it is timed.

// Dual-gradient energy of one pixel, identical to compute_energy's EnergyMetric::DualGradient.
double dual_gradient_at(const Image& img, int r, int c) {
    Pixel left = img.get_pixel(r, c - 1), right = img.get_pixel(r, c + 1);
    Pixel up = img.get_pixel(r - 1, c), down = img.get_pixel(r + 1, c);
    double rx = static_cast<double>(right.r) - left.r;
    double gx = static_cast<double>(right.g) - left.g;
    double bx = static_cast<double>(right.b) - left.b;
    double ry = static_cast<double>(down.r) - up.r;
    double gy = static_cast<double>(down.g) - up.g;
    double by = static_cast<double>(down.b) - up.b;
    double dx2 = rx * rx + gx * gx + bx * bx;
    double dy2 = ry * ry + gy * gy + by * by;
    return std::sqrt(dx2 + dy2);
}

// DP that keeps only two rolling rows of costs plus a 1-byte parent offset (-1, 0, +1) per
// pixel, instead of full H x W cost and predecessor tables. Same tie-breaking as DPSeamFinder.
std::vector<int> compact_dp_vertical_seam(const EnergyMap& e) {
    const int h = e.height(), w = e.width();
    std::vector<double> prev(static_cast<std::size_t>(w)), cur(static_cast<std::size_t>(w));
    std::vector<int8_t> parent(static_cast<std::size_t>(h) * w, 0);
    for (int c = 0; c < w; ++c) prev[c] = e.at(0, c);
    for (int r = 1; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            int best = c;
            if (c > 0 && prev[c - 1] < prev[best]) best = c - 1;
            if (c < w - 1 && prev[c + 1] < prev[best]) best = c + 1;
            cur[c] = e.at(r, c) + prev[best];
            parent[static_cast<std::size_t>(r) * w + c] = static_cast<int8_t>(best - c);
        }
        prev.swap(cur);
    }
    int c = 0;
    for (int k = 1; k < w; ++k)
        if (prev[k] < prev[c]) c = k;
    std::vector<int> seam(static_cast<std::size_t>(h));
    seam[h - 1] = c;
    for (int r = h - 1; r > 0; --r) {
        c += parent[static_cast<std::size_t>(r) * w + c];
        seam[r - 1] = c;
    }
    return seam;
}

// DP with the same full tables as DPSeamFinder, but the cells of each row split across threads.
// Row r needs row r-1, so threads must synchronise (an implicit OpenMP barrier) after every row.
std::vector<int> row_parallel_dp_vertical_seam(const EnergyMap& e, int threads) {
    const int h = e.height(), w = e.width();
    std::vector<double> cost(static_cast<std::size_t>(h) * w);
    std::vector<int> choice(static_cast<std::size_t>(h) * w, -1);
    for (int c = 0; c < w; ++c) cost[c] = e.at(0, c);
#pragma omp parallel num_threads(threads)
    for (int r = 1; r < h; ++r) {
        const double* prev = &cost[static_cast<std::size_t>(r - 1) * w];
#pragma omp for schedule(static)
        for (int c = 0; c < w; ++c) {
            int best = c;
            if (c > 0 && prev[c - 1] < prev[best]) best = c - 1;
            if (c < w - 1 && prev[c + 1] < prev[best]) best = c + 1;
            cost[static_cast<std::size_t>(r) * w + c] = e.at(r, c) + prev[best];
            choice[static_cast<std::size_t>(r) * w + c] = best;
        }
    }
    (void)threads;
    int c = 0;
    const std::size_t last = static_cast<std::size_t>(h - 1) * w;
    for (int k = 1; k < w; ++k)
        if (cost[last + k] < cost[last + c]) c = k;
    std::vector<int> seam(static_cast<std::size_t>(h));
    seam[h - 1] = c;
    for (int r = h - 2; r >= 0; --r) {
        c = choice[static_cast<std::size_t>(r + 1) * w + c];
        seam[r] = c;
    }
    return seam;
}

// Dual-gradient energy map with the rows split across threads. Every pixel is independent, so
// there is a single synchronisation point at the end.
EnergyMap parallel_energy(const Image& img, int threads) {
    const int h = img.height(), w = img.width();
    std::vector<double> data(static_cast<std::size_t>(h) * w);
#pragma omp parallel for num_threads(threads) schedule(static)
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) data[static_cast<std::size_t>(r) * w + c] = dual_gradient_at(img, r, c);
    (void)threads;
    return EnergyMap(w, h, std::move(data));
}

// After removing a vertical seam, only pixels next to the seam see different neighbours. This
// shifts the old energy map past the seam and recomputes just those pixels (at most 4 per row).
// `img` is the image after the seam was removed.
EnergyMap incremental_energy(const EnergyMap& old_energy, const Image& img, const std::vector<int>& seam) {
    const int h = img.height(), w = img.width(); // w = old width - 1
    EnergyMap e(w, h);
    for (int r = 0; r < h; ++r) {
        const int s = seam[static_cast<std::size_t>(r)];
        for (int c = 0; c < w; ++c) e.set(r, c, old_energy.at(r, c < s ? c : c + 1));
        int lo = s, hi = s;
        if (r > 0) { lo = std::min(lo, seam[r - 1]); hi = std::max(hi, seam[r - 1]); }
        if (r < h - 1) { lo = std::min(lo, seam[r + 1]); hi = std::max(hi, seam[r + 1]); }
        for (int c = std::max(0, lo - 1); c <= std::min(w - 1, hi); ++c) e.set(r, c, dual_gradient_at(img, r, c));
    }
    return e;
}

// Measures the Item 5d improvements against the production code and writes improvements.csv.
void improvements(const std::string& out) {
    std::ofstream csv(out + "/improvements.csv");
    csv << "experiment,image,variant,threads,ms,memory_bytes,matches_production\n";
    std::cout << "\n== Item 5d improvements (prototypes, each checked against the production code) ==\n";

    // 1. Do we need the entire DP table?
    std::cout << "-- DP memory: full tables (DPSeamFinder) vs rolling rows + 1-byte parents\n";
    for (int w : {640, 1280, 1920}) {
        int h = w * 3 / 4;
        EnergyMap e = compute_energy(make_scene(w, h));
        std::vector<int> full_seam, compact_seam;
        double full_ms = time_ms([&] { full_seam = DPSeamFinder::find_vertical_seam(e).seam; }, 5);
        double compact_ms = time_ms([&] { compact_seam = compact_dp_vertical_seam(e); }, 5);
        std::size_t full_mem = static_cast<std::size_t>(h) * w * (sizeof(double) + sizeof(int));
        std::size_t compact_mem = 2 * static_cast<std::size_t>(w) * sizeof(double) + static_cast<std::size_t>(h) * w;
        bool same = full_seam == compact_seam;
        std::string size = std::to_string(w) + "x" + std::to_string(h);
        csv << "dp_memory," << size << ",full_tables,1," << full_ms << "," << full_mem << ",1\n";
        csv << "dp_memory," << size << ",rolling_rows_1byte_parents,1," << compact_ms << "," << compact_mem << ","
            << same << "\n";
        std::cout << "   " << std::left << std::setw(10) << size << std::right << std::fixed << std::setprecision(2)
                  << "full " << std::setw(7) << full_ms << " ms " << std::setw(8) << full_mem / 1024 << " KB   compact "
                  << std::setw(7) << compact_ms << " ms " << std::setw(7) << compact_mem / 1024 << " KB   ("
                  << std::setprecision(1) << double(full_mem) / compact_mem << "x less memory, same seam: "
                  << (same ? "yes" : "NO") << ")\n";
    }

    // 2. Does multithreading help? Energy map (independent pixels) vs DP (barrier every row).
    std::cout << "-- Multithreading: speed-up over the serial version\n";
    for (int w : {320, 1920}) {
        const int h = w * 3 / 4;
        const std::string size = std::to_string(w) + "x" + std::to_string(h);
        std::cout << "   " << size << ":\n";
        Image img = make_scene(w, h);
        EnergyMap reference = compute_energy(img);
        std::vector<int> dp_seam = DPSeamFinder::find_vertical_seam(reference).seam;
        double energy_serial = time_ms([&] { compute_energy(img); }, 5);
        double dp_serial = time_ms([&] { DPSeamFinder::find_vertical_seam(reference); }, 5);
        csv << "multithreading," << size << ",energy_map,1," << energy_serial << ",,1\n";
        csv << "multithreading," << size << ",dp,1," << dp_serial << ",,1\n";
        for (int t : {2, 4, 8}) {
            EnergyMap pe;
            std::vector<int> ps;
            double e_ms = time_ms([&] { pe = parallel_energy(img, t); }, 5);
            double d_ms = time_ms([&] { ps = row_parallel_dp_vertical_seam(reference, t); }, 5);
            bool e_same = pe.data() == reference.data(), d_same = ps == dp_seam;
            csv << "multithreading," << size << ",energy_map," << t << "," << e_ms << ",," << e_same << "\n";
            csv << "multithreading," << size << ",dp," << t << "," << d_ms << ",," << d_same << "\n";
            std::cout << "     " << t << " threads: energy map " << std::setprecision(2) << std::setw(6)
                      << energy_serial / e_ms << "x  (" << e_ms << " ms, identical: " << (e_same ? "yes" : "NO")
                      << ")   row-parallel DP " << std::setw(5) << dp_serial / d_ms << "x  (" << d_ms
                      << " ms, same seam: " << (d_same ? "yes" : "NO") << ")\n";
        }
    }

    // 3. Incremental energy update over a full carve: recompute only pixels next to each seam.
    std::cout << "-- Incremental energy update: 640x480 scene, 192 vertical seams with DP\n";
    {
        Image start = make_scene(640, 480);
        const int seams = 192;
        double full_ms = 0, incr_ms = 0;
        bool all_same = true;
        Image img_full = start, img_incr = start;
        EnergyMap e_incr = compute_energy(img_incr);
        for (int k = 0; k < seams; ++k) {
            EnergyMap e_full;
            full_ms += time_ms([&] { e_full = compute_energy(img_full); });
            if (k > 0) all_same = all_same && e_full.data() == e_incr.data();
            std::vector<int> seam = DPSeamFinder::find_vertical_seam(e_full).seam;
            img_full.remove_vertical_seam(seam);
            img_incr.remove_vertical_seam(seam);
            incr_ms += time_ms([&] { e_incr = incremental_energy(e_incr, img_incr, seam); });
        }
        csv << "incremental_energy,640x480,full_recompute,1," << full_ms << ",,1\n";
        csv << "incremental_energy,640x480,incremental,1," << incr_ms << ",," << all_same << "\n";
        std::cout << "   energy maps for " << seams << " seams: full recompute " << std::setprecision(1) << full_ms
                  << " ms, incremental " << incr_ms << " ms (" << full_ms / incr_ms
                  << "x faster, identical every step: " << (all_same ? "yes" : "NO") << ")\n";
    }
}

} // namespace

// Benchmarks the three seam-finding algorithms - Greedy, DP and Graph Cut (Dinic and
// Edmonds-Karp) - on identical energy maps (Assignment 2, Item 5b/5c): time per seam across image
// sizes, optimality of the seam, working memory, and a full carve of one image. It then measures
// prototypes of the Item 5d improvements (compact DP table, multithreading, incremental energy).
// Usage: ./bin/benchmark_algorithms [--image PATH] [--cols N] [--rows N] [--out DIR] [--budget-ms N]
// (`make benchmark` runs it and writes to results/benchmark/).
int main(int argc, char** argv) {
    std::string image_path = "samples/synthetic_tower.png", out = "results/benchmark";
    int cols = -1, rows = -1;
    double budget_ms = 3000.0;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (i + 1 >= argc) { std::cerr << "Missing value for " << k << "\n"; return 1; }
        if (k == "--image") image_path = argv[++i];
        else if (k == "--cols") cols = std::atoi(argv[++i]);
        else if (k == "--rows") rows = std::atoi(argv[++i]);
        else if (k == "--out") out = argv[++i];
        else if (k == "--budget-ms") budget_ms = std::atof(argv[++i]);
        else { std::cerr << "Unknown option " << k << "\n"; return 1; }
    }
    if (!std::ofstream(out + "/scaling.csv")) {
        std::cerr << "Cannot write to '" << out << "/'. Create the folder first (make benchmark does this).\n";
        return 1;
    }
    Image img;
    if (!img.load_from_file(image_path)) { std::cerr << "Cannot load " << image_path << "\n"; return 1; }
    if (cols < 0) cols = img.width() * 3 / 10; // remove 30% of the width
    if (rows < 0) rows = img.height() / 10;    // and 10% of the height
    std::string label = image_path.substr(image_path.find_last_of("/\\") + 1);
    label = label.substr(0, label.find_last_of('.'));

    scaling(out, budget_ms);
    full_carve(img, label, cols, rows, out);
    improvements(out);
    std::cout << "\nWrote " << out << "/\n";
    return 0;
}

#include "dp_seam.hpp"
#include "energy.hpp"
#include "greedy_seam.hpp"
#include "image.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
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

// Builds a procedural "tower" scene (sky, cloud, tower, grass) at any size, so the timing
// experiment has the same content at every resolution.
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

// Maps every pixel of the carved image back to its index in the original, so all removed
// seams can be drawn on the original image.
struct CoordTracker {
    int w, h;
    std::vector<int> orig;
    CoordTracker(int width, int height) : w(width), h(height), orig(static_cast<std::size_t>(width) * height) {
        for (std::size_t i = 0; i < orig.size(); ++i) orig[i] = static_cast<int>(i);
    }
    std::vector<int> remove(const std::vector<int>& seam, bool vertical) {
        std::vector<int> removed, next;
        if (vertical) {
            for (int r = 0; r < h; ++r)
                for (int c = 0; c < w; ++c)
                    (c == seam[r] ? removed : next).push_back(orig[r * w + c]);
            --w;
        } else {
            next.assign(static_cast<std::size_t>(w) * (h - 1), 0);
            for (int c = 0; c < w; ++c) {
                int dr = 0;
                for (int r = 0; r < h; ++r) {
                    if (r == seam[c]) removed.push_back(orig[r * w + c]);
                    else next[static_cast<std::size_t>(dr++) * w + c] = orig[r * w + c];
                }
            }
            --h;
        }
        orig.swap(next);
        return removed;
    }
};

// Places two images next to each other (top-aligned) with a white gap, for the comparison figures.
Image side_by_side(const Image& a, const Image& b, int gap = 8) {
    Image out(a.width() + gap + b.width(), std::max(a.height(), b.height()));
    for (int r = 0; r < out.height(); ++r)
        for (int c = 0; c < out.width(); ++c) out.set_pixel(r, c, Pixel{255, 255, 255});
    for (int r = 0; r < a.height(); ++r)
        for (int c = 0; c < a.width(); ++c) out.set_pixel(r, c, a.get_pixel(r, c));
    for (int r = 0; r < b.height(); ++r)
        for (int c = 0; c < b.width(); ++c) out.set_pixel(r, a.width() + gap + c, b.get_pixel(r, c));
    return out;
}

using Finder = SeamResult (*)(const EnergyMap&);

// Compares running time: times one vertical seam for Greedy and DP on the same energy map at
// six image sizes (median of 5 runs each) and writes timing.csv.
void timing(const std::string& out) {
    std::ofstream csv(out + "/timing.csv");
    csv << "width,height,energy_map_ms,greedy_ms,dp_ms,dp_over_greedy,greedy_energy,dp_energy,greedy_vs_optimal\n";
    std::cout << "== Running time: one vertical seam on the same energy map (median of 5) ==\n"
              << std::left << std::setw(11) << "size" << std::right << std::setw(12) << "energy ms"
              << std::setw(12) << "greedy ms" << std::setw(10) << "DP ms" << std::setw(12) << "DP/greedy"
              << std::setw(16) << "greedy/optimal" << "\n";
    for (int w : {80, 160, 320, 640, 1280, 1920}) {
        int h = w * 3 / 4;
        Image img = make_scene(w, h);
        EnergyMap e;
        double e_ms = time_ms([&] { e = compute_energy(img); }, 5);
        SeamResult g, d;
        double g_ms = time_ms([&] { g = GreedySeamFinder::find_vertical_seam(e); }, 5);
        double d_ms = time_ms([&] { d = DPSeamFinder::find_vertical_seam(e); }, 5);
        double ratio = g.energy_sum / d.energy_sum;
        csv << w << "," << h << "," << e_ms << "," << g_ms << "," << d_ms << "," << d_ms / g_ms << ","
            << g.energy_sum << "," << d.energy_sum << "," << ratio << "\n";
        std::cout << std::left << std::setw(11) << (std::to_string(w) + "x" + std::to_string(h)) << std::right
                  << std::fixed << std::setprecision(3) << std::setw(12) << e_ms << std::setw(12) << g_ms
                  << std::setw(10) << d_ms << std::setprecision(0) << std::setw(11) << d_ms / g_ms << "x"
                  << std::setprecision(3) << std::setw(15) << ratio << "x\n";
    }
}

// Compares seams and visual quality: carves the image with Greedy and with DP, saves the
// removed seams and the carved results side by side, and writes quality_<label>.csv.
void quality(const Image& original, const std::string& label, int cols, int rows, const std::string& out) {
    std::cout << "\n== Seams & visual quality: " << label << " " << original.width() << "x" << original.height()
              << ", removing " << cols << " columns + " << rows << " rows ==\n";
    struct Algo { const char* name; Finder vertical, horizontal; Pixel colour; };
    const Algo algos[] = {
        {"greedy", GreedySeamFinder::find_vertical_seam, GreedySeamFinder::find_horizontal_seam, {255, 0, 0}},
        {"dp", DPSeamFinder::find_vertical_seam, DPSeamFinder::find_horizontal_seam, {0, 200, 0}},
    };
    std::ofstream csv(out + "/quality_" + label + ".csv");
    csv << "algorithm,step,orientation,seam_energy,cumulative_energy,seam_ms\n";

    Image overlays[2], carved[2];
    for (int ai = 0; ai < 2; ++ai) {
        const Algo& a = algos[ai];
        Image img = original, overlay = original;
        CoordTracker track(img.width(), img.height());
        double total = 0, seam_ms = 0, energy_ms = 0;
        for (int k = 0; k < cols + rows; ++k) {
            bool vertical = k < cols;
            EnergyMap e;
            energy_ms += time_ms([&] { e = compute_energy(img); });
            SeamResult s;
            double ms = time_ms([&] { s = vertical ? a.vertical(e) : a.horizontal(e); });
            seam_ms += ms;
            total += s.energy_sum;
            csv << a.name << "," << k + 1 << "," << (vertical ? "V" : "H") << "," << s.energy_sum << "," << total
                << "," << ms << "\n";
            for (int idx : track.remove(s.seam, vertical))
                overlay.set_pixel(idx / original.width(), idx % original.width(), a.colour);
            if (vertical) img.remove_vertical_seam(s.seam);
            else img.remove_horizontal_seam(s.seam);
        }
        std::cout << std::left << std::setw(8) << a.name << std::right << "total energy removed " << std::fixed
                  << std::setprecision(1) << std::setw(10) << total << "   seam search " << std::setprecision(2)
                  << std::setw(7) << seam_ms << " ms   energy maps " << std::setw(7) << energy_ms
                  << " ms   total " << seam_ms + energy_ms << " ms\n";
        overlays[ai] = overlay;
        carved[ai] = img;
    }
    side_by_side(overlays[0], overlays[1]).save_to_file(out + "/" + label + "_seams_greedy_vs_dp.png");
    side_by_side(carved[0], carved[1]).save_to_file(out + "/" + label + "_carved_greedy_vs_dp.png");

    // Follow the DP carve and, at every step, ask greedy for a seam on the identical energy map
    // to measure how far it is from the optimum. Also draw the very first seam of each.
    Image img = original;
    double sum = 0, worst = 1;
    int optimal_hits = 0;
    for (int k = 0; k < cols; ++k) {
        EnergyMap e = compute_energy(img);
        SeamResult d = DPSeamFinder::find_vertical_seam(e);
        SeamResult g = GreedySeamFinder::find_vertical_seam(e);
        double ratio = g.energy_sum / d.energy_sum;
        sum += ratio;
        worst = std::max(worst, ratio);
        if (ratio < 1 + 1e-9) ++optimal_hits;
        if (k == 0) {
            Image first = original;
            for (int r = 0; r < first.height(); ++r) {
                first.set_pixel(r, g.seam[r], algos[0].colour);
                first.set_pixel(r, d.seam[r], algos[1].colour);
            }
            first.save_to_file(out + "/" + label + "_first_seam.png");
        }
        img.remove_vertical_seam(d.seam);
    }
    std::cout << "Same energy map, " << cols << " steps: greedy seam = " << std::setprecision(3)
              << sum / std::max(1, cols) << "x the optimal (DP) on average, worst " << worst << "x; greedy optimal on "
              << optimal_hits << "/" << cols << " steps\n";
}

} // namespace

// Compares the Greedy seam finder with DP (Assignment 2, Item 3) on seams, visual quality and
// running time. Both always receive the identical energy map, so only the seam finder differs.
// Usage: ./bin/compare_greedy_dp [--image PATH] [--cols N] [--rows N] [--out DIR]
// (`make compare` runs it and writes to results/greedy_vs_dp/).
int main(int argc, char** argv) {
    std::string image_path = "samples/synthetic_tower.png", out = "results/greedy_vs_dp";
    int cols = -1, rows = -1;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (i + 1 >= argc) { std::cerr << "Missing value for " << k << "\n"; return 1; }
        if (k == "--image") image_path = argv[++i];
        else if (k == "--cols") cols = std::atoi(argv[++i]);
        else if (k == "--rows") rows = std::atoi(argv[++i]);
        else if (k == "--out") out = argv[++i];
        else { std::cerr << "Unknown option " << k << "\n"; return 1; }
    }
    if (!std::ofstream(out + "/timing.csv")) {
        std::cerr << "Cannot write to '" << out << "/'. Create the folder first (make compare does this).\n";
        return 1;
    }
    Image img;
    if (!img.load_from_file(image_path)) { std::cerr << "Cannot load " << image_path << "\n"; return 1; }
    if (cols < 0) cols = img.width() * 3 / 10; // remove 30% of the width
    if (rows < 0) rows = img.height() / 10;    // and 10% of the height
    std::string label = image_path.substr(image_path.find_last_of("/\\") + 1);
    label = label.substr(0, label.find_last_of('.'));

    timing(out);
    quality(img, label, cols, rows, out);
    std::cout << "\nWrote " << out << "/\n";
    return 0;
}

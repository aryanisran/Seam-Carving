#include "energy.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace seam_carving {

EnergyMap::EnergyMap(int width, int height)
    : width_(width), height_(height),
      data_(static_cast<std::size_t>(width) * height, 0.0) {}

EnergyMap::EnergyMap(int width, int height, std::vector<double> values)
    : width_(width), height_(height), data_(std::move(values)) {
    if (static_cast<int>(data_.size()) != width_ * height_) {
        throw std::invalid_argument("EnergyMap data size does not match width * height");
    }
}

double EnergyMap::at(int row, int col) const {
    return data_[static_cast<std::size_t>(row) * width_ + col];
}

void EnergyMap::set(int row, int col, double val) {
    data_[static_cast<std::size_t>(row) * width_ + col] = val;
}

namespace {

double dual_gradient_at(const Image& img, int r, int c) {
    Pixel left = img.get_pixel(r, c - 1);
    Pixel right = img.get_pixel(r, c + 1);
    Pixel up = img.get_pixel(r - 1, c);
    Pixel down = img.get_pixel(r + 1, c);

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

double sobel_at(const Image& img, int r, int c) {
    double tl = img.get_luminance(r - 1, c - 1);
    double tc = img.get_luminance(r - 1, c);
    double tr = img.get_luminance(r - 1, c + 1);

    double ml = img.get_luminance(r, c - 1);
    double mr = img.get_luminance(r, c + 1);

    double bl = img.get_luminance(r + 1, c - 1);
    double bc = img.get_luminance(r + 1, c);
    double br = img.get_luminance(r + 1, c + 1);

    double gx = -tl + tr - 2.0 * ml + 2.0 * mr - bl + br;
    double gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;
    return std::sqrt(gx * gx + gy * gy);
}

} // namespace

EnergyMap compute_energy(const Image& img, EnergyMetric metric) {
    if (img.empty()) {
        return EnergyMap();
    }
    int w = img.width();
    int h = img.height();
    EnergyMap map(w, h);

    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            double e = (metric == EnergyMetric::Sobel)
                           ? sobel_at(img, r, c)
                           : dual_gradient_at(img, r, c);
            map.set(r, c, e);
        }
    }
    return map;
}

} // namespace seam_carving

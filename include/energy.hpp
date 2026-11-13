#ifndef GRAPH_CUT_ENERGY_HPP
#define GRAPH_CUT_ENERGY_HPP

#include "image.hpp"
#include <vector>

namespace seam_carving {

enum class EnergyMetric {
    DualGradient,
    Sobel
};

class EnergyMap {
public:
    EnergyMap() = default;
    EnergyMap(int width, int height);
    EnergyMap(int width, int height, std::vector<double> values);

    int width() const { return width_; }
    int height() const { return height_; }
    bool empty() const { return width_ <= 0 || height_ <= 0 || data_.empty(); }

    double at(int row, int col) const;
    void set(int row, int col, double val);

    const std::vector<double>& data() const { return data_; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<double> data_;
};

EnergyMap compute_energy(const Image& img, EnergyMetric metric = EnergyMetric::DualGradient);

} // namespace seam_carving

#endif // GRAPH_CUT_ENERGY_HPP

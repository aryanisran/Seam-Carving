#ifndef GRAPH_CUT_IMAGE_HPP
#define GRAPH_CUT_IMAGE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace seam_carving {

struct Pixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

class Image {
public:
    Image() = default;
    Image(int width, int height, int channels = 3);
    Image(int width, int height, int channels, std::vector<uint8_t> data);

    bool load_from_file(const std::string& path);
    bool save_to_file(const std::string& path) const;

    int width() const { return width_; }
    int height() const { return height_; }
    int channels() const { return channels_; }
    bool empty() const { return width_ <= 0 || height_ <= 0 || data_.empty(); }

    Pixel get_pixel(int row, int col) const;
    void set_pixel(int row, int col, const Pixel& p);

    double get_luminance(int row, int col) const;

    // Returns a transposed copy (new width = height_, new height = width_)
    Image transposed() const;

    // Removes one pixel per row as specified by seam[row] = col
    void remove_vertical_seam(const std::vector<int>& seam);

    // Removes one pixel per col as specified by seam[col] = row
    void remove_horizontal_seam(const std::vector<int>& seam);

    const std::vector<uint8_t>& data() const { return data_; }

private:
    int width_ = 0;
    int height_ = 0;
    int channels_ = 3;
    std::vector<uint8_t> data_;
};

} // namespace seam_carving

#endif // GRAPH_CUT_IMAGE_HPP

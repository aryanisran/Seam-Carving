#include "image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace seam_carving {

namespace {
std::string to_lower_ext(const std::string& path) {
    auto pos = path.find_last_of('.');
    if (pos == std::string::npos) return "";
    std::string ext = path.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return ext;
}
} // namespace

Image::Image(int width, int height, int channels)
    : width_(width), height_(height), channels_(channels),
      data_(static_cast<std::size_t>(width) * height * channels, 0) {}

Image::Image(int width, int height, int channels, std::vector<uint8_t> data)
    : width_(width), height_(height), channels_(channels), data_(std::move(data)) {}

bool Image::load_from_file(const std::string& path) {
    int w = 0, h = 0, c = 0;
    // Force 3 RGB channels for consistent processing
    uint8_t* raw = stbi_load(path.c_str(), &w, &h, &c, 3);
    if (!raw) {
        return false;
    }
    width_ = w;
    height_ = h;
    channels_ = 3;
    std::size_t total_bytes = static_cast<std::size_t>(w) * h * 3;
    data_.assign(raw, raw + total_bytes);
    stbi_image_free(raw);
    return true;
}

bool Image::save_to_file(const std::string& path) const {
    if (empty()) return false;
    std::string ext = to_lower_ext(path);
    if (ext == "png") {
        return stbi_write_png(path.c_str(), width_, height_, channels_, data_.data(), width_ * channels_) != 0;
    } else if (ext == "bmp") {
        return stbi_write_bmp(path.c_str(), width_, height_, channels_, data_.data()) != 0;
    } else if (ext == "jpg" || ext == "jpeg") {
        return stbi_write_jpg(path.c_str(), width_, height_, channels_, data_.data(), 95) != 0;
    } else {
        // Default to PNG if extension is unknown
        return stbi_write_png(path.c_str(), width_, height_, channels_, data_.data(), width_ * channels_) != 0;
    }
}

Pixel Image::get_pixel(int row, int col) const {
    int r = std::clamp(row, 0, height_ - 1);
    int c = std::clamp(col, 0, width_ - 1);
    std::size_t idx = (static_cast<std::size_t>(r) * width_ + c) * channels_;
    return Pixel{data_[idx], data_[idx + 1], data_[idx + 2]};
}

void Image::set_pixel(int row, int col, const Pixel& p) {
    if (row < 0 || row >= height_ || col < 0 || col >= width_) {
        throw std::out_of_range("Pixel coordinates out of bounds");
    }
    std::size_t idx = (static_cast<std::size_t>(row) * width_ + col) * channels_;
    data_[idx] = p.r;
    data_[idx + 1] = p.g;
    data_[idx + 2] = p.b;
}

double Image::get_luminance(int row, int col) const {
    Pixel p = get_pixel(row, col);
    return 0.299 * p.r + 0.587 * p.g + 0.114 * p.b;
}

Image Image::transposed() const {
    Image res(height_, width_, channels_);
    for (int r = 0; r < height_; ++r) {
        for (int c = 0; c < width_; ++c) {
            res.set_pixel(c, r, get_pixel(r, c));
        }
    }
    return res;
}

void Image::remove_vertical_seam(const std::vector<int>& seam) {
    if (width_ <= 1) {
        throw std::runtime_error("Cannot remove vertical seam when width <= 1");
    }
    if (static_cast<int>(seam.size()) != height_) {
        throw std::invalid_argument("Vertical seam length does not match image height");
    }
    int new_w = width_ - 1;
    std::vector<uint8_t> new_data(static_cast<std::size_t>(new_w) * height_ * channels_);

    for (int r = 0; r < height_; ++r) {
        int seam_c = seam[r];
        if (seam_c < 0 || seam_c >= width_) {
            throw std::out_of_range("Seam column index out of range");
        }
        std::size_t src_row_offset = static_cast<std::size_t>(r) * width_ * channels_;
        std::size_t dst_row_offset = static_cast<std::size_t>(r) * new_w * channels_;

        // Copy pixels before seam_c
        if (seam_c > 0) {
            std::copy_n(data_.begin() + src_row_offset,
                        static_cast<std::size_t>(seam_c) * channels_,
                        new_data.begin() + dst_row_offset);
        }
        // Copy pixels after seam_c
        if (seam_c < width_ - 1) {
            std::copy_n(data_.begin() + src_row_offset + static_cast<std::size_t>(seam_c + 1) * channels_,
                        static_cast<std::size_t>(width_ - 1 - seam_c) * channels_,
                        new_data.begin() + dst_row_offset + static_cast<std::size_t>(seam_c) * channels_);
        }
    }

    width_ = new_w;
    data_ = std::move(new_data);
}

void Image::remove_horizontal_seam(const std::vector<int>& seam) {
    if (height_ <= 1) {
        throw std::runtime_error("Cannot remove horizontal seam when height <= 1");
    }
    if (static_cast<int>(seam.size()) != width_) {
        throw std::invalid_argument("Horizontal seam length does not match image width");
    }
    int new_h = height_ - 1;
    std::vector<uint8_t> new_data(static_cast<std::size_t>(width_) * new_h * channels_);

    for (int c = 0; c < width_; ++c) {
        int seam_r = seam[c];
        if (seam_r < 0 || seam_r >= height_) {
            throw std::out_of_range("Seam row index out of range");
        }
        int dst_r = 0;
        for (int r = 0; r < height_; ++r) {
            if (r == seam_r) continue;
            std::size_t src_idx = (static_cast<std::size_t>(r) * width_ + c) * channels_;
            std::size_t dst_idx = (static_cast<std::size_t>(dst_r) * width_ + c) * channels_;
            new_data[dst_idx] = data_[src_idx];
            new_data[dst_idx + 1] = data_[src_idx + 1];
            new_data[dst_idx + 2] = data_[src_idx + 2];
            ++dst_r;
        }
    }

    height_ = new_h;
    data_ = std::move(new_data);
}

} // namespace seam_carving

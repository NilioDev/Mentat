// SPDX-License-Identifier: GPL-3.0-or-later
//
// An 8-bit picture: one palette index per pixel, rows top to bottom.
#pragma once

#include <cstdint>
#include <vector>

namespace mentat {

class Surface {
public:
    static constexpr int kScreenWidth = 320;
    static constexpr int kScreenHeight = 200;

    Surface() : Surface(kScreenWidth, kScreenHeight) {}
    Surface(int width, int height)
        : width_(width), height_(height), pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0) {}

    int width() const { return width_; }
    int height() const { return height_; }
    std::uint8_t* data() { return pixels_.data(); }
    const std::uint8_t* data() const { return pixels_.data(); }
    std::size_t size() const { return pixels_.size(); }

    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }
    std::uint8_t at(int x, int y) const { return pixels_[index(x, y)]; }
    void set(int x, int y, std::uint8_t colour) {
        if (contains(x, y)) {
            pixels_[index(x, y)] = colour;
        }
    }
    void fill(std::uint8_t colour) { pixels_.assign(pixels_.size(), colour); }
    void fillRect(int x, int y, int w, int h, std::uint8_t colour);

    // Copies a rectangle of another surface onto this one, clipped to both.
    void blit(const Surface& from, int fromX, int fromY, int w, int h, int toX, int toY);

private:
    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
    }

    int width_;
    int height_;
    std::vector<std::uint8_t> pixels_;
};

} // namespace mentat

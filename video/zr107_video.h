#ifndef MIDNRUN_ZR107_VIDEO_H
#define MIDNRUN_ZR107_VIDEO_H

#include <cstddef>
#include <cstdint>

namespace midnrun {
// A 14-bit signed horizontal and 13-bit signed vertical coordinate in 12.4 units.
// 256, 200 are the board's projection origin. The visible raster is 512 x 384.
struct RasterPoint { float x, y; };
struct Viewport {
    unsigned width, height;
    RasterPoint polygon_xy(std::uint32_t fifo_word) const;
    RasterPoint hud_xy(float x, float y) const;
    float horizontal_limit() const;
};

// Each physical ROM lane carries two consecutive bytes in every eight-byte
// group. Pass 477a13, 477a14, 477a15, 477a16 in that order, with no swapping.
bool decode_texture_pages(const std::uint8_t *const lanes[4], std::size_t lane_bytes,
                          std::uint8_t *pages, std::size_t output_bytes);

std::uint32_t palette_rgba(std::uint16_t value);
std::uint32_t texture_color(const std::uint8_t *pages, const std::uint32_t *palette,
                            unsigned page, unsigned bank, int u, int v);
}
#endif

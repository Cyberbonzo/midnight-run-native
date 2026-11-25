#include "zr107_video.h"

namespace midnrun {
namespace {
int sign_extend(unsigned bits, unsigned value) {
    unsigned sign = 1u << (bits - 1);
    return static_cast<int>(value & (sign - 1)) - static_cast<int>(value & sign);
}
unsigned five_to_eight(unsigned v) {
    return (v << 3) | (v >> 2);
}
// For each 512 x 512 texture, 8 x 16 blocks are stored in Morton order.
unsigned block_number(unsigned x, unsigned y) {
    unsigned block = 0;
    for (unsigned bit = 0; bit < 6; ++bit) {
        block |= ((x >> bit) & 1) << (2 * bit);
        block |= ((y >> bit) & 1) << (2 * bit + 1);
    }
    return block;
}
}

RasterPoint Viewport::hud_xy(float x, float y) const {
    const float scale = height / 384.0f;
    return { (x - 256.0f) * scale + width * 0.5f, y * scale };
}
RasterPoint Viewport::polygon_xy(std::uint32_t word) const {
    const float x = sign_extend(14, word) / 16.0f;
    const float y = sign_extend(13, word >> 16) / 16.0f;
    return hud_xy(256.0f + x, 200.0f - y);
}
float Viewport::horizontal_limit() const {
    return height ? 192.0f * width / height : 0.0f;
}

bool decode_texture_pages(const std::uint8_t *const lanes[4], std::size_t lane_bytes,
                          std::uint8_t *pages, std::size_t output_bytes) {
    if (!lanes || !pages || lane_bytes != 0x200000 || output_bytes < 0x800000) return false;
    for (unsigned lane = 0; lane < 4; ++lane) if (!lanes[lane]) return false;
    for (unsigned page = 0; page < 32; ++page) {
        for (unsigned y = 0; y < 512; ++y) {
            for (unsigned x = 0; x < 512; ++x) {
                const unsigned local = ((x & 1) << 4) | (x & 6)
                    | ((y & 1) << 3) | ((y & 2) << 4)
                    | ((y & 4) >> 2) | ((y & 8) << 3);
                const unsigned address = (page << 18) | (block_number(x >> 3, y >> 4) << 7) | local;
                pages[(page << 18) | (y << 9) | x] =
                    lanes[(address >> 1) & 3][((address >> 3) << 1) | (address & 1)];
            }
        }
    }
    return true;
}

std::uint32_t palette_rgba(std::uint16_t value) {
    const unsigned alpha = (value & 0x8000) ? 0 : 255;
    return (alpha << 24) | (five_to_eight(value & 31) << 16)
         | (five_to_eight((value >> 5) & 31) << 8) | five_to_eight((value >> 10) & 31);
}
std::uint32_t texture_color(const std::uint8_t *pages, const std::uint32_t *palette,
                            unsigned page, unsigned bank, int u, int v) {
    unsigned index = pages[((page & 31) << 18) | ((static_cast<unsigned>(v) & 511) << 9)
                           | (static_cast<unsigned>(u) & 511)];
    return index ? palette[((bank & 7) << 8) | index] : 0;
}
}

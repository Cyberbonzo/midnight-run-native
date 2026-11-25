#ifndef MIDNRUN_TEXEL_BUS_H
#define MIDNRUN_TEXEL_BUS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct ZRTexelBus {
    const uint8_t *lanes[4];
    uint32_t addr;
    uint32_t select;
    uint16_t palette[0x800];
    uint16_t auxiliary[0x1000];
} ZRTexelBus;

/* lanes: 477a13.9h, 477a14.7h, 477a15.5h, 477a16.2h.
 * Caller retains all four 0x200000-byte arrays for the lifetime of the bus. */
bool zr_texel_init(ZRTexelBus *bus, const uint8_t *const lanes[4], size_t lane_size);
bool zr_texel_read(void *bus, uint32_t addr, unsigned bytes, uint32_t *value);
bool zr_texel_write(void *bus, uint32_t addr, unsigned bytes, uint32_t value);
#ifdef __cplusplus
}
#endif
#endif

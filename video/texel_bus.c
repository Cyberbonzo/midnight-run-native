#include "texel_bus.h"
#include <string.h>

bool zr_texel_init(ZRTexelBus *bus, const uint8_t *const lanes[4], size_t lane_size) {
    if (!bus || !lanes || lane_size != 0x200000) return false;
    for (unsigned i = 0; i < 4; ++i) if (!lanes[i]) return false;
    memset(bus, 0, sizeof(*bus));
    memcpy(bus->lanes, lanes, sizeof(bus->lanes));
    return true;
}

static bool valid(uint32_t addr, unsigned bytes) {
    return addr >= 0x78040000 && addr < 0x78040010 &&
           (bytes == 1 || bytes == 2 || bytes == 4) && !(addr & (bytes - 1));
}

static uint32_t mask_for(unsigned bytes) {
    return bytes == 4 ? UINT32_MAX : (1u << (8 * bytes)) - 1;
}

bool zr_texel_read(void *ctx, uint32_t addr, unsigned bytes, uint32_t *value) {
    ZRTexelBus *bus = (ZRTexelBus *)ctx;
    if (!bus || !value || !valid(addr, bytes)) return false;
    unsigned shift = (4 - bytes - (addr & 3)) * 8;
    uint32_t word = 0;
    if (((addr >> 2) & 3) == 1) {
        switch ((bus->select >> 16) & 15) {
        case 0xb: {
            unsigned pos = bus->addr & 0x7ffffe;
            const uint8_t *lane = bus->lanes[(pos >> 1) & 3];
            pos = (pos >> 3) << 1;
            word = ((uint32_t)lane[pos + 1] << 24) | ((uint32_t)lane[pos] << 16);
            break;
        }
        case 0xd:
            word = bus->palette[(bus->addr >> 1) & 0x7ff];
            bus->addr += 2;
            break;
        case 0xf:
            word = bus->auxiliary[bus->addr++ & 0xfff];
            break;
        }
    }
    *value = (word >> shift) & mask_for(bytes);
    return true;
}

bool zr_texel_write(void *ctx, uint32_t addr, unsigned bytes, uint32_t value) {
    ZRTexelBus *bus = (ZRTexelBus *)ctx;
    if (!bus || !valid(addr, bytes)) return false;
    unsigned shift = (4 - bytes - (addr & 3)) * 8;
    uint32_t mask = mask_for(bytes) << shift;
    uint32_t data = (value << shift) & mask;
    switch ((addr >> 2) & 3) {
    case 0:
        bus->addr = (bus->addr & ~mask) | data;
        break;
    case 1:
        switch ((bus->select >> 16) & 15) {
        case 0xd:
            bus->palette[(bus->addr >> 1) & 0x7ff] = (uint16_t)data;
            bus->addr += 2;
            break;
        case 0xf:
            bus->auxiliary[bus->addr++ & 0xfff] = (uint16_t)data;
            break;
        }
        break;
    case 2:
        bus->select = (bus->select & ~mask) | data;
        break;
    }
    return true;
}

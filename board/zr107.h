#ifndef MIDNRUN_ZR107_H
#define MIDNRUN_ZR107_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZRBoard ZRBoard;
/* All ROM pointers are borrowed, byte-addressed and big endian. */
ZRBoard *zr_create(const uint8_t *program, size_t size);
void zr_destroy(ZRBoard *b);
bool zr_set_tiles(ZRBoard *b, const uint8_t *tiles, size_t size);
bool zr_read(void *b, uint32_t guest_addr, unsigned size, uint32_t *value);
bool zr_write(void *b, uint32_t guest_addr, unsigned size, uint32_t value);
void zr_set_inputs(ZRBoard *b, uint8_t wheel, uint8_t gas, uint8_t brake,
                   uint8_t buttons_pressed, bool coin);
const uint8_t *zr_workram(const ZRBoard *b);
const uint8_t *zr_tile_page(const ZRBoard *b, unsigned page);
const uint8_t *zr_tile_regs(const ZRBoard *b);
const uint8_t *zr_palette(const ZRBoard *b);
void zr_clear_frame(const ZRBoard *b,uint32_t *pixels,unsigned stride,unsigned width,unsigned height);
bool zr_draw_tiles(const ZRBoard *b,unsigned layer,uint32_t *pixels,unsigned stride,unsigned width,unsigned height);
void zr_vblank(ZRBoard *b);
unsigned zr_ppc_irq(const ZRBoard *b);
bool zr_eeprom_load(ZRBoard *b, const uint8_t data[128]);
void zr_eeprom_dump(const ZRBoard *b, uint8_t data[128]);
uint8_t zr_sound_mail_read(ZRBoard *b, uint32_t sound_reg);
void zr_sound_mail_write(ZRBoard *b, uint32_t sound_reg, uint8_t value);
bool zr_set_soundrom(ZRBoard *b,const uint8_t *be_program,size_t size);
bool zr_set_samples(ZRBoard *b,const uint8_t *samples,size_t size);
bool zr_audio_read(void *b,uint32_t addr,unsigned size,uint32_t *value);
bool zr_audio_write(void *b,uint32_t addr,unsigned size,uint32_t value);
unsigned zr_audio_irq_level(const ZRBoard *b);
/* Call at 48000 Hz. Passing NULL advances timers/voices without storing output. */
void zr_audio_render(ZRBoard *b,int16_t *interleaved_stereo,size_t frames);
bool zr_sound_irq(const ZRBoard *b);
bool zr_sound_in_reset(const ZRBoard *b);
bool zr_sharc_read(ZRBoard *b, uint32_t word_address, uint32_t *value);
bool zr_sharc_write(ZRBoard *b, uint32_t word_address, uint32_t value);
typedef void (*ZRPixelPush)(void *,uint32_t fifo_word);
typedef void (*ZRPixelSwap)(void *);
typedef void (*ZRPixelReg)(void *,unsigned reg,uint32_t value);
void zr_set_pixel_io(ZRBoard *b,void *user,ZRPixelPush push,ZRPixelSwap swap,ZRPixelReg reg_write);
unsigned zr_sharc_irq(const ZRBoard *b);
void zr_sharc_ack_irq(ZRBoard *b, unsigned lines);
unsigned zr_sharc_flags(const ZRBoard *b);
void zr_sharc_wait_for_host_write(ZRBoard *b);
bool zr_sharc_waiting_for_host(const ZRBoard *b);
bool zr_sharc_in_reset(const ZRBoard *b);
bool zr_3d_enabled(const ZRBoard *b);
/* Optional I/O implementers return false for unattached/unsupported accesses. */
typedef bool (*ZRVideoRead)(void *, uint32_t, unsigned, uint32_t *);
typedef bool (*ZRVideoWrite)(void *, uint32_t, unsigned, uint32_t);
void zr_set_video_io(ZRBoard *b, void *user, ZRVideoRead read, ZRVideoWrite write);
#ifdef __cplusplus
}
#endif
#endif

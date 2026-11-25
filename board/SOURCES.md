# Midnight Run ZR107 board

The board module connects native translated code to the game's big-endian bus. The ROM inputs are supplied by the player. The main program, sound program, texture chips, samples, and tile data are external.

## Hardware references

- The CPU maps, cabinet controls, system pins, 60 Hz vblank, device clocks, tile offsets, and sound-chip routing follow [MAME's ZR107 driver](https://github.com/mamedev/mame/blob/master/src/mame/konami/zr107.cpp).
- The two CG RAM banks, flag and mail interfaces follow [Konami PowerPC hardware](https://github.com/mamedev/mame/blob/master/src/mame/konami/konppc.cpp).
- The 3D implementation uses the FIFO packet layout, pixel RAM, lighting, depth and register map documented by [K001005](https://github.com/mamedev/mame/blob/master/src/mame/konami/k001005.cpp) and the palette and texture interfaces documented by [K001006](https://github.com/mamedev/mame/blob/master/src/mame/konami/k001006.cpp).
- For register layout, interpretation of the tile ROM, page selection, scrolling, tile attributes and RGB composition see [K056832](https://github.com/mamedev/mame/blob/master/src/mame/konami/k054156_k054157_k056832.cpp).
- The sound implementation follows [Olivier Galibert's K054539 device](https://github.com/mamedev/mame/blob/master/src/devices/sound/k054539.cpp) and [the K056800 interface](https://github.com/mamedev/mame/blob/master/src/devices/sound/k056800.cpp).
- Serial cabinet inputs follow [ADC0838](https://github.com/mamedev/mame/blob/master/src/devices/machine/adc083x.cpp). [K056230](https://github.com/mamedev/mame/blob/master/src/devices/machine/k056230.cpp) documents network registers. The implementation supports a local cabinet.

## Integration

Compile `zr107.c` once. The C99 headers can be used from C and C++. Load program and asset ROMs before starting the processors. Video is RGBA represented as native-endian 0xffRRGGBB integers. Clear a 384-line target, draw tile layer 1, then geometry, then tile layer 0. At a width of 683 the original 512 pixels are centered at x=85. Native geometry must handle camera projection and culling separately. The tile renderer can reveal additional columns of a multi-page authored background. Single-page layouts use a transparent side area to avoid repeating text.

At the native 48 kHz chip rate, each stereo audio frame advances timers and the two eight-channel sample engines. Run translated audio code in small interleaved slices and sample `zr_audio_irq_level`. The 8 MHz audio processor uses `zr_audio_read` and `zr_audio_write` on its 24-bit bus. Save the 128 EEPROM bytes with `zr_eeprom_dump` and restore them using `zr_eeprom_load`.

## Checks

- `board_check.c` checks the actual reset word and bus aliases, VRAM banks, CG mail, vblank, serial wheel and pedal values, EEPROM write/readback and sound command handshakes.
- `audio_check.c` checks the original M68K vectors and assertions for real sample bytes in both K054539 chips, stereo output and IRQ2. This exercises the devices directly and does not establish in-game music playback.
- `tile_dump.c` combines the owner's tile ROM with a snapshot written by translated PowerPC code. It is a diagnostic that compiles standalone. At frame 600 of the initial PowerPC-only boot the real system ROM drew a legible self-test identifying European v1.11. Its native 512 x 384 raster and the central crop of the 683 x 384 target were pixel-identical. That frame has black side areas and several remaining board self-test errors. It does not validate race rendering.

BSD-3-Clause MAME references above identify their individual copyright holders and licenses in their source headers.

### Race renderer validation

In MAME 0.289, run the European v1.11 game with `-autoboot_script capture_raster.lua` and point `MIDNRUN_VERIFY` at an existing, private directory. The script applies a repeatable coin/start sequence and records palette, registers, FIFO and tile state at video frame 2800. Set `MIDNRUN_WIDE=1` to repeat the capture with the v1.11 viewport patches applied. The reference is a development tool on the PC.

Compile `raster_replay.cpp` with `zr_raster.cpp`, `zr107.c`, `../video/zr107_video.cpp` and `../video/texel_bus.c`. Supply the player's extracted local asset directory, private capture directory, its `registers.bin`, and width 512 or 683. The replay composes the frame with the same native rasterizer used by the port.

At 512 × 384, the replay matched the reference screenshot within 10 RGB levels on all channels for 99.985% of pixels. Mean absolute RGB error was 0.552 on a 0–255 scale. The measured interval contained 49,774 FIFO words across five geometry swaps. At 683 × 384, the enlarged frustum issued 50,387 words and rasterized 5,771 triangles across five swaps. The final frame drew 19,102 geometry pixels to the left of the original viewport and 19,355 to its right. The HUD stays centered. If a background panorama ends inside a side margin, its painted edge continues with a reflection. These figures describe one start-line scene. The five swaps over ten frame intervals are relevant when distinguishing gameplay updates from display refresh.

The MAME-derived references and arithmetic are attributed to Ville Linde, David Haywood, Olivier Galibert, Fabio Priuli and smf under the following terms.

> Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
>
> 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
> 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
> 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
>
> THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

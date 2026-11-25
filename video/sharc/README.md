# Native SHARC firmware build for Midnight Run EAA 1.11

From the project root, after preparing the private program from the fourteen verified user-supplied ROM chips, run:

```sh
python video/sharc/prepare.py video/sharc/upstream video/sharc/build --program private/assets/program.bin
```

The generator checks the SHA-256 of the unmodified interleaved PowerPC ROM, expands the two embedded LZSS images, and compiles one distinct C++ body per 48-bit instruction value. The diagnostic firmware contains 488 instructions. The graphics firmware contains 18,723, loaded at word addresses 0x20040 through 0x24962. Together they use 3,614 distinct instruction values. Each live program address binds to its compiled body when the firmware is loaded or replaced. An unexpected executable word stops the runner and reports its address.

The host links `dsp.cpp` and its private generated includes using C++20. `dsp.h` exposes the C ABI for board reset, execution and diagnostics. Set `MIDNIGHT_DSP_TRACE=1` for bounded logging of the DSP's communication registers and graphics queue positions.

The seven files in `upstream/` provide the SHARC arithmetic, instruction behavior, register definitions and interrupt and DMA logic under the BSD 3-Clause license. Their hashes are checked by `prepare.py`. MAME revision [835eb54f4b](https://github.com/mamedev/mame/tree/835eb54f4bec1aebc6ffffc7599dd84b81989449/src/devices/cpu/sharc) is the source. The copyright notice is in [LICENSE.mame](LICENSE.mame).

The local build directory contains constants derived from the ROM. Distribute the generator, source and license. Each player generates the build inputs from their own game copy.

The Windows native headless test ran 3,200 vertical blanks, including coin, start and acceleration input. The new ROM-derived catalog produced exactly the same compact trace of CPU positions, framebuffer swaps and triangle counts as the independently captured firmware catalog at all 27 sampled frames. At frame 3,120 the renderer had received 760,198 triangles and completed 1,228 swaps. The original arcade reference emitted 150 new race polygon lists over 300 continuous 60 Hz vertical blanks, one every second blank.

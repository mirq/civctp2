# Port overview

## Targets

| | 68060 | WarpOS (PowerPC) | MorphOS (PowerPC) |
|---|---|---|---|
| makefile | `Makefile.amiga68k` | `Makefile.warpos` | `Makefile.morphos` |
| output | `bin/amiga/ctp2.060` | `bin/amiga/ctp2.wos` | `bin/amiga/ctp2.mos` |
| compiler | Bebbo `m68k-amigaos-gcc` 6.5 + libnix (`/opt/amiga`) | `ppc-amigaos-gcc` 8.4 + MOS2WOS SDK | `ppc-morphos-gcc` 11.3 + libnix (`/gg`) |
| executable format | hunk file, written by the compiler | ELF converted by `ctp2_code/os/amiga/warpos/elf2warpos` (a host tool) | MorphOS ELF |
| define | `CTP2_AMIGA_060` | `CTP2_AMIGA_WOS` | `CTP2_AMIGA_MOS` |

All three define `CTP2_AMIGA_BUILD` and `WORDS_BIGENDIAN`, and select the
Amiga UI backend with `__AUI_USE_AMIGA__` (`ui/aui_common/auicfg.h`).
`CTP2_PORTABLE_BUILD` (`os/include/ctp2_config.h`) means "not the original
Windows build". Use it, not `USE_SDL`, to guard CD, MCI, Miles and GDI code.

Reference hardware: an A4000 with a 68060, 128 MB or more of fast RAM, and a
Radeon on Picasso96 behind a Zorro III / Prometheus PCI bridge. For WarpOS, a
Sonnet MPC7410 or a CyberStorm PPC. Milestone 1 is a single-player game with
AHI sound effects, without movies, music or multiplayer.

## Building

Both makefiles first generate the database classes and the SLIC/LDL parsers
on the host (`ctp2_code/os/amiga/gen.mk`). This needs **byacc**: bison rejects
the SLIC grammars.

```sh
make -f Makefile.amiga68k -j$(nproc)
make -f Makefile.warpos  -j$(nproc)
make -f Makefile.morphos -j$(nproc)    # CPU=7450 for a G4-only build
```

The full links with symbols stay in `build/amiga68k/ctp2.debug`,
`build/warpos/ctp2.debug.elf` and `build/morphos/ctp2.debug`. Use them for
`addr2line`/`nm` on crash addresses and on the caller offsets the perf log
prints.

Logging build (68060):

```sh
make -f Makefile.amiga68k OBJDIR=build/amiga68k-log TARGET=bin/amiga/ctp2log.060 \
     EXTRA_CXXFLAGS=-DUSE_LOGGING EXTRA_CFLAGS=-DUSE_LOGGING
```

Shared make fragments: `ctp2_code/os/amiga/{gen,sources,common}.mk`.

## Running

The directory layout is the same as the Linux build's. The program sits in
`ctp2_code/ctp/` and the game data from the CD or GOG is merged into
`ctp2_data/` and `Scenarios/` two levels up. `civpaths.txt` gives the data
path as `..\..\ctp2_data`, which becomes `//ctp2_data` in AmigaDOS.

Requirements: AmigaOS 3.x, `cybergraphics.library` V41+, a 15/16-bit RTG mode
at least as large as the game's resolution, `ahi.device` V4 (optional), and
`powerpc.library` for WarpOS. MorphOS needs CGX V50 and AHI. Give the shell a
large stack (`stack 2000000`); the 68k build also switches to a 2 MB stack of
its own through libnix `__stack`.

The program changes into its own drawer (`PROGDIR:`). The log is
`PROGDIR:ctp2.log`, flushed every 2 s and at exit. CTRL-C quits. CTRL-F
writes a snapshot (see [testing.md](testing.md)).

### ENV: settings

libnix `getenv()` does not read `ENV:`. The code uses `amiga_env_value` /
`amiga_env_flag` (`GetVar`) instead.

| variable | effect |
|---|---|
| `CTP2_MODE` | screen mode ID in hex. Without it, CyberGraphX's best 16-bit mode is used, then the smallest 15/16-bit mode that fits |
| `CTP2_AHI_RATE` | AHI mixing rate, 8000–48000 (default 16000 on the 68060, 22050 on PPC) |
| `CTP2_LANGUAGE` | ISO 639-1 language code. Without it, the Locale preferences are used |
| `CTP2_UIPRELOAD` | when the main windows are built: `idle` (default: one at a time on the map after 1.5 s without input, a short pause each), `lazy` (on first open), `start` (while a game loads, ~19 s more) |
| `CTP2_SCRUB` | full rows re-sent to the card every 80 ms, past the diff (default 4, `0` = off) |
| `CTP2_SCROLLSLACK` | spare pixels across round the map surface, so that a scroll moves no memory (default 1024, rows are half; about 11 MB at 1024×768; `0` = off) |
| `CTP2_SPRITECACHE` | unit sprite groups kept fully loaded after their animations, least recently used out first (default 8; `0` = off: each move reads the unit's sprite file before and after its animation) |
| `CTP2_EFFECTCACHE` | effect sprite groups (combat flash, special effects; some files are 0.7–2 MB) kept loaded after use (default 4; `0` = off) |
| `CTP2_DIRECTMAP` | `0`: the map is copied into the composed frame first, as before (default: the card gets it straight from the map's surface) |
| `CTP2_DIRECTWIN` | `0`: opaque windows are copied into the composed frame first, as before (default: where no window covers them the card gets them straight from their own surfaces; shaped and see-through windows always go through the frame) |
| `CTP2_MOVEBLIT` | `0`: a moved window is sent to the card again whole (default: the card's blitter carries its pixels to the new place, only what differs is written, about the strip it left) |
| `CTP2_CARDSCROLL` | `0`: no shifts on the card's blitter; a scrolled map is sent again instead (for displays whose blitter gets overlapping copies wrong, such as QEMU's ati-vga under MorphOS) |
| `CTP2_STRIPWRITE` | `1`: what a scroll step uncovers goes to the card without the compare (default off; worth it only where writing a pixel costs little more than comparing one) |
| `CTP2_BENCH` | `1`: a benchmark that runs by itself on a fixed world and quits; times as `Bench:` lines in `ctp2.log` (see [testing.md](testing.md)). `win`: the same for windows (three managers opened, moved and closed) |
| `CTP2_SEED` | the seed of the world, so that the quick start makes the same one every time |
| `CTP2_CHECK` | n: every n-th copy the card is read back and compared with the picture, and each scroll's result with a fresh repaint; differences go to `ctp2.log` |
| `CTP2_SOLIDROWS` | `0`: tiles are painted pixel by pixel as before (default: rows without a transition are copied whole) |
| `CTP2_TEST` | `1`: F9 explores the whole map, for display checks |
| `CTP2_PERF` | `1`: per-phase frame timings, copy statistics, slow events, the "Big present" callers and the cursor wait, every 3 s in `ctp2.log` |
| `CTP2_LOGSYNC` | writes `ctp2.log` line by line, for hangs and for load-step timing |

## Source map

| path | role |
|---|---|
| `ctp2_code/os/amiga/amiga_platform.h` | the only interface the game sees. AmigaOS headers are included only inside `os/amiga/*.c`, through `amiga_os.h` (`#pragma pack(2)`) |
| `amiga_platform.c` | libraries, PROGDIR:, CTRL-C/F, language, requesters, ENV: |
| `amiga_video.c` | mode choice, screen and backdrop window, the shadow diff, card-side scrolls, pixel-format conversion ([graphics-pipeline.md](graphics-pipeline.md)) |
| `amiga_input.c` | IDCMP. The port is read only when its signal is set. Mouse peeking for the busy cursor |
| `amiga_audio.c`, `amiga_mixer.cpp` | AHI ring buffer, and the subset of SDL_mixer the sound code uses |
| `amiga_clock.c` | timer.device E-clock (68k), `GetSysTimePPC` (WarpOS), and the WarpOS "waker" task |
| `amiga_malloc.c` | allocator for the 68k and WarpOS builds (libnix's allocator walks MemHeaders linearly; this one is 10–30× faster) |
| `amiga_posix.c` | AmigaDOS paths (`CI_FixName`), `stat`/`opendir` |
| `amiga_dl.c` | the map generators, linked in and looked up by name (`include/dlfcn.h`) |
| `anet_stub.c` | no multiplayer: every ActiveNet call fails |
| `ctp2_code/ui/aui_amiga/` | UI backend: surfaces, frame copy, software cursor, mouse, keyboard |
| `ctp2_code/ui/interface/amiga_uipreload.cpp` | `CTP2_UIPRELOAD` |
| `ctp2_code/libs/freetype-1.3.1/lib/arch/amigaos/` | FreeType file streams read from memory (`ttmmap.c`). stdio seeks cost ~7 s at startup |

## Byte order

Game files are little-endian and are byte-swapped on load: sprites
(`spritefile.cpp`), the tile set (`tileset.cpp`, `BaseTile.cpp`, the quick-load
path), RIM images (`ProjectFile::getData`), TGA (`targautils.cpp`), ZFS
archives (`prjfile.cpp`), map files, network messages (`net_util.h`), and the
typed values in save games (`civarchive.h`). Use the helpers in
`os/include/ctp2_byteorder.h` (no-ops on x86). In memory the pixels are
**native** big-endian R5G6B5 words. See [graphics-pipeline.md](graphics-pipeline.md).

On the 68k, 32-bit struct fields are 2-byte aligned, so do not rely on PC
struct layouts when reading files.

## Rules for shared code

- The Linux SDL build must not change. Add `#elif defined(__AUI_USE_AMIGA__)` /
  `CTP2_AMIGA_BUILD` branches; do not edit the existing ones.
- The Amiga builds use `-fno-exceptions`: guard any new throw/catch with
  `__EXCEPTIONS`.
- Many sources are ISO-8859-1 with CRLF line endings. Keep both.
- Skip static destructors on the 68k: `Network::~Network` hung at exit.
  `amiga_platform_exit` runs from libnix's exit list.

## Building with gcc 16

The 68060 build also works with m68k-amigaos-gcc 16.2 (7–15% faster
scrolling, about 17% faster game load on the benchmark). gcc 6.5 stays the
default. What it needs:

```sh
make -f Makefile.amiga68k -j$(nproc) PREFIX=<gcc16 prefix> \
  OBJDIR=build/amiga68k-gcc16 TARGET=bin/amiga/ctp2gcc16.060 \
  OPT_FLAGS="-O2 -fomit-frame-pointer" \
  EXTRA_CFLAGS="-idirafter $PWD/build/gcc16-include -DZ_HAVE_UNISTD_H" \
  EXTRA_CXXFLAGS="-idirafter $PWD/build/gcc16-include"
```

- Its NDK has no AHI headers: copy `devices/ahi.h`, `clib/ahi_protos.h` and
  `defines/ahi.h` from the gcc 6.5 toolchain's `ndk-include` into
  `build/gcc16-include` (same sub-folders).
- zlib's `gz*.c` call `read`/`write`/`lseek`/`close` without a prototype,
  an error since gcc 14: `-DZ_HAVE_UNISTD_H`.
- `int32_t` is a `long` there, so the game's `sint32`/`uint32` are plain
  `int` on Amiga builds (`os/include/ctp2_inttypes.h`).
- The gcc 6.5 workaround flags (`-fno-peephole2 -fbbb=-`) are left out.

## WarpOS specifics

- `elf2warpos` is our C port of OpenSWOS's `elf2warpos.py`. Its stub runs in
  place when there is no `MEMF_PPC` memory (CyberStorm PPC, Blizzard PPC).
  The original stub needs a Sonnet.
- A "waker" task at low priority calls `WaitTime(2 ms)` in a loop
  (`amiga_clock.c`). Without it, while WarpOS naps the PPC, a reply from the
  68k could wake the waiting task only ~59 s later.
- In Amiberry, `uaegfx.card` redraws only memory that the 68k wrote. See
  "uaegfx" in [graphics-pipeline.md](graphics-pipeline.md).

## MorphOS specifics

- libnix `unistd.h` / `sys/param.h` pull in exec headers whose
  `BYTE`/`WORD`/`LONG` clash with `os/nowin32/windows.h`. `amiga_prelude.h`
  sets their include guards.
- The AHI hook is PowerPC code behind an `EmulLibEntry` (`TRAP_LIB`) gate.
- The SDK compiler gives OS structures the 68k layout itself.

## Not done yet

- Save games store some objects as raw memory, so Amiga saves and PC saves
  are not interchangeable. Scenarios that ship as save games will not load.
- No movies, music, multiplayer or TIFF.
- The sprite editor writes sprites in native byte order. The slow tile-set
  load path is not byte-order aware.
- The clipboard is internal to the game.
- WarpOS and MorphOS builds have not been tested on real hardware.

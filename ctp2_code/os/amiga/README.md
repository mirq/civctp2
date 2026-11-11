# Call to Power II on AmigaOS: 68060, WarpOS and MorphOS

Three native builds, two for an Amiga with an RTG card (Picasso96 or
CyberGraphX) and one for MorphOS, sharing one platform layer (this directory
and `ctp2_code/ui/aui_amiga`). No SDL.

| | 68060 | WarpOS (PowerPC) | MorphOS (PowerPC) |
|---|---|---|---|
| makefile | `Makefile.amiga68k` | `Makefile.warpos` | `Makefile.morphos` |
| output | `bin/amiga/ctp2.060` | `bin/amiga/ctp2.wos` | `bin/amiga/ctp2.mos` |
| compiler | Bebbo's `m68k-amigaos-gcc` 6.5 + libnix (`/opt/amiga`) | `ppc-amigaos-gcc` 8.4 + the MOS2WOS SDK (`MGL_DIR`'s `Makefile.wos.gcc.common`) | the MorphOS SDK's `ppc-morphos-gcc` 11.3 + libnix (`/gg`, `MOS_PREFIX`) |
| conversion | the compiler writes the hunk file | `warpos/elf2warpos` (C, built for the host) | the compiler writes the MorphOS ELF |
| define | `CTP2_AMIGA_060` | `CTP2_AMIGA_WOS` | `CTP2_AMIGA_MOS` |

All define `CTP2_AMIGA_BUILD` and `WORDS_BIGENDIAN`, and select the Amiga UI
backend with `__AUI_USE_AMIGA__` (`ui/aui_common/auicfg.h`).

## Building

The database classes and the SLIC/LDL parsers are generated on Linux first
(`os/amiga/gen.mk`, run by either makefile). That needs **byacc** (bison
rejects the SLIC grammars). Without a system byacc:

    mkdir -p build/host-tools && cd build/host-tools && apt-get download byacc && dpkg-deb -x byacc_*.deb root

Then:

    make -f Makefile.amiga68k -j$(nproc)
    make -f Makefile.warpos  -j$(nproc)
    make -f Makefile.morphos -j$(nproc)    # CPU=7450 for a G4-only build (default 750)

The full links with symbols stay in `build/amiga68k/ctp2.debug`,
`build/warpos/ctp2.debug.elf` and `build/morphos/ctp2.debug`, for `addr2line`
against a crash address.

## Running

The same tree as the Linux build: the program in `ctp2_code/ctp/`, the game's
data (from the CD or GOG, merged into the repository's `ctp2_data` and
`Scenarios`) two levels up. `civpaths.txt` beside the program says so
(`..\..\ctp2_data`, which becomes `//ctp2_data` for AmigaDOS).

    CTP2/ctp2_code/ctp/ctp2.060   (or ctp2.wos)
    CTP2/ctp2_code/ctp/civpaths.txt, userprofile.txt, ...
    CTP2/ctp2_data/...
    CTP2/Scenarios/...

Needs: AmigaOS 3.x, `cybergraphics.library` V41+, a 15 or 16-bit RTG mode of at
least the game's resolution, `ahi.device` V4 for sound (optional), and for
WarpOS `powerpc.library`. 128 MB of fast RAM is the target. MorphOS (any
version with CGX V50 and AHI) runs `ctp2.mos` in a 16-bit screen of its own.

The program enters its own drawer (`PROGDIR:`) and keeps everything there;
the log is `PROGDIR:ctp2.log`, written every 2 seconds and at exit.
CTRL-C quits.

| ENV: variable | |
|---|---|
| `CTP2_MODE` | screen mode id in hex (from the screen mode prefs); else the closest 16-bit RTG mode |
| `CTP2_AHI_RATE` | AHI mixing rate, 8000-48000 (default 16000 on the 68060, 22050 on the PowerPC) |
| `CTP2_LANGUAGE` | ISO 639-1 code (`en`, `de`, ...); else the Locale preferences |

## How it is put together

| file | |
|---|---|
| `amiga_platform.h` | the one interface the game sees; AmigaOS headers stay in this directory (`amiga_os.h`, under `#pragma pack(2)`) |
| `amiga_platform.c` | libraries, PROGDIR:, CTRL-C, language, requesters |
| `amiga_video.c` | mode choice, screen, backdrop window, the frame copy (converts to the card's 16-bit layout) |
| `amiga_input.c` | IDCMP; the port is drained only when it holds messages |
| `amiga_audio.c`, `amiga_mixer.cpp` | the AHI ring and the SDL_mixer subset the sound code uses (`sound/soundmixer.h`) |
| `amiga_clock.c` | timer.device E-clock (68060), `GetSysTimePPC` (WarpOS) |
| `amiga_posix.c` | AmigaDOS paths (`CI_FixName`), `stat`/`opendir` on dos.library |
| `amiga_dl.c` | the map generators, linked in, found by name (`include/dlfcn.h`) |
| `anet_stub.c` | no multiplayer: every ActiveNet call fails |
| `../../ui/aui_amiga` | the UI backend: surfaces in fast RAM, dirty-rectangle frame copy, software cursor, keys |

Only what changed crosses the bus: the 68060 writes a Zorro III card at about
5 MB/s, so a whole 800x600 frame would take ~180 ms.

## Byte order

The game's files are little-endian. Swapped on load: sprites
(`gfx/spritesys/spritefile.cpp`), the tile set (`tileset.cpp`, `BaseTile.cpp`,
the quick-load path), RIM images (`ProjectFile::getData`), TGA images
(`targautils.cpp`), ZFS archives (`prjfile.cpp`), map files and network
messages (`net/io/net_util.h`), the typed values of save games
(`civarchive.h`). Helpers: `os/include/ctp2_byteorder.h`.

## MorphOS

The same code as the Amiga builds, compiled natively for MorphOS's PowerPC;
`CTP2_AMIGA_MOS` covers what differs:

- MorphOS's libnix `unistd.h` and `sys/param.h` include exec and bsdsocket
  headers, whose `BYTE`/`WORD`/`LONG` clash with `os/nowin32/windows.h`:
  `amiga_prelude.h` sets their include guards for the game, `amiga_os.h`
  clears exec's again for the platform layer.
- The AHI channel-info hook is PowerPC code behind an `EmulLibEntry`
  (`TRAP_LIB`) gate, not the 68k opcodes the other builds hand AHI.
- `LocaleBase` and `TimerBase` are `struct Library *` in the SDK's protos.
- The OS structures get the 68k layout from the SDK compiler itself
  (`struct Message` is 20 bytes); the `pack(2)` around the headers is harmless.

## Not done yet

- Not run on hardware yet (none of the three builds).
- Save games store some objects as raw memory: Amiga saves load on the Amiga,
  not on a PC, and the reverse. Scenarios that ship as save games will not load.
- No movies, no music, no multiplayer, no TIFF images (only for modding).
- The sprite editor writes sprites in the machine's byte order; the tile set's
  slow load path (`TileSet::Load*`) is not byte-order aware (the game uses the
  quick one).
- The text fields' clipboard is inside the game, not the Amiga clipboard.

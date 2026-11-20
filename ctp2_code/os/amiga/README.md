# Call to Power II on AmigaOS: 68060, WarpOS, MorphOS and AROS

Four native builds, two for an Amiga with an RTG card (Picasso96 or
CyberGraphX), one for MorphOS and one for AROS x86_64, sharing one platform
layer (this directory and `ctp2_code/ui/aui_amiga`). No SDL.

| | 68060 | WarpOS (PowerPC) | MorphOS (PowerPC) | AROS x86_64 (ABIv11) |
|---|---|---|---|---|
| makefile | `Makefile.amiga68k` | `Makefile.warpos` | `Makefile.morphos` | `Makefile.aros` |
| output | `bin/amiga/ctp2.060` | `bin/amiga/ctp2.wos` | `bin/amiga/ctp2.mos` | `bin/amiga/ctp2.aros` |
| compiler | Bebbo's `m68k-amigaos-gcc` 6.5 + libnix (`/opt/amiga`) | `ppc-amigaos-gcc` 8.4 + the MOS2WOS SDK (`warpos/Makefile.wos.gcc.common`) | the MorphOS SDK's `ppc-morphos-gcc` 11.3 + libnix (`/gg`, `MOS_PREFIX`) | deadwood2's `x86_64-aros-gcc` (`toolchain-core-x86_64`, `AROS_TOOLCHAIN`) + the `Development` drawer of an ABIv11 system (`AROS_SYSROOT`) |
| conversion | the compiler writes the hunk file | `warpos/elf2warpos` (C, built for the host) | the compiler writes the MorphOS ELF | the compiler writes the relocatable ELF |
| define | `CTP2_AMIGA_060` | `CTP2_AMIGA_WOS` | `CTP2_AMIGA_MOS` | `CTP2_AMIGA_AROS` |

All define `CTP2_AMIGA_BUILD` and select the Amiga UI backend with
`__AUI_USE_AMIGA__` (`ui/aui_common/auicfg.h`). `WORDS_BIGENDIAN` follows the
CPU (`config.h`): all but AROS are big-endian.

## Building

The database classes and the SLIC/LDL parsers are generated on Linux first
(`os/amiga/gen.mk`, run by either makefile). That needs **byacc** (bison
rejects the SLIC grammars). Without a system byacc:

    mkdir -p build/host-tools && cd build/host-tools && apt-get download byacc && dpkg-deb -x byacc_*.deb root

Then:

    make -f Makefile.amiga68k -j$(nproc)
    make -f Makefile.warpos  -j$(nproc)
    make -f Makefile.morphos -j$(nproc)    # CPU=7450 for a G4-only build (default 750)
    make -f Makefile.aros    -j$(nproc)    # AROS_TOOLCHAIN=..., AROS_SYSROOT=... (see AROS below)

The full links with symbols stay in `build/amiga68k/ctp2.debug`,
`build/warpos/ctp2.debug.elf`, `build/morphos/ctp2.debug` and
`build/aros-x86_64/ctp2.debug`, for `addr2line` against a crash address.

## Launcher

`bin/amiga/CTP2` (`launcher/launcher.c`, built by `make -f Makefile.amiga68k`
for the 68020, so it runs on every Amiga and on MorphOS) is what a player
double-clicks: the release puts it at the top of the `CTP2` drawer with an
icon (`tools/ci/make-icon.py`). A GadTools window chooses the program (the
ones present; WarpOS when `LIBS:powerpc.library` is there, MorphOS on
MorphOS), `C:InitPPC` before `ctp2.wos`, the screen mode (the card's 16-bit
RTG modes of 800x600 and more, or automatic; cursor up/down move through
them), autosave, and the ENV: settings below a player may want (language,
sound rate, background autosave, save compression, window building, sprite
cache, card refresh rows, card settle time, windows straight to the card,
scrolling on the card, performance log). Play, Save
(keys P, S, Q) write:

- ENV:CTP2_MODE and the others; a setting at its default is deleted, so the
  game's own default applies;
- `AutoSave` and, with a chosen mode, `ScreenResWidth`/`ScreenResHeight` in
  `userprofile.txt` (the mode's size is the game's resolution); every other
  line is kept;
- its own choices in `ctp2_code/ctp/launcher.prefs`.

Play then runs the program from its drawer with `nointromovie`, its output
in `ctp2out.txt`, and ends when the game does.

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
WarpOS `powerpc.library`. 128 MB of fast RAM is the target. On a Sonnet card
(SonnetAmiga `powerpc.library`) run `C:InitPPC` once before `ctp2.wos`: without
it the program can hang in its loader or freeze the machine. MorphOS (any
version with CGX V50 and AHI) runs `ctp2.mos` in a 16-bit screen of its own.

The program enters its own drawer (`PROGDIR:`) and keeps everything there;
the log is `PROGDIR:ctp2.log`, written every 2 seconds and at exit.
CTRL-C quits.

| ENV: variable | |
|---|---|
| `CTP2_MODE` | screen mode id in hex (from the screen mode prefs); else the closest 16-bit RTG mode |
| `CTP2_AHI_RATE` | AHI mixing rate, 8000-48000 (default 16000 on the 68060, 22050 on the PowerPC) |
| `CTP2_LANGUAGE` | ISO 639-1 code (`en`, `de`, ...); else the Locale preferences |
| `CTP2_UIPRELOAD` | when the main windows (build manager, city window, managers, diplomacy...) are built: `idle` (default; or `lazypreload`) one at a time on the map while the mouse and keys rest, `lazy` at their first opening, `start` (or `preload`) while a game loads (a few seconds longer) |
| `CTP2_SCRUB` | full rows sent again to the card every 80 ms, past the comparison (default 4, `0`: none) |
| `CTP2_SCROLLSLACK` | spare pixels across round the map's surface, so that a scroll moves no memory (default 1024, about 11 MB at 1024x768; `0`: none) |
| `CTP2_SPRITECACHE` | unit sprite groups kept fully loaded after their animations, least recently used out first (default 8; `0`: none, every move reads the sprite file twice) |
| `CTP2_BGSAVE` | `0`: the turn's autosave is compressed and written at once, as before (default: in the background, a few ms a frame; finished before any other save, load or quit) |
| `CTP2_SAVELEVEL` | zlib level of saves and autosaves, 0 (stored, larger file) to 9 (default 1) |
| `CTP2_PROF` | `1` (68060): a sampling profiler (`2`: follow frame pointers; `CTP2_PROF_EVENT`=name: only that game event); CTRL-E (`break <process> E`) writes `RAM:ctp2prof.bin`, resolved with `host/amiga-prof.py` (see docs/performance.md) |
| `CTP2_EFFECTCACHE` | effect sprite groups (combat flash, special effects) kept loaded after use (default 4; `0`: none, each effect reads its file again) |
| `CTP2_DIRECTMAP` | `0`: the map goes through the composed frame, as before (default: straight from its own surface to the card) |
| `CTP2_DIRECTWIN` | `0`: opaque windows go through the composed frame, as before (default: straight from their own surfaces to the card) |
| `CTP2_MOVEBLIT` | `0`: a moved window is sent again whole (default: the card's blitter moves it, only the uncovered strip is written) |
| `CTP2_CARDSCROLL` | `0`: no shifts on the card's blitter; a scrolled map is sent again instead (for displays whose blitter gets overlapping copies wrong, such as QEMU's ati-vga under MorphOS) |
| `CTP2_STRIPWRITE` | `1`: what a scroll step uncovers goes to the card without the compare (default off) |
| `CTP2_BENCH` | `1`: a benchmark that runs by itself on a fixed world (`CTP2_SEED`) and quits; `Bench:` lines in `ctp2.log` |
| `CTP2_CHECK` | n: every n-th copy the card is compared with the picture, each scroll with a fresh repaint; `Check:` lines |
| `CTP2_SOLIDROWS` | `0`: tiles pixel by pixel, as before |
| `CTP2_TEST` | `1`: F9 explores the whole map (display checks) |
| `CTP2_CARDSETTLE` | WarpOS: ms the PowerPC waits after opening the screen before it writes the card's memory (default 500; the card clears the screen itself first). The first writes right after the mode switch froze an A4000 with a Sonnet behind the PCI bridge |
| `CTP2_PPCCARD` | WarpOS, `0`: the PowerPC never touches the card's memory, every copy goes through WritePixelArray on the 68k (slow; a fallback for boards where direct writes fail) |
| `CTP2_STEPDELAY` | WarpOS, `1`: each step of opening the screen logged, with 3 s between them (to find a freeze) |
| `CTP2_DTORS` | `1`: the game's C++ static destructors run at exit (default: skipped; one of them, `Network::~Network`, never ended on real hardware) |
| `CTP2_PERF` | `1`: timings of the frame's phases, the copies, scrolling, resource loads and slow game events every 3 s in `ctp2.log` |
| `CTP2_LOGSYNC` | `1`: `ctp2.log` written line by line (default: every 2 s), for hangs and load timings |
| `CTP2_LOGTO` | a DOS path that gets every log line at once and outlives a reset: a serial console (`TFCON:`) or the network (`TCP:<pc>/<port>`, a listener on the PC such as `nc -l <port>`). A file written just before a reset often never reaches the disk |
| `CTP2_LOGDELAY` | ms of pause after each line sent to `CTP2_LOGTO`, so that it leaves the machine before a reset (50 on a PiStorm's wifi) |
| `CTP2_TRACE` | `1`: trace lines (`T file:line`) for a machine that resets without a guru: the F4 quick start, the UI passes after it, each step of a new world |

Pictures, fonts, sprites and sounds stay in memory once loaded (until less than 24 MB of fast memory is free), so a window opens faster the second time; 128 MB or more is assumed.

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

Only what changed crosses the bus: Zorro III carries about 15 MB/s, and the
game's own copy (read, byte swap, write) was measured at about 4.6 MB/s on an
A4000, so a whole 800x600 frame takes 64 to 205 ms. More in `docs/`.

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

## AROS

`Makefile.aros` builds for AROS x86_64 ABIv11 (deadwood2/AROS, the 64-bit
line AROS One x86_64 is built on). The compiler is deadwood2's cross
toolchain, made once (about half an hour) with their script:

    git clone --branch ABIv11_2026.09 https://github.com/deadwood2/AROS.git AROS
    cp AROS/scripts/rebuild.sh . && echo 1 | ./rebuild.sh     # toolchain-core-x86_64

and the headers and libraries are the `Development` drawer of an ABIv11
system, for example the Linux-hosted release
(`AROS-202609-linux-x86_64-system.tar.gz`):

    make -f Makefile.aros AROS_TOOLCHAIN=<dir>/toolchain-core-x86_64 \
        AROS_SYSROOT=<system>/Development -j$(nproc)

(The `amigadev/crosstools:x86_64-aros` image is of the other x86_64 line,
whose startup code fails on ABIv11 in `OpenResource` before `main`.)

`CTP2_AMIGA_AROS` covers what differs:

- Little-endian and 64-bit: `WORDS_BIGENDIAN` is off, so the game's files are
  read as they are; a pointer in a tag (`ti_Data`, `LBMI_BASEADDRESS`) is an
  `IPTR` (`amiga_iptr`); the OS headers keep their natural alignment (no
  `pack(2)`).
- The card's 16-bit "PC" byte order is the CPU's own: `host_pixfmt` in
  `amiga_video.c` turns the screen's format round, so the copies (written for
  a big-endian CPU) stay as they are.
- 32-bit screens: AROS's displays often have no 15/16-bit modes (the hosted
  X11 one, several native drivers). When there is no 16-bit mode, a 32-bit
  one is taken and each pixel goes out through a 64K-entry table; the game
  still draws R5G6B5.
- Its libstdc++ is built with threads: `<pthread.h>` brings in exec's types,
  lists and ports, whose `BYTE`, `WORD`, `BOOL`, `Message` and list macros
  clash with the game's. `amiga_prelude.h` includes it first with those names
  renamed.
- No libnix stack cookie: `main` moves to a 2 MB stack of its own
  (`NewStackSwap`) when it was given less.
- The AHI channel-info hook is a native function (`AROS_UFH3`). Sound
  defaults to 44100 Hz.
- The C++ static destructors are skipped at exit, as on the 68060 and
  WarpOS (with them the program never got back to the Shell): their entries
  in the startup's `DTORS` set become empty functions. `ENV:CTP2_DTORS` runs
  them.
- An AROS program is a relocatable ELF linked with `ld -r`, which keeps every
  C++ COMDAT group as sections of its own (41,000); `--force-group-allocation`
  merges them.

Run it from its drawer (`ctp2_code/ctp/ctp2.aros`, double-click or Shell);
there is no launcher (the 68k one does not run on x86). Settings are the
ENV: variables above.

Tested in Linux-hosted AROS ABIv11 2026.09 (X11, 32-bit screen, where
`LockBitMap` fails and the copy goes through `WritePixelArray`): menu, new
game, turns, quit back to the Shell.

## Not done yet

- Not run on hardware yet (none of the three builds).
- Save games store some objects as raw memory: Amiga saves load on the Amiga,
  not on a PC, and the reverse. Scenarios that ship as save games will not load.
  The 68060, WarpOS and MorphOS builds share one format (`SaveLayout.h` gives
  the PowerPC builds the 68k layout of those classes), so a save from one loads
  on the others; WarpOS/MorphOS saves made before that do not load any more.
- No movies, no music, no multiplayer, no TIFF images (only for modding).
- The sprite editor writes sprites in the machine's byte order; the tile set's
  slow load path (`TileSet::Load*`) is not byte-order aware (the game uses the
  quick one).
- The text fields' clipboard is inside the game, not the Amiga clipboard.

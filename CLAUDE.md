# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Apolyton fork of Activision's *Call to Power II* (CTP2) source code: a C++ turn-based strategy game, originally built with MSVC 6, now built with GCC on Linux (autotools, SDL2) and Visual Studio 2017+ on Windows (`ctp2_code/ctp/civctp.sln`, SDL and legacy DirectX configurations). The repo does **not** contain the original game assets; a working game needs files from the CD/GOG copy merged into `ctp2_data/`, `Scenarios/` (see README.md).

## Build (Linux)

Dependencies (Debian): `libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev libtiff-dev libavcodec-dev libavformat-dev libswscale-dev byacc flex`.

```sh
./autogen.sh
CFLAGS="-O3 -fuse-ld=gold" CXXFLAGS="-O3 -fuse-ld=gold" ./configure --enable-silent-rules
make -j$(nproc)
```

- `--enable-debug` → asserts, logging, leak detector (use `-g -O0 -fno-omit-frame-pointer`). `--enable-logging` → logging only. `--enable-ffmpeg4movies` → movie playback (off by default). Other options: `--enable-precisetraderoutecalc`, `--disable-anet`.
- The first parallel `make` can fail because generated sources aren't fully captured in the dependencies; rerun `make`.
- Builds must happen in-tree (include paths assume it; no separate build dir).
- `ctp2_code/meson.build` exists but is stale/incomplete; autotools is the supported Linux build.
- `make` puts the binary at `ctp2_code/ctp/ctp2` and copies the mapgen plugins (`ctp2_code/mapgen/.libs/*.so`) into `ctp2_code/ctp/dll/map/`.

## Run / test

- Run from the program dir: `cd ctp2_code/ctp && ./ctp2` (pass `nointromovie` to skip the intro; `runspriteeditor` starts the sprite editor).
- There are no unit tests. `tests/*.sikuli` are GUI smoke tests (SikuliX image matching under Xvfb) run in GitLab CI via the `Dockerfile` (`.gitlab-ci.yml`); they need the original game data and aren't practical locally.
- AI regression check (from CONTRIBUTING.md): start a new game, save before doing anything, open chat with `'`, then `/attach 1` and `/rnd 200` to let the AI play 200 turns; behaviour shouldn't change unless intended. `/reloadslic` after loading old saves. Other chat cheat/debug commands are in `ChatWindow::CheckForEasterEggs` (`ctp2_code/ui/interface/ChatBox.cpp`).
- Debug/logging builds write logs to `ctp2_code/ctp/logs/`; log flags are set in `main_InitializeLogs` (`ctp2_code/ctp/civ3_main.cpp`) and defined in `ctp2_code/ctp/ctp2_utils/c3debug.h`. Debug builds write leak reports `CTP_LEAKS_99999.TXT` / `CTP_LEAKS_ALT_99999.TXT` next to the executable on exit.

## Architecture

All game code is under `ctp2_code/`; each subdir builds a libtool convenience library linked into `ctp/ctp2` (see `ctp2_code/Makefile.am`).

- `ctp/` — entry point (`civ3_main.cpp`, `civapp.cpp`), the precompiled/common header `c3.h` (every `.cpp` includes it first), debug utilities (`ctp2_utils/`, `debugtools/`), VS project files.
- `gs/` — game state:
  - `gameobj/` — core objects (Player, Unit, Army, City, Agreements…). Objects are lightweight ID handles (`Army`, `Unit`) backed by `*Data` classes stored in global pools (`g_theArmyPool`, etc.).
  - `newdb/` — the game rules database. `*.cdb` files are schemas compiled by the `dbgen` tool (`gs/dbgen/`, a flex/byacc-based generator `ctpdb`) into `*Record.h/.cpp` classes at build time (generated, don't edit them; edit the `.cdb`). At runtime these records parse the text files in `ctp2_data/default/gamedata/*.txt` and `ctp2_data/default/aidata/*.txt`. Adding a field to a record = change the `.cdb` + the data txt files. `gs/database/` holds older hand-written DBs.
  - `slic/` — SLIC, the game's scripting language (lexer/parser `slic.l`/`slic.y`, `SlicEngine`, builtin functions in `slicfunc*.cpp`). Scripts (`*.slc`) live in `ctp2_data/default/gamedata/` and scenario folders.
  - `events/` — `GameEventManager`: most game actions are dispatched as queued events (`GEV_*`) with hooks that SLIC and the AI can intercept.
  - `world/`, `fileio/` (save games, `CivPaths`), `outcom/`, `utility/`.
- `ai/` — computer players: `strategy/` (Agents/Goals/Squads matched by the `scheduler`), `diplomacy/` (`Diplomat`), `CityManagement/governor`, `mapanalysis/`. Tunable behaviour mostly comes from `ctp2_data/default/aidata/` (Goals.txt, strategies, personalities).
- `ui/` — `aui_common` widget toolkit with backends `aui_sdl` / `aui_directx`; `aui_ctp2` + `interface/` hold the game screens; layouts come from `.ldl` files in `ctp2_data/*/uidata` parsed by `ui/ldl`.
- `gfx/` — tile engine (`tilesys`), sprites (`spritesys`), map layers.
- `net/` — multiplayer on top of the bundled `libs/anet`. `robot/` — shared infrastructure despite the name: containers/serialization (`aibackdoor/`: `CivArchive`, dynamic arrays, pools), A* pathing (`pathing/`), utilities. `mapgen/` — map generator plugins built as shared libs. `sound/`.
- `os/nowin32/` — Win32 API shims (`windows.h` replacement) so the Windows-centric code compiles on Linux; `os/autoconf/` holds m4 macros and `Makefile.common`.
- `libs/` — vendored third-party libraries (anet, FFmpeg, freetype 1.3.1, SDL2*, tiff, zlib). Don't modify unless the change is about that library.

`ctp2_data/` is per-language (`default/` plus `english/`, `german/`, …); string tables and localized UI override `default/`. `Advance-Graph/` and `tools/` contain scripts for generating the tech tree graph from game data.

## Conventions (CONTRIBUTING.md)

- Indent with **tabs (4 wide)**; spaces only for alignment. Don't mix whitespace-only reformatting with functional changes — put it in separate commits.
- CamelCase names; variables start lowercase; members `m_`, statics `s_`, globals `g_`; enums ALL_CAPS.
- Files carry a header block with a "Modifications from the original Activision code" list; keep it intact.
- Many source files are ISO-8859-1, not UTF-8 — preserve the encoding (and existing line endings; `.gitattributes` disables auto-conversion) when editing.
- Commit messages: imperative subject line, often prefixed with the area (`AI scheduler: …`, `Combat: …`). Changes go in via PRs; WIP PRs are prefixed `WIP`.

## Amiga builds (branch `amiga-port`)

Two native AmigaOS builds without SDL: `make -f Makefile.amiga68k` (68060, Bebbo's m68k-amigaos-gcc in `/opt/amiga`) and `make -f Makefile.warpos` (PowerPC, ppc-amigaos-gcc + MOS2WOS SDK via `~/MiniGL_WOS_V19_mglQ3/Makefile.wos.gcc.common`, converted by our host tool `ctp2_code/os/amiga/warpos/elf2warpos`, a C port of OpenSWOS's `elf2warpos.py` whose stub also runs on CyberStorm/Blizzard PPC). A third, `make -f Makefile.morphos` (MorphOS, ppc-morphos-gcc 11 + libnix from the MorphOS SDK in `/gg`), reuses the same platform layer. Details and the file map: `ctp2_code/os/amiga/README.md`.
- Shared parts: `ctp2_code/os/amiga/{gen,sources,common}.mk` (generated sources need host byacc), platform layer in `ctp2_code/os/amiga/`, UI backend `ctp2_code/ui/aui_amiga/` selected by `__AUI_USE_AMIGA__`. AmigaOS headers only in `os/amiga/*.c` via `amiga_os.h`.
- `CTP2_AMIGA_BUILD` (all three), `CTP2_AMIGA_060`, `CTP2_AMIGA_WOS`, `CTP2_AMIGA_MOS`. `CTP2_PORTABLE_BUILD` (`os/include/ctp2_config.h`) means "not the original Windows build" (SDL or Amiga): use it instead of `USE_SDL` for CD/MCI/Miles/GDI guards.
- Big-endian: data files are little-endian; read multi-byte fields through `os/include/ctp2_byteorder.h` (no-ops on x86). Struct layout also differs on the 68k (32-bit fields 2-byte aligned).
- No C++ exceptions on Amiga (`-fno-exceptions`): guard any new throw/catch with `__EXCEPTIONS`.
- Emulator testing (amiga-mcp, devbench profile `amiberry-rtg`: Amiberry, 68060 + CyberStorm PPC/WarpOS, RTG): `Dev:` is the host dir `/home/mirek/Amiberry/share/Dev`; the test tree is `Dev:CTP2` (`ctp2_code/ctp/ctp2.060` + `ctp2_data`, `Scenarios`). Run with `cd Dev:CTP2/ctp2_code/ctp`, `stack 2000000`, `run >T:ctp2out.txt ctp2.060 nointromovie`; read `ctp2.log` / `logs/` from the host. Host screenshot: `import -window <id>` (id from `xwininfo -root -tree | grep -i amiberry`). When it hangs or the bridge drops, reboot it yourself with `amiga_emulator_restart`; no need to ask. If the bridge says "Not connected" while the Amiga is fine, `amiga_disconnect` + `amiga_connect` usually brings it back. Its mouse/click injection is unreliable (timeouts, lost button-ups): for clicking through the game, run Amiberry in Xephyr instead — `Xephyr :7 -screen 1040x784 -ac -nolisten tcp &`, then `DISPLAY=:7 /var/lib/flatpak/exports/bin/com.blitterstudio.amiberry -G --config /home/mirek/civctp2/build/amiberry-xephyr.uae` (the user's working config: CSPPC ROM v1.8, 1024x768 window), screenshots with `DISPLAY=:7 import -window root`, input via python-xlib XTEST on `:7` (Amiberry takes relative motion, ~1:1; move, screenshot, correct). The game needs a few minutes to reach the menu (~95 s) and ~60 s more to start a game; `CTP2_LOGSYNC` (setenv) writes ctp2.log line by line, with load-step timings. The original game data (from the user's CD image) is merged into `Dev:CTP2` only, not into the repository. A logging build: `make -f Makefile.amiga68k OBJDIR=build/amiga68k-log TARGET=bin/amiga/ctp2log.060 EXTRA_CXXFLAGS=-DUSE_LOGGING EXTRA_CFLAGS=-DUSE_LOGGING`.
- Shared-code edits must keep the Linux SDL build unchanged: add `#elif defined(__AUI_USE_AMIGA__)` / `CTP2_AMIGA_BUILD` branches rather than altering existing ones. Many sources are ISO-8859-1 with CRLF; preserve both.

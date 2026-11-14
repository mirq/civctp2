# Performance notes

Measured on the reference A4000 (68060, Radeon RTG behind Zorro III/Prometheus,
`PIXFMT_RGB16PC`) unless marked as emulator figures.

## Machine limits

| operation | cost |
|---|---|
| CPU write to card | ~2.3 Mpixels/s, 0.43 µs/px, a full 800×600 frame takes ~205 ms |
| fast RAM read | ~36 MB/s |
| `memcpy` / `memset` in fast RAM | ~17 / ~16 MB/s (every copy method measured 18–27 MB/s; `move16` was no faster) |
| shadow compare | 0.10 µs/px (branchless longword XOR), 0.17 µs/px with early-out |
| card `BltBitMap` scroll | ~0 ms of CPU time |
| full-map copy in RAM | ~50 ms |

Rendering is bounded by memory bandwidth, not by the card's bandwidth.

## What each change bought

| change | effect |
|---|---|
| shadow diff, sending only the rects that changed | baseline for every result below |
| mix dirty grid 128 → 32 px (`TiledMap::InitGrid`) | idle map 15.5 → 24 fps |
| stencil window (control panel) redraws only what changed | ~150 ms → near 0 per status-bar change |
| map layer at 15 fps, UI at 30 fps | per-tile walks (`RepaintLayerSprites`, `DrawCityNames`) at half the rate |
| radar map at 1–4 Hz, fixed-point blit | it was 50 ms per copy, 10×/s |
| idle mouse events 60 → 10/s | each walks all controls, ~6 ms |
| keyboard scroll no longer ends the scroll loop each step | ~180 ms/step of full refresh gone |
| card-side `BltBitMap` scroll of the uncovered band | ~10k pixels per step to the card instead of ~400k |
| compare first, then blit and write back to back | the step appears at once, with no visible top-down wipe |
| shadow per-row pointers with slack | a scroll is a pointer move or rotation |
| frame band shifted in RAM (`amiga_ui_scroll_frame`) | only the new strip is composed and compared |
| one-axis scroll loops, shift only visible rows/cols | the surface shift (60–75 ms) is cut |
| slow steps go 2 tiles across / 4 rows down | ~410 → ~1040 px/s of scroll |
| upward-only copies (`amiga_copy_up`) | backward `memmove` ran at half speed |
| `amiga_malloc.c` instead of libnix malloc | End Turn ~12 → ~6 s (emulator) |
| stdio buffers (`CTP2_AMIGA_STDIO_BUFFER`) | libnix `_IONBF` made one DOS call per byte; an 800×600 TGA took 136 s |
| FreeType in-memory stream (`ttmmap.c`) | ~7 s of seeks at startup |
| `amiga_fullpath` caches the drawer name | fewer DOS calls on path lookups |
| resources kept until < 24 MB free | windows open again without disk reads |
| `CTP2_UIPRELOAD` | moves 0.5–3 s window builds out of play |

## Current figures (A4000, 2026-11-13)

- Idle map: 24 fps (was 9).
- Scroll step: horizontal ~150 ms, vertical 160–220 ms.
- Main menu ~78–84 s. F4 quick start to map ~40–55 s (world generation 9–17
  s, tile set read 2 s, map icons 3 s, PostProcessMap 3.4 s).
- End Turn: ~7–10 s at turn 1.
- Founding a city: CreateCity ~1 s + MakePop 0.45 s + build manager 3.1 s
  (1.1 s if prebuilt). `CTP2_UIPRELOAD=start` builds the 10 main windows in
  18.8 s at load (city window 4.9 s, domestic 3.6 s, national 2.9 s, build
  manager 2.0 s).

Emulator (Amiberry, `Best_warpos_a4000` ≈ 68060 @ 50 MHz without JIT): idle
28 fps, window drag ~27 fps, edge scroll ~7.5 steps/s. WarpOS build: menu
~56 s, End Turn ~2 s.

## Profiling

Set `ENV:CTP2_PERF` to 1 (and `CTP2_LOGSYNC` to 1 for load timings). Every
3 s `ctp2.log` gets:

- `Perf:` frames, game time, copy pixels/µs/calls, wait time;
- per-phase microseconds (`AMIGA_PERF_*` in `amiga_platform.h`): map layer
  (mix, water, sprites, restore, highlight), UI (idle, mouse, draw,
  minimize, max rects), scroll (pixels, tiles, overlays, sprites, copy, loop,
  UI, net, steps), card (shift, lock, diff, write), fonts, resources
  (hits/misses/find/load), path searches, LDL builds, busy ticks;
- the cursor's longest wait between mouse reads, with its call sites;
- `Event X took` lines for slow game events;
- `Big present` callers, `Shift:` / `Band:` / `ScrollPixels:` for the first
  scrolls, and `Mix restore:` rects.

Call-site addresses are logged as `(return address − &amiga_log)`. Resolve
them with `m68k-amigaos-nm` / `addr2line` on `build/amiga68k/ctp2.debug`.

To add a timer: add a slot to the enum in `amiga_platform.h`, wrap the code
in `AMIGA_PERF_BEGIN(t)` / `AMIGA_PERF_END(t, SLOT)`, and print it from the
Perf block in `civ3_main.cpp`.

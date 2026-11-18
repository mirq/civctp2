# Performance notes

Measured on the reference A4000 (68060, Radeon RTG behind Zorro III/Prometheus,
`PIXFMT_RGB16PC`) unless marked as emulator figures.

## Machine limits

| operation | cost |
|---|---|
| Zorro III bus to the card | ~15 MB/s (the bus's own rate; not measured by the game) |
| the game's copy to the card (read, byte swap, write) | measured ~2.3 Mpixels/s, 0.43 µs/px, ~4.6 MB/s; a full 800×600 frame took ~205 ms. Well below the bus rate; the cause is not known yet |
| fast RAM read | ~36 MB/s |
| `memcpy` / `memset` in fast RAM | ~17 / ~16 MB/s (every copy method measured 18–27 MB/s; `move16` was no faster) |
| shadow compare | 0.10 µs/px (branchless longword XOR), 0.17 µs/px with early-out |
| card `BltBitMap` scroll | ~0 ms of CPU time |
| full-map copy in RAM | ~50 ms |

Rendering is bounded by memory bandwidth. At the bus rate a pixel costs
about 0.13 µs to write and 0.10 µs to compare, so the diff pays only where
less than about a quarter of the compared pixels changed. See
[graphics-pipeline.md](graphics-pipeline.md), section 1.

## What each change bought

| change | effect |
|---|---|
| screen cleared to black through the lock at open | fixes the grey 16-pixel runs first taken for lost writes; the scrub (`CTP2_SCRUB`) is now only a safety net |
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
| sprite cache (`CTP2_SPRITECACHE`, `SpriteGroupList`) | a move read the unit's `GU###.SPR` twice (full ~430 ms before, basic ~200 ms after); now the first move of a unit type reads it once |
| re-centre shifts the map surface (`TiledMap::AmigaShiftView`) | a centring that moves the view by less than half of it repaints only the uncovered strip (~105 ms vs ~200 ms+ for `Refresh`); one that does not move it repaints nothing |
| radar map not rendered again for an unchanged view | ~190 ms per centring on a unit already in view (after every move) |
| effect sprites cached (`CTP2_EFFECTCACHE`); special effects the player cannot see are not queued | every combat flash / special effect read its `GX##.SPR` (41 KB – 2.1 MB) again, unseen ones too |
| end of a scroll, a wheel notch, a radar click shift the map surface (`AmigaRefreshMoved`) | 90–110 ms instead of a full `Refresh` (~175–200 ms early in a game, more later) |
| removed city borders out of view paint nothing (`terrainutil_RemoveBorders`) | a full `Refresh` per city taken or razed anywhere, AI wars included |
| ASCII `stricmp`/`strnicmp` (`amiga_posix.c`) | libnix's were utility.library `Stricmp` calls, locale-aware, on every layout, resource and data lookup |
| `memcpy` copies small blocks itself | libnix's called exec `CopyMem` for every copy, a few bytes included |
| `sprintf` formats `%s`/`%d`/`%u` itself, the rest goes to libnix | libnix formats in two passes; the controls build a dozen attribute names per image slot with it |
| clock: one FPU multiply per reading, a full conversion once a minute (`amiga_clock.c`) | each `GetTickCount` was four software 64-bit divisions; idle CPU on the main menu halved (255 → 130 ms per 3 s) |
| layout names: an FNV index of the stored names; unknown names are no attribute (`ldlif.cpp`, `ldl_data.cpp`) | each attribute lookup hashed the name with a weak hash (long chains, case-insensitive compares) and stored every absent name |
| End Turn: busy ticks in the long loops (scheduler goals, settle search, A*, distances, continents, the save in 32 KB pieces); the tick reads the clock every n-th call, n adapting | the cursor stood still for whole events (the autosave 1.45 s); now ≤ ~0.45 s |
| `MapPoint::GetNeighborPosition` / `NormalizedSubtract` computed directly (`MapPoint.cpp`) | the OrthogonalPoint object, a modulo and repeated normalising per neighbour; under every flood fill, path and AI distance |
| SLIC line offsets only for a debugger break (`SlicFrame.cpp`; `DebugSlic` is on in the default profile) | the first run of each script line opened the .slc and read it up to that line (goody huts: half of MoveUnits) |
| the autosave in the background (`CTP2_BGSAVE`, `GameFile.cpp`): the snapshot is taken as before (~0.4 s), compressing and writing it go on a few ms a frame | ~1.1 s of every turn, at the start of the player's own turn |
| `HasCityInSettleDistance` asks the cities, not every cell round every candidate (`settlemap.cpp`; not on worlds that wrap north-south) | a fifth of an AI player's begin-turn |
| `GetNeighborPosition` in row/column terms on worlds that do not wrap north-south | 29% of founding a city (flood fills, distances), 12% of path searches |
| End Turn marks the control panel for redraw instead of drawing it at once (`Player.cpp`) | the panel was drawn twice, ~0.1 s |
| `CtpAi::BeginTurn`'s city flags computed once, not 31 times (`ctpai.cpp`) | the loop reads `g_player[player]` for every i (an upstream bug, kept: same flags) |
| AI match facts (`Agent::Get_Match_Facts`, `Goal::Compute_Agent_Matching_Value_Cached`): what a goal's score reads from an agent's army (cargo, wounded/obsolete, characterisation, tie breaker, `TestOrder` per goal type) worked out once per match pass; a pass is `Process_Goal_Changes` or one `ProcessMatches` (no army changes inside: orders are events that run afterwards) | turn-101 game, 5 rounds: 75.1 → 67.8 s with the goal index below |
| goal index in `Scheduler::Add_New_Goal` during `CtpAi::BeginTurn` (every city, army and explore point is offered again each turn; the list was walked per offer); a duplicate is looked up before the completeness test | quadratic in goals; part of the line above |
| empire centres kept (`MapAnalysis::CalcEmpireCenter`): a path search per city, for every player at every player's turn; kept while the cities and `Cell::MoveCostVersion` are the same | `AiBeginMapAnalysis` was ~370 ms, 80 % of it these searches; 5 rounds 72.1 → 58.5 s |
| danger test kept per cell and search (`RobotAstar2::CheckIsDangerForPosKept`): asked for every way into a cell, eight neighbours each time | 5 rounds → 57.2 s |
| `terrainutil_GetTerrainEffect` from a table built from the databases; the slave-order wonder test only for slave orders | road paths and tile improvement choices (`AiBeginTurn`) |
| `-O3` for the pathing, map, unit-data and vision files (`AMIGA_O3_FILES` in `Makefile.amiga68k`) | turn 191, 1 round: 184.6 → 172.8 s; turn 101, 5 rounds: 102.6 → 95.5 s (all 6 players AI); quick-saves identical |
| transport/airlift costs kept per cell and search (`RobotAstar2::IsNextToForeignerKept`, `CargoCanUnloadAtKept`): eight neighbours and every cargo unit, for every way into a cell; `MapPoint ==/!=`, `World::IsLand/IsMountain/IsTunnel` inline | turn 191: 172.8 → 149.6 s (path searches 115 → 74 s); turn 101: 95.5 → 94.3 s |
| A* skips `EntryCost` for a cell it has already seen when even the least possible entry cost could not make the way through the current cell cheaper (`Astar::MinEntryCost`: `RobotAstar2::EntryCost` never allows less than 1.0, `DecayOrtho` takes 5 %; single-precision rounding is monotonic, so the original does nothing in exactly those cases); the usual neighbour step inline in `FindPath` (`astar_Neighbour`) | turn 191: 149.6 → 125.5 s, 444,001 of the round's `EntryCost` calls skipped, path searches 74 → 53 s; turn 101: 94.3 → 90.0 s; quick-saves identical, also against `CTP2_AICACHE 0` |
| radar: `RadarMap::CenterMap`'s full render waits for the next copy to the screen (`UpdateMap`), once for any number of centrings; `RedrawTile` skipped while it is pending (the picture does not depend on the view; the original rendered it whole on every centring to show what changed) | turn 191, 21 centrings in a round (~0.38 s radar + ~0.5 s map each): 126.9 → 118.0 s with `EnemyMoves=No`; 232.6 → 229.5 s with `EnemyMoves=Yes` (the radar is shown between the animated moves) |
| `aui_DirtyList::Minimize` on an array, pairs that cannot merge passed over without the `Rectangle_SmartConsolidate` call (the same merges in the same order: 200,000 random lists compared on the host) | window lists of ~360 rectangles while units move on screen; ~12 s of CPU in a turn-191 round with `EnemyMoves=Yes`, but the round is not shorter: moves are paced (`10 - UnitSpeed` steps of 75 ms each), the frames just get cheaper |
| path searches: the usual neighbour step inline for the danger and foreign-army tests too (`world_Neighbour`, `World.h`); what the kept per-cell answers depend on compared once per search, not per cell (`RobotAstar2::KeptIndex`) | turn 191: 116.0 → 106.7 s (two runs each, within 1 s), path searches 52.5 → 43.9 s; measured first: only 6 % of a search's entry costs are asked twice, and keeping the danger answers across searches would save 3 % of them, so no more caching there |
| empire centres kept while the cells their searches read keep their move costs (`CityAstar::CostReads`), not only while no move cost changed anywhere | turn 101, 5 rounds: `AiBeginMapAnalysis` 3.0 → 0.8 s; turn 191: 6.0 → 5.6 s (the read cells change every round there) |
| path searches' danger test: whether the searching army is civilian and where it stands asked once per search, and whether it is weaker than the army in a cell once per cell and search (`UnitAstar::IsWeakerThan`; nothing moves during a search); `Cell::UnitArmy` inline | turn 191: 108.5 → 105.1 s (A/B, two runs each: 110.2 → 104.9 s), path searches 44.2 → 40.6 s; with `CTP2_AICACHE 0` 207 → 186 s. Inlining `OrthogonalPoint`'s steps for the map iterators was measured too: no change, not kept |

## Current figures (A4000, 2026-11-13)

- Idle map: 24 fps (was 9).
- Scroll step: horizontal ~150 ms, vertical 160–220 ms.
- Main menu ~78–84 s. F4 quick start to map ~40–55 s (world generation 9–17
  s, tile set read 2 s, map icons 3 s, PostProcessMap 3.4 s).
- End Turn: ~7–10 s at turn 1.
- End Turn (2026-11-16, same world via `CTP2_SEED`, turns 1–3 with a city):
  7.5 / 7.3 / 10.8 s; the cursor stood still up to 1.45 s (the autosave,
  ~1 s of it zlib at level 1, ~95 KB written). `ENV:CTP2_SAVELEVEL` sets
  the level (0: stored).
- Window builds (2026-11-16): city window first opening 5.6 → 3.7 s (its
  layout 3.1 → 2.4 s, of which 1.3 s is 15 images read from disk at ~1 MB/s);
  build manager after founding a city 3.4 → 2.0 s.
- Unit move (2026-11-16): the game logic is cheap (no event over 150 ms);
  the Director's animation was the cost. Before: full sprite load ~430 ms +
  basic reload ~200 ms + a centring on the next unit (radar ~190 ms, map
  `Refresh` ~200–500 ms). Now: ~460 ms once per unit type (the disk reads
  ~1 MB/s), then nothing; the centring takes ~5 ms.
- Founding a city: CreateCity ~1 s + MakePop 0.45 s + build manager 3.1 s
  (1.1 s if prebuilt). `CTP2_UIPRELOAD=start` builds the 10 main windows in
  18.8 s at load (city window 4.9 s, domestic 3.6 s, national 2.9 s, build
  manager 2.0 s).

Late game (turn-191 save, all 6 players AI, 1 round, frame-pointer profile):
path searches 65 % (`RobotAstar2::RecalcEntryCost` 52 s of it before the
transport caches: `IsNextToForeigner` 19 s, cargo unload tests 18 s), the
AI's goal logic itself 4 %, map redraw at the end of the round 12 %. No
68060-trapping instruction in the game's code; FPU instructions ~2 % of
the samples, math library calls 0.03 %. Round time (Amiberry): 251 s (original) →
190 s (caches) → 173 s (`-O3`) → 150 s (transport caches, inlining) → 125.5 s (A* skips steps that cannot help) → 118.0 s (radar render deferred) → ~107 s (inline neighbour step, cheaper cache index) → ~105 s (danger test per search). With the
profile's `EnemyMoves=Yes` the same round takes ~230 s: ~105 s of it are the
enemy units' moves shown one after another, 29 centrings ~12 s. With
`EnemyMoves=No` the view still centres on what the AI does where the player
sees it (`AutoCenter=Yes`): ~8 s of the round; `AutoCenter=No` took it from
105 to 93.5 s (the AI's moves the same; 4 bytes of the quick-save differ, one
flag in 4 records of one kind).

The same rounds on the A4000 itself (2026-11-18, `EnemyMoves=No`, the
A4000's own profile; two or three runs each, within 1 s of each other):

| build | turn 191, 1 round | path searches | turn 101, 5 rounds |
|---|---|---|---|
| release 0.5 (`0a7f67290`) | 144.1 / 145.0 s | 57.4 s | 119.8 s |
| `d7eab732f` (neighbour step, cache index) | 133.8 / 133.6 s | 47.9 s | – |
| `98b9ebd6d` (danger test per search) | 126.4 / 126.5 / 126.5 s | 40.0 s | 115.9 / 116.3 s |
| `98b9ebd6d`, `CTP2_AICACHE 0` | 210.5 s | 95.8 s | 140.0 s |

The A4000 is ~20 % slower than Amiberry over the round: the path searches
take the same time (40.0 s against 40.6 s, the same 1047 searches and
196 227 nodes), the scheduler, `AiBeginTurn` and the map work ~25 % longer.
The gains are larger there: −18 s a round from release 0.5 (Amiberry −11 s).
Every turn-191 quick-save is the same game as release 0.5's; the turn-101
ones differ only in the known uninitialised bytes. A first release-0.5
turn-101 run took 174.6 s (its loading 2 min longer) and did not repeat; file
transfers from the A4000 overlapped it.

AI turns on a saved game (Amiberry, 2026-11-17, turn-101 test game, 6 AI
players, `CTP2_AUTORND 5`, see [testing.md](testing.md)): 75.1 s for 5
rounds with `CTP2_AICACHE 0`, 57.2 s with the caches (−24 %); the
quick-saves are the same game. Where a round went before the empire-centre
and danger caches (profile, 3 rounds): `ProcessMatches` 24 % (mostly unit
path searches), `AiBeginMapAnalysis` 17 % (80 % empire centres),
`BeginScheduler` 13 %, `AiBeginTurn` 11 % (tile improvements, road paths),
`EndTurn` 6 % (radar map redraw), UI between events 16 %.

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

### Sampling profiler (68060)

`ENV:CTP2_PROF_EVENT` = an event's name (as in `GameEventDescription.h`)
samples only while that event is processed; every sample also carries the
event it was taken in (`amiga-prof.py --event NAME`). `CTP2_PROF` = 2 follows
the a5 frame chain: build with frame pointers for exact callers,
`make -f Makefile.amiga68k OBJDIR=build/amiga68k-fp TARGET=bin/amiga/ctp2fp.060
EXTRA_CXXFLAGS=-fno-omit-frame-pointer EXTRA_CFLAGS=-fno-omit-frame-pointer`
(and resolve with `build/amiga68k-fp/ctp2.debug`).

`ENV:CTP2_PROF` = 1 starts a task above the game's priority that samples the
game's PC every millisecond (`os/amiga/amiga_prof.c`), with up to 8 code
addresses further up its stack. `break <process> E` (CTRL-E) writes the
samples to `RAM:ctp2prof.bin` and starts again, so: CTRL-E, do the slow
thing, CTRL-E, fetch the file. A game that hangs never looks at CTRL-E:
`break <process> F` (CTRL-F) makes the sampler itself write the samples so
far to `RAM:ctp2hang.bin` (once per run; the last samples show the loop,
with `CTP2_PROF 2` and its callers). On the host:

    ctp2_code/os/amiga/host/amiga-prof.py ctp2prof.bin build/amiga68k/ctp2.debug [top]

prints the functions the PCs fell in ("self") and those found anywhere in a
sample ("in the sample", a heuristic stack walk: stale return addresses
count too). `[ROM/library]` is a PC outside the game's code. Use the
`ctp2.debug` of the very build that ran. Sampling slows the game by about a
quarter. `setenv` writes `ENV:`, so unset `CTP2_PROF` afterwards.

The PC is read at a fixed offset of exec's saved context (+124 on the A4000
with the 68060.library and a task that uses the FPU; the FPU state comes
first, and its FPIAR at +12 looks like a PC but is not). If the samples look
wrong on another setup, log a few saved contexts and check
`AMIGA_PROF_PC_AT`.

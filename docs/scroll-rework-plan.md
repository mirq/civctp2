# Plan: faster map scrolling

Status: stages 1 to 4 are done (stage 4 as an option that is off by
default), stage 5 in part (solid rows), with a repeatable benchmark, a
yardstick and a self-check. See "Results so far" at the end.

## Why

Map scrolling is the slowest interactive part of the Amiga builds. Another
RTG game on the same class of machine (BobrHopper, 640×480, 8-bit) scrolls
smoothly with a much simpler display path, so its design was compared with
this one.

### What BobrHopper does

- 8-bit pixels: a 640×480 frame is 307 KB. CTP2 at 1024×768×16 is 1.57 MB.
- A plain `memcpy` per row into the locked card bitmap. No shadow, no compare.
- The static background lives in a wrapped cache: world pixel (X, Y) is at
  (X & TW-1, Y & TH-1). A scroll moves no memory. Only the uncovered band is
  painted, in batches, thanks to a 16-pixel margin.
- A frame is three passes: copy the window out of the cache, draw what
  moves, copy to the card.

### What a CTP2 scroll step costs today

Measured in Amiberry (CPU close to a 68060 at 50 MHz; the RTG card is much
faster than a real one), 1024×768, whole map explored, 113 steps, two tiles
per step. Per step:

| part | ms | what it is |
|---|---|---|
| surface shift | 36 | the background surface's visible rows moved in fast RAM (`AmigaShiftSurface`) |
| frame shift | ~13 | the same band moved in the composed frame (`ScrollFrame`) |
| tiles | 49 | painting the uncovered strip (`RepaintTiles`) |
| overlays | 12 | hats, borders, improvements, cell text |
| sprites | 12 | trade routes and units in the strip |
| other | 12 | offsetting dirty rectangles and sprites |
| UI draw | 69 | composing the open areas into the frame; 29 ms of it is the shadow compare |
| card shift | 5 | `BltBitMap` |
| **total** | **~206** | |

Two things stand out:

- About 50 ms a step is spent moving pixels that only change position.
- The compare covers about 290,000 pixels a step, far more than the
  uncovered strip (about 80,000). Only one full-width band is shifted on the
  card. The other open areas (beside the radar and the control panel) are
  composed, compared and sent whole on every step. On a real card that also
  costs 20–65 ms of writes per step, which the emulator does not show.

Painting the new tiles (49 ms) is real work that no display trick removes.
So the realistic target is about **100–110 ms a step**, roughly twice as
fast as today, not the four times first guessed.

## Stages

Each stage is checked the same way: `ENV:CTP2_PERF` per-step timings, and
CTRL-F dumps showing card = shadow = frame, mid-scroll and at rest, in all
four directions over explored terrain.

### Stage 1: a scroll moves no memory in the map surface

Give the background window's surface spare room on all sides (a wider pitch
and extra rows). A scroll then moves the surface's origin pointer instead of
its pixels. When the spare room runs out in one direction, the view is
copied once to the opposite side, which restores the full run.

- `aui_AmigaSurface`: `EnsureSlack(sx, sy)` and `ScrollOrigin(dx, dy)`.
- `TiledMap::ScrollPixels`: use `ScrollOrigin` for the background surface;
  keep `AmigaShiftSurface` as the fallback.
- `ENV:CTP2_SCROLLSLACK`: spare pixels across (rows are half of it); `0`
  turns it off. It trades memory for speed (about 11 MB at 1024×768 with the
  default of 1024).
- Expected saving: most of the 36 ms.

### Stage 2: shift every open rectangle, not one band

Decompose the map area that no window covers into rectangles and shift each
one on the card, in the shadow and in the frame. Only the uncovered strip of
each is then composed, compared and sent.

- Expected saving: 30–40 ms of UI draw per step in the emulator, plus the
  card writes on real hardware.

### Stage 3: present the map straight from the map surface

For the map area, take the pixels for the card from the background surface
itself. The composed frame then holds only the windows above the map, and
the frame shift and the compose copy for the map go away.

- This changes how the cursor's background is restored over the map, and how
  `AmigaDrawScreen` picks its source per rectangle.
- Expected saving: the frame shift (~13 ms) and most of the remaining
  compose time.

### Stage 4: skip the compare where the change is known

Needs the raw write rate measured on the A4000 first (a plain longword copy
into the locked bitmap). If the bus does about 15 MB/s, the uncovered strip
should be written directly, without the compare.

### Stage 5: the cost of painting the strip

Profile `RepaintTiles` (49 ms a step). Look for per-tile work that is
repeated, and for batching as BobrHopper does with its margin.

### Smaller items

- The scroll loop keeps spinning when held against the map edge
  (about 26 ms a pass, nothing written). End the loop when no step is taken.
- A permanent test aid: with `ENV:CTP2_TEST` set, F9 explores the whole map,
  so display checks have terrain everywhere.

## Results so far

Measured in Amiberry at 1024×768 with the whole map explored (so every tile
is drawn under the fog of war), per scroll step. The first column is the
state before this plan. Both columns have the cost of the CTRL-F snapshot
taken during each run removed (about 2.3 s per run).

| part | before | now (left / right / down) |
|---|---|---|
| surface shift | 36 | 2 |
| frame shift and open-rectangle search | ~13 | 17–20 |
| tiles | 49 | 17 / 41 / 20 |
| overlays | 12 | 7 / 16 / 6 |
| sprites | 12 | 10 / 9 / 8 |
| other | 12 | 12 / 11 / 12 |
| scroll loop (compose, compare, card) | ~52 | ~51 / ~53 / ~48 |
| **total, ms** | **~186** | **~118 / ~147 / ~112** |

So a step is about 1.5 times faster. What was done:

- **Stage 1.** `aui_AmigaSurface::EnsureSlack` / `ScrollOrigin`: the
  background surface has spare room and a scroll moves its origin. The
  surface shift went from 36 ms to about 2 ms a step, including the copies to
  the far side when the room runs out. `ENV:CTP2_SCROLLSLACK` sets the spare
  pixels across (default 1024, rows are half; `0` turns it off).
- **Stage 2.** `AmigaOpenRects`: every rectangle of the map that no window
  covers is shifted on the card, in the shadow and in the frame. For a scroll
  across the map is cut into row bands, for a scroll up or down into
  columns, so that what a shift uncovers comes from the same rectangle. The
  saving in the emulator is small (a few ms of compare); the larger effect
  should be on a real card, where the areas beside the radar and the control
  panel are no longer sent whole on every step. That is not measured yet.
- **Fog-of-war tiles.** `TiledMap::DrawBlendedTile` blended every pixel with
  the general routine and worked out the destination address per pixel. The
  fog is black at one half, which is a shift and a mask; the result is the
  same for all 65,536 pixel values. Tile painting under fog fell to less than
  half.
- **Map edge.** `AmigaScrollMap` now reports when no step was taken, so the
  scroll loop ends there. Holding a key against the edge used to keep the
  loop going (about 20 s of wasted work in a test run).
- **Test key.** With `ENV:CTP2_TEST` set, F9 explores the whole map.

Checked after each change: CTRL-F dumps mid-scroll in all four directions
and at rest, frame = card = shadow, **and the pictures looked at**. The
second check matters: a bug in the first version of the column rectangles
shifted the top bar along with the map, and the three dumps still agreed
with each other, because the frame itself was wrong.

### Stages 3 and 4 (second round)

- **Stage 3.** Where no window covers the map, the card gets the map's
  pixels straight from the background window's surface
  (`aui_AmigaUI::DirectSource`, `IsDirectRect`, `ComposeBegin`;
  `amiga_video_present_multi` takes several sources in one copy). The
  composition only marks those rectangles and no longer copies them into the
  frame, and the frame is not shifted on a scroll. `ENV:CTP2_DIRECTMAP=0`
  turns it off.
- **The busy cursor** now takes its background from the shadow of the card
  and from what the last full copy saved under the cursor
  (`aui_AmigaUI::BusyCursor`), not from the frame. It no longer depends on a
  frame that may be half drawn or already shifted. This removes the wrong
  cursor patch during a scroll that the review had found.
- **Stage 4.** `ENV:CTP2_STRIPWRITE=1` sends what a step uncovered to the card
  whole, past the compare (`amiga_video_force_rect`). In the emulator it
  saves about 5 ms a step. It is off by default, because whether it pays
  depends on the card's write rate, which has to be measured on the A4000.
- **Past the map's edge.** No tile is painted beyond the edge of a world
  that does not wrap. With stage 1 the uncovered strip showed leftover
  memory there, where shifted pixels used to bring the black along. The
  strip is now cleared when the view reaches past the edge.
- **Pointer resting at the map's edge.** A scroll loop began and ended with
  every other frame, and its end paints the whole map again. The mouse path
  now asks `TiledMap::CanScrollMap` first.

Measured after stage 3, same method (per step, ms):

| part | over sea and fog, partly along the map's edge | over land |
|---|---|---|
| surface and frame shift | 3–4 | 2–3 |
| tiles | 15 | 52 |
| overlays | 7–10 | 20–25 |
| sprites | 9 | 11 |
| other | 12 | 12 |
| scroll loop (compose, compare, card) | 38–42 | ~70 |
| **total** | **~85–100** | **~170** |

Stage 3 took about 25 ms off a step: the frame shift (17–20 ms) is gone and
the scroll loop is about 10 ms shorter. The numbers above also show how much
a step depends on the terrain: land tiles carry transitions, hats and
improvements and cost three times as much to paint as sea. The earlier
measurements were taken mostly over sea, so they understate the cost over
land. The two columns come from different random maps and are not a
before-and-after pair.

Checked: frame = card = shadow and the pictures looked at, at the menu, on
the idle map, mid-scroll in all four directions, along the top and bottom
edges of the map, with a window (the science manager) open over the map and
after closing it. One case showed 76 differing pixels mid-scroll: two small
marks drawn into the kept part of the map without being marked as changed
(overlays are repainted one tile beyond the strip that is marked). They are
corrected when the scroll ends, as before; the comparison could not see this
while the frame lacked them too.

### Third round: a repeatable benchmark, solid rows, yardstick, self-check

Ideas taken from BobrHopper's blitter and its measuring code.

- **A repeatable benchmark** (`ENV:CTP2_BENCH=1`). The game starts itself
  from the main menu on a fixed world (`ENV:CTP2_SEED`, 12345 in a
  benchmark), explores the map, scrolls a fixed number of tiles each way in
  single-tile steps (first with every tile under the fog of war, then right
  and left with everything in sight), writes the times as `Bench:` lines to
  `ctp2.log`, writes three pictures (`RAM:bench_fog.ppm`, `bench_sight.ppm`,
  `bench_end.ppm`) and quits. Two runs of the same build agree within about
  3%; the pictures of two builds can be compared byte for byte. All earlier
  figures in this document came from a different random map each run.
- **Solid rows** (`AmigaSolidRows` in `tiledraw.cpp`). For each base tile the
  rows without a transition marker are worked out once and copied in long
  words, with no test per pixel; the fog path halves them two pixels at a
  time. `ENV:CTP2_SOLIDROWS=0` turns it off. The pictures are identical with
  it on and off. Tile painting fell from 26 to 19 ms a step under fog (27%)
  and from 19 to 17 ms in sight (12%); a whole step by about 7% and 3%. That
  is less than the third to a half first expected.
- **Yardstick** (with `ENV:CTP2_PERF`). When the screen opens, one log line
  gives MB/s for a copy in fast RAM, the compare, and writes to the card
  plain and through the pixel conversion. In Amiberry: copy 65, compare 36,
  card 65 plain and 22 through the conversion. On the A4000 the same line
  answers what the card's write rate really is, and whether
  `CTP2_STRIPWRITE` should be on.
- **Self-check** (`ENV:CTP2_CHECK=n`). After every n-th copy the card is read
  back and compared with the picture as it should be; and at the end of a
  scroll the map as the steps left it is compared with the map painted anew
  (the first three differences also as pictures, `RAM:ctp2check<n>.ppm`).
  It costs about 35 ms a step with n = 8. On the benchmark it reports:
  the card right in all but 2 of about 130 copies (30 pixels each time, the
  unmarked overlay marks known from before); and after each scroll 2,400 to
  3,800 pixels of the map that differ from a fresh repaint. The picture shows
  all of them on sprites: the shadows of goods are darker after a scroll
  (drawn again over themselves in the strip's margin), and the selected unit
  animates. The terrain itself matches exactly, which is the check that the
  origin moves, the rectangle shifts and the direct copy are right.

The benchmark on the fixed map, gcc 6.5, ms a step (single-tile steps):

| | right | left | down | up |
|---|---|---|---|---|
| under fog, before solid rows | 102 | 103 | 74 | 75 |
| under fog, with solid rows | 94 | 96 | 71 | 70 |
| in sight, before | 84 | 92 | | |
| in sight, with | 81 | 90 | | |

Where a step under fog goes now (right, ms): tiles 19, overlays 11, sprites
8, per-tile walks 11, the scroll loop 43 (of which the compare 22).

### The compiler

The 68060 build also compiles and runs with m68k-amigaos-gcc 16.2. On the
benchmark (before solid rows; one run each, the gcc 6.5 run repeated and
within 3%):

| | gcc 6.5 -O2 | gcc 16.2 -O2 | gcc 16.2 -O3 |
|---|---|---|---|
| start to menu, s | 58.0 | 56.0 | 54.6 |
| menu to map, s | 26.2 | 21.8 | 21.4 |
| step right / left, ms | 101 / 108 | 92 / 101 | 93 / 95 |
| step down / up, ms | 76 / 80 | 70 / 68 | 68 / 65 |
| executable, MB | 7.5 | 7.7 | 9.1 |

So gcc 16.2 is 7–15% faster in scrolling and about 17% faster loading a
game; `-O3` adds little over `-O2` for 1.4 MB more. What the build needs is
in `overview.md`. gcc 6.5 is still the default.

Open:

- The overlay passes (`RepaintHats`, `RepaintBorders`,
  `RepaintImprovements`) and the per-tile walks `OffsetLayerSprites` and
  `RepaintLayerSprites` (about 30 ms a step together).
- The compare (22 ms a step) against writing the strip directly
  (`CTP2_STRIPWRITE`): decide with the yardstick's figures from the A4000.
- For a scroll up or down the shadow of the column rectangles is shifted by
  copying; only full-width rectangles use the row pointers.
- Goods' shadows are drawn twice in the strip's margin during a scroll (the
  self-check shows it). It was so before this work.
- In the emulator the plain copy loops are bound by instructions, not by
  memory (libnix `memcpy` 167 MB/s against 65 for the long-word loop), which
  a real 68060 is not: speed-ups of copies measured there may not carry
  over.
- Nothing here is measured on real hardware.

## Not planned

- 8-bit pixels: the game's art and blending are 16-bit throughout.
- 640×480: the game's windows need 800×600.

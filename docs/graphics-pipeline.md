# The display graphics pipeline

This document explains how the Amiga builds get the game's picture onto an
RTG card, and why the code is shaped as it is. Read it before you touch
`os/amiga/amiga_video.c`, `ui/aui_amiga/aui_amigaui.cpp`, the scroll code in
`gfx/tilesys/tiledmap.cpp`, or the composition in `ui/aui_common/aui_ui.cpp`.

## 1. The constraint

Everything follows from one measured fact. **On an A4000 the 68060 writes to
the graphics card at about 2.3 Mpixels/s (0.43 µs per 16-bit pixel, ~5 MB/s
over Zorro III).** A full 800×600 frame takes about **205 ms** to send.

Fast RAM is not fast either: reads run at ~36 MB/s, and `memcpy`/`memset`
at 16–17 MB/s. Comparing a pixel in fast RAM costs ~0.10–0.17 µs, a few
times cheaper than writing it to the card, but not free. Copying the whole
map surface costs ~50 ms.

So the pipeline is built around three ideas:

1. **Compose in fast RAM, then send only the pixels that changed.**
2. **Let the card's blitter move pixels that are already on the card**
   (scrolling) instead of sending them again.
3. **Touch as few pixels as possible in fast RAM too**: finer dirty cells, shift
   only what is visible, and skip windows whose content is unchanged.

The original game used DirectDraw, and the Linux port uses SDL, which copies
the whole secondary surface to the screen every frame. Neither approach is
viable here.

## 2. The overall flow

```
 game state ──► TiledMap / sprites ──► background window surface (map layer, 15 fps)
                                            │
 aui windows (controls, text, panels) ──────┤  window surfaces, own dirty lists
                                            ▼
                     aui_UI::Draw / ClipAndConsolidate      (ui/aui_common/aui_ui.cpp)
                                            │  BltToSecondary + Show(rect)
                                            ▼
          m_secondary: 16-bit R5G6B5 frame in fast RAM      (aui_AmigaSurface)
                     + present list of changed rects        (aui_AmigaUI::AddPresentRect)
                                            │
                     aui_AmigaUI::AmigaDrawScreen           (ui/aui_amiga/aui_amigaui.cpp)
                       - scrub strip, cursor composited into the frame
                                            │
                     amiga_video_present                     (os/amiga/amiga_video.c)
                       1. compare rects against the SHADOW in 16-px segments → runs
                       2. pending card scrolls: BltBitMap on the card
                       3. LockBitMap, convert + write the runs, UnLockBitMap
                                            ▼
                                RTG card (Picasso96 / CyberGraphX)
```

## 3. Screen and mode (`amiga_video_open`)

- The mode is chosen in this order: `ENV:CTP2_MODE` (a hex display ID), then
  `BestCModeIDTagList` (depth 16, nominal size), then the smallest 15/16-bit
  mode from `AllocCModeListTagList`. `mode_ok` accepts only real CyberGraphX
  IDs with 2 bytes per pixel and a supported pixel format, because
  `BestCModeIDTagList` returns a native chipset mode when nothing fits.
- The screen is a `CUSTOMSCREEN`: quiet, no title bar, not draggable. On it
  sits a borderless **backdrop window** that only receives input
  (`IDCMP_RAWKEY | MOUSEBUTTONS | MOUSEMOVE`, `WA_RMBTrap`, `WA_ReportMouse`,
  simple refresh, `NoCareRefresh`). The OS never draws the game.
- If the mode is larger than the game's resolution, the picture is centred
  (`s_ox`, `s_oy`) and the border is cleared to black with `SetRast`.
- The system pointer is replaced by an empty 1×1 sprite in chip RAM. The game
  draws its own cursor (section 8).
- `display.cpp` lists the card's 16-bit modes of 600 lines or more
  (`amiga_video_modes`) for the in-game resolution choice. The screen cannot
  change size while the game runs (`aui_AmigaUI::ChangeSize` only
  invalidates).

### Pixel formats

The game always draws **R5G6B5 in native big-endian words**
(`g_is565Format`). The card's 16-bit layout varies, and `convert_row`
converts while writing:

| `PIXFMT_` | conversion |
|---|---|
| `RGB16` | `memcpy` |
| `RGB16PC` (common on PCI and Zorro boards, and the A4000's Radeon) | byte swap, two pixels per longword when both pointers are aligned |
| `BGR16`, `BGR16PC` | swap R and B, plus the byte swap |
| `RGB15`, `RGB15PC` | drop the low green bit |

The conversion costs very little compared with the bus. Runs start on
16-pixel boundaries (section 6), so the longword path is the common case.

If `LockBitMapTagList` fails (a driver that refuses it), the copy falls back
to `WritePixelArray` with RGB24 rows. That is slow, it is logged once, and
it turns the shadow off.

## 4. Surfaces (`aui_AmigaSurface`)

- Every surface, the primary one included, is a plain 16-bit buffer in fast
  RAM with rows aligned to 4 bytes. `Buffer()` is always valid, as in the SDL
  build, so the generic software blitters (`aui_Blitter`, `C3Blitter`) draw
  everything. `aui_AmigaBlitter` adds nothing. MMX paths are compiled out
  (`CheckMMXTechnology` returns false).
- There are no OS locks or critical sections: `Lock` only computes a pointer
  and keeps the lock list.
- **`m_secondary` is the frame.** The UI composes into it, and it is what goes
  to the card. `m_primary` only receives the few full-screen copies some
  screens make (the splash screen). `aui_AmigaSurface::Flip` on the primary
  surface calls `AddPresentAll` and then `AmigaDrawScreen`.
- RIM images from the picture archive: there is no memory mapping on the
  Amiga, so `ProjectFile` allocates the block, the pixels are copied into a
  buffer the surface owns, and the block is freed at once
  (`c3imageformats.cpp`).
- Loaded images, patterns, icons, cursors and fonts stay cached
  (`aui_resource.h`, `TrimUnused`) until less than 24 MB of fast RAM is free.
  Opening a window a second time therefore reads nothing from disk.

## 5. Composition and the present list

### Windows to the frame

`aui_UI::Draw` → `ClipAndConsolidate` walks the windows from the back to the
front. For each dirty rectangle it calls the mouse backend's blit functions,
which on the Amiga are `aui_AmigaMouse::BltWindowToPrimary`,
`BltDirtyRectInfoToPrimary`, `BltBackgroundColorToPrimary` and
`BltBackgroundImageToPrimary`. Each one blits to **the secondary surface**
(`BltToSecondary`) and then calls `Show(rect)`, which only **records** the
rectangle:

```cpp
void aui_AmigaMouse::Show(const RECT &screenRect)
{
    static_cast<aui_AmigaUI *>(g_ui)->AddPresentRect(screenRect);
}
```

The SDL build copied the secondary surface to the primary one and then to
the screen. Here the record is all that happens until the next present.

### `AddPresentRect`

- Clips the rectangle to the screen and drops it if an existing rectangle
  already contains it.
- With more than 64 rectangles, it replaces them all with their union: past
  that point the per-call cost outweighs the extra pixels, and the diff
  filters out the unchanged pixels anyway.
- `AddPresentAll` sends the whole screen. It is used for the first frame, on
  Alt-Tab-in, and on a primary `Flip`.

### Shaped windows (the control panel)

The control panel is the only window with a **stencil** (a shaped,
partly transparent window). Stock code redraws a stencil window completely,
with the windows below mixed in and every control drawn again, whenever
anything in it changes. That costs ~150 ms on a 68060, and the status-bar
text changes on every mouse move.

On the Amiga (`aui_ui.cpp`, `AllSolid`, `aui_StencilRectIsSolid` in
`aui_blitter.cpp`):

- A control that changed has already drawn itself. If its rectangle lies
  entirely on the solid part of the stencil, it goes to the frame as it would
  from an opaque window.
- The windows below are mixed in again only where a change reaches a
  transparent part, or where a window below changed (`FindDirtiedFromBelow`,
  or the screen's own dirty areas), and only in those rectangles.

The result was verified with a pixel diff of the panel before and after
hovering every ring button.

### The map layer

- `TiledMap` draws terrain, sprites, borders and names into the background
  window's surface and mixes them through its own dirty grid. On the Amiga
  the grid is **32×32** instead of 128×128 (`InitGrid`). A selected unit used
  to dirty six 128×128 cells (98 kpixels); this change alone took the idle
  map from 15.5 to 24 fps.
- The map layer (unit redraw, water, tile highlight, city names) runs at
  **15 fps** (`CivApp::ProcessUI`). The UI and cursor run at 30 fps. Between
  map passes the background window keeps the last pass, and anything other
  windows uncover is copied from it.
- The radar map is recomposed 4 times a second while the view moves and once
  a second otherwise (`radarmap.cpp`). Its blit uses 16.16 fixed point
  instead of doubles, with a plain row copy when it is unscaled
  (`aui_blitter.cpp`).

## 6. The frame copy (`amiga_video_present`)

### The shadow

`amiga_video.c` keeps a **shadow**: a copy in fast RAM, in the game's own
format, of what the card currently shows for the picture area. Every
rectangle to present is compared with the shadow, and only differing pixels
are written to the card. This is the same technique as OpenSWOS's 68060 RTG
build.

- Rows are compared in **16-pixel segments aligned to absolute multiples of
  16**. A full aligned segment is compared as eight longword XORs ORed
  together, with no branch per pixel: 0.10 µs/px on the 68060, against 0.17
  for an early-out loop. Partial segments at the row ends and odd addresses
  are compared pixel by pixel.
- Adjacent differing segments merge into **runs** `(y, x, n)` (`add_run`
  also merges with the previous run). The shadow takes the new pixels at
  once.
- **Two phases.** First every rectangle is compared and the runs are
  collected (in fast RAM, no lock). Then the pending card scrolls are
  blitted, the bitmap is locked, and all runs are converted and written back
  to back. As a result a scroll step appears on the card within milliseconds
  of the blitter shift. The viewport compare (~50–70 ms) comes before it,
  so the user never sees the new strip being drawn down the screen.
- **Only the copy runs between `LockBitMapTagList` and `UnLockBitMap`**,
  with no other OS call.

### The shadow is stored by row

```c
#define SHADOW_SLACK 1024
typedef struct { uint16_t *buf; int off; } ShadowRow;
#define SHADOW(y) (s_shadowRows[y].buf + SHADOW_SLACK + s_shadowRows[y].off)
```

Each row has 1024 spare pixels on both sides. A full-width horizontal scroll
of the shadow therefore moves each row's **start offset** instead of copying
the row. A vertical scroll **rotates the row pointers**. Only the uncovered
strip is copied, and it gets the old pixels that the card still shows there
after the blitter shift. The strip must match what the card really shows:
if it held whatever the slack happened to contain, a new picture that
happened to equal that garbage would never be sent. When a row's offset runs
out of slack, the row is moved back to the middle once.

### The scrub

Section 1 assumes that every write reaches the card. **On the A4000 that is
not always true**: a Radeon behind a Prometheus PCI bridge occasionally
drops writes. The CTRL-F card dump showed grey 16-pixel runs where black was
written. The shadow believes those pixels are correct, so the diff would
never fix them.

To recover, `AmigaDrawScreen` sends **4 full rows every 80 ms**, cycling
round the screen (about 12 s per pass), and bypasses the diff for them
(`amiga_video_force_rows`). After this change the card dumps matched the
frame exactly: 0 differing pixels at the menu, on the idle map, mid-scroll
and after a scroll. The scrub never runs in cursor-only presents
(section 8), because those copy from a frame that may be half drawn.

### Rules that keep the shadow correct

- **Every write to the card's picture area must go through
  `amiga_video_present`**, or turn the shadow off (`s_shadowOff = 1`). After
  that every pixel is sent, which is slow but correct.
  `amiga_video_present_patch` does exactly this. The fallback path does it
  too.
- The shadow starts as all zeros, which is correct because the screen was
  just cleared to black, and black is 0 in every pixel format.
- A card scroll and the matching shadow scroll must always happen together
  (section 7).

### uaegfx (Amiberry, WarpOS build only)

Amiberry's `uaegfx.card` redraws only memory pages its 68k write handlers
marked dirty. The emulated PPC writes VRAM directly, so the screen stayed
grey. When `uaegfx.card` is present (`FindName` on the library list), the
WarpOS build calls `CopyMem(p, p, n)` from the 68k over each presented
rectangle so that Amiberry redraws it. Real boards are not affected.

## 7. Scrolling the map

A map scroll step is the worst case: the whole viewport changes. Sending it
in full would cost ~200 ms. Instead, most of the work happens in the card's
blitter and in pointer moves.

### The chain of a step

1. **`AmigaScrollMap`** (`civ3_main.cpp`) wraps `TiledMap::ScrollMap`.
   - Within one scroll loop the steps **stay on one axis**. A change of axis
     within 500 ms ends the loop (`return false`). The loop's end repaints
     everything, and the next loop moves the new way.
   - Steps that come slowly (120–1000 ms apart) go further: **2 tiles across,
     or 4 rows (one tile height) down**. A step costs roughly the same however
     far it goes, so this keeps the scroll speed in pixels per second up.
   - It sets `g_amigaScrollOneAxis` around the call.
2. **`TiledMap::ScrollMap`** clamps multi-tile steps to the map edges one tile
   at a time (stock code dropped the whole step), offsets the mix dirty
   rectangles and sprites, and calls `ScrollPixels`.
3. **`TiledMap::ScrollPixels`** shifts the background surface in fast RAM.
   With `g_amigaScrollOneAxis` set and even, aligned sizes, it uses
   `AmigaShiftSurface`, which shifts **only what is on screen**:
   `AmigaOpenRows` finds the rows not fully covered by other windows. A
   horizontal step shifts only those rows; a vertical step shifts only the
   visible columns. The rest of the surface is stale until the loop ends.
   All copies run **upwards in memory** (`amiga_copy_up`,
   `amiga_shift_row`), because a downward `memmove` runs at about half speed
   on the 68060.
4. **The band.** After the surface shift, ScrollPixels finds the **longest
   run of screen rows that no other window covers** within the background's
   width. In those rows (the "band") the card shows the map and nothing
   else. A card shift elsewhere, for example under the control panel, the
   radar or the top bar, would move those windows' pixels and force them to
   be sent again. For the band:
   - `amiga_video_scroll(x, top, w, h, dx, dy)` **queues** a card scroll
     (up to 4; if the queue is full, the earlier ones run at once);
   - `amiga_ui_scroll_frame(...)` → `aui_AmigaUI::ScrollFrame` shifts **the
     same rectangle of the composed frame** (`m_secondary`) in RAM. The UI
     then has to compose, and the diff to compare, only the newly uncovered
     strip, which is all `TiledMap::ScrollMap` marks dirty.
5. The newly uncovered tiles are repainted (`RepaintTiles`) and composited as
   usual.
6. **At the next full present**, phase 1 first applies the queued scroll to
   the shadow and compares the rectangles (now mostly just the new strip).
   Phase 2 runs `BltBitMap(..., 0xC0, ...)` on the card (Picasso96 handles
   overlapping areas) and then writes the runs. The next `LockBitMap` waits
   for the blitter.
7. `CivApp::ProcessUI` runs the scroll loop. Each step it calls
   `CopyMixDirtyRects`, `g_c3ui->Process()`, `PumpEvents` and
   `AmigaDrawScreen` (on the Amiga the mouse moves only through window
   messages; without them the cursor stays at the edge and the map scrolls
   forever). At the loop's end the whole map is mixed again
   (`g_amigaMixStale`).

The scroll invalidation on the Amiga is the background viewport minus the
other windows' rectangles, added as whole rectangles. Adding 32-pixel grid
cells instead produced ~475 rectangles and 135 ms of composition per step.

### Why the scroll waits for the next full present

Between a scroll and the next full present, cursor-only presents
(section 8) can still run. They copy from the composed frame **before** the
scroll. If the card had already been shifted, the cursor patch would be
drawn over shifted card content from a frame that does not match it. The
queued scroll keeps the card, the shadow and the frame consistent. Any rect
sent while a scroll is pending is recorded with `amiga_video_touch` (up to
32, then the whole scrolled area), and the next full present compares it
both where it is and where the scroll moves it.

## 8. The software cursor

The hardware pointer was tried and rejected. On AmigaOS 3, Picasso96's
sprite is 16 pixels wide with 3 colours, while the game's cursors are 48×48
in 16 bits. A 64-pixel-wide pointerclass image garbled the sprite and once
crashed the emulator. Do not try again without a new idea.

### Drawing it

- `aui_AmigaMouse::ActivateCursor` gives the UI the cursor surface and its
  hot spot. `SetCursorImage` **copies** the pixels (read through `Lock`,
  because a RIM cursor's buffer exists only while it is locked). The game
  unloads and reloads cursor surfaces, so a stored pointer would go stale.
  `owner` prevents an old mouse, deleted after its successor, from clearing
  the new cursor.
- In `AmigaDrawScreen` the cursor is drawn **into the frame itself** for the
  copy (chroma-keyed, with the pixels underneath saved in `m_composite`), and
  the saved pixels are restored afterwards. The card receives each pixel
  once and never shows the frame without the cursor. The earlier approach,
  a frame copy followed by a cursor patch, flickered because the card has no
  double buffer.
- When the cursor moves, the old rectangle and the new one are added to the
  present list. The diff sends only what actually differs.
- The animation is stepped by `aui_AmigaMouse::Tick` on the main task. The
  SDL build used a timer thread.

### The busy cursor (`BusyTick`)

The game often runs for a long time without returning to the main loop: AI
turns, a map mix after a scroll step (~50 ms), window builds. To keep the
cursor moving, `amiga_ui_busy_tick()` is called from many places in shared
code: `GameEventManager` between events, `aui_UI::Draw` between windows,
every 64 rows in `ScrollPixels`, around `UpdateMixFromMap`, after the
background's sprite pass, and during `CivApp::ProcessUI`.

`BusyTick` (at most once every 20 ms):

1. reads the latest mouse position without consuming the events
   (`amiga_input_peek_mouse`; they stay queued for `PumpEvents`);
2. steps the cursor animation;
3. **puts the pending present list aside**, because the UI may have drawn
   half of a frame, and presents **only the cursor**: the old and new cursor
   rectangles, from the current frame contents;
4. sets `amiga_video_cursor_only(1)`, which keeps pending card scrolls for the
   next full present and records the cursor rectangles as touched (section 7);
5. restores the present list.

`ENV:CTP2_PERF` logs the longest gap between two mouse reads and the call
sites on both sides (as offsets from `amiga_log`; resolve them with `nm` or
`addr2line` on `ctp2.debug`). Use this to find a new place that needs a busy
tick.

`aui_AmigaUI::CursorFollow` (move the cursor during a long copy) and
`amiga_video_present_patch` are kept but are not called at the moment.

## 9. Frame pacing (`civ3_main.cpp`)

Each frame of the main loop runs:

1. `PumpEvents()`: IDCMP to mouse, keyboard and `ui_HandleKeypress`, using the
   same key translation as the SDL message handler;
2. `amiga_audio_update()`: refills the AHI ring ahead of the play position;
3. the game and UI processing;
4. `AmigaDrawScreen()`;
5. until 1/30 s has passed: `amiga_input_wait`, then `PumpEvents` and
   `AmigaDrawScreen` again. A **mouse move moves the cursor immediately**,
   and a button, wheel or key ends the wait so the game sees it at once.

Idle mouse events go to the UI 10 times a second instead of 60 (rollovers
and tooltips need them, but each one walks every window's controls, ~6 ms).

## 10. Debugging the pipeline

- **CTRL-F** (or `Break <n> F`) writes three PPMs:
  `RAM:ctp2shot.ppm` (the frame as presented, cursor included),
  `RAM:ctp2card.ppm` (read back from the card under `LockBitMap` and
  converted back to R5G6B5), and `RAM:ctp2shadow.ppm`. If the card differs
  from the shadow, writes were lost. If the shadow differs from the frame,
  the present list or a scroll missed something. Most screen grabbers cannot
  read a 16-bit RTG screen, and `amiga_screenshot` returns grey.
- **Test maps are mostly black** (unexplored), which hides scroll bugs. Always
  compare the card with the frame after scrolling **in every direction**. The
  card-side scroll once never ran because the band search started above the
  screen, and only a card dump showed it.
- `ENV:CTP2_PERF`: "Big present" lines (≥ 50 kpixels, at most 2 per second,
  with the caller) show who sends large updates. Do not count scroll steps
  from them; the Perf line has "N scroll steps". The "Shift:" and "Band:"
  lines show the first few card scrolls and the band found.
- See [performance.md](performance.md) for the counters.

## 11. Checklist for changes

- New drawing code that writes the frame must make sure its rectangles reach
  `AddPresentRect`, through the normal window dirty lists or directly.
- Never write to the card outside `amiga_video_present` without turning the
  shadow off.
- If you scroll the card, also scroll the shadow and the frame, and queue the
  card scroll the way `amiga_video_scroll` does.
- Long loops in shared code: add `amiga_ui_busy_tick()` under
  `#if defined(__AUI_USE_AMIGA__)`.
- Verify with CTRL-F: card = shadow = frame.
- Keep the Linux SDL path byte-for-byte unchanged.

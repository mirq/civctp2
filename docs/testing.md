# Testing

There are no automated tests for the Amiga builds. Testing means running the
game in an emulator or on the real machine and reading `ctp2.log` and the
snapshots.

## Emulator: Amiberry in Xephyr

The amiga-mcp bridge's mouse and click injection is unreliable (timeouts,
lost button-ups). To click through the game, run Amiberry inside a nested X
server and drive it with XTEST:

```sh
Xephyr :7 -screen 1100x860 -ac -nolisten tcp &
DISPLAY=:7 /var/lib/flatpak/exports/bin/com.blitterstudio.amiberry -G --config build/amiberry-xephyr-best.uae
DISPLAY=:7 import -window root shot.png      # screenshot
```

- Configurations in `build/`: `amiberry-xephyr-best.uae` (≈ 68060 @ 50 MHz
  with no JIT, use for timings), `amiberry-xephyr-best-nommu.uae` (without the
  MMU line, needed for `ctp2.wos`), `amiberry-xephyr.uae` (JIT, fast, for WarpOS).
- Scripts in `build/xephyr/`: `xin.py` (type, key, move, click, down/up, abs),
  `launch.sh` (starts the game from the test copy `Dev:CTP2/ctp2_code/ctpx`;
  takes `CTP2_EXTRA_ENV`, `CTP2_STACKCMD`, `CTP2_OUT`, `CFG`, `CTP2_BIN`),
  `togame.sh` (menu, then New Game, then Launch), `f4game.sh` (restart, F4
  quick start, centre the mouse).
- Amiberry takes **relative** mouse motion, at roughly 1:1. Move the mouse,
  take a screenshot, correct. When the host pointer reaches the Xephyr edge,
  further moves are lost; re-centre with `xin.py abs 550 430`.
- Keep the mouse away from the screen edges when measuring, or the map
  scrolls.
- Kill only your own Amiberry instance: check `/proc/PID/cmdline` (it
  contains NULs, so pipe it through `tr "\0" " "`). `pkill -f` matches the
  calling shell too.
- The game reaches the menu in ~95 s and needs about 60 s more to start a
  game. **F4** on the main menu starts a game with the last settings
  (`initialplayscreen_QuickStart`).

## Real hardware (A4000)

- Copy the files over HTTP: run a Python `http.server` on the host and `wget`
  from the Amiga (~1.5 MB/s). For uploads, use a small POST receiver and
  `wget --post-file`.
- Start the game from an execute script (`run <NIL: >NIL: execute
  Data:CTP2/perf.script`, which does `cd`, `setenv CTP2_LOGSYNC 1`, `setenv
  CTP2_PERF 1`, then runs the binary). A plain `run >log cmd` from the bridge
  did not start the program.
- MCP calls longer than 300 s are aborted, so run long jobs in the background
  and poll.
- `amiga_reboot` is fine; the bridge comes back after ~40 s. After a reboot
  the pointer sits at (0,0) and the map edge-scrolls until the mouse is
  moved.
- Keys injected through the bridge reach the game: F4 = `0x53`,
  `b` = `0x35` (found a city), cursor right = `0x4E`. Hold an arrow key to
  test scrolling. Mouse injection is too unreliable for that.

## The card check (CTRL-F)

`Break <n> F` makes the game write `RAM:ctp2shot.ppm` (the frame),
`RAM:ctp2card.ppm` (the card, read back) and `RAM:ctp2shadow.ppm` (the diff's
shadow). Copy them to the host and compare:

- card ≠ shadow: the card does not show what the diff believes (a write that did not land, or a wrong starting state, as with the grey screen);
- shadow ≠ frame: a present or scroll missed a rectangle;
- all equal: the display is correct.

Check at the menu, on the idle map, mid-scroll and after scrolling in
**each** direction, on an explored area: black unexplored maps hide errors.
With `ENV:CTP2_TEST` set, F9 explores the whole map. Look at the pictures
too: when the frame itself is drawn wrong, the three dumps still agree.
See [graphics-pipeline.md](graphics-pipeline.md), section 11.

## The benchmark and the self-check

`setenv CTP2_BENCH 1` (with `CTP2_PERF 1` for the per-phase lines and the
yardstick) and start the game: it starts a game on a fixed world, scrolls,
logs `Bench:` lines and quits, about two and a half minutes in Amiberry.
Leave the pointer away from the screen's edges. Compare two builds by their
`Bench:` lines and by the three pictures it leaves in `RAM:` (`bench_fog`,
`bench_sight`, `bench_end`), which must be identical when only speed
changed.

`setenv CTP2_BENCH win` runs the window benchmark instead of the scrolling:
the science, national and domestic managers are opened and closed three
times each (the first opening builds the window) and moved 24 steps of 8
pixels on the third round. It logs the time of each `Open` call and of the
frame after it, the close, and per move the time and the pixels written to
the card, and writes `RAM:bench_w0..2.ppm` with each window open (to compare
two builds; the pointer's place changes the status bar and tooltips). With
`CTP2_PROF` set it profiles each opening from the second round into
`RAM:prof_<window><round>.bin`. Set `CTP2_UIPRELOAD lazy` for comparable
times: idle building runs in the benchmark's pauses otherwise.

`setenv CTP2_CHECK 8` adds the self-check (`Check:` lines). Expect a few
tens of pixels now and then from the card check and a few thousand after
each scroll, all on sprites; anything on the terrain, or large numbers, is a
fault in the display path. The pictures `RAM:ctp2check<n>.ppm` show where.

## AI regression

The same procedure as on other platforms (CONTRIBUTING.md): start a game,
save, then type `/attach 1` and `/rnd 200` in the chat (`'`). Behaviour
should not change unless intended.

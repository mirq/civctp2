# Call to Power II on AmigaOS: port documentation

Notes on the native AmigaOS port (branch `amiga-port`): three builds (68060,
WarpOS PowerPC, MorphOS) that share one platform layer and use no SDL.

| document | contents |
|---|---|
| [overview.md](overview.md) | targets, toolchains, building, running, ENV: settings, source map, byte order, porting rules, open issues |
| [graphics-pipeline.md](graphics-pipeline.md) | **how a pixel gets from the game to the RTG card**: surfaces, composition, dirty rectangles, the shadow diff, card-side scrolling, the software cursor, busy ticks, invariants |
| [performance.md](performance.md) | measured costs on the A4000 and in Amiberry, what made each speed-up, how to profile (`ENV:CTP2_PERF`) |
| [testing.md](testing.md) | emulator (Amiberry in Xephyr) and real-hardware test loops, the CTRL-F card check |

The short reference that lives next to the code is
[`ctp2_code/os/amiga/README.md`](../ctp2_code/os/amiga/README.md). These
documents go into more depth, mainly on the display path.

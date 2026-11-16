Call to Power II (Apolyton Edition) for AmigaOS (68060, WarpOS, MorphOS), plus a Linux x86_64 build.

## Which file do I need?

| File | What it is |
|---|---|
| **CTP2-Amiga-@VERSION@.lha** | **Start here.** The `CTP2` drawer: the launcher, the three Amiga programs, the Apolyton data files, the README files |
| CTP2-Amiga-@VERSION@-programs-only.lha | Only the launcher and the programs, to update a copy you already installed |
| CTP2-MorphOS-@VERSION@.lha | The same drawer for MorphOS only: the launcher and `ctp2.mos` |
| CTP2-Linux-x86_64-@VERSION@.tar.gz | Linux (SDL2) build |
| SHA256SUMS.txt | Checksums |

## New in 0.2

- **MorphOS** runs (tested in QEMU, pegasos2, MorphOS 3.21), with its own package.
- **Windows draw much faster.** Opaque windows go straight from their own pictures to the card; a moved window is carried by the card's blitter, so only the strip it leaves is sent; the science manager reopens in 0.1 s instead of 2.2 s; panel backgrounds are drawn faster. The look is unchanged.
- **Windows are built while you rest** on the map (one at a time after 1.5 s without input), so their first opening is no longer a wait. The launcher can choose the old behaviour.
- **Launcher 1.1:** new switches "Windows straight to the card" and "Scroll on the card" (turn the latter off on displays whose blitter gets overlapping copies wrong).

## You need the original game

The original graphics, sounds and scenarios are **not** included: Call to Power II is not free software, so you need your own copy, from the CD or from [GOG](https://www.gog.com/game/call_to_power_2). From it you need two folders, about 300 MB together:

- **`ctp2_data`**: unit and city sprites (`graphics/sprites/*.spr`), pictures, cursors, patterns, sounds (`sound/sound.zfs`), the English layouts
- **`Scenarios`**: the scenario packs

Where to find them:
- **CD:** unzip `civmain.ctp` and `civlang.ctp`. They are zip archives; the extensions may differ.
- **GOG:** `innoextract -m -I app setup_call_to_power2_2.0.0.13.exe`, then use the `app` folder. An installed copy has the same folders.

You don't need `ctp2_program`, the music or the videos.

## Installing

1. Unpack `CTP2-Amiga-@VERSION@.lha` (on MorphOS `CTP2-MorphOS-@VERSION@.lha` is enough). It makes the drawer `CTP2`.
2. Copy the original `ctp2_data` and `Scenarios` into `CTP2`, merging them with the folders already there.
3. Unpack the archive over them again, so the updated Apolyton files replace the original ones.
4. Check that these exist: `CTP2/ctp2_data/default/graphics/sprites/GU003.SPR` and `CTP2/ctp2_data/default/sound/sound.zfs`.

On an Amiga it is easiest to prepare the drawer on a PC and copy it over (about 330 MB).

## Playing on the Amiga

Open the `CTP2` drawer and double-click **CTP2** (the launcher). In the launcher:
- **Program:** the 68060, WarpOS or MorphOS version. The launcher picks the right one for your machine.
- **Run C:InitPPC first:** for Sonnet PowerPC cards. Leave it ticked.
- **Screen mode:** the game's resolution. Any 16-bit RTG mode of 800×600 or more; Automatic lets the game choose.
- **Autosave each turn:** on or off.
- Language, sound rate, save compression and memory settings.

Press **Play**. Settings are kept for the next time. CTRL-C in a Shell, or Quit in the game, ends it.

Needs: a 68060 with FPU, a WarpOS PowerPC card or MorphOS; AmigaOS 3.x with an RTG card (Picasso96 or CyberGraphX); 128 MB of fast RAM; AHI for sound (optional). There's more in `README-Amiga.md` inside the archive.

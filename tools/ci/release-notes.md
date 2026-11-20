Call to Power II (Apolyton Edition) for AmigaOS (68060, WarpOS), MorphOS and AROS x86_64, plus a Linux x86_64 build.

## Which file do I need?

| File | What it is |
|---|---|
| **CTP2-Amiga-@VERSION@.lha** | **Start here.** The `CTP2` drawer: the launcher, the three Amiga programs, the Apolyton data files, the README files |
| CTP2-Amiga-@VERSION@-programs-only.lha | Only the launcher and the programs, to update a copy you already installed |
| CTP2-MorphOS-@VERSION@.lha | The same drawer for MorphOS only: the launcher and `ctp2.mos` |
| CTP2-AROS-x86_64-@VERSION@.lha | The drawer for AROS x86_64 (ABIv11, e.g. AROS One x86_64): `ctp2.aros`, no launcher |
| CTP2-Linux-x86_64-@VERSION@.tar.gz | Linux (SDL2) build |
| SHA256SUMS.txt | Checksums |

## New in 0.9

- **Cities no longer starve on a Workbench set to Polish, German or another country that writes numbers with a decimal comma** (68060 and MorphOS versions). The C library took the comma from the system's Locale settings and then read the game's data, which is written with a point, wrongly: "0.85" became 0. Every government's food, production and gold factors were 0, so the city screens showed food, production and gold as 0 and the cities starved, whatever the farmers did. The game now always reads and writes numbers with a point. Until you update: set the country to United Kingdom or USA in Locale prefs, or use the WarpOS version, which was never affected. Thanks to **fazior** and **twardy** for reporting it.

## New in 0.8

- **A new game no longer resets fast machines (PiStorm, Emu68)** - and no longer starts with the wrong settings anywhere. On a new install the launcher wrote a `userprofile.txt` with only the autosave setting in it; the game then took that file instead of its default settings, so the map generators were missing (a new game built its world from uninitialised memory: a reset on a PiStorm, at "Progress 440" or after F4), the computer players were off and there were three players. The launcher now starts the file from the game's defaults, and the game repairs a profile written that way by itself (its own settings for the screen size and autosave are kept). Thanks to the PiStorm users who kept reporting it.
- **WarpOS on a PCI PowerPC card (Sonnet):** the mouse no longer faults, and leaving a game for the main menu no longer stops with a Data Storage fault.
- **One save format** for the 68060, WarpOS and MorphOS versions: a game saved on one loads on the others.
- Settings changed in the game (and the language) are kept after Quit.
- The domestic and science windows built ahead of time (`CTP2_UIPRELOAD`) no longer come up by themselves, empty.
- For bug reports from machines that reset: `CTP2_LOGTO` sends the log to a path that survives a reset (a serial console, or the network with `TCP:<pc>/<port>`), `CTP2_LOGDELAY` and `CTP2_TRACE` (see `README-Amiga.md`).

## New in 0.7

- **AI turns faster again, the same AI.** A route search now checks once per search, not for every map cell, whether the moving army is a civilian one and how it measures up against armies next to its path. On a real A4000 (68060), a late-game round (turn 191, six computer players, enemy moves not shown) takes 126.5 s, against 133.7 s in 0.6 and 144.1 s in 0.5. The computer players still make exactly the same moves (checked save against save).

## New in 0.6

- **AI turns faster again, the same AI.** The route finding's commonest steps are cheaper (the next map cell is worked out in place instead of by a call, and what a search's remembered answers depend on is checked once per search), and each empire's centre is worked out again only when the terrain its routes cross changes, not whenever a road is finished anywhere. A late-game round (turn 191, six computer players, 68060 at 50 MHz, enemy moves not shown) takes 107 s, against 117 s in 0.5 and 251 s before 0.4. The computer players still make exactly the same moves (checked save against save).
- Tip: with **enemy moves shown**, most of a late round is the moves themselves, each shown for a fixed time. Moving the **unit speed** slider (Graphics options) to the right takes a late round from about 230 s to about 160 s.

## New in 0.5

- **Loading a saved game during a game no longer hangs.** Loading a save from inside a running game (and so also going back to the main menu) stopped at the start of the loading bar: the cleanup waited for a network thread that the Amiga versions never start. Thanks to **Fazior** for finding this bug.
- A save loaded during a game now plays on exactly as one loaded from the main menu (a value of the map was never set, so the cities' distances to the capital could be taken from the save or worked out anew, depending on memory).
- **AI turns faster again:** the route finding skips steps that cannot lead to a shorter route, the radar map is drawn only when it is shown, and the screen's changed areas are merged faster. A late-game round (turn 191, six computer players, 68060 at 50 MHz) takes 118 s, against 150 s in 0.4 and 251 s before 0.4. The computer players still make exactly the same moves.

## New in 0.4

- **Faster AI turns, the same AI.** The computer players make exactly the same moves as before (checked save against save), with less work: answers that cannot change during a turn are worked out once instead of over and over, and the 68060 version is compiled harder where the AI spends its time. Measured on a 68060 at 50 MHz with six computer players: a late-game round (turn 191) takes 150 s instead of 251 s (−40 %), a mid-game round (turn 101) about a quarter less. Most of what is left is the AI's route finding.
- Fonts that are loaded again (after changing the screen size or language) are no longer searched for under a wrong name.

## New in 0.3

- **AROS x86_64** (ABIv11): a native build, `ctp2.aros`, with its own package (tested in Linux-hosted AROS 2026.09: menu, new game, turns, quit). It takes a 16-bit screen when the display has one, else a 32-bit one; sound through AHI. Start it from `CTP2/ctp2_code/ctp` (double-click `ctp2.aros`, or from a Shell); settings are the ENV: variables in `README-Amiga.md`.

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

1. Unpack `CTP2-Amiga-@VERSION@.lha` (on MorphOS `CTP2-MorphOS-@VERSION@.lha` is enough, on AROS `CTP2-AROS-x86_64-@VERSION@.lha`). It makes the drawer `CTP2`.
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

## Playing on AROS

Open `CTP2/ctp2_code/ctp` and double-click **ctp2.aros**, or from a Shell: `cd CTP2/ctp2_code/ctp` and `ctp2.aros`. The game's resolution is set in `userprofile.txt` (`ScreenResWidth`, `ScreenResHeight`, 1024×768 by default); `setenv CTP2_MODE <id>` picks a screen mode. Needs AROS x86_64 ABIv11 with a 16 or 32-bit screen mode at least that big; AHI for sound (optional).

Needs: a 68060 with FPU, a WarpOS PowerPC card or MorphOS; AmigaOS 3.x with an RTG card (Picasso96 or CyberGraphX); 128 MB of fast RAM; AHI for sound (optional). There's more in `README-Amiga.md` inside the archive.

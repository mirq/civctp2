Call to Power II - Apolyton Edition
===================================

This archive holds the game program and the Apolyton project's updated data
files. It does NOT hold the original game assets (unit and city graphics,
pictures, cursors, sounds, scenarios): Call to Power II is not free software,
so you need your own copy of the game, from the CD or from GOG
(https://www.gog.com/game/call_to_power_2).


Original files needed
---------------------

From your copy of the game you need its two data folders, about 300 MB:

  ctp2_data   graphics (graphics/sprites/*.spr - the units, cities and
              effects - plus the pictures, cursors and patterns *.tga/*.zfs),
              the sounds (sound/sound.zfs), the English layouts (uidata)
  Scenarios   the scenario packs (AlexanderTheGreat, MagnificentSamurai,
              NuclearDetente, WorldMaps)

Where to find them:

  CD   The CD has two zip archives, civmain.ctp (files of every language)
       and civlang.ctp (your language); the extensions may differ. Unzip
       both: they hold ctp2_data, ctp2_program and Scenarios.

  GOG  Extract the installer, for example on Linux:
         innoextract -m -I app setup_call_to_power2_2.0.0.13.exe
       The "app" folder then holds ctp2_data, ctp2_program and Scenarios.
       An installed GOG or CD copy (on Windows usually
       C:\Program Files\Activision\Call to Power II\) has the same folders.

ctp2_program (the original program) is not needed. Music and videos are not
needed either (the Amiga builds do not play them).

Check: after installing, these files must exist:

  CTP2/ctp2_data/default/graphics/sprites/GU003.SPR
  CTP2/ctp2_data/default/sound/sound.zfs
  CTP2/Scenarios/WorldMaps/packlist.txt


Installing
----------

1. Unpack this archive. It makes a folder (drawer) "CTP2".
2. Copy the original game's "ctp2_data" and "Scenarios" folders into "CTP2",
   so they merge with the ones already there.
3. Unpack this archive over them again, so the updated files from this
   release replace the original ones (the Apolyton data must win).
4. Start it: on the Amiga double-click CTP2 (the launcher) in the CTP2
   drawer; on Linux see below.

On the Amiga it is easiest to do steps 1-3 on a PC and copy the finished
CTP2 folder over (about 330 MB); UnZip and LhA from Aminet do the same on
the Amiga itself.


Running
-------

Linux:   cd CTP2/ctp2_code/ctp && ./ctp2
         Needs SDL2, SDL2_mixer, SDL2_image and libtiff (Debian/Ubuntu:
         libsdl2-2.0-0 libsdl2-mixer-2.0-0 libsdl2-image-2.0-0 libtiff6).

Amiga:   Open the CTP2 drawer and double-click CTP2, the launcher. It sets
         the program (68060, WarpOS or MorphOS: it picks the one for your
         machine), C:InitPPC first for a Sonnet card, the screen mode (the
         game's resolution: any 16-bit RTG mode of 800x600 or more), autosave
         on or off, the language, sound rate, memory and display settings,
         then runs the game. Settings are kept in ctp2_code/ctp/launcher.prefs.

         Without the launcher, from a Shell:
           cd CTP2/ctp2_code/ctp
           ctp2.060 nointromovie        (or ctp2.wos / ctp2.mos)
         On a Sonnet PowerPC card run C:InitPPC first, before ctp2.wos.

         ctp2.060  68060 with FPU, AmigaOS 3.x, RTG card (CyberGraphX/P96)
         ctp2.wos  PowerPC with WarpOS (powerpc.library), RTG card
         ctp2.mos  MorphOS
         128 MB of fast RAM is the target; AHI for sound (optional).
         CTRL-C in the Shell quits. The log is ctp2.log beside the program.
         Requirements and ENV: settings are in README-Amiga.md.

AROS:    (CTP2-AROS-x86_64 archive; AROS x86_64 ABIv11, e.g. AROS One)
         Open CTP2/ctp2_code/ctp and double-click ctp2.aros, or from a Shell:
           cd CTP2/ctp2_code/ctp
           ctp2.aros
         The game's resolution is in userprofile.txt (ScreenResWidth and
         ScreenResHeight, 1024x768); it opens a 16-bit screen of at least that
         size, or a 32-bit one when the display has no 16-bit modes. AHI for
         sound (optional). ENV: settings as on the Amiga (README-Amiga.md).

Source code and bug reports: https://github.com/civctp2/civctp2
Amiga port: https://github.com/mirq/civctp2 (branch amiga-port)

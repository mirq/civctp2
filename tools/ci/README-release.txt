Call to Power II - Apolyton Edition
===================================

This archive holds the game program and the Apolyton project's updated data
files. It does NOT hold the original game assets (graphics, sounds, videos):
you need your own copy of Call to Power II (CD or GOG).

Installing
----------

1. Copy the original game's "ctp2_data" and "Scenarios" folders (from the CD
   or the GOG install) into this "CTP2" folder.
2. Unpack this archive over them again, so the updated files from this
   release replace the original ones.
3. Run the game from CTP2/ctp2_code/ctp/.

Linux:   cd CTP2/ctp2_code/ctp && ./ctp2
         Needs SDL2, SDL2_mixer, SDL2_image and libtiff (Debian/Ubuntu:
         libsdl2-2.0-0 libsdl2-mixer-2.0-0 libsdl2-image-2.0-0 libtiff6).

Amiga:   ctp2.060  68060 with FPU, AmigaOS 3.x, RTG card (CyberGraphX/P96)
         ctp2.wos  PowerPC with WarpOS (powerpc.library)
         ctp2.mos  MorphOS
         Open a Shell, then:
           cd CTP2/ctp2_code/ctp
           stack 2000000
           ctp2.060 nointromovie        (or ctp2.wos / ctp2.mos)
         Requirements and ENV: settings are in README-Amiga.md.

Source code and bug reports: https://github.com/civctp2/civctp2

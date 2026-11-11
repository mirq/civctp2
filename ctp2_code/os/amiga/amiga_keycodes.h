/*
 * The SDL key codes the game's key handling uses, for the Amiga builds.
 *
 * The SDL build hands SDL2 key codes to ui_HandleKeypress (SDLK_UP + 256 for an
 * arrow, SDLK_DELETE, ...) and aui_textfield.cpp compares against them. The
 * Amiga event pump (ui/aui_amiga/aui_amigaui.cpp) produces the same numbers,
 * so that code is shared unchanged. The values are SDL2's (SDL_keycode.h).
 */
#ifndef CTP2_AMIGA_KEYCODES_H
#define CTP2_AMIGA_KEYCODES_H 1

#define CTP2_SDLK_SCANCODE_MASK (1 << 30)
#define CTP2_SDLK_FROM_SCANCODE(x) ((x) | CTP2_SDLK_SCANCODE_MASK)

#define SDLK_RETURN    '\r'
#define SDLK_ESCAPE    '\033'
#define SDLK_BACKSPACE '\b'
#define SDLK_TAB       '\t'
#define SDLK_SPACE     ' '
#define SDLK_DELETE    '\177'

#define SDLK_RIGHT     CTP2_SDLK_FROM_SCANCODE(79)
#define SDLK_LEFT      CTP2_SDLK_FROM_SCANCODE(80)
#define SDLK_DOWN      CTP2_SDLK_FROM_SCANCODE(81)
#define SDLK_UP        CTP2_SDLK_FROM_SCANCODE(82)
#define SDLK_SELECT    CTP2_SDLK_FROM_SCANCODE(119)
#define SDLK_CUT       CTP2_SDLK_FROM_SCANCODE(123)
#define SDLK_COPY      CTP2_SDLK_FROM_SCANCODE(124)
#define SDLK_PASTE     CTP2_SDLK_FROM_SCANCODE(125)

#endif

//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Amiga backend: the UI, its RTG screen and the frame copy
//
//----------------------------------------------------------------------------
//
// Disclaimer
//
// THIS FILE IS NOT GENERATED OR SUPPORTED BY ACTIVISION.
//
// This material has been developed at apolyton.net by the Apolyton CtP2
// Source Code Project. Contact the authors at ctp2source@apolyton.net.
//
//----------------------------------------------------------------------------
//
// The game composes every frame in m_secondary, in main memory. The 68060
// writes to a Zorro III card at about 5 MB/s (a whole 800x600x16 frame would
// take some 180 ms), so only what changed crosses the bus: aui_AmigaMouse
// records each rectangle it puts on the secondary surface (AddPresentRect),
// and AmigaDrawScreen copies those rectangles from there, with the software
// cursor drawn over them, to the card. m_primary only takes the full-screen
// copies of the secondary surface some screens make (the splash screen).
//
//----------------------------------------------------------------------------

#ifndef __AUI_AMIGAUI_H__
#define __AUI_AMIGAUI_H__

#ifdef __AUI_USE_AMIGA__

#include <vector>

#include "aui_ui.h"
#include "aui_amiga.h"

class aui_Cursor;

class aui_AmigaUI : public aui_UI, public aui_Amiga
{
public:
	aui_AmigaUI(
		AUI_ERRCODE *retval,
		HINSTANCE hinst,
		HWND hwnd,
		sint32 width,
		sint32 height,
		sint32 bpp,
		const MBCHAR *ldlFilename = NULL,
		BOOL useExclusiveMode = FALSE );

	virtual ~aui_AmigaUI();

protected:
	aui_AmigaUI()
	:   aui_UI   (),
	    aui_Amiga()
	{ ; };

	AUI_ERRCODE InitCommon( void );

public:
	virtual AUI_ERRCODE AltTabOut( void );
	virtual AUI_ERRCODE AltTabIn( void );

	AUI_ERRCODE CreateNativeScreen( BOOL useExclusiveMode );
	AUI_ERRCODE DestroyNativeScreen(void);

	AUI_ERRCODE TearDownMouse(void);
	AUI_ERRCODE RestoreMouse(void);

	void SetWidth(sint32 width) { m_width = width; }
	void SetHeight(sint32 height) { m_height = height; }
	AUI_ERRCODE ChangeSize(sint32 width, sint32 height);

	aui_MovieManager* CreateMovieManager( void );

	// What changed on the screen surfaces since the last frame copy, in
	// screen coordinates.
	void AddPresentRect(const RECT &rect);
	void AddPresentAll(void);

	virtual AUI_ERRCODE AmigaDrawScreen( void );

	// The software cursor: the image (with its chroma key) and its hot spot,
	// and where the pointer is. Moving it shows the area it left and the area
	// it covers, nothing else. The image is copied: the game unloads and
	// reloads cursor surfaces (a new mouse, a screen change), so a pointer to
	// one would go stale. owner is the mouse that set it; ClearCursorImage
	// from another (an old mouse deleted after its successor) does nothing.
	void SetCursorImage(aui_Surface *surface, sint32 hotX, sint32 hotY, const void *owner);
	void ClearCursorImage(const void *owner);
	void SetCursorPosition(sint32 x, sint32 y);

	// Takes what the window and the keyboard reported to the mouse, the
	// keyboard and the game's key handler. Returns FALSE on CTRL-C or quit.
	// pressed, when given, tells whether a button, the wheel or a key came:
	// what the game should see at once, where a move only moves the cursor.
	BOOL PumpEvents(bool *pressed = NULL);

	// While the game is busy outside the main loop (an AI turn, between its
	// events): the cursor follows the mouse and animates, nothing else
	// changes on the screen and no event is handled. At most every 20 ms.
	void BusyTick(void);

private:
	void CursorRect(RECT &rect) const;
	void PresentRect(const RECT &rect);

	std::vector<RECT>   m_present;
	bool                m_presentAll;

	uint16 *            m_cursorImage;   // a copy of the cursor's pixels
	size_t              m_cursorImageSize;
	sint32              m_cursorWidth;
	sint32              m_cursorHeight;
	uint16              m_cursorKey;
	const void *        m_cursorOwner;
	sint32              m_cursorHotX;
	sint32              m_cursorHotY;
	sint32              m_cursorX;
	sint32              m_cursorY;
	RECT                m_cursorShown;   // where the cursor is on the card
	bool                m_cursorVisible;

	uint16 *            m_composite;     // a cursor-sized patch: frame + cursor
	size_t              m_compositeSize;
};

typedef aui_AmigaUI aui_NativeUI;

#endif // __AUI_USE_AMIGA__

#endif

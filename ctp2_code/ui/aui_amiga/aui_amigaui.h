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
class aui_Window;

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
	void ScrollFrame(sint32 x, sint32 y, sint32 w, sint32 h, sint32 dx, sint32 dy);

	// The map's window (the background): where no other window covers it,
	// the card gets its pixels straight from its own surface, and they are
	// not copied into the composed frame first (ENV:CTP2_DIRECTMAP, 0: as
	// before). NULL when the window goes away.
	void SetDirectWindow(aui_Window *window);
	// Before the windows' changes are put together (aui_UI::Draw): which
	// parts of the map's window are open now.
	void ComposeBegin(void);
	// Whether rect (screen) of window goes to the card from the window's own
	// surface: the composition then only marks it.
	bool IsDirectRect(aui_Window *window, const RECT &rect) const;

	// The other opaque windows (no stencil, not transparent or translucent)
	// go to the card from their own surfaces too, where no window above
	// covers them (ENV:CTP2_DIRECTWIN, 0: through the frame as before). The
	// look is the same: only where the pixels are read from changes.
	// Whether any pixels go to the card from a surface of their own (the
	// map or a direct window); out then holds the parts of rect (screen)
	// that still have to be copied into the frame from window (often none:
	// the frame is not read where a surface of its own shows).
	bool FramePieces(aui_Window *window, const RECT &rect, std::vector<RECT> &out) const;

	// An opaque window moved (screen rectangles): its pixels move on the
	// card (ENV:CTP2_MOVEBLIT, 0: sent again instead).
	void WindowMoved(aui_Window *window, const RECT &oldRect, const RECT &newRect);

private:
	// An opaque window that goes to the card from its own surface.
	struct DirectWindow
	{
		aui_Window *      window;
		aui_Surface *     surface;
		uint8 *           pixels;   // its surface's first pixel at the compose
		sint32            x, y;     // where it was then
		std::vector<RECT> rects;    // its parts no window above covers, screen
	};
	// Which windows go direct, as the windows stand now (ComposeBegin).
	void FindDirectWindows(void);
	// Whether they still stand as they did at the compose (a copy between
	// two composes, for the cursor, must not read a window that went).
	bool DirectWindowsValid(void) const;

	// One picture the card gets pixels from, besides the frame.
	struct Source
	{
		uint16 *                  base;    // its screen pixel (0, 0)
		sint32                    pitch;
		const std::vector<RECT> * rects;   // where it shows, screen
	};
	// The map and the direct windows, as sources (none in a busy tick).
	void CollectSources(std::vector<Source> &out, bool direct, uint16 *mapBase, sint32 mapPitch) const;
	// Row y of the picture as it should be on the card: the frame, and the
	// sources where they show.
	void PictureRow(sint32 y, uint16 *row, const uint16 *frame, sint32 pitch,
	                const std::vector<Source> &sources) const;

private:
	void CursorRect(RECT &rect) const;
	void PresentRect(const RECT &rect);
	// The cursor's pixels over rect (screen) of a picture whose screen
	// pixel (0, 0) is at base; what lay there goes to saved (rect's size).
	void CursorInto(uint16 *base, sint32 pitch, const RECT &rect, uint16 *saved) const;
	void CursorOutOf(uint16 *base, sint32 pitch, const RECT &rect, const uint16 *saved) const;
	// The map window's surface as a picture of the screen, and the window's
	// part of the screen. false: not used now.
	bool DirectSource(uint16 *&base, sint32 &pitch, RECT &rect) const;
	// The cursor alone, from what the card shows (BusyTick).
	void BusyCursor(void);

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
	bool                m_busyTick;      // inside BusyTick: a copy of the cursor alone

	uint16 *            m_composite;     // what the cursor covers in the frame
	uint16 *            m_compositeMap;  // and in the map's surface
	uint16 *            m_under;         // what the card would show without the cursor
	uint16 *            m_patchOld;      // BusyCursor: where the cursor was,
	uint16 *            m_patchNew;      //             where it is,
	uint16 *            m_underNew;      //             and what it covers there
	size_t              m_compositeSize; // pixels in each of these
	bool                m_underValid;    // m_under holds m_cursorShown's pixels
	bool                m_cursorChanged; // another image since the card got the cursor

	aui_Window *        m_directWindow;
	bool                m_directOn;
	std::vector<RECT>   m_open;          // the open parts of the map's window, screen

	std::vector<DirectWindow> m_windows; // the direct windows (the first m_windowCount)
	size_t              m_windowCount;
	sint32              m_directWanted;  // ENV:CTP2_DIRECTWIN; -1: not read yet
	std::vector<RECT>   m_directBefore;  // what went direct after the last compose
	std::vector<RECT>   m_covered;       // scratch for FindDirectWindows

	// What the cursor covers in each source, while it is drawn there.
	struct CursorPiece
	{
		uint16 * base;
		sint32   pitch;
		RECT     rect;
		size_t   save;                   // which of m_cursorSaves
	};
	std::vector<CursorPiece>          m_cursorPieces;
	std::vector<std::vector<uint16> > m_cursorSaves;
};

typedef aui_AmigaUI aui_NativeUI;

#endif // __AUI_USE_AMIGA__

#endif

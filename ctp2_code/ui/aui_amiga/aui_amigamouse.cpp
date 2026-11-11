//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : Amiga backend: the mouse
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
// Follows aui_SDLMouse (ui/aui_sdl/aui_sdlmouse.cpp). The differences:
//
// - Events arrive from aui_AmigaUI::PumpEvents on the main task, so no lock.
// - The cursor is drawn by aui_AmigaUI over the frame; ActivateCursor hands it
//   the image, Tick steps the animation (the SDL build used a timer thread).
// - What is drawn on the secondary surface goes to the card from there, only
//   the rectangles that changed (aui_amigaui.h), where the SDL build copied
//   the whole surface to the primary one and then to the screen.
//
//----------------------------------------------------------------------------

#include "ctp2_config.h"
#include "c3.h"

#ifdef __AUI_USE_AMIGA__

#include "aui_ui.h"
#include "aui_amigamouse.h"
#include "aui_amigaui.h"
#include "aui_cursor.h"
#include "aui_dirtylist.h"
#include "aui_image.h"
#include "aui_window.h"
#include "civapp.h"
#include "profileDB.h"

#include "amiga_platform.h"

extern CivApp *          g_civApp;

sint32                   aui_Mouse::m_mouseRefCount = 0;

aui_AmigaMouse::aui_AmigaMouse
(
	AUI_ERRCODE *   retval,
	const MBCHAR *  ldlBlock,
	BOOL            useExclusiveMode
)
:
	aui_Input            (),
	aui_Mouse            (retval, ldlBlock),
	aui_AmigaInput       (retval, useExclusiveMode),
	m_currentCursor      (NULL),
	m_lastFrameTick      (0),
	m_nextAnimTick       (0),
	m_queued             (0)
{
	Assert(AUI_SUCCESS(*retval));
	if (!AUI_SUCCESS(*retval)) return;

	int x = 0, y = 0;
	amiga_input_mouse(&x, &y);
	m_data.position.x = x;
	m_data.position.y = y;

	m_mouseRefCount++;
}

aui_AmigaMouse::~aui_AmigaMouse()
{
	--m_mouseRefCount;

	aui_AmigaUI * ui = static_cast<aui_AmigaUI *>(g_ui);
	if (ui) ui->ClearCursorImage(this);

	End();
}

AUI_ERRCODE aui_AmigaMouse::HandleAnim()
{
	if (m_lastIndex <= m_firstIndex) return AUI_ERRCODE_OK;

	if (m_curCursor++ >= m_cursors + m_lastIndex)
	{
		m_curCursor = m_cursors + m_firstIndex;
	}

	ActivateCursor(*m_curCursor);
	return AUI_ERRCODE_HANDLED;
}

void aui_AmigaMouse::Tick(uint32 now)
{
	if (m_firstIndex != m_lastIndex && m_animDelay > 0 && now >= m_nextAnimTick)
	{
		HandleAnim();
		m_nextAnimTick = now + m_animDelay;
	}
}

void aui_AmigaMouse::Queue(uint32 time)
{
	m_data.time  = time;
	m_data.flags = m_flags;
	ManipulateInputs(&m_data, TRUE);
}

void aui_AmigaMouse::Moved(sint32 x, sint32 y, uint32 time)
{
	m_data.position.x = x;
	m_data.position.y = y;
	Queue(time);
}

void aui_AmigaMouse::Button(sint32 button, BOOL down, uint32 time)
{
	bool const leftHanded = g_theProfileDB && g_theProfileDB->GetLeftHandedMouse();

	switch (button)
	{
	case 0:
		if (leftHanded) m_data.rbutton = down; else m_data.lbutton = down;
		break;
	case 1:
		if (leftHanded) m_data.lbutton = down; else m_data.rbutton = down;
		break;
	case 2:
		m_data.mbutton = down;
		break;
	default:
		break;
	}

	Queue(time);
}

sint32 aui_AmigaMouse::ManipulateInputs(aui_MouseEvent *data, BOOL add)
{
	sint32 numManipulated = 0;

	if (add)
	{
		if (m_queued <= k_MOUSE_MAXINPUT)
		{
			// Two moves in a row to the same place with the same buttons are
			// one, as in the SDL and DirectX mice.
			if (m_queued > 1)
			{
				aui_MouseEvent *event = m_inputs + m_queued - 2;
				if (event->position.x == data->position.x
				&&  event->position.y == data->position.y
				&&  event->lbutton == data->lbutton
				&&  event->rbutton == data->rbutton)
				{
					event++;
					if (event->position.x == data->position.x
					&&  event->position.y == data->position.y
					&&  event->lbutton == data->lbutton
					&&  event->rbutton == data->rbutton)
						m_queued--;
				}
			}

			if (m_queued < k_MOUSE_MAXINPUT)
			{
				memcpy(m_inputs + m_queued, data, sizeof(aui_MouseEvent));
				numManipulated = 1;
				m_queued++;
			}
		}
	}
	else
	{
		if (m_queued)
			memcpy(data, m_inputs, m_queued * sizeof(aui_MouseEvent));

		numManipulated = m_queued;
		m_queued = 0;
	}

	uint32 const currentFrameTick = amiga_ticks_ms();

	if (numManipulated > 0)
	{
		m_lastFrameTick = currentFrameTick;
	}
	else if (!add)
	{
		// The UI wants a mouse event now and then even when nothing moved
		// (rollovers, timed tooltips), as the SDL mouse gives it.
		uint32 const TICKS_PER_FRAME = 1000 / 60;
		if (currentFrameTick > m_lastFrameTick + TICKS_PER_FRAME)
		{
			m_lastFrameTick = currentFrameTick;
			data[0] = m_data;
			data[0].time = currentFrameTick;
			numManipulated = 1;
		}
	}

	return numManipulated;
}

void aui_AmigaMouse::SetAnimIndexes(sint32 firstIndex, sint32 lastIndex)
{
	if (firstIndex == m_firstIndex && lastIndex == m_lastIndex)
		return;

	aui_Mouse::SetAnimIndexes(firstIndex, lastIndex);
	m_nextAnimTick = amiga_ticks_ms() + m_animDelay;
}

void aui_AmigaMouse::ActivateCursor(aui_Cursor *cursor)
{
	// Not skipped when the cursor is the last one: a cursor freed and loaded
	// again can come back at the same address with another image.
	if (!cursor) return;

	POINT hotspot;
	cursor->GetHotspot(hotspot);

	aui_AmigaUI * ui = static_cast<aui_AmigaUI *>(g_ui);
	if (ui)
	{
		ui->SetCursorImage(cursor->TheSurface(), hotspot.x, hotspot.y, this);
	}

	m_currentCursor = cursor;
}

void aui_AmigaMouse::Show(const RECT &screenRect)
{
	// The secondary surface goes to the card itself (AmigaDrawScreen): what
	// was drawn there only has to be marked, where a copy to the primary
	// surface first cost a pass over every changed pixel.
	static_cast<aui_AmigaUI *>(g_ui)->AddPresentRect(screenRect);
}

AUI_ERRCODE	aui_AmigaMouse::BltWindowToPrimary(aui_Window *window)
{
	AUI_ERRCODE retcode = AUI_ERRCODE_OK;
	sint32 windowX = window->X();
	sint32 windowY = window->Y();
	aui_Surface *windowSurface = window->TheSurface();
	aui_DirtyList *windowDirtyList = window->GetDirtyList();

	ListPos position = windowDirtyList->GetHeadPosition();
	for (size_t j = windowDirtyList->L(); j; j--)
	{
		RECT *windowDirtyRect = windowDirtyList->GetNext(position);
		RECT screenDirtyRect = *windowDirtyRect;
		OffsetRect(&screenDirtyRect, windowX, windowY);

		AUI_ERRCODE errcode = g_ui->BltToSecondary(
				screenDirtyRect.left,
				screenDirtyRect.top,
				windowSurface,
				windowDirtyRect,
				k_AUI_BLITTER_FLAG_COPY );

		Assert( errcode == AUI_ERRCODE_OK );
		if ( errcode != AUI_ERRCODE_OK )
		{
			retcode = AUI_ERRCODE_BLTFAILED;
			break;
		}

		Show(screenDirtyRect);
	}

	return retcode;
}

AUI_ERRCODE	aui_AmigaMouse::BltDirtyRectInfoToPrimary()
{
	AUI_ERRCODE retcode = AUI_ERRCODE_OK;

	if (g_civApp->IsInBackground()) return AUI_ERRCODE_OK;

	tech_WLList<aui_UI::DirtyRectInfo *> *driList = g_ui->GetDirtyRectInfoList();

	uint32 const blitFlags = k_AUI_BLITTER_FLAG_COPY | k_AUI_BLITTER_FLAG_FAST;

	ListPos position = driList->GetHeadPosition();
	for (size_t j = driList->L(); j; j--)
	{
		aui_UI::DirtyRectInfo *dri = driList->GetNext(position);
		aui_Window *window = dri->window;
		aui_Surface *windowSurface = window->TheSurface();
		if (!windowSurface) continue;

		RECT *windowDirtyRect = &dri->rect;
		RECT screenDirtyRect = *windowDirtyRect;
		OffsetRect(&screenDirtyRect, window->X(), window->Y());

		AUI_ERRCODE errcode = g_ui->BltToSecondary(
				screenDirtyRect.left,
				screenDirtyRect.top,
				windowSurface,
				windowDirtyRect,
				blitFlags);
		Assert(errcode == AUI_ERRCODE_OK);
		if (errcode != AUI_ERRCODE_OK)
		{
			retcode = AUI_ERRCODE_BLTFAILED;
			break;
		}

		Show(screenDirtyRect);
	}

	return retcode;
}

AUI_ERRCODE	aui_AmigaMouse::BltBackgroundColorToPrimary(COLORREF color, aui_DirtyList *colorAreas)
{
	if (g_civApp->IsInBackground()) return AUI_ERRCODE_OK;

	ListPos position = colorAreas->GetHeadPosition();
	for (size_t j = colorAreas->L(); j; j--)
	{
		RECT *screenDirtyRect = colorAreas->GetNext(position);

		AUI_ERRCODE errcode = g_ui->ColorBltToSecondary(screenDirtyRect, color, 0);
		Assert(errcode == AUI_ERRCODE_OK);
		if (errcode != AUI_ERRCODE_OK)
		{
			return AUI_ERRCODE_BLTFAILED;
		}

		Show(*screenDirtyRect);
	}

	return AUI_ERRCODE_OK;
}

AUI_ERRCODE	aui_AmigaMouse::BltBackgroundImageToPrimary(aui_Image *image, RECT *imageRect, aui_DirtyList *imageAreas)
{
	if (g_civApp->IsInBackground()) return AUI_ERRCODE_OK;

	ListPos position = imageAreas->GetHeadPosition();
	for ( size_t j = imageAreas->L(); j; j-- )
	{
		RECT*         rectPtr = imageAreas->GetNext(position);
		RECT  screenDirtyRect = *rectPtr;
		OffsetRect(&screenDirtyRect, -imageRect->left, -imageRect->top);

		AUI_ERRCODE errcode = g_ui->BltToSecondary(
			rectPtr->left,
			rectPtr->top,
			image->TheSurface(),
			&screenDirtyRect,
			k_AUI_BLITTER_FLAG_COPY);

		Assert(errcode == AUI_ERRCODE_OK);
		if (errcode != AUI_ERRCODE_OK)
		{
			return AUI_ERRCODE_BLTFAILED;
		}

		Show(*rectPtr);
	}

	return AUI_ERRCODE_OK;
}

#endif // __AUI_USE_AMIGA__

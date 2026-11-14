//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : Amiga backend: surfaces in fast RAM
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

#include "ctp2_config.h"
#include "c3.h"

#ifdef __AUI_USE_AMIGA__

#include "aui_ui.h"
#include "aui_uniqueid.h"
#include "aui_amigasurface.h"
#include "aui_amigaui.h"

uint32 aui_AmigaSurface::m_AmigaSurfaceClassId = aui_UniqueId();

aui_AmigaSurface::aui_AmigaSurface
(
	AUI_ERRCODE *   retval,
	sint32          width,
	sint32          height,
	sint32          bpp,
	BOOL            isPrimary
)
:
	aui_Surface (),
	aui_Amiga   (),
	m_pixels    (NULL),
	m_block     (NULL),
	m_slackX    (0),
	m_slackY    (0),
	m_originX   (0),
	m_originY   (0)
{
	*retval = aui_Surface::InitCommon(width, height, bpp, isPrimary);
	Assert(AUI_SUCCESS(*retval));
	if (!AUI_SUCCESS(*retval)) return;

	// Rows on 4-byte boundaries, as aui_Surface's own buffers: the blitters
	// copy whole longwords.
	m_pitch = m_bytewidth + (uint32(-m_bytewidth) & 0x3);
	m_size  = m_pitch * m_height;

	m_pixels = reinterpret_cast<uint8 *>(new uint32[(m_size + 3) >> 2]);
	if (!m_pixels)
	{
		*retval = AUI_ERRCODE_MEMALLOCFAILED;
		return;
	}
	memset(m_pixels, 0, m_size);
	m_block = m_pixels;

	// As with SDL, the buffer is always there: the generic blitters draw
	// into it without the lock list.
	m_buffer    = m_pixels;
	m_allocated = FALSE; // m_pixels is ours, not aui_Surface's m_saveBuffer
}

aui_AmigaSurface::~aui_AmigaSurface()
{
	delete [] reinterpret_cast<uint32 *>(m_block);
	m_block  = NULL;
	m_pixels = NULL;
	m_buffer = NULL;
}

AUI_ERRCODE aui_AmigaSurface::Lock(RECT *rect, LPVOID *buffer, DWORD flags)
{
	*buffer = m_pixels;
	m_saveBuffer = m_pixels;

	if (rect != NULL)
	{
		*buffer = static_cast<uint8 *>(*buffer) +
		    rect->top * m_pitch +
		    rect->left * m_Bpp;
	}

	return ManipulateLockList(rect, buffer, AUI_SURFACE_LOCKOP_ADD);
}

AUI_ERRCODE aui_AmigaSurface::Unlock(LPVOID buffer)
{
	AUI_ERRCODE errcode =
		ManipulateLockList(NULL, &buffer, AUI_SURFACE_LOCKOP_REMOVE);

	m_saveBuffer = NULL;

	return errcode;
}

AUI_ERRCODE aui_AmigaSurface::Blank(const uint32 &color)
{
	if (!m_pixels) return AUI_ERRCODE_BLTFAILED;

	uint16 const pixel = static_cast<uint16>(color);

	for (sint32 y = 0; y < m_height; ++y)
	{
		uint16 * row = reinterpret_cast<uint16 *>(m_pixels + y * m_pitch);
		for (sint32 x = 0; x < m_width; ++x)
		{
			row[x] = pixel;
		}
	}

	return AUI_ERRCODE_OK;
}

BOOL aui_AmigaSurface::EnsureSlack(sint32 slackX, sint32 slackY)
{
	slackX &= ~1;       // the rows stay on longword boundaries
	if (slackX < 0 || slackY < 0 || !m_block) return FALSE;
	if (slackX == m_slackX && slackY == m_slackY) return TRUE;
	if (m_locksRemain != k_SURFACE_MAXLOCK) return FALSE;
	if (HasSlack()) return FALSE;   // set once

	sint32 const  oldPitch = m_pitch;
	sint32 const  pitch    = oldPitch + slackX * 2 * m_Bpp;
	size_t const  size     = static_cast<size_t>(pitch) * (m_height + 2 * slackY);
	uint8 * const block    = reinterpret_cast<uint8 *>(new uint32[(size + 3) >> 2]);
	if (!block) return FALSE;
	memset(block, 0, size);

	uint8 * const pixels = block + slackY * pitch + slackX * m_Bpp;
	for (sint32 y = 0; y < m_height; ++y)
	{
		memcpy(pixels + y * pitch, m_pixels + y * oldPitch, static_cast<size_t>(m_width) * m_Bpp);
	}

	delete [] reinterpret_cast<uint32 *>(m_block);
	m_block   = block;
	m_pixels  = pixels;
	m_pitch   = pitch;
	m_size    = pitch * m_height;
	m_slackX  = slackX;
	m_slackY  = slackY;
	m_originX = slackX;
	m_originY = slackY;
	if (m_buffer) m_buffer = m_pixels;

	return TRUE;
}

BOOL aui_AmigaSurface::ScrollOrigin(sint32 dx, sint32 dy, BOOL clearUncovered)
{
	if (!HasSlack() || m_locksRemain != k_SURFACE_MAXLOCK) return FALSE;
	if (dx & 1) return FALSE;
	if (abs(dx) >= m_width || abs(dy) >= m_height) return FALSE;

	sint32 nx = m_originX + dx;
	sint32 ny = m_originY + dy;

	if (nx < 0 || nx > 2 * m_slackX || ny < 0 || ny > 2 * m_slackY)
	{
		// Out of room that way: the picture as it will be (what stays of
		// it) goes to the far side, once, and the room is whole again.
		sint32 const tx = (nx < 0) ? 2 * m_slackX : (nx > 2 * m_slackX) ? 0 : nx;
		sint32 const ty = (ny < 0) ? 2 * m_slackY : (ny > 2 * m_slackY) ? 0 : ny;

		// Of the picture at (nx, ny), the columns and rows inside the block.
		sint32 const blockW = m_width + 2 * m_slackX;
		sint32 const blockH = m_height + 2 * m_slackY;
		sint32 const c0 = (nx < 0) ? -nx : 0;
		sint32 const c1 = (nx + m_width > blockW) ? blockW - nx : m_width;
		sint32 const r0 = (ny < 0) ? -ny : 0;
		sint32 const r1 = (ny + m_height > blockH) ? blockH - ny : m_height;

		if (c0 < c1 && r0 < r1)
		{
			size_t const bytes = static_cast<size_t>(c1 - c0) * m_Bpp;
			uint8 * const src  = m_block + ny * m_pitch + (nx + c0) * m_Bpp;
			uint8 * const dst  = m_block + ty * m_pitch + (tx + c0) * m_Bpp;
			if (dst <= src)
			{
				for (sint32 r = r0; r < r1; ++r)
					memmove(dst + r * m_pitch, src + r * m_pitch, bytes);
			}
			else
			{
				for (sint32 r = r1 - 1; r >= r0; --r)
					memmove(dst + r * m_pitch, src + r * m_pitch, bytes);
			}
		}
		nx = tx;
		ny = ty;
	}

	m_originX = nx;
	m_originY = ny;
	m_pixels  = m_block + ny * m_pitch + nx * m_Bpp;
	if (m_buffer) m_buffer = m_pixels;

	if (clearUncovered)
	{
		if (dx)
		{
			sint32 const  cols  = abs(dx);
			uint8 * const first = m_pixels + ((dx > 0) ? (m_width - cols) * m_Bpp : 0);
			for (sint32 y = 0; y < m_height; ++y)
				memset(first + y * m_pitch, 0, static_cast<size_t>(cols) * m_Bpp);
		}
		if (dy)
		{
			sint32 const rows  = abs(dy);
			sint32 const first = (dy > 0) ? m_height - rows : 0;
			for (sint32 y = first; y < first + rows; ++y)
				memset(m_pixels + y * m_pitch, 0, static_cast<size_t>(m_width) * m_Bpp);
		}
	}

	return TRUE;
}

void aui_AmigaSurface::Flip()
{
	aui_AmigaUI * ui = static_cast<aui_AmigaUI *>(g_ui);
	if (ui && IsPrimary())
	{
		ui->AddPresentAll();
		ui->AmigaDrawScreen();
	}
}

#endif // __AUI_USE_AMIGA__

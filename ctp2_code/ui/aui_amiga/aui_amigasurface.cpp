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
	m_pixels    (NULL)
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

	// As with SDL, the buffer is always there: the generic blitters draw
	// into it without the lock list.
	m_buffer    = m_pixels;
	m_allocated = FALSE; // m_pixels is ours, not aui_Surface's m_saveBuffer
}

aui_AmigaSurface::~aui_AmigaSurface()
{
	delete [] reinterpret_cast<uint32 *>(m_pixels);
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

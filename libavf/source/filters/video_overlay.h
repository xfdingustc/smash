
#ifndef __VIDEO_OVERLAY_H__
#define __VIDEO_OVERLAY_H__

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

#include "list.h"

class CFontCache;
class CVideoOverlay;

//-----------------------------------------------------------------------
//
//  CFontCache
//
//-----------------------------------------------------------------------
class CFontCache
{
public:
	CFontCache(const char *font_filename, avf_size_t memLimit);
	~CFontCache();
	avf_status_t Status() { return mStatus; }

public:
	typedef avf_u32_t hash_val_t;

	struct Item {
		list_head_t list_hash;	// in hash list
		list_head_t list_pool;	// in the cache pool

		avf_u32_t c;
		hash_val_t hash_val;
		avf_u16_t font_size;

		avf_s16_t bitmap_y_off;
		avf_s16_t bitmap_left;	// may be negative
		avf_s16_t bitmap_width;
		avf_s16_t bitmap_pitch;
		avf_s16_t bitmap_rows;
		avf_s16_t bitmap_advance;
		avf_u16_t mem_size;		// Item + bitmap; should < 64K

		// follows bitmap data

		INLINE avf_u8_t *GetData() {
			return (avf_u8_t*)this + sizeof(Item);
		}

		INLINE const avf_u8_t *GetData() const {
			return (const avf_u8_t*)this + sizeof(Item);
		}

		INLINE void Release() {
			avf_free(this);
		}
	};

public:
	Item* RenderChar(avf_u32_t c, avf_uint_t font_size);
	INLINE static hash_val_t Hash(avf_u32_t c, avf_uint_t font_size) {
		return c ^ font_size;
	}

private:
	Item *CreateItem(avf_u32_t c, avf_uint_t font_size);
	FT_Glyph CreateGlyph(avf_u32_t c, avf_uint_t font_size);
	Item* ConvertToItem(FT_Glyph glyph, int border_size, FT_Render_Mode render_mode);
	bool TransformGlyph(FT_Glyph *pglyph);
	void AdjustItemColor(Item *pItem, const avf_u8_t *tbl);
	void ItemAdd(avf_u32_t c, Item *to, const Item *from);

	static void CopyMonoData(avf_u8_t *ptr, const avf_u8_t *psrc, 
		avf_size_t width, avf_size_t rows,
		avf_size_t pitch, avf_size_t src_pitch);

private:
	enum { HASH_SIZE = 256 };

	avf_status_t mStatus;
	FT_Library mFontLib;
	FT_Face mFontFace;

	avf_uint_t mFontSize;
	avf_size_t mMemLimit;
	avf_size_t mCurrMem;
	avf_uint_t mCacheHit;
	avf_uint_t mCacheMiss;
	list_head_t mPoolList;
	list_head_t mHashList[HASH_SIZE];
};

// 8-bit bitmap
struct bmp8_data_s
{
	avf_atomic_t ref_count;

	avf_u8_t *clut;
	avf_u8_t *data;

	avf_uint_t width;
	avf_uint_t height;
	avf_uint_t stride;

	avf_u8_t tc_index;	// transparent color

	bmp8_data_s();
	~bmp8_data_s();

	void AddRef() {
		avf_atomic_inc(&ref_count);
	}

	void Release() {
		if (avf_atomic_dec(&ref_count) == 1) {
			avf_delete this;
		}
	}

	static bmp8_data_s *LoadFromFile(const char *filename);
	static bmp8_data_s *LoadFromIO(IAVIO *io);
	static bmp8_data_s *LoadFromBuffer(CReadBuffer *buffer, const avf_u8_t *mem);
};

//-----------------------------------------------------------------------
//
//  CVideoOverlay
//
//-----------------------------------------------------------------------
class CVideoOverlay
{
public:
	CVideoOverlay(IEngine *pEngine);
	~CVideoOverlay();
	avf_status_t Status() { return mStatus; }

public:
	enum {
		TEXT_AREA = 0,
		LOGO_AREA = 1,
	};

	struct config_t {
		bool	valid;
		int		x_off;
		int		y_off;
		int		width;
		int		height;
		int		font_size;	// not used for logo
	};

	struct area_paint_info_t {
		avf_uint_t	font_size;	// not used for logo
		bmp8_data_s	*bmp;		// not used for logo

		area_paint_info_t(): font_size(0), bmp(NULL) {}
		~area_paint_info_t();
	};

	struct stream_paint_info_t {
		area_paint_info_t area[NUM_OVERLAY_AREAS];
	};

	enum {
		LOGO_96,
		LOGO_64,
		LOGO_48,
	};

public:
	avf_status_t InitOverlay(vsrc_device_t *pDevice);
	avf_status_t ConfigOverlay(avf_uint_t stream, config_t *pConfig);
	avf_status_t ConfigLogo(avf_uint_t stream, config_t *pConfig, bmp8_data_s *bmp);
	avf_status_t EnableInternalLogo(avf_uint_t stream, int logo_size);
	avf_status_t UpdateOverlayConfig(avf_uint_t stream);
	avf_status_t ShowOverlay(avf_uint_t stream, avf_u32_t area_mask, bool bShow = true);
	INLINE avf_status_t HideOverlay(avf_uint_t stream, avf_u32_t area_mask) {
		return ShowOverlay(stream, area_mask, false);
	}
	avf_status_t DrawText(avf_uint_t stream, avf_u32_t area_mask, const char *pText, avf_size_t size);

private:
	vsrc_overlay_info_t *GetOverlayInfo(avf_uint_t stream);
	vsrc_overlay_area_t *GetArea(avf_uint_t stream, avf_uint_t area);
	area_paint_info_t *GetPaintInfo(avf_uint_t stream, avf_uint_t area);

	avf_u32_t Utf8ToUnicode(const char *pText, avf_size_t *plen);
	avf_status_t RenderText(vsrc_overlay_area_t *pArea, area_paint_info_t *pPaintInfo,
		const char *pText, avf_size_t size);
	static bool GotoNextLine(vsrc_overlay_area_t *pArea, area_paint_info_t *pPaintInfo, int *px, int *py);

private:
	avf_status_t mStatus;
	IEngine *mpEngine;
	vsrc_device_t *mpDevice;

	// driver info
	vsrc_overlay_info_t mOverlayInfo[NUM_VSRC_STREAMS];

	// draw info
	stream_paint_info_t mPaintInfo[NUM_VSRC_STREAMS];

private:
	CFontCache *mpFontCache;
};

#endif


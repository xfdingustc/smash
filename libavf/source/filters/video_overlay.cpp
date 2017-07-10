
#define LOG_TAG "video_overlay"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_util.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_queue.h"
#include "avf_base.h"
#include "avio.h"
#include "sys_io.h"

#include "amba_vsrc.h"
#include "video_overlay.h"

#define CLUT_ID_TEXT	0

//#define RENDER_MONO
#define BORDER_SIZE		1

#ifdef RENDER_MONO
#define FONT_RENDER_FLAG	FT_RENDER_MODE_MONO
#else
#define FONT_RENDER_FLAG	FT_RENDER_MODE_NORMAL
#endif

#include "video_overlay_data.h"

#define LEFT_MARGIN		8
#define RIGHT_MARGIN	8
#define LINE_MARGIN		2

bmp8_data_s::bmp8_data_s():
	ref_count(1),
	clut(NULL),
	data(NULL),
	width(0),
	height(0),
	stride(0)
{
}

bmp8_data_s::~bmp8_data_s()
{
	avf_safe_free(clut);
	avf_safe_free(data);
}

bmp8_data_s *bmp8_data_s::LoadFromFile(const char *filename)
{
	IAVIO *io = CSysIO::Create();

	if (io == NULL)
		return NULL;

	if (io->OpenRead(filename) != E_OK) {
		io->Release();
		return NULL;
	}

	bmp8_data_s *result = LoadFromIO(io);
	io->Release();

	return result;
}

bmp8_data_s *bmp8_data_s::LoadFromIO(IAVIO *io)
{
	CReadBuffer buffer(io);
	return LoadFromBuffer(&buffer, NULL);
}

bmp8_data_s *bmp8_data_s::LoadFromBuffer(CReadBuffer *buffer, const avf_u8_t *mem)
{
	const avf_u8_t *p = mem;

	//-----------------------------------------------
	//	bitmap file header
	//-----------------------------------------------
	if (buffer) {
		if (buffer->read_le_8() != 'B' || buffer->read_le_8() != 'M') {	// BM
			AVF_LOGE("no BM");
			return NULL;
		}
		buffer->Skip(8);	// file_size, reserved
	} else {
		p += 10;	// BM, file_size, reserved
	}

	avf_uint_t data_offset;
	if (buffer) {
		data_offset = buffer->read_le_32();
	} else {
		data_offset = load_le_32(p);
		p += 4;
	}
	if (data_offset == 0) {
		AVF_LOGE("bad data_offset");
		return NULL;
	}

	//-----------------------------------------------
	//	bitmap header
	//-----------------------------------------------
	avf_uint_t header_size;
	if (buffer) {
		header_size = buffer->read_le_32();
	} else {
		header_size = load_le_32(p);
		p += 4;
	}
	if (header_size < 40) {
		AVF_LOGE("bad header size: %d", header_size);
		return NULL;
	}

	avf_uint_t width;
	avf_uint_t height;
	avf_uint_t planes;

	if (buffer) {
		width = buffer->read_le_32();
		height = buffer->read_le_32();
		planes = buffer->read_le_16();
	} else {
		width = load_le_32(p);
		p += 4;
		height = load_le_32(p);
		p += 4;
		planes = load_le_16(p);
		p += 2;
	}

	if (width == 0 || height == 0 || planes != 1) {
		AVF_LOGE("bad bitmap header, width: %d, height: %d, planes: %d",
			width, height, planes);
		return NULL;
	}

	avf_uint_t bpp;
	avf_uint_t compression;

	if (buffer) {
		bpp = buffer->read_le_16();
		compression = buffer->read_le_32();
	} else {
		bpp = load_le_16(p);
		p += 2;
		compression = load_le_32(p);
		p += 4;
	}

	if (bpp != 8 || compression != 0) {
		AVF_LOGE("not 8-bit uncompressed bmp, bpp: %d, compression: %d", bpp, compression);
		return NULL;
	}

	if (buffer) {
		buffer->Skip(12);	// image size, h-resolution, v-resolutio
	} else {
		p += 12;
	}

	avf_uint_t colors_used;
	if (buffer) {
		colors_used = buffer->read_le_32();
	} else {
		colors_used = load_le_32(p);
		p += 4;
	}
	if (colors_used == 0 || colors_used > 256) {
		AVF_LOGE("bad num colors: %d", colors_used);
		return NULL;
	}

	if (buffer) {
		buffer->Skip(4);	// important colors
	} else {
		p += 4;
	}

	//-----------------------------------------------
	//	color table
	//-----------------------------------------------
	if (buffer) {
		buffer->Skip(header_size - 40);
	} else {
		p += header_size - 40;
	}

	avf_u8_t *clut = NULL;
	avf_u8_t *data = NULL;
	bmp8_data_s *result = NULL;

	avf_uint_t stride = ROUND_UP(width, 4);
	avf_uint_t data_size = stride * height;

	if ((clut = avf_malloc_bytes(4*256)) == NULL)
		goto Fail;

	if (buffer) {
		if (!buffer->read_mem(clut, colors_used * 4))
			goto Fail;
	} else {
		::memcpy(clut, p, colors_used * 4);
		p += colors_used * 4;
	}

	::memset(clut + colors_used * 4, 0, (256 - colors_used) * 4);

	//-----------------------------------------------
	//	bitmap data
	//-----------------------------------------------
	if (buffer) {
		if (buffer->SeekTo(data_offset) != E_OK)
			goto Fail;
	} else {
		p = mem + data_offset;
	}

	if ((data = avf_malloc_bytes(data_size)) == NULL)
		goto Fail;

	if (buffer) {
		if (!buffer->read_mem(data, data_size))
			goto Fail;
	} else {
		::memcpy(data, p, data_size);
		p += data_size;
	}

	if ((result = avf_new bmp8_data_s()) == NULL)
		goto Fail;

	result->clut = clut;
	result->data = data;

	result->width = width;
	result->height = height;
	result->stride = stride;

	return result;

Fail:
	avf_safe_free(clut);
	avf_safe_free(data);
	return NULL;
}

//-----------------------------------------------------------------------
//
//  CVideoOverlay
//
//-----------------------------------------------------------------------

CVideoOverlay::area_paint_info_t::~area_paint_info_t()
{
	avf_safe_release(bmp);
}

CVideoOverlay::CVideoOverlay(IEngine *pEngine):
	mStatus(E_OK),
	mpEngine(pEngine),
	mpDevice(NULL),
	mpFontCache(NULL)
{
	::memset(mOverlayInfo, 0, sizeof(mOverlayInfo));

	char buffer[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_VIDEO_OVERLAY_FONTNAME, 0, buffer, VALUE_VIDEO_OVERLAY_FONTNAME);

	int cache_size = 128*KB;
		//mpEngine->ReadRegInt32("config.subtitle.cachesize", 0, 128*1024);
	if (cache_size <= 0) {
		AVF_LOGD("Set font cache to 0 MB");
		cache_size = 0;
	} else if (cache_size > 4*1024*1024) {
		AVF_LOGD("Set font cache to 4 MB");
		cache_size = 4*1024*1024;
	}

	mpFontCache = avf_new CFontCache(buffer, cache_size);
	CHECK_STATUS(mpFontCache);
	CHECK_OBJ(mpFontCache);
}

CVideoOverlay::~CVideoOverlay()
{
	avf_delete mpFontCache;
}

vsrc_overlay_info_t *CVideoOverlay::GetOverlayInfo(avf_uint_t stream)
{
	if (stream >= ARRAY_SIZE(mOverlayInfo)) {
		AVF_LOGE("GetOverlayInfo: no such stream %d", stream);
		return NULL;
	}
	return mOverlayInfo + stream;
}

vsrc_overlay_area_t *CVideoOverlay::GetArea(avf_uint_t stream, avf_uint_t area)
{
	vsrc_overlay_info_t *pOverlayInfo;

	if ((pOverlayInfo = GetOverlayInfo(stream)) == NULL)
		return NULL;

	if (area >= ARRAY_SIZE(pOverlayInfo->area)) {
		AVF_LOGE("No such overlay area %d", area);
		return NULL;
	}

	return pOverlayInfo->area + area;
}

CVideoOverlay::area_paint_info_t *CVideoOverlay::GetPaintInfo(avf_uint_t stream, avf_uint_t area)
{
	if (stream >= ARRAY_SIZE(mPaintInfo)) {
		AVF_LOGE("GetPaintInfo: bad stream %d", stream);
		return NULL;
	}
	stream_paint_info_t *pStreamConfig = mPaintInfo + stream;
	if (area >= ARRAY_SIZE(pStreamConfig->area)) {
		AVF_LOGE("GetPaintInfo: bad area %d", area);
		return NULL;
	}
	return pStreamConfig->area + area;
}

// set clut and clear all overlay info
avf_status_t CVideoOverlay::InitOverlay(vsrc_device_t *pDevice)
{
	mpDevice = pDevice;

	if (mpDevice->SetOverlayClut(mpDevice, CLUT_ID_TEXT, g_overlay_clut_font) < 0) {
		AVF_LOGE("Set font clut failed");
		return E_ERROR;
	}

	::memset(mOverlayInfo, 0, sizeof(mOverlayInfo));

	for (int i = 0; i < NUM_VSRC_STREAMS; i++) {
		for (int j = 0; j < NUM_OVERLAY_AREAS; j++) {
			avf_safe_release(mPaintInfo[i].area[j].bmp);
		}
	}

	::memset(mPaintInfo, 0, sizeof(mPaintInfo));

	return E_OK;
}

// set config for one stream one area
avf_status_t CVideoOverlay::ConfigOverlay(avf_uint_t stream, config_t *pConfig)
{
	vsrc_overlay_area_t *pArea;
	area_paint_info_t *pPaintInfo;

	if ((pArea = GetArea(stream, TEXT_AREA)) == NULL)
		return E_INVAL;

	if ((pPaintInfo = GetPaintInfo(stream, TEXT_AREA)) == NULL)
		return E_INVAL;

	if (pConfig->font_size == 0 || pConfig->font_size > 128) {
		AVF_LOGE("bad font_size %d", pConfig->font_size);
		return E_INVAL;
	}

	vsrc_overlay_area_t *pLogo = GetArea(stream, LOGO_AREA);

	pArea->valid = pConfig->valid;
	pArea->visible = 1;
	pArea->clut_id = CLUT_ID_TEXT;
	pArea->width = pConfig->width;
	pArea->height = pConfig->height + 4;	// some font be truncated
	pArea->pitch = 0;
	pArea->x_off = pConfig->x_off;
	pArea->y_off = pConfig->y_off;
	pArea->y_off_inner = 0;
	pArea->data = NULL;
	pArea->total_size = 0;

	if (pLogo && pLogo->valid) {
		pArea->x_off = pLogo->x_off + pLogo->width;
		pArea->y_off = pLogo->y_off;
		pArea->y_off_inner = (pLogo->height - pArea->height) / 2;
		pArea->height += pArea->y_off_inner;
	}

	pPaintInfo->font_size = pConfig->font_size;
	avf_safe_release(pPaintInfo->bmp);

	// for debug use
	int debug_fontsize = mpEngine->ReadRegInt32(NAME_DBG_OVERLAY_FONTSIZE,
		IRegistry::GLOBAL, VALUE_DBG_OVERLAY_FONTSIZE);
	if (debug_fontsize > 0) {
		pPaintInfo->font_size = debug_fontsize;
		int height = mpEngine->ReadRegInt32(NAME_DBG_OVERLAY_HEIGHT,
			IRegistry::GLOBAL, VALUE_DBG_OVERLAY_HEIGHT);
		if (height > 0) {
			pArea->height = height;
		}
	}

	return E_OK;
}

avf_status_t CVideoOverlay::EnableInternalLogo(avf_uint_t stream, int logo_size)
{
	const avf_u8_t *logo_data = g_logo_bmp_48x48;
	if (logo_size == LOGO_96)
		logo_data = g_logo_bmp_96x96;
	else if (logo_size == LOGO_64)
		logo_data = g_logo_bmp_64x64;

	bmp8_data_s *bmp = bmp8_data_s::LoadFromBuffer(NULL, logo_data);
	if (bmp == NULL)
		return E_INVAL;

	config_t config;
	::memset(&config, 0, sizeof(config));
	config.valid = 1;
	config.x_off = 0;
	config.y_off = 0;
	config.width = ROUND_UP(bmp->width, 32);
	config.height = bmp->height;

	avf_status_t status = ConfigLogo(stream, &config, bmp);
	avf_safe_release(bmp);

	return status;
}

avf_status_t CVideoOverlay::ConfigLogo(avf_uint_t stream, config_t *pConfig, bmp8_data_s *bmp)
{
	vsrc_overlay_area_t *pArea;
	area_paint_info_t *pPaintInfo;

	AVF_LOGI(C_YELLOW "ConfigLogo(%d,%d)" C_NONE, stream, LOGO_AREA);

	if ((pArea = GetArea(stream, LOGO_AREA)) == NULL)
		return E_INVAL;

	if ((pPaintInfo = GetPaintInfo(stream, LOGO_AREA)) == NULL)
		return E_INVAL;

	pArea->valid = pConfig->valid;
	pArea->visible = 1;
	pArea->clut_id = VSRC_CLUT_ID(stream, LOGO_AREA);
	pArea->width = pConfig->width;
	pArea->height = pConfig->height;
	pArea->pitch = 0;
	pArea->x_off = pConfig->x_off;
	pArea->y_off = pConfig->y_off;
	pArea->data = NULL;
	pArea->total_size = 0;

	pPaintInfo->font_size = 0;
	avf_assign(pPaintInfo->bmp, bmp);

	return E_OK;
}

// make the overlay take effect
avf_status_t CVideoOverlay::UpdateOverlayConfig(avf_uint_t stream)
{
	vsrc_overlay_info_t *pOverlayInfo;
	area_paint_info_t *pPaintInfo;

	if ((pOverlayInfo = GetOverlayInfo(stream)) == NULL)
		return E_INVAL;

	if ((pPaintInfo = GetPaintInfo(stream, LOGO_AREA)) == NULL)
		return E_INVAL;

	bmp8_data_s	*bmp = pPaintInfo->bmp;

	if (bmp) {
		avf_u8_t *pclut = bmp->clut;

		for (int i = 0; i < 256; i++, pclut += 4) {
			int b = pclut[0];
			int g = pclut[1];
			int r = pclut[2];
			int a = pclut[3];

			int y = (76*r + 150*g + 29*b) >> 8;
			int u = ((-43*r -84*g + 127*b) >> 8) + 128;
			int v = ((127*r -106*g -21*b) >> 8) + 128;
			*(avf_u32_t*)pclut = CLUT_ENTRY(y, u, v, a);

//			int tmp;
//			tmp = pclut[0]; pclut[0] = pclut[3]; pclut[3] = tmp;
//			tmp = pclut[1]; pclut[1] = pclut[2]; pclut[2] = tmp;
//			pclut += 4;
		}

		bmp->tc_index = bmp->data[(bmp->height - 1) * bmp->stride];
		pclut = bmp->clut + 4 * bmp->tc_index;
		pclut[3] = 0;	// r g b a

		if (mpDevice->SetOverlayClut(mpDevice, VSRC_CLUT_ID(stream, LOGO_AREA),
				(const avf_u32_t*)bmp->clut) != 0) {
			return E_ERROR;
		}
	}

	if (mpDevice->ConfigOverlay(mpDevice, stream, pOverlayInfo) < 0) {
		AVF_LOGE("ConfigOverlay failed");
		return E_INVAL;
	}

	if (bmp) {
		vsrc_overlay_area_t *area = GetArea(stream, LOGO_AREA);
		if (area == NULL)
			return E_ERROR;

		::memset(area->data, bmp->tc_index, area->total_size);

		int bytes_per_line = MIN(area->pitch, (int)bmp->stride);
		int lines = MIN(area->height, (int)bmp->height);
		avf_u8_t *to = area->data;
		avf_u8_t *from = bmp->data + (bmp->height - 1) * bmp->stride;
		for (int i = 0; i < lines; i++) {
			::memcpy(to, from, bytes_per_line);
			to += area->pitch;
			from -= bmp->stride;
		}
	}

	if (mpDevice->UpdateOverlay(mpDevice, stream, pOverlayInfo) < 0) {
		AVF_LOGE("UpdateOverlay failed");
		return E_ERROR;
	}

	return E_OK;
}

// toggle visible/hiden of one stream/multiple areas
avf_status_t CVideoOverlay::ShowOverlay(avf_uint_t stream, avf_u32_t area_mask, bool bShow)
{
	vsrc_overlay_info_t *pOverlayInfo;
	int i;

	if ((pOverlayInfo = GetOverlayInfo(stream)) == NULL)
		return E_INVAL;

	for (i = 0; i < NUM_OVERLAY_AREAS; i++) {
		if (pOverlayInfo->area[i].valid && test_flag(area_mask, 1 << i) != 0) {
			pOverlayInfo->area[i].visible = bShow ? 1 : 0;
		}
	}

	if (mpDevice->UpdateOverlay(mpDevice, stream, pOverlayInfo) < 0) {
		AVF_LOGE("UpdateOverlay failed");
		return E_ERROR;
	}

	return E_OK;
}

avf_status_t CVideoOverlay::DrawText(avf_uint_t stream, avf_u32_t area_mask, const char *pText, avf_size_t size)
{
	vsrc_overlay_info_t *pOverlayInfo;
	int i;

	if ((pOverlayInfo = GetOverlayInfo(stream)) == NULL)
		return E_INVAL;

//	pText = "\xe4\xb8\xad\xe5\x9b\xbd";
//	size = strlen(pText);

//	pText = "abcdefghijklmnopqrsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
//	pText = "0123456789-=,./<>?;';\"[]{}~!@#$%^&*()_+\\|";
//	size = strlen(pText);

	for (i = 0; i < NUM_OVERLAY_AREAS; i++) {
		vsrc_overlay_area_t *pArea = pOverlayInfo->area + i;
		if (pArea->valid && test_flag(area_mask, 1 << i) != 0) {
			area_paint_info_t *pPaintInfo = GetPaintInfo(stream, i);
			if (pPaintInfo == NULL) {
				continue;
			}
			if (pArea->data == NULL) {
				AVF_LOGE("Overlay [%d,%d] not created", stream, i);
				continue;
			}
			RenderText(pArea, pPaintInfo, pText, size);
		}
	}

	return E_OK;
}

avf_u32_t CVideoOverlay::Utf8ToUnicode(const char *pText, avf_size_t *plen)
{
	avf_u32_t unicode = pText[0];

	if (unicode >= 0xF0) {
		unicode  = (pText[0] & 0x07) << 18;
		unicode |= (pText[1] & 0x3F) << 12;
		unicode |= (pText[2] & 0x3F) << 6;
		unicode |= (pText[3] & 0x3F);
		*plen = 4;
	} else if (unicode >= 0xE0) {
		unicode  = (pText[0] & 0x0F) << 12;
		unicode |= (pText[1] & 0x3F) << 6;
		unicode |= (pText[2] & 0x3F);
		*plen = 3;
	} else if (unicode >= 0xC0) {
		unicode  = (pText[0] & 0x1F) << 6;
		unicode |= (pText[1] & 0x3F);
		*plen = 2;
	} else {
		*plen = 1;
	}

	return unicode;
}

bool CVideoOverlay::GotoNextLine(vsrc_overlay_area_t *pArea,
	area_paint_info_t *pPaintInfo, int *px, int *py)
{
	*px = LEFT_MARGIN;
	*py += pPaintInfo->font_size + LINE_MARGIN;
	return (int)(*py + pPaintInfo->font_size + LINE_MARGIN) <= pArea->height;
}

avf_status_t CVideoOverlay::RenderText(vsrc_overlay_area_t *pArea,
	area_paint_info_t *pPaintInfo,
	const char *pText, avf_size_t size)
{
	avf_u8_t *buffer;

//	pText = "\xEE\xC0\xC1" " Camera Name";
//	size = ::strlen(pText);

	int px = LEFT_MARGIN;
	int py = pArea->y_off_inner; //0;

	int ax, ay;
	int bx, by;
	int rows, width;

	if ((buffer = avf_malloc_bytes(pArea->total_size)) == NULL)
		return E_NOMEM;

	::memset(buffer, 0, pArea->total_size);

	for (avf_size_t i = 0; i < size; ) {
		avf_size_t len;
		avf_u32_t c;

		c = Utf8ToUnicode(pText, &len);
		pText += len;
		i += len;

		// new line char
		if (c == '\n') {
			if (!GotoNextLine(pArea, pPaintInfo, &px, &py))
				break;
			continue;
		}

		CFontCache::Item *pItem = mpFontCache->RenderChar(c, pPaintInfo->font_size);
		if (pItem == NULL)
			break;

		// check for new line
		if (px + pItem->bitmap_advance + RIGHT_MARGIN > pArea->width) {
			if (!GotoNextLine(pArea, pPaintInfo, &px, &py))
				break;
		}

		ax = px + pItem->bitmap_left;

		if (ax < 0) {
			bx = -ax;
			ax = 0;
		} else {
			bx = 0;
		}

		ay = py + pItem->bitmap_y_off;
		if (ay < 0) {
			by = -ay;
			ay = 0;
		} else {
			by = 0;
		}

		width = pItem->bitmap_width - bx;
		if (ax + width > pArea->width)
			width = pArea->width - ax;

		rows = pItem->bitmap_rows - by;
		if (ay + rows > pArea->height)
			rows = pArea->height - ay;

		if (width > 0 && rows > 0) {
			avf_u8_t *pdst = buffer + (ay * pArea->pitch + ax);
			avf_u8_t *psrc = pItem->GetData() + (by * pItem->bitmap_pitch + bx);

			for (int j = rows; j > 0; j--) {
				avf_u8_t *pto = pdst;
				avf_u8_t *pfrom = psrc;

				for (int k = width/4; k; k--) {
					pto[0] |= pfrom[0];
					pto[1] |= pfrom[1];
					pto[2] |= pfrom[2];
					pto[3] |= pfrom[3];
					pto += 4;
					pfrom += 4;
				}
				for (int k = width&3; k; k--) {
					*pto++ |= *pfrom++;
				}

				pdst += pArea->pitch;
				psrc += pItem->bitmap_pitch;
			}
		}

		px += pItem->bitmap_advance;
	}

	::memcpy(pArea->data, buffer, pArea->total_size);
	avf_free(buffer);

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CFontCache
//
//-----------------------------------------------------------------------

#define item_of_hash(_node) \
	(Item*)list_entry(_node, Item, list_hash)

#define item_of_pool(_node) \
	(Item*)list_entry(_node, Item, list_pool)

CFontCache::CFontCache(const char *font_filename, avf_size_t memLimit):
	mStatus(E_OK),
	mFontLib(NULL),
	mFontFace(NULL),

	mFontSize(0),
	mMemLimit(memLimit),
	mCurrMem(0),
	mCacheHit(0),
	mCacheMiss(0)
{
	int rval;

	list_init(mPoolList);
	for (avf_size_t i = 0; i < HASH_SIZE; i++) {
		list_init(mHashList[i]);
	}

	rval = FT_Init_FreeType(&mFontLib);
	if (rval) {
		AVF_LOGE("FT_Init_FreeType failed %d", rval);
		mStatus = E_ERROR;
		return;
	}

	rval = FT_New_Face(mFontLib, font_filename, 0, &mFontFace);
	if (rval) {
		AVF_LOGE("Load %s failed %d", font_filename, rval);
		mStatus = E_ERROR;
		return;
	}
}

CFontCache::~CFontCache()
{
	list_head_t *node;
	list_head_t *next;
	list_for_each_del(node, next, mPoolList) {
		Item *pItem = item_of_pool(node);
		pItem->Release();
	}

	if (mFontFace) {
		FT_Done_Face(mFontFace);
	}
	if (mFontLib) {
		FT_Done_FreeType(mFontLib);
	}

#ifdef AVF_DEBUG
	AVF_LOGI("FontCache: cache miss: %d, cache hit: %d", mCacheMiss, mCacheHit);
#endif
}

CFontCache::Item* CFontCache::RenderChar(avf_u32_t c, avf_uint_t font_size)
{
	hash_val_t hash_val = Hash(c, font_size);
	list_head_t *pHashList = mHashList + (hash_val % HASH_SIZE);
	list_head_t *node;

	// check hash list
	list_for_each_p(node, pHashList) {
		Item *pItem = item_of_hash(node);
		if (pItem->c == c && pItem->font_size == font_size) {
			// remove from the pool, and then insert at the pool head
			list_del(&pItem->list_pool);
			list_add(&pItem->list_pool, mPoolList);
			// return this
			mCacheHit++;
			return pItem;
		}
	}

	// not found in cache pool. create it.
	Item *pItem = CreateItem(c, font_size);
	if (pItem == NULL)
		return NULL;

	pItem->c = c;
	pItem->hash_val = hash_val;
	pItem->font_size = font_size;

	// now insert the new Item
	list_add(&pItem->list_pool, mPoolList);
	list_add(&pItem->list_hash, *pHashList);

	// check memory, and remove old ones
	mCurrMem += pItem->mem_size;
	while (mCurrMem > mMemLimit) {
		if (list_is_empty(mPoolList)) {
			AVF_LOGD("Pool is empty, mem/limit is %d/%d", mCurrMem, mMemLimit);
			break;
		}
		node = list_tail_node(mPoolList);
		Item *tmp = item_of_pool(node);
		// remove it and release it
		list_del(&tmp->list_hash);
		list_del(&tmp->list_pool);
		mCurrMem -= tmp->mem_size;
		tmp->Release();
	}

	mCacheMiss++;
	return pItem;
}

void CFontCache::AdjustItemColor(Item *pItem, const avf_u8_t *tbl)
{
	avf_u8_t *ptr = pItem->GetData();
	avf_size_t size = pItem->mem_size - sizeof(Item);
	for (avf_size_t i = size/4; i; i--) {
		ptr[0] = tbl[ptr[0]];
		ptr[1] = tbl[ptr[1]];
		ptr[2] = tbl[ptr[2]];
		ptr[3] = tbl[ptr[3]];
		ptr += 4;
	}
	for (avf_size_t i = size&3; i; i--) {
		*ptr = tbl[*ptr];
		ptr++;
	}
}

CFontCache::Item *CFontCache::CreateItem(avf_u32_t c, avf_uint_t font_size)
{
	FT_Glyph glyph;
	Item *pItem;
	Item *pItem2;

	if ((glyph = CreateGlyph(c, font_size)) == NULL)
		return NULL;

	pItem = ConvertToItem(glyph, 0, FONT_RENDER_FLAG);
	if (pItem) {
		AdjustItemColor(pItem, g_fore_tbl);	
		if (TransformGlyph(&glyph)) {
			pItem2 = ConvertToItem(glyph, BORDER_SIZE, FONT_RENDER_FLAG);
			if (pItem2) {
				AdjustItemColor(pItem2, g_border_tbl);
				ItemAdd(c, pItem2, pItem);
				pItem->Release();
				pItem = pItem2;
			}
		}
	}

	FT_Done_Glyph(glyph);
	return pItem;
}

bool CFontCache::TransformGlyph(FT_Glyph *pglyph)
{
	FT_Stroker stroker;

	if (FT_Stroker_New(mFontLib, &stroker) != 0)
		return false;

	FT_Stroker_Set(stroker, BORDER_SIZE << 6,
		FT_STROKER_LINECAP_ROUND,
		FT_STROKER_LINEJOIN_ROUND,
		0);

	FT_Glyph_StrokeBorder(pglyph, stroker, 0, 1);
	FT_Stroker_Done(stroker);

	return true;
}

// to += from
void CFontCache::ItemAdd(avf_u32_t c, Item *to, const Item *from)
{
	avf_size_t src_pitch = from->bitmap_pitch;
	avf_size_t dst_pitch = to->bitmap_pitch;
	avf_size_t x_offset = ((dst_pitch - src_pitch) + (from->bitmap_left - to->bitmap_left)) / 2;
	avf_size_t y_offset = ((to->bitmap_rows - from->bitmap_rows) - 1 + (from->bitmap_y_off - to->bitmap_y_off)) / 2;

//	printf("%c: left=%d, width=%d, pitch=%d\n", c, to->bitmap_left, to->bitmap_width, to->bitmap_pitch);
//	printf("%c: left=%d, width=%d, pitch=%d\n", c, from->bitmap_left, from->bitmap_width, from->bitmap_pitch);
//	printf("\n");

//	printf("%c: y_off=%d, rows=%d\n", c, to->bitmap_y_off, to->bitmap_rows);
//	printf("%c: y_off=%d, rows=%d\n", c, from->bitmap_y_off, from->bitmap_rows);
//	printf("\n");

	const avf_u8_t *psrc = from->GetData();
	avf_u8_t *pdst = to->GetData() + dst_pitch * y_offset + x_offset;

	for (avf_size_t i = from->bitmap_rows; i; i--) {
		const avf_u8_t *pfrom = psrc;
		avf_u8_t *pto = pdst;

		for (avf_size_t j = from->bitmap_width; j; j--) {
			if (*pfrom) {
				*pto = *pfrom;
			}

			pfrom++;
			pto++;
		}

		psrc += src_pitch;
		pdst += dst_pitch;
	}
}

FT_Glyph CFontCache::CreateGlyph(avf_u32_t c, avf_uint_t font_size)
{
	FT_UInt gindex;
	FT_Glyph glyph;
	FT_Error error;

	if (mFontSize != font_size) {
		error = FT_Set_Pixel_Sizes(mFontFace, 0, font_size);
		if (error) {
			AVF_LOGE("FT_Set_Char_Size failed");
			return NULL;
		}
		mFontSize = font_size;
	}

	gindex = FT_Get_Char_Index(mFontFace, c);

	error = FT_Load_Glyph(mFontFace, gindex, FT_LOAD_NO_BITMAP);
	if (error) {
		AVF_LOGE("FT_Load_Glyph failed");
		return NULL;
	}

	error = FT_Get_Glyph(mFontFace->glyph, &glyph);
	if (error) {
		AVF_LOGE("FT_Get_Glyph failed");
		return NULL;
	}

	return glyph;
}

// Convert glyph to item
CFontCache::Item* CFontCache::ConvertToItem(FT_Glyph glyph, int border_size, 
	FT_Render_Mode render_mode)
{
	FT_Glyph old_glyph = glyph;
	FT_BitmapGlyph bitmap_glyph;
	int bitmap_pitch;
	avf_size_t bitmap_size;
	avf_size_t mem_size;
	Item *pItem = NULL;
	avf_u8_t *ptr;
	avf_u8_t *psrc;
	FT_Error error;

	// convert to bitmap
	error = FT_Glyph_To_Bitmap(&glyph, render_mode, NULL, 0);
	if (error) {
		AVF_LOGE("FT_Glyph_To_Bitmap failed");
		return NULL;
	}

	// cast to bitmap type
	if (glyph->format != FT_GLYPH_FORMAT_BITMAP) {
		AVF_LOGE("Cannot get bitmap glyph");
		goto Done;
	}

	bitmap_glyph = (FT_BitmapGlyph)glyph;

	switch (bitmap_glyph->bitmap.pixel_mode) {
	case FT_PIXEL_MODE_MONO:
		bitmap_pitch = bitmap_glyph->bitmap.width;
		break;

	case FT_PIXEL_MODE_GRAY:
		bitmap_pitch = bitmap_glyph->bitmap.pitch;
		break;

	default:
		AVF_LOGE("Unknown bitmap format");
		goto Done;
	}

	bitmap_size = bitmap_pitch * bitmap_glyph->bitmap.rows;

	mem_size = sizeof(Item) + bitmap_size;
	if ((pItem = (Item*)avf_malloc(mem_size)) == NULL)
		goto Done;


	// init the item
	pItem->bitmap_y_off = (mFontFace->size->metrics.ascender >> 6) - bitmap_glyph->top;
	pItem->bitmap_left = bitmap_glyph->left + border_size;
	pItem->bitmap_width = bitmap_glyph->bitmap.width;
	pItem->bitmap_pitch = bitmap_pitch;
	pItem->bitmap_rows = bitmap_glyph->bitmap.rows;
	pItem->bitmap_advance = (glyph->advance.x >> 16) + border_size;
	pItem->mem_size = mem_size;

	// copy bitmap data
	ptr = pItem->GetData();
	psrc = bitmap_glyph->bitmap.buffer;

	if (bitmap_glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
		CopyMonoData(ptr, psrc,
			bitmap_glyph->bitmap.width, bitmap_glyph->bitmap.rows,
			bitmap_pitch, bitmap_glyph->bitmap.pitch);
	} else {
		// 256 color gray
		::memcpy(ptr, psrc, bitmap_size);
	}

Done:
	if (glyph != old_glyph) {
		// should free this glyph but not the one passed to us
		FT_Done_Glyph(glyph);
	}
	return pItem;
}

void CFontCache::CopyMonoData(
	avf_u8_t *ptr, const avf_u8_t *psrc, 
	avf_size_t width, avf_size_t rows,
	avf_size_t pitch, avf_size_t src_pitch)
{
	for (avf_size_t i = rows; i; i--) {
		avf_u8_t *pto = ptr;
		const avf_u8_t *pfrom = psrc;

		for (avf_size_t j = width/8; j; j--) {
			avf_int_t value = *pfrom++;
			pto[0] = 256 - ((value >> 7) & 1);
			pto[1] = 256 - ((value >> 6) & 1);
			pto[2] = 256 - ((value >> 5) & 1);
			pto[3] = 256 - ((value >> 4) & 1);
			pto[4] = 256 - ((value >> 3) & 1);
			pto[5] = 256 - ((value >> 2) & 1);
			pto[6] = 256 - ((value >> 1) & 1);
			pto[7] = 256 - ((value >> 0) & 1);
			pto += 8;
		}

		if (width & 7) {
			avf_int_t value = *pfrom;
			avf_size_t j = width & 7;
			do {
				*pto++ = 256 - ((value >> 7) & 1);
				value <<= 1;
			} while (--j);
		}

		ptr += pitch;
		psrc += src_pitch;
	}
}



#define LOG_TAG "player_creator"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"

#include "base_player.h"

#include "camera.h"
#include "file_playback.h"
#include "vdb_playback.h"
#include "remuxer.h"
#include "format_converter.h"

extern "C" IMediaPlayer* avf_create_player(
	const ap<string_t>& player,
	const ap<string_t>& mediaSrc,
	int conf,
	CConfig *pConfig,
	IEngine *pEngine)
{
	AVF_LOGD(C_WHITE "create player '%s'" C_NONE, player->string());

#ifndef REMUXER

#if !defined(WIN32_OS) && !defined(MAC_OS)
	if (::strcmp(player->string(), "camera") == 0) {
		return CCamera::Create(pEngine, mediaSrc, conf, pConfig);
	}
#endif

	if (::strcmp(player->string(), "playback") == 0) {
		// vdb:url
		const char *pMediaSrc = mediaSrc->string();
		if (::strncasecmp(pMediaSrc, "vdb:", 4) == 0) {
			AVF_LOGD("Create vdb player");
			return CVdbPlayback::Create(pEngine, mediaSrc, conf, pConfig);
		}
		// file
		AVF_LOGD("create file player");
		return CFilePlayback::Create(pEngine, mediaSrc, conf, pConfig);
	}

#if !defined(WIN32_OS) && !defined(MAC_OS)
	if (::strcmp(player->string(), "converter") == 0) {
		return CFormatConverter::Create(pEngine, mediaSrc, conf, pConfig);
	}
#endif

#endif

	if (::strcmp(player->string(), "remuxer") == 0) {
		return CRemuxer::Create(pEngine, mediaSrc, conf, pConfig);
	}

	AVF_LOGE("no such player: %s", player->string());
	return NULL;
}


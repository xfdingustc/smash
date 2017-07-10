
#include "avf_common.h"
#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_new.h"
#include "avf_remuxer_api.h"
#include "avf_remuxer_if.h"

struct avf_remuxer_s {
	IMediaControl *pMediaControl;
	avf_remuxer_callback_t callback;
	void *cb_context;
	char bDisableAudio;
	char bIframeOnly;
	int bFileFinished;
	char *pOutputFilename;
	char *pAudioFilename;
	char *pAudioFormat;
	int audio_fade_ms;
};

static void avf_remuxer_callback(void *context, avf_intptr_t tag, const avf_msg_t& msg)
{
	avf_remuxer_t *remuxer = (avf_remuxer_t*)context;
	switch (msg.code) {
	case IMediaControl::APP_MSG_STATE_CHANGED: {
			IMediaControl::StateInfo info;
			remuxer->pMediaControl->GetStateInfo(info);
			if (info.state == IMediaControl::STATE_ERROR) {
				remuxer->callback(remuxer->cb_context, AVF_REMUXER_ERROR, info.error, 0);
			} else if (info.state == IMediaControl::STATE_STOPPED) {
				if (remuxer->bFileFinished) {
					remuxer->callback(remuxer->cb_context, AVF_REMUXER_FINISHED, 0, 0);
				}
			}
		}
		break;

	case IMediaControl::APP_MSG_FILE_END:
		remuxer->bFileFinished = true;
		// remuxer->callback(remuxer->cb_context, AVF_REMUXER_FINISHED, 0, 0);
		break;

	case IMediaControl::APP_MSG_MEDIA_SOURCE_PROGRESS:
		remuxer->callback(remuxer->cb_context, AVF_REMUXER_PROGRESS, msg.p1, 0);
		break;

	case IMediaControl::APP_MSG_ATTACH_THREAD:
		remuxer->callback(remuxer->cb_context, AVF_REMUXER_ATTACH_THREAD, 0, 0);
		break;

	case IMediaControl::APP_MSG_DETACH_THREAD:
		remuxer->callback(remuxer->cb_context, AVF_REMUXER_DETACH_THREAD, 0, 0);
		break;

	default:
		break;
	}
}

avf_remuxer_t *avf_remuxer_create(avf_remuxer_callback_t callback, void *context)
{
	avf_remuxer_t *remuxer = avf_malloc_type(avf_remuxer_t);
	if (remuxer) {
		remuxer->callback = callback;
		remuxer->cb_context = context;
		remuxer->bDisableAudio = false;
		remuxer->bIframeOnly = false;
		remuxer->bFileFinished = false;
		remuxer->pOutputFilename = NULL;
		remuxer->pAudioFilename = NULL;
		remuxer->pAudioFormat = NULL;
		remuxer->audio_fade_ms = 0;
		if ((remuxer->pMediaControl = avf_create_media_control(NULL, avf_remuxer_callback, remuxer, false)) == NULL) {
			avf_free(remuxer);
			return NULL;
		}
	}
	return remuxer;
}

void avf_remuxer_destroy(avf_remuxer_t *remuxer)
{
	if (remuxer) {
		avf_safe_release(remuxer->pMediaControl);
		avf_safe_free(remuxer->pOutputFilename);
		avf_safe_free(remuxer->pAudioFilename);
		avf_safe_free(remuxer->pAudioFormat);
		avf_free(remuxer);
	}
}

void avf_remuxer_set_iframe_only(avf_remuxer_t *remuxer, int bIframeOnly)
{
	remuxer->bIframeOnly = bIframeOnly;
}

void avf_remuxer_set_audio_fade_ms(avf_remuxer_t *remuxer, int audio_fade_ms)
{
	remuxer->audio_fade_ms = audio_fade_ms;
}

void avf_remuxer_set_audio(avf_remuxer_t *remuxer, int bDisableAudio, const char *pAudioFilename, const char *format)
{
	remuxer->bDisableAudio = bDisableAudio;
	avf_safe_free(remuxer->pAudioFilename);
	if (pAudioFilename) {
		remuxer->pAudioFilename = avf_strdup(pAudioFilename);
	}
	avf_safe_free(remuxer->pAudioFormat);
	if (format) {
		remuxer->pAudioFormat = avf_strdup(format);
	}
	if (pAudioFilename) {
		remuxer->bDisableAudio = true;
	}
}

int avf_remuxer_register_vdb(avf_remuxer_t *remuxer, IVdbInfo *pVdbInfo)
{
	remuxer->pMediaControl->RegisterComponent(pVdbInfo);
	return 0;
}

int avf_remuxer_unregister_vdb(avf_remuxer_t *remuxer, IVdbInfo *pVdbInfo)
{
	remuxer->pMediaControl->UnregisterComponent(pVdbInfo);
	return 0;
}

const char *avf_remuxer_get_output_filename(avf_remuxer_t *remuxer)
{
	return remuxer->pOutputFilename;
}

//	source {
//		filter=FileDemuxer{format=ts;[iframeonly=true;]}
//		audio=FileDemuxer{format=mp3;}
//	}
//	sink {
//		filter=FileMuxer{format=mp4;filename=[filename];}
//	}
int avf_remuxer_run(avf_remuxer_t *remuxer,
	const char *input_file,
	const char *input_format,
	const char *output_file,
	const char *output_format,
	int duration_ms)
{
	char *buffer = (char*)avf_malloc(1024);
	char *ptr = buffer;

	avf_safe_free(remuxer->pOutputFilename);
	remuxer->pOutputFilename = avf_strdup(output_file);

	// source

	ptr += sprintf(ptr,
		"source{" EOL
		"	filter=FileDemuxer{format=%s;",
		input_format);
	if (remuxer->bIframeOnly) {
		ptr += sprintf(ptr, "	iframeonly=1;" EOL);
	}
	ptr += sprintf(ptr, "}" EOL);

	if (remuxer->pAudioFilename) {
		ptr += sprintf(ptr,
			"	audio=FileDemuxer{format=%s;}" EOL,
			remuxer->pAudioFormat);
	}

	ptr += sprintf(ptr, "}" EOL);

	// sink

	ptr += sprintf(ptr,
		"sink{" EOL
		"	filter=FileMuxer{format=%s;filename=[%s];}" EOL
		"}" EOL,
		output_format, output_file);

	// configs

#if 0
	// estimate moov size for mp4
	int moov_size = 0;
	if (duration_ms > 0) {
		moov_size = (duration_ms+999)/1000 * 2000;	// 2KB/s
		if (moov_size < 1024)
			moov_size = 1024;
	}
#endif

	ptr += sprintf(ptr,
		"configs{" EOL
//		"	" NAME_MP4_MOOV_SIZE "=%d;" EOL, moov_size
		);

	if (remuxer->pAudioFilename) {
		ptr += sprintf(ptr,
			"	" NAME_ALIGN_TO_VIDEO "=1;" EOL);
	}

	ptr += sprintf(ptr, "}" EOL);

	avf_status_t status = remuxer->pMediaControl->SetMediaSource(
		"remuxer", input_file, buffer, 0, 0);
	avf_free(buffer);

	if (status != E_OK)
		return -1;

	if (remuxer->bDisableAudio) {
		remuxer->pMediaControl->WriteRegBool(NAME_HIDE_AUDIO, 0, 1);
	}
	if (remuxer->pAudioFilename) {
		remuxer->pMediaControl->WriteRegString(NAME_EXTRA_AUDIO, 0, remuxer->pAudioFilename);
		remuxer->pMediaControl->WriteRegInt32(NAME_REMUXER_DURATION, 0, duration_ms);
		remuxer->pMediaControl->WriteRegInt32(NAME_REMUXER_FADE_MS, 0, remuxer->audio_fade_ms);
		remuxer->pMediaControl->WriteRegBool(NAME_AVSYNC, 0, false);
	}
	remuxer->pMediaControl->WriteRegInt32(NAME_RECORD, 0, 1);

	status = remuxer->pMediaControl->RunMedia();
	if (status != E_OK)
		return -1;

	return 0;
}

int avf_remuxer_get_progress(avf_remuxer_t *remuxer,
	uint64_t *total_bytes, uint64_t *remain_bytes)
{
	Remuxer rm(remuxer->pMediaControl);
	IReadProgress::progress_t progress;
	avf_status_t status = rm.GetRemuxProgress(&progress);
	if (status != E_OK)
		return status;
	*total_bytes = progress.total_bytes;
	*remain_bytes = progress.remain_bytes;
	return E_OK;
}


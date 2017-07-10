
#ifndef __AVF_REMUX_API_H__
#define __AVF_REMUX_API_H__

typedef struct avf_remuxer_s avf_remuxer_t;
class IVdbInfo;

enum {
	AVF_REMUXER_FINISHED,
	AVF_REMUXER_ERROR,
	AVF_REMUXER_PROGRESS,
	AVF_REMUXER_ATTACH_THREAD,
	AVF_REMUXER_DETACH_THREAD,
};

typedef void (*avf_remuxer_callback_t)(void *context, int event, int arg1, int arg2);

avf_remuxer_t *avf_remuxer_create(avf_remuxer_callback_t callback, void *context);
void avf_remuxer_set_iframe_only(avf_remuxer_t *remuxer, int bIframeOnly);
void avf_remuxer_set_audio_fade_ms(avf_remuxer_t *remuxer, int audio_fade_ms);
void avf_remuxer_set_audio(avf_remuxer_t *remuxer, int bDisableAudio, const char *pAudioFilename, const char *format);

const char *avf_remuxer_get_output_filename(avf_remuxer_t *remuxer);

int avf_remuxer_register_vdb(avf_remuxer_t *remuxer, IVdbInfo *pVdbInfo);
int avf_remuxer_unregister_vdb(avf_remuxer_t *remuxer, IVdbInfo *pVdbInfo);

// abort remuxer and destroy it
void avf_remuxer_destroy(avf_remuxer_t *remuxer);

// input_file: http://192.168.110.1:8085/buffer/xxxx.ts,0,-1;
// input_format: ts
// output_file: /tmp/mmcblk0p1/test.mp4
// output_format: mp4
int avf_remuxer_run(avf_remuxer_t *remuxer,
	const char *input_file,
	const char *input_format,
	const char *output_file,
	const char *output_format,
	int duration_ms);

int avf_remuxer_get_progress(avf_remuxer_t *remuxer,
	uint64_t *total_bytes, uint64_t *remain_bytes);

#endif


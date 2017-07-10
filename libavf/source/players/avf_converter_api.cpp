
#include "avf_common.h"
#include "avf_if.h"
#include "avf_new.h"
#include "avf_converter_api.h"

struct avf_converter_s {
	IMediaControl *pMediaControl;
	avf_converter_callback_t callback;
	void *cb_context;
	int c_type;
};

static void avf_converter_callback(void *context, avf_intptr_t tag, const avf_msg_t& msg)
{
	avf_converter_t *converter = (avf_converter_t*)context;
	switch (msg.code) {
	case IMediaControl::APP_MSG_STATE_CHANGED: {
			IMediaControl::StateInfo info;
			converter->pMediaControl->GetStateInfo(info);
			if (info.state == IMediaControl::STATE_ERROR) {
				if (converter->callback) {
					converter->callback(converter->cb_context, AVF_CONV_ERROR, info.error, 0);
				}
			}
		}
		break;

	case IMediaControl::APP_MSG_FILE_END:
		if (converter->callback) {
			converter->callback(converter->cb_context, AVF_CONV_FINISHED, 0, 0);
		}
		break;

	case IMediaControl::APP_MSG_MEDIA_SOURCE_PROGRESS:
		if (converter->callback) {
			converter->callback(converter->cb_context, AVF_CONV_PROGRESS, msg.p1, 0);
		}
		break;

	default:
		break;
	}
}

avf_converter_t *avf_converter_create(avf_converter_callback_t callback, void *context)
{
	avf_converter_t *converter = avf_malloc_type(avf_converter_t);
	if (converter) {
		if ((converter->pMediaControl = avf_create_media_control(NULL, avf_converter_callback, converter, false)) == NULL) {
			avf_free(converter);
			return NULL;
		}
		converter->callback = callback;
		converter->cb_context = context;
	}
	return converter;
}

void avf_converter_destroy(avf_converter_t *converter)
{
	if (converter) {
		if (converter->pMediaControl) {
			converter->pMediaControl->Release();
		}
		avf_free(converter);
	}
}

static const char *g_mp3_aac_config = 
	"filters{" EOL
	"	FileDemuxer;" EOL
	"	Mp3Decoder;" EOL
	"	AACEncoder { output=1; bitrate=96000; }" EOL
	"	FileMuxer { format=%s; }" EOL
	"}" EOL
	"source=FileDemuxer;" EOL
	"connections{" EOL
	"	FileDemuxer[0] { peer=Mp3Decoder; in=0; }" EOL
	"	Mp3Decoder[0] { peer=AACEncoder; in=0; }" EOL
	"	AACEncoder[0] { peer=FileMuxer; in=0; }" EOL
	"}" EOL
	"configs{" EOL
	"	"NAME_MUXER_FILE"=%s;" EOL
//	"	"NAME_REALTIME"=0;" EOL
	"}" EOL
	;

static const char *g_mp3_ts_config = 
	"filters{" EOL
	"	FileDemuxer;" EOL
	"	FileMuxer { format=ts; }" EOL
	"}" EOL
	"source=FileDemuxer;" EOL
	"connections{" EOL
	"	FileDemuxer[0] { peer=FileMuxer; in=0; }" EOL
	"}" EOL
	"configs{" EOL
	"	"NAME_MUXER_FILE"=%s;" EOL
//	"	"NAME_REALTIME"=0;" EOL
	"}" EOL
	;

int avf_converter_run(avf_converter_t *converter,
	int c_type,
	const char *input_file,
	const char *output_file)

{
	switch (c_type) {
	case CONV_TYPE_MP3_AAC:
	case CONV_TYPE_MP3_AAC_TS: {
			char *buffer = (char*)avf_malloc(1024);
			sprintf(buffer, g_mp3_aac_config, 
				c_type == CONV_TYPE_MP3_AAC ? "raw" : "ts",
				output_file);

			avf_status_t status = converter->pMediaControl->SetMediaSource(
				"converter", input_file, buffer, 0, 0);
			avf_free(buffer);

			if (status != E_OK)
				return -1;

			status = converter->pMediaControl->RunMedia();
			if (status != E_OK)
				return -1;
		}
		break;

	case CONV_TYPE_MP3_TS: {
			char *buffer = (char*)avf_malloc(1024);
			sprintf(buffer, g_mp3_ts_config, output_file);

			avf_status_t status = converter->pMediaControl->SetMediaSource(
				"converter", input_file, buffer, 0, 0);
			avf_free(buffer);

			if (status != E_OK)
				return -1;

			status = converter->pMediaControl->RunMedia();
			if (status != E_OK)
				return -1;
		}
		break;

	default:
		break;
	}

	return 0;
}


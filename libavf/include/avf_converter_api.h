
#ifndef __AVF_CONVERTER_API_H__
#define __AVF_CONVERTER_API_H__

typedef struct avf_converter_s avf_converter_t;

typedef void (*avf_converter_callback_t)(void *context, int event, int arg1, int arg2);

enum {
	CONV_NONE,
	CONV_TYPE_MP3_AAC = 1,
	CONV_TYPE_MP3_AAC_TS = 2,
	CONV_TYPE_MP3_TS = 3,
	CONV_TYPE_TS_MP4 = 4,
	CONV_TYPE_MP4_TS = 5,
};

enum {
	AVF_CONV_FINISHED,
	AVF_CONV_ERROR,
	AVF_CONV_PROGRESS,
};

avf_converter_t *avf_converter_create(avf_converter_callback_t callback, void *context);
void avf_converter_destroy(avf_converter_t *converter);

int avf_converter_run(avf_converter_t *converter,
	int c_type,
	const char *input_file,
	const char *output_file);

#endif


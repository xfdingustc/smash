
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>

#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
}

#define INBUF_SIZE (4*1024)
#define AUDIO_INBUF_SIZE (20*1024)
#define AUDIO_REFILL_THRESH	(4*1024)

int main(int argc, char **argv)
{
	const char *filename = "/tmp/mmcblk0p1/pal.mp3";
	FILE *f;
	AVCodec *codec;
	AVCodecContext *c = NULL;
    AVPacket avpkt;
    AVFrame *decoded_frame = NULL;
    uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    int len;

	avcodec_register_all();

	av_init_packet(&avpkt);

	codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
	if (codec == NULL) {
		fprintf(stderr, "codec not found\n");
		return -1;
	}

	c = avcodec_alloc_context3(codec);
	if (c == NULL) {
		fprintf(stderr, "could not alloc codec context\n");
		return -1;
	}

	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "could not open codec\n");
		return -1;
	}

	f = fopen(filename, "rb");
	if (f == NULL) {
		fprintf(stderr, "could not open %s\n", filename);
		return -1;
	}

	avpkt.data = inbuf;
	avpkt.size = fread(inbuf, 1, AUDIO_INBUF_SIZE, f);

	int n = 0;
	while (avpkt.size > 0) {
		int got_frame = 0;

		if (decoded_frame == NULL) {
			if ((decoded_frame = av_frame_alloc()) == NULL) {
				fprintf(stderr, "could not alloc frame\n");
				return -1;
			}
		}

		len = avcodec_decode_audio4(c, decoded_frame, &got_frame, &avpkt);
		if (len < 0) {
			fprintf(stderr, "error while decoding\n");
			return -1;
		}

		if (got_frame) {
			int data_size = av_samples_get_buffer_size(NULL, c->channels,
				decoded_frame->nb_samples, c->sample_fmt, 1);
			if (data_size < 0) {
				fprintf(stderr, "failed to calculate data size\n");
				return -1;
			}
			printf("got frame %d, %d\n", data_size, ++n);
		}

		avpkt.size -= len;
		avpkt.data += len;
		avpkt.dts = 
		avpkt.pts = AV_NOPTS_VALUE;
		if (avpkt.size < AUDIO_REFILL_THRESH) {
			memmove(inbuf, avpkt.data, avpkt.size);
			avpkt.data = inbuf;
			len = fread(avpkt.data + avpkt.size, 1, AUDIO_INBUF_SIZE - avpkt.size, f);
			if (len > 0) {
				avpkt.size += len;
			}
		}
	}

	fclose(f);

	avcodec_close(c);
	av_free(c);
	av_frame_free(&decoded_frame);

	return 0;
}

#if 0
#include "codec_common.h"
#include "mp3_dec.h"

mp3_dec_t g_dec;

u8 *mp3_malloc(void *ctx, uint_t size)
{
	return (u8*)malloc(size);
}

void mp3_free(void *ctx, void *ptr)
{
	free(ptr);
}

int_t mp3_input(void *ctx, u8 *buf, uint_t size)
{
	return 0;
}

int_t mp3_output(void *ctx, u8 *buf, uint_t size)
{
	return 0;
}

int main(int argc, char **argv)
{
	g_dec.malloc_cb = mp3_malloc;
	g_dec.free_cb = mp3_free;
	g_dec.input_cb = mp3_input;
	g_dec.output_cb = mp3_output;
	g_dec.ctx = NULL;

	if (mp3_dec_init(&g_dec) < 0)
		return -1;

	mp3_dec_run(&g_dec);

	mp3_dec_release(&g_dec);

	return 0;
}
#endif


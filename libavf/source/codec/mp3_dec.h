
#ifndef __MP3_DEC_H__
#define __MP3_DEC_H__

#include "setjmp.h"
#include "codec_common.h"
#include "bs_reader.h"
#include "bs_reader.h"

#define MP3_SIZE		(512 + 2048)
#define MP3_PADDING	8

typedef struct mp3_channel_s {
	u16 part2_3_length;
	u16 big_values;
	u16 global_gain;
	u16 scalefac_compress;
	
	u8 flags;
	u8 block_type;
	u8 table_select[3];
	u8 subblock_gain[3];
	u8 region0_count;
	u8 region1_count;
	
	u8 scalefac[39];
} mp3_channel_t;

typedef struct mp3_granule_s {
	mp3_channel_t ch[2];
} mp3_granule_t;

typedef struct mp3_sideinfo_s {
	u16 main_data_begin;
	u8 private_bits;

	u8 scfsi[2];
	mp3_granule_t gr[2];

} mp3_sideinfo_t;

typedef struct mp3_dec_s {
	// by caller
	u8 *(*malloc_cb)(void *ctx, uint_t size);
	void (*free_cb)(void *ctx, void *ptr);
	int_t (*input_cb)(void *ctx, u8 *buf, uint_t size);	// -1: error; 0: eof; >0: number of bytes
	int_t (*output_cb)(void *ctx, u8 *buf, uint_t size);
	void *ctx;

	/// private
	int error;
	jmp_buf jmpbuf;

	sbool eos;

	u16 total_frames;

	u8 *this_frame;
	u8 *next_frame;
	u8 *main_data;

	u16 this_frame_size;
	u16 this_frame_data_off;
	sbool this_frame_copied;
	u16 next_frame_size;
	u16 main_data_size;

	u32 flags;
	u8 layer;
	u8 mode;
	u8 ext;
	u8 emphasis;
	u32 bitrate;
	u32 samplerate;

	bs_reader_t bsr;
	u8 inbuf[MP3_SIZE + MP3_PADDING];
} mp3_dec_t;

int mp3_dec_init(mp3_dec_t *dec);
void mp3_dec_release(mp3_dec_t *dec);

int mp3_dec_run(mp3_dec_t *dec);

#endif


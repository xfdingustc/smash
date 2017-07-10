
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "mp3_dec.h"

extern "C" void avf_dump_mem(const void *p, uint_t size);

#define MP3F_MPEG_2_5_EXT		(1 << 0)
#define MP3F_LSF				(1 << 1)
#define MP3F_PROTECT			(1 << 2)
#define MP3F_PADDING			(1 << 3)
#define MP3F_COPYRIGHT			(1 << 4)
#define MP3F_ORIGINAL			(1 << 5)

#define L3_count1table_select	(1 << 0)
#define L3_scalefac_scale		(1 << 1)
#define L3_preflag				(1 << 2)
#define L3_mixed_block_flag	(1 << 3)

static u32 samplerate_table[3] = {
	44100, 48000, 32000,
};

static void dec_error(mp3_dec_t *dec, int error, const char *msg)
{
	printf("mp3 dec error: %s\n", msg);
	dec->error = error;
	longjmp(dec->jmpbuf, 1);
}

int mp3_dec_init(mp3_dec_t *dec)
{
//	dec->bsr.read_cb = read_input;
//	dec->bsr.read_ctx = (void*)dec;
	return 0;
}

void mp3_dec_release(mp3_dec_t *dec)
{
}

static uint_t read_input(mp3_dec_t *dec, u8 *buf, uint_t size)
{
	int result = dec->input_cb(dec->ctx, buf, size);
	if (result < 0) {
		dec_error(dec, -1, "read error");
	}
	if ((uint_t)result < size) {
		uint_t remain = size - (uint_t)result;
		memset(buf + result, 0, remain);
		dec->eos = 1;
	}
	return (uint_t)result;
}

INLINE int is_sync(const u8 *ptr)
{
	return ((ptr[0] << 8) | (ptr[1] & 0xE0)) == 0xFFE0;
}

#define HEADER_LEN			8	// header + (CRC) + main_data_begin
#define CHECK_SYNC_LEN		(16 + HEADER_LEN)

// at least 8 bytes at ptr
// after called, should check eos
static void find_sync(mp3_dec_t *dec)
{
	uint_t toread = CHECK_SYNC_LEN - dec->next_frame_size;
	u8 *next_frame = dec->next_frame;
	u8 *p = next_frame + dec->next_frame_size;

	while (!dec->eos) {

		read_input(dec, p, toread);

		for (uint_t i = 0; i < (CHECK_SYNC_LEN-HEADER_LEN); i++) {
			if (is_sync(next_frame + i)) {
				dec->next_frame_size = CHECK_SYNC_LEN - i;
				if (i) {
					codec_fast_copy(next_frame, next_frame + i, dec->next_frame_size);
				}
				return;
			}
		}

		codec_fast_copy(next_frame, next_frame + (CHECK_SYNC_LEN-HEADER_LEN), HEADER_LEN);

		p = next_frame + HEADER_LEN;
		toread = CHECK_SYNC_LEN - HEADER_LEN;
	}
}

static void refill_frame(mp3_dec_t *dec)
{
	uint_t pad_slot, N;
	uint_t remain, read;

	if (dec->bitrate == 0) {
		dec_error(dec, -1, "freerate not supported");
	}

	pad_slot = (dec->flags & MP3F_PADDING) ? 1 : 0;

	if (dec->layer == 1) {
		N = ((12 * dec->bitrate / dec->samplerate) + pad_slot) * 4;
	} else {
		uint_t slots_per_frame =
			(dec->layer == 3 && (dec->flags & MP3F_LSF)) ? 72 : 144;
		N = (slots_per_frame * dec->bitrate / dec->samplerate) + pad_slot;
	}

	if (N + CHECK_SYNC_LEN > ARRAY_SIZE(dec->inbuf) - (dec->this_frame - dec->inbuf)) {
		dec_error(dec, -1, "frame buffer overflow");
	}

	remain = (N + HEADER_LEN) - dec->this_frame_size;
	read = read_input(dec, dec->this_frame + dec->this_frame_size, remain);
	if (read + dec->this_frame_size < N) {
		dec_error(dec, -1, "incomplete frame");
	}

	dec->this_frame_size = N;
	dec->next_frame = dec->this_frame + N;
	dec->next_frame_size = HEADER_LEN;
}

INLINE uint_t get_main_data_begin(const u8 *frame)
{
	u32 header = codec_get_be_16(frame);
	const u8 *ptr = frame + ((header & 1) ? 4 : 6);
	return codec_get_be_16(ptr) >> 7;
}

static void goto_next_frame(mp3_dec_t *dec)
{
	if (!is_sync(dec->next_frame)) {
		find_sync(dec);
	}

	if (!dec->eos) {
		uint_t next_main_data_begin = get_main_data_begin(dec->next_frame);
		printf("frame %d next_main_data_begin: %d\n", dec->total_frames, next_main_data_begin);

		if (next_main_data_begin == 0) {
			// prev data & this frame are not need any more
			dec->this_frame = dec->inbuf;
		} else {
			uint_t data_size = dec->this_frame_size - dec->this_frame_data_off;
			if (next_main_data_begin < data_size) {
				// tail part of this frame
				const u8 *ptr = dec->next_frame - next_main_data_begin;
				if (dec->this_frame_copied)
					ptr -= dec->this_frame_data_off;
				::memmove(dec->inbuf, ptr, next_main_data_begin);
				dec->this_frame = dec->inbuf + next_main_data_begin;
			} else {
				// all of this frame
				if (!dec->this_frame_copied) {
					::memmove(dec->this_frame,
						dec->this_frame + dec->this_frame_data_off,
						data_size);
				}
				dec->this_frame += data_size;
			}
		}

		// next frame start
		codec_fast_copy(dec->this_frame, dec->next_frame, dec->next_frame_size);
		dec->this_frame_size = dec->next_frame_size;
	}
}

static void decode_header(mp3_dec_t *dec)
{
	u32 header = codec_get_be_32(dec->this_frame);
	uint_t index;

	dec->flags = 0;
	if ((header & 0x00100000) == 0)
		dec->flags |= MP3F_MPEG_2_5_EXT;

	if ((header & 0x00080000) == 0)
		dec->flags |= MP3F_LSF;

	dec->layer = 4 - ((header >> 17) & 3);
	if (dec->layer == 4)
		dec_error(dec, -1, "bad layer");

	if ((header & (1 << 16)) == 0)
		dec->flags |= MP3F_PROTECT;

	index = (header >> 12) & 0xf;
	dec->bitrate = mp3_get_bitrate(dec->layer, dec->flags & MP3F_LSF, index);
	if (dec->bitrate == 0)
		dec_error(dec, -1, "bad bitrate_index");

	index = (header >> 10) & 3;
	if (index == 3)
		dec_error(dec, -1, "bad samplerate");

	dec->samplerate = samplerate_table[index];

	if (dec->flags & MP3F_LSF) {
		dec->samplerate /= 2;
		if (dec->flags & MP3F_MPEG_2_5_EXT)
			dec->samplerate /= 2;
	}

	if ((header >> 9) & 1)
		dec->flags |= MP3F_PADDING;

	dec->mode = 3 - ((header >> 6) & 3);
	dec->ext = (header >> 4) & 3;
	dec->flags |= MP3F_COPYRIGHT << ((header >> 3) & 1);
	dec->flags |= MP3F_ORIGINAL << ((header >> 2) & 1);

	dec->emphasis = header & 3;
}

static void mp3_underrun_cb(void *ctx, bs_reader_t *bsr)
{
	mp3_dec_t *dec = (mp3_dec_t*)ctx;
	dec_error(dec, -1, "bits buffer underrun");
}

static void layer3_sideinfo(mp3_dec_t *dec,
	mp3_sideinfo_t *si, uint_t ngr, uint_t nch, uint_t lsf)
{
	BSR_START(&dec->bsr);

	read_bits(9 - lsf, si->main_data_begin);	// 9
	int priv_bitlen = lsf ? ((nch == 1) ? 1 : 2) : ((nch == 1) ? 5 : 3);
	__read_bits(priv_bitlen, si->private_bits);	// 14

	uint_t gr, ch;

	if (!lsf) {
		for (ch = 0; ch < nch; ch++)
			read_bits(4, si->scfsi[ch]);
	}

	for (gr = 0; gr < ngr; gr++) {
		mp3_granule_t *granule = si->gr + gr;

		for (ch = 0; ch < nch; ch++) {
			mp3_channel_t *channel = granule->ch + ch;

			read_bits(12, channel->part2_3_length);
			read_bits(9, channel->big_values);
			if (channel->big_values > 288) {
				dec_error(dec, -1, "bad bigvalues");
			}

			read_bits(8, channel->global_gain);
			read_bits(lsf ? 9 : 4, channel->scalefac_compress);

			channel->flags = 0;

			int tmp;
			read_bits(1, tmp);	// window_switching_flag
			if (tmp) {
				read_bits(2, channel->block_type);	// 2

				if (channel->block_type == 0) {
					dec_error(dec, -1, "bad block_type");
				}
				if (!lsf && channel->block_type == 2 && si->scfsi[ch]) {
					dec_error(dec, -1, "bad scfsi");
				}

				channel->region0_count = 7;
				channel->region1_count = 36;

				__read_bits(1, tmp);	// 3, mixed_block_type
				if (tmp)
					channel->flags |= L3_mixed_block_flag;
				else if (channel->block_type == 2)
					channel->region0_count = 8;

				__read_bits(5, channel->table_select[0]);	// 8
				__read_bits(5, channel->table_select[1]);	// 13

				read_bits(5, channel->table_select[0]);	// 5
				__read_bits(5, channel->table_select[1]);	// 10

				read_bits(3, channel->subblock_gain[0]);	// 3
				__read_bits(3, channel->subblock_gain[1]);	// 6
				__read_bits(3, channel->subblock_gain[2]);	// 9

			} else {
				channel->block_type = 0;

				read_bits(5, channel->table_select[0]);	// 5
				__read_bits(5, channel->table_select[1]);	// 10
				__read_bits(5, channel->table_select[2]);	// 15

				read_bits(4, channel->region0_count);	// 4
				__read_bits(3, channel->region1_count);	// 7
			}

			// preflag, scalefac_scale, count1table_select
			read_bits(lsf ? 2 : 3, tmp);
			channel->flags |= tmp;
		}
	}

	BSR_END
}

static void decode_main_data(mp3_dec_t *dec,
	mp3_sideinfo_t *si, uint_t ngr, uint_t nch, uint_t lsf,
	const u8 *ptr, uint_t size)
{
	uint_t gr;

	for (gr = 0; gr < ngr; gr++) {
		//mp3_granule_t *granule = si->gr + gr;
		uint_t ch;

		for (ch = 0; ch < nch; ch++) {
			//mp3_channel_t *channel = granule->ch + ch;

			
		}
	}
}

static void decode_frame(mp3_dec_t *dec, mp3_sideinfo_t *si)
{
	bs_reader_t *bsr = &dec->bsr;
	uint_t header_len = (dec->flags & MP3F_PROTECT) ? 6 : 4;

	//avf_dump_mem(dec->this_frame, 8);

	bsr->re_ptr = dec->this_frame + header_len;
	bsr->re_ptr_end = dec->this_frame + dec->this_frame_size;
	bs_reader_init(bsr);

	uint_t lsf = (dec->flags & MP3F_LSF) != 0;
	uint_t nch = dec->mode ? 2 : 1;
	uint_t ngr = lsf ? 1 : 2;
	uint_t si_len = lsf ? (nch == 1 ? 9 : 17) : (nch == 1 ? 17 : 32);

	dec->this_frame_data_off = header_len + si_len;
	dec->this_frame_copied = 0;

	layer3_sideinfo(dec, si, ngr, nch, lsf);

	if (si->main_data_begin > dec->this_frame - dec->inbuf) {
		// prev data is not enough, do not decode

	} else {
		uint_t data_size = dec->this_frame_size - dec->this_frame_data_off;

		if (si->main_data_begin == 0) {
			decode_main_data(dec, si, ngr, nch, lsf,
				dec->this_frame + dec->this_frame_data_off,
				data_size);

		} else {
			::memmove(dec->this_frame,
				dec->this_frame + dec->this_frame_data_off,
				data_size);
			dec->this_frame_copied = 1;

			decode_main_data(dec, si, ngr, nch, lsf,
				dec->this_frame - si->main_data_begin,
				si->main_data_begin + data_size);

		}
	}
}

int mp3_dec_run(mp3_dec_t *dec)
{
	dec->error = 0;
	if (setjmp(dec->jmpbuf))
		return dec->error;

	dec->eos = 0;
	dec->total_frames = 0;

	dec->this_frame = dec->inbuf;
	dec->this_frame_size = 0;

	dec->next_frame = dec->inbuf;
	dec->next_frame_size = 0;

	dec->bsr.read_cb = NULL;
	dec->bsr.underrun_cb = mp3_underrun_cb;
	dec->bsr.ctx = (void*)dec;

	find_sync(dec);
	dec->this_frame_size = dec->next_frame_size;

	while (!dec->eos) {
		mp3_sideinfo_t si;
		//printf("decoded frame: %d\n", dec->total_frames);
		decode_header(dec);
		refill_frame(dec);
		decode_frame(dec, &si);
		goto_next_frame(dec);
		dec->total_frames++;
		//printf("decoded frame: %d\n", dec->total_frames);
	}

	return dec->error;
}


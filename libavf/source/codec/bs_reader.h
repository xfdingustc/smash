
#ifndef __BS_READER_H__
#define __BS_READER_H__

typedef void (*bs_read_cb)(void *ctx, struct bs_reader_s *bsr, const u8 *end_ptr);
typedef void (*bs_underrun_cb)(void *ctx, struct bs_reader_s *bsr);

typedef struct bs_reader_s {
	bs_read_cb read_cb;
	bs_underrun_cb underrun_cb;
	void *ctx;
	const u8 *re_ptr;
	const u8 *re_ptr_end;
	/// private
	u32 re_cache;
	int re_bit_count;	// 16:0; 0:16; -15:31
	sbool underrun;
} bs_reader_t;

INLINE void bs_reader_init(bs_reader_t *bsr)
{
	bsr->re_cache = 0;
	bsr->re_bit_count = 16;	// 0 bits avail
	bsr->underrun = 0;
}

void bs_reader_read(bs_reader_t *bsr, const u8 *end_ptr);

#define BSR_SAVE \
	do { \
		__bsr->re_ptr = re_ptr; \
		__bsr->re_cache = re_cache; \
		__bsr->re_bit_count = re_bit_count; \
	} while (0)

#define BSR_LOAD \
	do { \
		re_ptr = __bsr->re_ptr; \
		re_ptr_end = __bsr->re_ptr_end; \
		re_cache = __bsr->re_cache; \
		re_bit_count = __bsr->re_bit_count; \
	} while (0)

#define BSR_START(_bsr) { \
	bs_reader_t *__bsr = (_bsr); \
	const u8 *RESTRICT re_ptr; \
	const u8 *RESTRICT re_ptr_end; \
	u32 re_cache; \
	int re_bit_count; \
	BSR_LOAD

#define BSR_END \
	BSR_SAVE; }

// _nbits != 0
#define __peek_bits(_nbits) \
	(re_cache >> (32 - (_nbits)))

// _nbits == 0
#define __peek_bits_0(_nbits) \
	((re_cache >> (31 - (_nbits))) >> 1)

// 16 bits avail after refill
#define REFILL_RE_CACHE \
	do { \
		if unlikely(re_bit_count > 0) { \
			re_cache += ((re_ptr[0] << 8) + re_ptr[1]) << re_bit_count; \
			re_ptr += 2; \
			re_bit_count -= 16; \
			if unlikely(re_ptr >= re_ptr_end) { \
				bs_reader_read(__bsr, re_ptr); \
				re_ptr = __bsr->re_ptr; \
				re_ptr_end = __bsr->re_ptr_end; \
			} \
		} \
	} while (0)

#define __skip_bits(_nbits) \
	do { \
		re_cache <<= (_nbits); \
		re_bit_count += (_nbits); \
	} while (0)

#define skip_bits(_nbits) \
	do { \
		REFILL_RE_CACHE; \
		__skip_bits(_nbits); \
	} while (0)

#define __read_bits(_nbits, _v) \
	do { \
		_v = __peek_bits(_nbits); \
		__skip_bits(_nbits); \
	} while (0)

#define read_bits(_nbits, _v) \
	do { \
		REFILL_RE_CACHE; \
		__read_bits(_nbits, _v); \
	} while (0)

#endif



#ifndef __CODEC_COMMON_H__
#define __CODEC_COMMON_H__

typedef unsigned char	sbool;
typedef unsigned char	u8;
typedef signed char		s8;

typedef signed short	s16;
typedef unsigned short	u16;

typedef signed int		s32;
typedef unsigned int	u32;

typedef signed int		int_t;
typedef unsigned int	uint_t;

#ifndef INLINE
#define INLINE static inline
#endif

#ifndef RESTRICT
#define RESTRICT __restrict__
#endif

#define likely(x)	(__builtin_expect((x), 1))
#define unlikely(x)	(__builtin_expect((x), 0))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a) \
	(sizeof(_a) / sizeof((_a)[0]))
#endif

INLINE u32 codec_get_be_16(const u8 *p)
{
	return (p[0] << 8) | p[1];
}

INLINE u32 codec_get_be_32(const u8 *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

INLINE void codec_fast_copy(u8 *to, u8 *from, uint_t n)
{
	for (; n; n--) *to++ = *from++;
}


#endif



#include "avf_common.h"
#include "mpeg_utils.h"
#include "avf_util.h"

static uint32_t mp3_bitrate_table[5][15] = {
	// MPEG-1
	{ 0,  32000,  64000,  96000, 128000, 160000, 192000, 224000,	// layer I
       256000, 288000, 320000, 352000, 384000, 416000, 448000 },
	{ 0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,	// layer II
       128000, 160000, 192000, 224000, 256000, 320000, 384000 },
	{ 0,  32000,  40000,  48000,  56000,  64000,  80000,  96000,	// layer III
		   112000, 128000, 160000, 192000, 224000, 256000, 320000 },

	// MPEG-2
	{ 0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,	// layer I
       128000, 144000, 160000, 176000, 192000, 224000, 256000 },
	{ 0,   8000,  16000,  24000,  32000,  40000,  48000,  56000,	// layer II & III
        64000,  80000,  96000, 112000, 128000, 144000, 160000 }
};

static uint32_t mpeg4_sample_rates[16] = 
{
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

extern "C" int mp3_get_bitrate(int layer, int lsf, int index)
{
	if (index < 0 || index >= 15)
		return 0;
	return lsf ?
		mp3_bitrate_table[3 + (layer >> 1)][index] :
		mp3_bitrate_table[layer - 1][index];
}

extern "C" int mp3_get_samplerate(int version, int samplerate_index)
{
	int result = -1;

	switch (version) {
	case 3:	// MPEG1
		switch (samplerate_index) {
		case 0: result = 44100; break;
		case 1: result = 48000; break;
		case 2: result = 32000; break;
		default: break;
		}
		break;

	case 2:	// MPEG2
		switch (samplerate_index) {
		case 0: result = 22050; break;
		case 1: result = 24000; break;
		case 2: result = 16000; break;
		default: break;
		}
		break;

	case 0:	// MPEG2.5
		switch (samplerate_index) {
		case 0: result = 11025; break;
		case 1: result = 12000; break;
		case 2: result = 8000; break;
		default: break;
		}
		break;
	}

	return result;
}

extern "C" int mp3_parse_header(uint32_t frame_header, mp3_header_info_t *header)
{
	if ((frame_header & 0xFFE00000) != 0xFFE00000) {
		return -1;
	}

	int version = (frame_header>>19) & 0x3;
	if (version != 0 && version != 2 && version != 3) {
		return -1;
	}
	header->mpeg_verion = version;

	int layer = 4 - ((frame_header>>17) & 0x3);
	if (layer == 4) {
		return -1;
	}
	header->layer = layer;

	header->bitrate = mp3_get_bitrate(layer, (frame_header & 0x00080000) == 0, (frame_header >> 12) & 0xf);
	if (header->bitrate == 0) {
		return -1;
	}

	int samplerate_index = (frame_header>>10) & 3;
	int samplerate = mp3_get_samplerate(version, samplerate_index);
	if (samplerate < 0) {
		return -1;
	}
	header->sampling_rate = samplerate;

	int channel = (frame_header>>6) & 3;
	switch (channel) {
	case 0: header->num_channels = 2; break;
	case 1: header->num_channels = 2; break;
	case 2: header->num_channels = 2; break;
	case 3: header->num_channels = 1; break;
	}

	header->protection_bit = (frame_header>>16) & 1;

	int samples_per_frame = -1;
	switch (version) {
	case 0:		// mpeg 2.5 LSF
	case 2:		// mpeg 2 LSF
		switch (layer) {
		case 1: samples_per_frame = 384; break;
		case 2: samples_per_frame = 1152; break;
		case 3: samples_per_frame = 576; break;
		}
		break;

	case 3:		// mpeg 1
		switch (layer) {
		case 1: samples_per_frame = 384; break;
		case 2: samples_per_frame = 1152; break;
		case 3: samples_per_frame = 1152; break;
		}
		break;
	}

	if (samples_per_frame < 0)
		return -1;

	header->samples_per_frame = samples_per_frame;

	return 0;
}

extern "C" uint32_t aac_gen_specific_config(uint32_t samplerate, uint32_t nchannels, int version)
{
	int samplerate_index = aac_get_sample_rate_index(samplerate);
	return (2 << 11) | // AAC-LC
			(samplerate_index << 7) |
			(nchannels << 3) | 
			(0 << 2) |	// frame length - 1024 samples
			(0 << 1) |	// does not depend on core coder
			(0);		// is not extension
}

extern "C" int aac_get_sample_rate_index(uint32_t sample_rate)
{
	avf_uint_t i;
	for (i = 0; i < ARRAY_SIZE(mpeg4_sample_rates); i++) {
		if (sample_rate == mpeg4_sample_rates[i])
			return i;
	}
	return -1;
}

extern "C" int aac_parse_header(const uint8_t *buffer, uint32_t size, aac_adts_header_t *header)
{
	int len, rdb, ch, sr, aot, crc_abs;
	const uint8_t *tmp;
	uint32_t value;

	// check
	if (size < 7)
		return -1;

	if ((load_be_16(buffer) & 0xFFF0) != 0xFFF0)
		return -2;

	// parse
	crc_abs = buffer[1] & 1;
	aot = (buffer[2] >> 6) & 0x3;
	sr = (buffer[2] >> 2) & 0xF;

	tmp = buffer + 2;
	value = load_be_16(tmp);

	ch = (value >> 6) & 0x07;

	tmp = buffer + 3;
	value = load_be_32(tmp);

	len = (value >> 13) & 0x1FFF;
	rdb = value & 0x3;

	// check
	if (mpeg4_sample_rates[sr] == 0)
		return -3;

	if (len < 7)
		return -4;

	// fill
	header->object_type = aot + 1;
	header->chan_config = ch;
	header->crc_absent = crc_abs;
	header->num_aac_frames = rdb + 1;
	header->sampling_index = sr;
	header->sample_rate = mpeg4_sample_rates[sr];
	header->samples = (rdb + 1) * 1024;
	header->bitrate = len * 8 * header->sample_rate / header->samples;

	return 0;
}

// find start pos after 00 00 01
extern "C" const uint8_t *avc_find_nal(const uint8_t *ptr, const uint8_t *ptr_end)
{
	// next aligned address
	const uint8_t *a = ptr + 4 - ((uint32_t)(uint64_t)ptr & 3);

	for (ptr_end -= 3; ptr < a && ptr < ptr_end; ptr++) {
		if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1)
			return ptr + 3;
	}

	for (ptr_end -= 3; ptr < ptr_end; ptr += 4) {
		uint32_t val = *(const uint32_t *)ptr;
		if ((val - 0x01010101) & ((~val) & 0x80808080)) {
			if (ptr[1] == 0) {
				if (ptr[0] == 0 && ptr[2] == 1)
					return ptr + 3;
				if (ptr[2] == 0 && ptr[3] == 1)
					return ptr + 4;
			}
			if (ptr[3] == 0) {
				if (ptr[2] == 0 && ptr[4] == 1)
					return ptr + 5;
				if (ptr[4] == 0 && ptr[5] == 1)
					return ptr + 6;
			}
		}
	}

	for (ptr_end += 3; ptr < ptr_end; ptr++) {
		if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1)
			return ptr + 3;
	}

	return ptr_end + 6;
}

extern "C" uint8_t *mpeg_get_descr_len(uint8_t *p, uint32_t *size)
{
	unsigned tmp;

	*size = 0;
	for (unsigned i = 4; i; i--) {
		tmp = *p++;
		*size = (*size << 7) + (tmp & 0x7F);
		if ((tmp & 0x80) == 0)
			break;
	}

	return p;
}


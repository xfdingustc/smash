
#ifndef __MPEG_UTILS_H__
#define __MPEG_UTILS_H__

typedef struct aac_adts_header_s {
	uint32_t	sample_rate;	// 48000
	uint32_t	samples;		// n * 1024
	uint32_t	bitrate;		// 
	uint8_t		crc_absent;
	uint8_t		object_type;
	uint8_t		sampling_index;
	uint8_t		chan_config;	// number of channels
	uint8_t		num_aac_frames;	// n
} aac_adts_header_t;

typedef struct mp3_header_info_s {
	uint8_t		mpeg_verion;	// 3: mpeg-1; 2: mpeg-2; 0: mpeg-2.5
	uint8_t		layer;			// 1: layer 1; 2: layer 2; 3: layer 3
	uint8_t		num_channels;
	uint8_t		protection_bit;	// 0: protected
	uint32_t	samples_per_frame;
	uint32_t	bitrate;		//
	uint32_t	sampling_rate;
} mp3_header_info_t;

extern "C" int mp3_get_bitrate(int layer, int lsf, int index);
extern "C" int mp3_get_samplerate(int version, int samplerate_index);

extern "C" int mp3_parse_header(uint32_t frame_header, mp3_header_info_t *header);

extern "C" uint32_t aac_gen_specific_config(uint32_t samplerate, uint32_t nchannels, int version);
extern "C" int aac_get_sample_rate_index(uint32_t sample_rate);
extern "C" int aac_parse_header(const uint8_t *buffer, uint32_t size, aac_adts_header_t *header);

extern "C" const uint8_t *avc_find_nal(const uint8_t *ptr, const uint8_t *ptr_end);

extern "C" uint8_t *mpeg_get_descr_len(uint8_t *p, uint32_t *size);

#endif


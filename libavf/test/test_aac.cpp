
#include "avf_common.h"
#include "avf_new.h"
#include "avf_util.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_config.h"

#include "avf_media_format.h"
#include "aac_encoder.h"

#include "aacenc_lib.h"

#include "mpeg_utils.h"

#include <fcntl.h>
#include <sys/stat.h>

#define CHECK_AACERR(_res) \
	do { \
		if (_res != AACENC_OK) { \
			printf("failed %d, line: %d\n", _res, __LINE__); \
			return -1; \
		} \
	} while (0)

int get_file_size(int fd)
{
	struct stat stat;
	if (fd < 0) {
		AVF_LOGE("File not open");
		return 0;
	}
	if (fstat(fd, &stat) < 0) {
		AVF_LOGE("fstat failed");
		return 0;
	}
	return (int)stat.st_size;
}

int encode_one_frame(HANDLE_AACENCODER haac, unsigned char *buffer, int buflen)
{
	unsigned char out_buffer[20*1024];

	AACENC_BufDesc in_buf = {0};
	AACENC_BufDesc out_buf = {0};
	AACENC_InArgs in_args = {0};
	AACENC_OutArgs out_args = {0};

	AACENC_ERROR res;
	
	int in_identifier = IN_AUDIO_DATA;
	void *in_ptr = (void*)buffer;
	int in_size = buflen;
	int in_elem_size = 2;
	
	int out_identifier = OUT_BITSTREAM_DATA;
	void *out_ptr = (void*)out_buffer;
	int out_size = sizeof(out_buffer);
	int out_elem_size = 1;

	in_buf.numBufs = 1;
	in_buf.bufs = &in_ptr;
	in_buf.bufferIdentifiers = &in_identifier;
	in_buf.bufSizes = &in_size;
	in_buf.bufElSizes = &in_elem_size;

	out_buf.numBufs = 1;
	out_buf.bufs = &out_ptr;
	out_buf.bufferIdentifiers = &out_identifier;
	out_buf.bufSizes = &out_size;
	out_buf.bufElSizes = &out_elem_size;

	res = aacEncEncode(haac, &in_buf, &out_buf, &in_args, &out_args);
	if (res != AACENC_OK) {
		printf("aacEncEncode failed, res: %d\n", res);
		return -1;
	}

	return 0;
}

#define BLOCK_SIZE 2048

int encode(int in_fd, HANDLE_AACENCODER haac)
{
	unsigned char buffer[BLOCK_SIZE];

	int size = get_file_size(in_fd);
	int nread;

	if (size == 0) {
		printf("file is empty\n");
		return -1;
	}

	printf("file size: %d\n", size);

	while (size >= BLOCK_SIZE) {
		nread = read(in_fd, buffer, BLOCK_SIZE);
		if (nread != BLOCK_SIZE) {
			printf("read failed, res %d\n", nread);
			return -1;
		}

		if (encode_one_frame(haac, buffer, BLOCK_SIZE) < 0)
			return -1;

		size -= BLOCK_SIZE;
		printf("encode one frame done, size: %d\n", size);
	}

	return 0;
}

int main(void)
{
	HANDLE_AACENCODER haac;
	AACENC_InfoStruct aac_enc_info;
	AACENC_ERROR res;

	int in_fd;

	res = aacEncOpen(&haac, 0, 1);
	CHECK_AACERR(res);

	res = aacEncoder_SetParam(haac, AACENC_AOT, 129);
	CHECK_AACERR(res);

	res = aacEncoder_SetParam(haac, AACENC_SAMPLERATE, 48000);
	CHECK_AACERR(res);

	res = aacEncoder_SetParam(haac, AACENC_CHANNELMODE, MODE_1);
	CHECK_AACERR(res);

	res = aacEncoder_SetParam(haac, AACENC_CHANNELORDER, 1);
	CHECK_AACERR(res);

	res = aacEncoder_SetParam(haac, AACENC_BITRATE, 96000);
	CHECK_AACERR(res);

	res = aacEncoder_SetParam(haac, AACENC_TRANSMUX, 2);
	CHECK_AACERR(res);

	res = aacEncoder_SetParam(haac, AACENC_AFTERBURNER, 0);
	CHECK_AACERR(res);

	res = aacEncEncode(haac, NULL, NULL, NULL, NULL);
	CHECK_AACERR(res);

	res = aacEncInfo(haac, &aac_enc_info);
	CHECK_AACERR(res);

	in_fd = avf_open_file("/tmp/mmcblk0p1/save.pcm", O_RDONLY, 0);
	if (in_fd < 0) {
		printf("cannot open file\n");
		return -1;
	}

	encode(in_fd, haac);

	avf_close_file(in_fd);

	aacEncClose(&haac);

	return 0;
}


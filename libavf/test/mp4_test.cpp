
#define LOG_TAG	"mp4_test"

#include <limits.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>

#include "avf_common.h"
#include "avf_queue.h"
#include "avf_new.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "avf_socket.h"

#include "avio.h"
#include "sys_io.h"
#include "mp4_builder.h"

#include "test_common.cpp"

struct mp4_item_s {
	const char *filename;
	CMP4Builder::range_s range;
};

mp4_item_s g_mp4_items[32];
mp4_item_s *g_curr_item;
int g_num_items;

const char *g_output_filename;
int gb_compare;
int gb_read;
uint32_t g_offset;
uint32_t g_size;
int gb_big_file;

static void test_write(CMP4Builder *builder, IAVIO *io)
{
	uint8_t buffer[4*KB];
	uint32_t remain = builder->GetSize();
	uint32_t total_write = 0;

	while (remain > 0) {
		uint32_t toread = MIN(remain, sizeof(buffer));
		int n = builder->Read(buffer, toread);
		if (n != (int)toread) {
			AVF_LOGE("Read %d returns %d", toread, n);
			if (n < 0)
				break;
		}
		n = io->Write(buffer, n);
		if (n != (int)toread)
			break;
		remain -= toread;
		total_write += toread;
	}

	AVF_LOGI("total write %d", total_write);
}

static void test_read(CMP4Builder *builder)
{
	if (builder->Seek(g_offset) != E_OK) {
		AVF_LOGE("Seek to %d failed", g_offset);
		return;
	}

	uint8_t *buf = avf_malloc_bytes(g_size);

	int nread = builder->Read(buf, g_size);
	if (nread != (int)g_size) {
		AVF_LOGE("Read %d bytes failed", g_size);
		avf_free(buf);
		return;
	}

	avf_free(buf);
	AVF_LOGI("test read done");
}

static void seek_to(CMP4Builder *builder, IAVIO *io, uint32_t offset)
{
	if (builder->Seek(offset) != E_OK) {
		AVF_LOGE("Seek builder failed, offset: %d", offset);
		AVF_DIE;
	}

	if (io->Seek(offset) != E_OK) {
		AVF_LOGE("Seek io failed, offset: %d", offset);
		AVF_DIE;
	}
}

static void read_and_compare(CMP4Builder *builder, IAVIO *io,
	uint8_t *i_buf, uint8_t *o_buf, uint32_t offset, uint32_t size)
{
	int nread;

	if ((nread = builder->Read(i_buf, size)) != (int)size) {
		AVF_LOGE("cannot read builder, offset: %d, size: %d, nread: %d", offset, size, nread);
		AVF_DIE;
	}

	if ((nread = io->Read(o_buf, size)) != (int)size) {
		AVF_LOGE("cannot read io, offset: %d, size: %d, nread: %d", offset, size, nread);
		AVF_DIE;
	}

	if (::memcmp(i_buf, o_buf, size)) {
		AVF_LOGE("not same, offset: %d, size: %d, nread: %d", offset, size, nread);
		AVF_DIE;
	}
}

#define TEST_BUF_SZ	(16*1024)

static uint32_t generate_size(uint32_t fsize, uint32_t offset)
{
	uint32_t size = ((uint32_t)rand() % TEST_BUF_SZ) + 1;
	uint32_t remain = fsize - offset;
	return size <= remain ? size : remain;
}

static void test_compare(CMP4Builder *builder, IAVIO *io)
{
	uint8_t *i_buf = avf_malloc_bytes(TEST_BUF_SZ);
	uint8_t *o_buf = avf_malloc_bytes(TEST_BUF_SZ);
	uint32_t nloop = 0;

	uint32_t fsize = builder->GetSize();
	::srand(avf_get_curr_tick());

	for (;;) {
		uint32_t offset = (uint32_t)rand() % fsize;
		uint32_t size = generate_size(fsize, offset);

		seek_to(builder, io, offset);

		read_and_compare(builder, io, i_buf, o_buf, offset, size);
		offset += size;

		size = generate_size(fsize, offset);

		read_and_compare(builder, io, i_buf, o_buf, offset, size);

		if ((++nloop % 5000) == 0) {
			AVF_LOGI("loop: %d", nloop);
		}
	}

	avf_free(i_buf);
	avf_free(o_buf);
}

static struct option long_options[] = {
	{"add",		HAS_ARG,	0,	'a'},
	{"from",	HAS_ARG,	0,	'f'},
	{"to",		HAS_ARG,	0,	't'},
	{"output",	HAS_ARG,	0,	'o'},
	{"compare",	NO_ARG,		0,	'c'},
	{"read",	NO_ARG,		0,	'r'},
	{"offset",	HAS_ARG,	0,	0x80},
	{"size",	HAS_ARG,	0,	0x81},
	{"big",		NO_ARG,		0,	0x82},
	{NULL, NO_ARG, 0, 0}
};
static const char *short_options = "a:cf:t:o:r";

static int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'a':
			if (g_num_items >= (int)ARRAY_SIZE(g_mp4_items)) {
				AVF_LOGE("too many files");
				return -1;
			} else {
				g_curr_item = g_mp4_items + g_num_items;
				g_num_items++;
				g_curr_item->filename = optarg;
				g_curr_item->range.start_time_ms = 0;
				g_curr_item->range.end_time_ms = INT_MAX;
				g_curr_item->range.b_set_duration = false;
			}
			break;

		case 'c':
			gb_compare = 1;
			break;

		case 'f':
			if (g_curr_item == NULL) {
				AVF_LOGE("error");
				return -1;
			} else {
				g_curr_item->range.start_time_ms = atoi(optarg);
			}
			break;

		case 't':
			if (g_curr_item == NULL) {
				AVF_LOGE("error");
				return -1;
			} else {
				g_curr_item->range.end_time_ms = atoi(optarg);
				g_curr_item->range.b_set_duration = true;
			}
			break;

		case 'o':
			g_output_filename = optarg;
			break;

		case 'r':
			gb_read = 1;
			break;

		case 0x80:
			g_offset = atoi(optarg);
			break;

		case 0x81:
			g_size = atoi(optarg);
			break;

		case 0x82:
			gb_big_file = 1;
			break;

		default:
			AVF_LOGE("unknown option found: %c, index: %d", ch, option_index);
			return -1;
		}
	}

	return 0;
}

// mp4_test -a file -f from -t to -a file -o outputfile -c
int main(int argc, char **argv)
{
	if (init_param(argc, argv) < 0)
		return -1;

	AVF_LOGI("total %d files", g_num_items);

	avf_status_t status;

	CMP4Builder *builder = CMP4Builder::Create(gb_big_file);
	for (int i = 0; i < g_num_items; i++) {
		status = builder->AddFile(g_mp4_items[i].filename, &g_mp4_items[i].range);
		if (status != E_OK)
			break;
	}

	if (builder->GetState() == CMP4Builder::STATE_PARSING) {
		builder->FinishAddFile();
	}

	if (builder->GetState() == CMP4Builder::STATE_READY) {
		AVF_LOGI("file size: " LLD, builder->GetSize());

		if (gb_read) {
			test_read(builder);
		} else {

			if (g_output_filename) {
				IAVIO *io = CSysIO::Create();

				if (gb_compare) {
					if (io->OpenRead(g_output_filename) == E_OK) {
						test_compare(builder, io);
					}
				} else {
					if (io->CreateFile(g_output_filename) == E_OK) {
						test_write(builder, io);
					}
				}

				avf_safe_release(io);
			}
		}
	}

	builder->Release();

	// for debug
	avf_mem_info(1);
	avf_file_info();
	avf_socket_info();

	return 0;
}


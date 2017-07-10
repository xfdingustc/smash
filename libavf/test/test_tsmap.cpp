
#define LOG_TAG	"tsmap"

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
#include "file_io.h"
#include "tsmap.h"

#include "test_common.cpp"

int g_test = 0;
int g_offset = 0;
int g_size = 0;
char g_input_filename[256];
char g_output_filename[256];
char g_error_filename[256];

static struct option long_options[] = {
	{"input",		HAS_ARG,		0,	'i'},
	{"output",		HAS_ARG,		0,	'o'},
	{"error",		HAS_ARG,		0,	'e'},
	{"test",			NO_ARG,		0,	't'},
	{"offset",		HAS_ARG,		0,	'O'},
	{"size",			HAS_ARG,		0,	'S'},
	{NULL, NO_ARG, 0, 0}
};

static const char *short_options = "O:S:i:o:e:t";

#define TEST_BUF_SZ	(188*64)

static void test_read(IAVIO *i_io, IAVIO *o_io)
{
	uint8_t buffer[TEST_BUF_SZ];
	uint32_t size;
	uint32_t tocopy;
	int rval;

	AVF_LOGI("TS size: " LLD, i_io->GetSize());

	if (g_offset || g_size) {
		avf_status_t status = i_io->Seek(g_offset);
		if (status != E_OK) {
			AVF_LOGE("cannot seek to %d", g_offset);
			return;
		}
		size = g_size;
	} else {
		size = i_io->GetSize();
	}

	while (size > 0) {
		tocopy = MIN(size, sizeof(buffer));

		rval = i_io->Read(buffer, tocopy);
		if (rval != (int)tocopy) {
			AVF_LOGE("read failed, it returns %d", rval);
			return;
		}

		rval = o_io->Write(buffer, tocopy);
		if (rval != (int)tocopy) {
			AVF_LOGE("write failed, it returns %d", rval);
			return;
		}

		size -= tocopy;
	}

	AVF_LOGI("pos: %d, size: %d", (int)i_io->GetPos(), (int)i_io->GetSize());
}

static void test_compare(IAVIO *i_io, IAVIO *o_io)
{
	uint8_t buffer[TEST_BUF_SZ];
	uint8_t o_buffer[TEST_BUF_SZ];
	int rval;

	AVF_LOGI("TS size: " LLD, i_io->GetSize());
	srand(avf_get_curr_tick());

	int mx = (i_io->GetSize() / 188);
	int count = 0;

	int offset;
	int size;

	for (;;) {
		int offset_n = count < 100 ? count : rand() % mx;
		int size_n = mx - offset_n;

		offset = offset_n * 188;
		size = size_n * 188;

		if (size > TEST_BUF_SZ)
			size = TEST_BUF_SZ;

		rval = i_io->Seek(offset);
		if (rval != E_OK) {
			AVF_LOGE("seek to %d failed", offset);
			break;
		}

		rval = i_io->Read(buffer, size);
		if (rval != size) {
			AVF_LOGE("cannot read from %d, size %d", offset, size);
			break;
		}

		rval = o_io->Seek(offset);
		if (rval != E_OK) {
			AVF_LOGE("cannot seek to %d", offset);
			break;
		}

		rval = o_io->Read(o_buffer, size);
		if (rval != size) {
			AVF_LOGE("cannot read from %d, size %d", offset, size);
			break;
		}

		if (::memcmp(buffer, o_buffer, size)) {
			AVF_LOGE("not match, offset: %d, size: %d", offset, size);
			break;
		}

		if ((++count % 1000) == 0) {
			AVF_LOGI("loop %d", count);
		}
	}

	IAVIO *eio = CFileIO::Create();
	if (eio->CreateFile(g_error_filename) != E_OK) {
		AVF_LOGE("cannot create %s", g_error_filename);
		goto Done;
	}

	rval = eio->Write(buffer, size);
	if (rval != size) {
		AVF_LOGE("write to %s failed", g_error_filename);
		goto Done;
	}

	AVF_LOGI("result write to %s", g_error_filename);

Done:
	eio->Release();
}

static void test_main(int open, void(*proc)(IAVIO*,IAVIO*))
{
	IAVIO *i_io = CTSMapIO::Create();
	IAVIO *o_io = CFileIO::Create();
	avf_status_t status;

	status = i_io->OpenRead(g_input_filename);
	if (status != E_OK) {
		AVF_LOGE("cannot open input %s", g_input_filename);
		goto Done;
	}

	status = open ? o_io->OpenRead(g_output_filename) : o_io->CreateFile(g_output_filename);
	if (status != E_OK) {
		AVF_LOGE("cannot create output %s", g_output_filename);
		goto Done;
	}

	(proc)(i_io, o_io);

Done:
	i_io->Release();
	o_io->Release();
}

static int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'i':
			strcpy(g_input_filename, optarg);
			break;

		case 'o':
			strcpy(g_output_filename, optarg);
			break;

		case 'e':
			strcpy(g_error_filename, optarg);
			break;

		case 't':
			g_test = 1;
			break;

		case 'O':
			g_offset = atoi(optarg);
			break;

		case 'S':
			g_size = atoi(optarg);
			break;

		default:
			AVF_LOGE("unknown option found: %c", ch);
			return -1;
		}
	}

	return 0;
}

// test_tsmap --input /input_filename/ --output /output_filename/
int main(int argc, char **argv)
{
	if (init_param(argc, argv) < 0)
		return -1;

	if (g_input_filename[0] == 0 || g_output_filename[0] == 0) {
		AVF_LOGE("no filenames");
		return -1;
	}

	if (g_test) {
		test_main(1, test_compare);
	} else {
		test_main(0, test_read);
	}

	return 0;
}


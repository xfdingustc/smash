
#define LOG_TAG "writer_list"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_media_format.h"
#include "avio.h"
#include "buffer_io.h"

#include "async_writer.h"
#include "mp4_writer.h"
#include "ts_writer.h"
#include "raw_writer.h"

typedef IMediaFileWriter* (*avf_create_writer_proc)(IEngine *pEngine, 
	CConfigNode *node, int index, const char *avio);

typedef struct writer_create_struct_s {
	const char *name;
	avf_create_writer_proc create_writer;
} writer_create_struct_t;

static const writer_create_struct_t g_writer_create_struct[] =
{
	{"async", CAsyncWriter::Create},
	{"mp4", CMP4Writer::Create},
	{"ts", CTSWriter::Create},
#ifdef LINUX_OS
	{"raw", CRawWriter::Create},
#endif
};

IMediaFileWriter* avf_create_file_writer(IEngine *pEngine, 
	const char *name, CConfigNode *node, int index, const char *avio)
{
	for (avf_size_t i = 0; i < ARRAY_SIZE(g_writer_create_struct); i++) {
		if (::strcmp(name, g_writer_create_struct[i].name) == 0) {
			return g_writer_create_struct[i].create_writer(pEngine, node, index, avio);
		}
	}
	return NULL;
}


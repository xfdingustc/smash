
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>

#ifdef WIN32_OS
#include <windows.h>
#endif

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

#include "vdb_cmd.h"
#include "vdb_api.h"
#include "vdb_local.h"

#include "test_common.cpp"

//#define REMUX_AUDIO

#ifndef WIN32_OS
#define OUTPUT_FILE		"/tmp/mmcblk0p1/out.mp4"
#define INPUT_AUDIO_FILE	"/tmp/mmcblk0p1/test.mp3"
#define DUMP_FILE_NAME	"/tmp/mmcblk0p1/dump.dat"
#else
#define OUTPUT_FILE		"C:\\temp\\out.mp4"
#define INPUT_AUDIO_FILE	"C:\\temp\\test.mp3"
#define DUMP_FILE_NAME	"C:\\temp\\dump.dat"
#endif

#define SRC_VDB_ID	"vdb_1"
#define DEST_VDB_ID	"vdb_2"

vdb_local_t *g_vdb;
vdb_local_t *g_dest_vdb;

vdb_server_t *g_vdb_server;
vdb_http_server_t *g_http_server;

CMutex g_lock_clips;
vdb_local_item_t *g_items;
vdb_clip_desc_t *g_clips;
int g_num_items;
int g_marked_clips;

#ifndef WIN32_OS
char g_src_vdb_path[256] = "/tmp/mmcblk0p1/100TRANS/";
char g_dst_vdb_path[256] = "/tmp/mmcblk0p1/copy/";
#else
char g_src_vdb_path[256] = "C:\\temp\\100TRANS\\";
char g_dst_vdb_path[256] = "C:\\temp\\100TRANS-copy\\";
#endif

int g_b_interrupt;
int g_b_transfer;
int g_b_noaction;
int g_b_remux;
int g_b_server;
int g_b_dump;
int g_b_upload;

// for transfer
TDynamicArray<uint32_t> g_clip_indices;

int g_clip_index;
int g_data_type;
int g_stream;
int g_upload_opt;

uint16_t my_read_le_16(const uint8_t **pp)
{
	const uint8_t *p = *pp; *pp += 2;
	return load_le_16(p);
}

uint32_t my_read_le_32(const uint8_t **pp)
{
	const uint8_t *p = *pp; *pp += 4;
	return load_le_32(p);
}

uint64_t my_read_le_64(const uint8_t **pp)
{
	uint32_t lo = my_read_le_32(pp);
	uint32_t hi = my_read_le_32(pp);
	return MK_U64(lo, hi);
}

const char *get_clip_type_name(int clip_type)
{
	switch (clip_type) {
	case CLIP_TYPE_BUFFER: return "buffered";
	case CLIP_TYPE_MARKED: return "marked";
	default: return "playlist";
	}
}

void read_clips_info(vdb_local_item_t *item, vdb_clip_desc_t *clip, const uint8_t *p)
{
	// vdb_ack_GetClipSetInfoEx_t
	p += 4;	// clip_type
	uint32_t num = my_read_le_32(&p);	// total_clips
	p += 4;		// total_length_ms
	p += 4;		// live_clip_id

	for (uint32_t i = 0; i < num; i++, item++, clip++) {
		// vdb_clip_info_t
		//item->clip_type = clip_type;
		item->clip_id = my_read_le_32(&p);

		//clip->clip_type = item->clip_type;
		clip->clip_id = item->clip_id;
		clip->clip_date = my_read_le_32(&p);

		uint32_t duration = my_read_le_32(&p);	// clip_duration_ms
		uint64_t start_time_ms = my_read_le_64(&p);	// clip_start_time_ms_lo, clip_start_time_ms_hi
		int num_streams = my_read_le_16(&p);		// num_streams
		int flags = my_read_le_16(&p);			// flags

		avf_size_t size = num_streams * sizeof(avf_stream_attr_t);
		::memcpy(clip->stream_attr, p, size);
		p += size;

		int clip_type = my_read_le_32(&p);
		item->clip_type = clip_type;
		clip->clip_type = clip_type;

		int extra_size = my_read_le_32(&p);
		const uint8_t *next_p = p + extra_size;

		item->start_time_ms = start_time_ms;
		item->end_time_ms = start_time_ms + duration;
		item->flags = VDB_LOCAL_TRANSFER_SCENE_DATA;
		item->reserved = 0;

		clip->clip_duration_ms = duration;
		clip->clip_start_time = start_time_ms;
		clip->num_streams = num_streams;
		clip->reserved = 0;
		clip->vdb_id = avf_strdup(SRC_VDB_ID);	// per vdb

		AVF_LOGI("read %s clip, start_time_ms: " LLD ", duration: %d, num_streams: %d",
			get_clip_type_name(clip_type), start_time_ms, duration, num_streams);

		if (flags & GET_CLIP_EXTRA) {
			// p += UUID_LEN + (3*4);	// uuid, ref_clip_date, gmtoff, real_clip_id
		}

		p = next_p;
	}
}

int read_source_clips(void)
{
	AUTO_LOCK(g_lock_clips);

	vdb_local_ack_t ack_0;
	vdb_local_ack_t ack_1;

	if (!g_marked_clips) {
		if (vdb_local_get_clip_set_info(g_vdb, CLIP_TYPE_BUFFER, GET_CLIP_EXTRA, &ack_0) < 0)
			return -1;
	}

	if (vdb_local_get_clip_set_info(g_vdb, CLIP_TYPE_MARKED, GET_CLIP_EXTRA, &ack_1) < 0)
		return -1;

	// vdb_ack_GetClipSetInfoEx_t
	int num_0 = g_marked_clips ? 0 : ((vdb_ack_GetClipSetInfoEx_t*)ack_0.data)->total_clips;
	int num_1 = ((vdb_ack_GetClipSetInfoEx_t*)ack_1.data)->total_clips;
	int num = num_0 + num_1;
	AVF_LOGI("total clips: %d (buffered) + %d (marked) = %d", num_0, num_1, num);

	g_items = avf_malloc_array(vdb_local_item_t, num);
	g_clips = avf_malloc_array(vdb_clip_desc_t, num);
	g_num_items = num;

	if (!g_marked_clips) {
		read_clips_info(g_items + 0, g_clips + 0, (const uint8_t*)ack_0.data);
	}
	read_clips_info(g_items + num_0, g_clips + num_0, (const uint8_t*)ack_1.data);

	if (!g_marked_clips) {
		vdb_local_free_ack(g_vdb, &ack_0);
	}
	vdb_local_free_ack(g_vdb, &ack_1);

	return 0;
}

void print_clips_info(void)
{
	AUTO_LOCK(g_lock_clips);
	for (int i = 0; i < g_num_items; i++) {
		vdb_local_item_t *item = g_items + i;
		printf("clip %d:\n", i);
		printf("	type: %d (%s)\n", item->clip_type, get_clip_type_name(item->clip_type));
		printf("	id: 0x%x (%d)\n", item->clip_id, item->clip_id);
		printf("	start_time_ms: " LLD "\n", item->start_time_ms);
		printf("	end_time_ms: " LLD "\n", item->end_time_ms);
		printf("	duration_ms: %d\n", (int)(item->end_time_ms - item->start_time_ms));
		char url[256];
		vdb_local_get_playback_url("vdb_id", item->clip_type, item->clip_id, 0, URL_TYPE_TS,
			item->start_time_ms, (uint32_t)(item->end_time_ms - item->start_time_ms), url);
		printf("	%s\n", url);
	}
}

int print_transfer_info(int *bFinished)
{
	vdb_transfer_info_t info;

	if (vdb_transfer_get_info(g_vdb, &info) < 0) {
		AVF_LOGE("vdb_transfer_get_info failed");
		return -1;
	}

	uint64_t total = info.total.picture_size + info.total.video_size + info.total.index_size;
	AVF_LOGI("total size: " LLD " = " LLD " (P) + " LLD " (V) + " LLD " (I)", total, 
		info.total.picture_size, info.total.video_size, info.total.index_size);

	uint64_t transfered = info.transfered.picture_size + info.transfered.video_size + info.transfered.index_size;
	AVF_LOGI("transfered: " LLD " = " LLD " (P) + " LLD " (V) + " LLD " (I), finished: %d", transfered, 
		info.transfered.picture_size, info.transfered.video_size, info.transfered.index_size, info.b_finished);

	if (bFinished) {
		*bFinished = info.b_finished;
	}

	return 0;
}

void print_copy_result(void)
{
	vdb_local_ack_t ack;

	if (vdb_transfer_get_result(g_vdb, &ack) < 0)
		return;

	int num = ((vdb_ack_GetClipSetInfoEx_t*)ack.data)->total_clips;
	const uint8_t *p = ack.data;

	// vdb_ack_GetClipSetInfoEx_t
	AVF_LOGI("clip_type: %d", my_read_le_32(&p));
	AVF_LOGI("total_clips: %d", my_read_le_32(&p));
	p += 4;	// total_length_ms
	p += 4;	// live_clip_id

	// num of vdb_clip_info_t
	for (int i = 0; i < num; i++) {
		AVF_LOGI("  ref clip %d", i);
		AVF_LOGI("    clip_id: 0x%x", my_read_le_32(&p));
		AVF_LOGI("    clip_date: 0x%x", my_read_le_32(&p));
		AVF_LOGI("    clip_duration_ms: %d", my_read_le_32(&p));
		AVF_LOGI("    clip_start_time_ms_lo: %d", my_read_le_32(&p));
		AVF_LOGI("    clip_start_time_ms_hi: %d", my_read_le_32(&p));

		int num_streams = my_read_le_16(&p);
		AVF_LOGI("    num_streams: %d", num_streams);

		int flags = my_read_le_16(&p);
		AVF_LOGI("    flags: %d", flags);

		p += num_streams * sizeof(avf_stream_attr_t);	// skip stream_attr[]

		// vdb_clip_info_ex_t
		int clip_type = my_read_le_32(&p);
		AVF_LOGI("	 clip_type: %s (%d)", get_clip_type_name(clip_type), clip_type);

		int extra_size = my_read_le_32(&p);
		const uint8_t *next_p = p + extra_size;

		if (flags & GET_CLIP_EXTRA) {
			p += UUID_LEN;	// skip uuid
			p += 12;		// skip ref_clip_date, gmtoff, real_clip_id
		}

		p = next_p;
	}

	vdb_local_free_ack(g_vdb, &ack);
}

static void test_install_signal_handler(void (*handler)(int))
{
	signal(SIGINT, handler);
#ifndef MINGW
	signal(SIGQUIT, handler);
#endif
	signal(SIGTERM, handler);
}

static void test_signal_handler(int signo)
{
	AVF_LOGI("signal: %d", signo);
	g_b_interrupt = 1;
}

#ifdef WIN32_OS
void my_print(const char *msg)
{
	printf("%s\n", msg);
}
#endif

static struct option long_options[] = {
	{"dump",		HAS_ARG,		0,	'd'},
	{"transfer", NO_ARG,		0,	't'},
	{"notranser", NO_ARG,	0,	'n'},
	{"remux",	HAS_ARG,		0,	'r'},
	{"server",	HAS_ARG,		0,	's'},
	{"type",		HAS_ARG,		0,	'T'},
	{"stream",	HAS_ARG,		0,	'S'},
	{"source",	HAS_ARG,		0,	'f'},
	{"dest",		HAS_ARG,		0,	'o'},
	{"marked",	NO_ARG,		0,	'm'},
	{"clip",		HAS_ARG,		0,	'c'},
	{NULL, NO_ARG, 0, 0}
};

static const char *short_options = "S:T:c:d:tr:s:u:f:o:mn";

int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'r':
			g_b_remux = 1;
			g_clip_index = atoi(optarg);
			break;

		case 's':
			g_b_server = 1;
			g_clip_index = atoi(optarg);
			break;

		case 't':
			g_b_transfer = 1;
			g_b_noaction = 0;
			break;

		case 'n':
			g_b_transfer = 1;
			g_b_noaction = 1;
			break;

		case 'c': {
				uint32_t clip_index = atoi(optarg);
				g_clip_indices._Append(&clip_index);
			}
			break;

		case 'd':
			g_b_dump = 1;
			g_clip_index = atoi(optarg);
			break;

		case 'u':
			g_b_upload = 1;
			g_clip_index = atoi(optarg);
			break;

		case 'S':
			g_stream = atoi(optarg);
			break;

		case 'T':
			if (g_b_dump) {
				g_data_type = atoi(optarg);
			} else if (g_b_upload) {
				const char *p = optarg;
				for (int c = *p++; c; c = *p++) {
					switch (c) {
					case '0': g_upload_opt |= UPLOAD_GET_V0; break;
					case '1': g_upload_opt |= UPLOAD_GET_V1; break;
					case 'p': g_upload_opt |= UPLOAD_GET_PIC; break;
					case 'g': g_upload_opt |= UPLOAD_GET_GPS; break;
					case 'o': g_upload_opt |= UPLOAD_GET_OBD; break;
					case 'a': g_upload_opt |= UPLOAD_GET_ACC; break;
					default:
						printf("unknown upload type '%c'\n", c);
						return -1;
					}
				}
			} else {
				printf("must specify -d or -u\n");
				return -1;
			}
			break;

		case 'f':
			strcpy(g_src_vdb_path, optarg);
			break;

		case 'o':
			strcpy(g_dst_vdb_path, optarg);
			break;

		case 'm':
			g_marked_clips = 1;
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

int load_source_vdb(void)
{
	avf_safe_mkdir(g_src_vdb_path);

	// -----------------------------------
	// load source vdb
	// -----------------------------------
	AVF_LOGI("-----------------------");
	AVF_LOGI("--- load source vdb ---");
	AVF_LOGI("-----------------------");
	g_vdb = vdb_local_create(g_vdb_server);
	if (vdb_local_load(g_vdb, g_src_vdb_path, false, SRC_VDB_ID) < 0) {
		AVF_LOGE("cannot load %s", g_src_vdb_path);
		return -1;
	}

	uint64_t total_space = 0;
	uint64_t used_space = 0;
	uint64_t marked_space = 0;
	uint64_t clip_space = 0;
	if (vdb_local_get_space_info(g_vdb, &total_space, &used_space, &marked_space, &clip_space) < 0) {
		AVF_LOGE("vdb_local_get_space_info failed");
		return -1;
	}

	AVF_LOGI("marked: " LLD ", clip: " LLD, marked_space, clip_space);

	// -----------------------------------
	// read all clips info (buffered + marked) from source vdb
	// -----------------------------------
	AVF_LOGI("-------------------------");
	AVF_LOGI("--- read source clips ---");
	AVF_LOGI("-------------------------");
	if (read_source_clips() < 0)
		return -1;

	return 0;
}

void unload_source_vdb(void)
{
	// -----------------------------------
	// unload source vdb
	// -----------------------------------
	vdb_local_unload(g_vdb);
	vdb_local_destroy(g_vdb);

	AUTO_LOCK(g_lock_clips);
	avf_safe_free(g_items);

	for (int i = 0; i < g_num_items; i++) {
		vdb_clip_desc_t *clip = g_clips + i;
		avf_safe_free(clip->vdb_id);
	}
	avf_safe_free(g_clips);
}

void unload_dest_vdb(void)
{
	// -----------------------------------
	// unload dest vdb
	// -----------------------------------
	vdb_local_unload(g_dest_vdb);
	vdb_local_destroy(g_dest_vdb);
}

int load_dest_vdb(void)
{
	avf_safe_mkdir(g_dst_vdb_path);

	// -----------------------------------
	// load dest vdb
	// -----------------------------------
	AVF_LOGI("---------------------");
	AVF_LOGI("--- load dest vdb ---");
	AVF_LOGI("---------------------");
	g_dest_vdb = vdb_local_create(g_vdb_server);
	if (vdb_local_load(g_dest_vdb, g_dst_vdb_path, true, DEST_VDB_ID) < 0) {
		AVF_LOGE("cannot load %s", g_dst_vdb_path);
		return -1;
	}

	return 0;
}

int test_transfer(void)
{
	if (load_source_vdb() < 0)
		return -1;

	if (load_dest_vdb() < 0)
		return -1;

	vdb_local_item_t *items = g_items;
	int num_items = g_num_items;

	if (g_clip_indices.count > 0) {
		items = avf_malloc_array(vdb_local_item_t, g_clip_indices.count);
		num_items = g_clip_indices.count;

		for (int i = 0; i < num_items; i++) {
			uint32_t index = g_clip_indices.elems[i];
			if (index >= (uint32_t)g_num_items) {
				AVF_LOGD("no such clip: %d", index);
				return -1;
			}
			AVF_LOGD("transfer: %d", index);
			items[i] = g_items[index];
		}
	}

	// -----------------------------------
	// init transfer session
	// -----------------------------------
	AVF_LOGI("---------------------");
	AVF_LOGI("--- init transfer ---");
	AVF_LOGI("---------------------");
	if (vdb_transfer_init(g_vdb, items, num_items) < 0) {
		AVF_LOGE("vdb_transfer_init failed");
		return -1;
	}

	// print size info
	if (print_transfer_info(NULL) < 0)
		return -1;

	if (!g_b_noaction) {
		// -----------------------------------
		// start transfer session
		// -----------------------------------
		AVF_LOGI("----------------------");
		AVF_LOGI("--- start transfer ---");
		AVF_LOGI("----------------------");
		if (vdb_transfer_start(g_vdb, g_dest_vdb) < 0) {
			AVF_LOGE("vdb_transfer_start failed");
			return -1;
		}

		// catch user interrupt
		test_install_signal_handler(test_signal_handler);

		// -----------------------------------
		// wait until transfer finished
		// -----------------------------------
		while (true) {
			// check for user interrupt
			for (int i = 0; i < 10; i++) {
				if (g_b_interrupt) {
					AVF_LOGI("call vdb_transfer_cancel");
					if (vdb_transfer_cancel(g_vdb) < 0) {
						AVF_LOGE("vdb_transfer_cancel failed");
					}
					goto Done;
				}
				avf_msleep(100);
			}

			int bFinished;
			if (print_transfer_info(&bFinished) < 0)
				return -1;

			if (bFinished) {
				AVF_LOGI("-------------------------");
				AVF_LOGI("--- transfer finished ---");
				AVF_LOGI("-------------------------");
				break;
			}
		}
	}

Done:
	// -----------------------------------
	// show the copied clips (in dest vdb)
	// -----------------------------------
	print_copy_result();

	if (items != g_items) {
		avf_safe_free(items);
	}

	unload_source_vdb();
	unload_dest_vdb();

	return 0;
}

int print_remux_info(int *bFinished)
{
	vdb_remux_info_t info;

	if (vdb_remux_get_info(g_vdb, &info) < 0) {
		return -1;
	}

	AVF_LOGI("progress: %d%%, error: %d, running: %d",
		info.percent, info.error, info.b_running);
	AVF_LOGI("bytes: " LLD " of " LLD, info.total_bytes - info.remain_bytes, info.total_bytes);

	if (bFinished) {
		*bFinished = !info.b_running;
	}

	return 0;
}

int test_remux(void)
{
	if (load_source_vdb() < 0)
		return -1;

	if (g_num_items == 0) {
		AVF_LOGE("no clips");
		return -1;
	}

	// remux the first clip
	vdb_local_item_t *item = g_items + g_clip_index;

	// get clip size
	uint64_t size_video;
	int rval = vdb_remux_get_clip_size(g_vdb, item, 0, &size_video);
	if (rval < 0) {
		AVF_LOGE("vdb_remux_get_clip_size failed %d", rval);
		return rval;
	}
	AVF_LOGI("clip size is " LLD, size_video);

#ifdef REMUX_AUDIO
	AVF_LOGI("set audio: %s", INPUT_AUDIO_FILE);
	rval = vdb_remux_set_audio(g_vdb, 1, INPUT_AUDIO_FILE, "mp3");
	if (rval < 0){
		AVF_LOGE("vdb_remux_set_audio failed %d", rval);
		return rval;
	}
#endif

	// remux video stream 0
	rval = vdb_remux_clip(g_vdb, item, 0, OUTPUT_FILE, "mp4");
	if (rval < 0) {
		AVF_LOGE("call vdb_remux_clip failed %d", rval);
		return rval;
	}

	if (print_remux_info(NULL) < 0) {
		if (vdb_remux_stop(g_vdb, 0) < 0) {
			AVF_LOGE("vdb_remux_stop failed");
		}
		goto Done;
	}

	// catch user interrupt
	test_install_signal_handler(test_signal_handler);

	// polling until finished
	while (true) {
		// check for user interrupt
		for (int i = 0; i < 10; i++) {
			if (g_b_interrupt) {
				AVF_LOGI("call vdb_remux_stop");
				//if (vdb_remux_stop(g_vdb, 1) < 0) {
				if (vdb_remux_stop(g_vdb, 0) < 0) {
					AVF_LOGE("vdb_remux_stop failed");
				}
				goto Done;
			}
			avf_msleep(100);
		}

		int bFinished;
		if (print_remux_info(&bFinished) < 0) {
			return -1;
		}

		if (bFinished) {
			AVF_LOGI("----------------------");
			AVF_LOGI("--- remux finished ---");
			AVF_LOGI("----------------------");
			break;
		}
	}

Done:
	unload_source_vdb();
	return 0;
}

static vdb_local_t *test_get_vdb(void *context, const char *vdb_id)
{
	AVF_LOGI("get_vdb, vdb_id = %s", vdb_id);
	if (vdb_id != NULL && ::strcmp(vdb_id, SRC_VDB_ID) == 0)
		return g_vdb;
	AVF_LOGD("no such vdb: %s", vdb_id);
	return NULL;
}

static void test_release_vdb(void *context, const char *vdb_id, vdb_local_t *vdb)
{
	AVF_LOGI("release_vdb, vdb_id = %s", vdb_id);
}

static int test_get_all_clips(void *context, uint32_t *p_num_clips, vdb_clip_desc_t **p_clips)
{
	AVF_LOGI("get_all_clips");
	AUTO_LOCK(g_lock_clips);

	// if there're no clips
	if (g_num_items == 0) {
		*p_num_clips = 0;
		*p_clips = NULL;
		return 0;
	}

	// allocate result array
	*p_clips = avf_malloc_array(vdb_clip_desc_t, g_num_items);
	if (*p_clips == NULL) {
		*p_num_clips = 0;
		return 0;
	}

	// copy
	for (int i = 0; i < g_num_items; i++) {
		vdb_clip_desc_t *src = g_clips + i;
		vdb_clip_desc_t *dst = *p_clips + i;
		*dst = *src;
		dst->vdb_id = avf_strdup(src->vdb_id);
	}
	*p_num_clips = g_num_items;

	return 0;
}

static void test_release_clips(void *context, uint32_t num_clips, vdb_clip_desc_t *clips)
{
	AVF_LOGI("release_clips");
	if (clips) {
		for (unsigned i = 0; i < num_clips; i++) {
			vdb_clip_desc_t *clip = clips + i;
			avf_safe_free(clip->vdb_id);	// strdup
		}
		avf_free(clips);
	}
}

static int test_get_clip_poster(void *context, const char *vdb_id,
		int clip_type, uint32_t clip_id, uint32_t *p_size, void **p_data)
{
	AUTO_LOCK(g_lock_clips);

	if (vdb_id == NULL || ::strcmp(vdb_id, SRC_VDB_ID) != 0) {
		AVF_LOGE("test_get_clip_poster: unknown vdb_id %s", vdb_id);
		return -1;
	}

	// find the item
	vdb_clip_desc_t *clip = NULL;
	int i = 0;
	for (; i < g_num_items; i++) {
		clip = g_clips + i;
		if ((uint32_t)clip_type == clip->clip_type && clip_id == clip->clip_id)
			break;
	}
	if (i >= g_num_items) {
		AVF_LOGE("test_get_clip_poster: clip not found");
		return -1;
	}

	vdb_local_ack_t ack;
	if (vdb_local_get_index_picture(g_vdb, clip_type, clip_id, clip->clip_start_time, &ack) < 0) {
		AVF_LOGE("vdb_local_get_index_picture failed");
		return -1;
	}

	// parse vdb_ack_GetIndexPicture_t
	vdb_ack_GetIndexPicture_t *param = (vdb_ack_GetIndexPicture_t*)ack.data;
	*p_data = avf_malloc(param->picture_size);
	::memcpy(*p_data, param + 1, param->picture_size);
	*p_size = param->picture_size;

	vdb_local_free_ack(g_vdb, &ack);

	return 0;
}

static void test_release_clip_poster(void *context, const char *vdb_id, uint32_t size, void *data)
{
	avf_safe_free(data);
}

static void test_on_clip_created(void *context, const char *vdb_id, int clip_type, uint32_t clip_id)
{
	// TODO
}

static void test_on_clip_deleted(void *context, const char *vdb_id, int clip_type, uint32_t clip_id)
{
	// TODO
}

static const local_vdb_set_t local_vdb_set = {
	// 1
	test_get_vdb,
	test_release_vdb,
	// 2
	test_get_all_clips,
	test_release_clips,
	// 3
	test_get_clip_poster,
	test_release_clip_poster,
	// 4
	test_on_clip_created,
	test_on_clip_deleted,
};

int test_local_server(void)
{
#ifdef WIN32_OS
	char url[256];
	char buffer[512];
	vdb_local_item_t *item;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#endif

	if (load_source_vdb() < 0)
		return -1;

	g_vdb_server = vdb_create_server(&local_vdb_set, NULL);
	if (g_vdb_server == NULL) {
		goto Done;
	}

	g_http_server = vdb_create_http_server(&local_vdb_set, NULL);
	if (g_http_server == NULL) {
		goto Done;
	}

	// catch user interrupt
	test_install_signal_handler(test_signal_handler);

	AVF_LOGD("run test_local_server");

	vdb_run_server(g_vdb_server);
	vdb_run_http_server(g_http_server);

#ifdef WIN32_OS
	if (g_clip_index < 0 || g_clip_index >= g_num_items) {
		AVF_LOGE("clip index %d out of range", g_clip_index);
		goto Done;
	}

	item = g_items + g_clip_index;
	vdb_local_get_playback_url(SRC_VDB_ID, item->clip_type, item->clip_id, 0, URL_TYPE_TS,
		item->start_time_ms, (uint32_t)(item->end_time_ms - item->start_time_ms), url);
	AVF_LOGI("playback url is %s", url);

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0, sizeof(pi));

	sprintf(buffer, "\"%s\" %s", "C:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe", url);
	if (!CreateProcessA(NULL, buffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		AVF_LOGE("create process failed");
		goto Done;
	} else {
		// close handles, we don't need them
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		AVF_LOGI("process created");
	}
#endif

	// wait for Ctrl-C
	while (true) {
		if (g_b_interrupt) {
			AVF_LOGD("interrupted");
			break;
		}

		avf_msleep(100);
	}

Done:
	vdb_destroy_server(g_vdb_server);
	vdb_destroy_http_server(g_http_server);
	unload_source_vdb();
	return 0;
}

int test_dump(void)
{
	const char *output_filename = DUMP_FILE_NAME;
	vdb_local_clip_reader_t reader = NULL;
	vdb_local_item_t item;

	if (load_source_vdb() < 0)
		return -1;

	if (g_clip_index < 0 || g_clip_index >= g_num_items) {
		AVF_LOGE("clip index %d out of range", g_clip_index);
		goto Done;
	}

	item = g_items[g_clip_index];
	//item.end_time_ms = item.start_time_ms + 2000;
	reader = vdb_local_create_clip_reader(g_vdb, &item);
	if (reader == NULL) {
		AVF_LOGE("create clip reader failed");
		goto Done;
	}

	for (;;) {
		int rval = vdb_local_read_clip(reader, g_data_type, g_stream, output_filename);
		if (rval > 0) {
			// end
			break;
		}
		if (rval < 0) {
			AVF_LOGE("vdb_local_read_clip failed %d", rval);
			break;
		}
		AVF_LOGI("file size: " LLD ", press Enter to continue...",
			avf_get_file_size(output_filename, false));
		char c;
		read(STDIN_FILENO, &c, sizeof(c));
	}

Done:
	if (reader != NULL) {
		vdb_local_destroy_clip_reader(g_vdb, reader);
	}
	unload_source_vdb();
	return 0;
}

int test_upload(void)
{
	vdb_local_reader_t reader = NULL;
	uint8_t *buffer = NULL;
	IAVIO *output = NULL;
	vdb_local_item_t item;
	vdb_local_reader_info_t info;

	if (load_source_vdb() < 0)
		return -1;

	if (g_clip_index < 0 || g_clip_index >= g_num_items) {
		AVF_LOGE("clip index %d out of range", g_clip_index);
		goto Done;
	}

	item = g_items[g_clip_index];
	reader = vdb_local_create_reader(g_vdb, &item, 0, g_upload_opt);
	if (reader == NULL) {
		AVF_LOGE("create reader failed");
		goto Done;
	}

	if (vdb_local_get_reader_info(reader, &info) < 0)
		goto Done;

	AVF_LOGI("clip_time_ms: " LLD ", length_ms: %d, size: " LLD ", pos: " LLD,
		info.clip_time_ms, info.length_ms, info.size, info.pos);

	if ((buffer = avf_malloc_bytes(8*1024)) == NULL)
		goto Done;

	if ((output = CSysIO::Create()) == NULL)
		goto Done;

	if (output->CreateFile(DUMP_FILE_NAME) != E_OK) {
		AVF_LOGE("cannot create %s", DUMP_FILE_NAME);
		goto Done;
	}

	for (;;) {
		int rval = vdb_local_read(reader, buffer, 8*1024);
		if (rval <= 0) {
			if (rval == 0) {
				// done
				break;
			}
			// error
			AVF_LOGE("vdb_local_read error %d", rval);
			break;
		}
		// write to file
		int n = output->Write(buffer, rval);
		if (n != rval) {
			AVF_LOGE("write file failed");
			goto Done;
		}
	}

Done:
	avf_safe_release(output);
	avf_safe_free(buffer);
	if (reader != NULL) {
		vdb_local_destroy_reader(g_vdb, reader);
	}
	unload_source_vdb();
	return 0;
}

// usage:
//	test_local
//		display source clips info
//	test_local -t
//		transfer all source clips to dest path
//	test_local -r n
//		remux source clip n to output.mp4
//	test_local -s
//		run server
//	test_local -d n --type t --stream s
//		dump clip n to DUMP_FILE_NAME
//	test_local -u --type 01pgoa
//		dump upload data to DUMP_FILE_NAME
int main(int argc, char **argv)
{
#ifdef WIN32_OS
	set_print_proc(my_print);
	vdb_local_enable_logs("ewivdp");
#endif

	g_clip_indices._Init();

	if (argc > 1) {
		if (init_param(argc, argv) < 0)
			return -1;
	}

	int rval;

	if (g_b_transfer) {
		rval = test_transfer();
	} else if (g_b_remux) {
		rval = test_remux();
	} else if (g_b_server) {
		avf_WSAStartup(2, 2);
		rval = test_local_server();
		avf_WSACleanup();
	} else if (g_b_dump) {
		rval = test_dump();
	} else if (g_b_upload) {
		rval = test_upload();
	} else {
		load_source_vdb();
		print_clips_info();
		unload_source_vdb();
		rval = 0;
	}

	g_clip_indices._Release();

	// print memory for debug
	avf_mem_info(1);
	avf_file_info();
	avf_socket_info();

	return rval;
}


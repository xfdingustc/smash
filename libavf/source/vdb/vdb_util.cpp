
#define LOG_TAG "vdb_util"

#include <stdlib.h>
#include <sys/stat.h>
#ifdef MINGW
#include <time.h>
#elif defined(MAC_OS)
#include <sys/mount.h>
#include <sys/param.h>
#else
#include <sys/statfs.h>
#endif
#include <dirent.h>

#include "rbtree.h"
#include "avf_common.h"
#include "list.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_socket.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_registry.h"
#include "avf_util.h"
#include "avio.h"
#include "mem_stream.h"

#include "avf_util.h"
#include "vdb_util.h"
#include "vdb_cmd.h"
#include "vdb_clip.h"

//-----------------------------------------------------------------------
//
//  utils
//
//-----------------------------------------------------------------------

// p[0]..p[n-1] -> int
static bool do_get_number(const char *p, int n, int *value)
{
	*value = 0;
	for (; n > 0; n--) {
		int c = *p++;
		int v = c - '0';
		if ((unsigned)v > 9)
			return false;
		*value = (*value) * 10 + v;
	}
	return true;
}

#define get_number(_p, _n, _value) \
	do { \
		if (!do_get_number(_p, _n, _value)) \
			return false; \
		_p += _n; \
	} while (0)

// convert string "20130515-222602.ext" to avf_time_t
bool name_to_local_time(const char *name, avf_time_t *local_time, const char *ext, const char *ext2, int *ptype)
{
	int type = -1;
	if (ptype == NULL)
		ptype = &type;

	struct tm local_tm = {0};

	get_number(name, 4, &local_tm.tm_year);
	local_tm.tm_year -= 1900;
	get_number(name, 2, &local_tm.tm_mon);
	local_tm.tm_mon--;
	get_number(name, 2, &local_tm.tm_mday);

	if (*name != '-')
		return false;
	name++;

	get_number(name, 2, &local_tm.tm_hour);
	get_number(name, 2, &local_tm.tm_min);
	get_number(name, 2, &local_tm.tm_sec);

	// tm_wday, tm_yday are ignored
	local_tm.tm_isdst = -1;	// let mktime decide
	*local_time = avf_mktime(&local_tm);

	if (*local_time == (avf_time_t)-1) {
		AVF_LOGE("mktime failed for %s", name);
		return false;
	}

	// check extension
	if (ext == NULL) {
		if (*name != 0)
			return false;
		*ptype = -1;
		return true;
	} else if (::strcmp(name, ext) == 0) {
		*ptype = 0;
		return true;
	} else {
		if (ext2 && ext2[0]) {
			if (::strcmp(name, ext2) == 0) {
				*ptype = 1;
				return true;
			}
		}
		return false;
	}
}

// yyyymmdd-hhmmss.ext
void local_time_to_name(avf_time_t local_time, const char *ext, local_time_name_t& lname)
{
	lname.len = sprintf(lname.buf, "%04d%02d%02d-%02d%02d%02d%s", SPLIT_TIME(local_time), ext ? ext : "");
}

const char *get_raw_data_name(int raw_type)
{
	switch (raw_type) {
	case kRawData_NULL: return "null";
	case kRawData_GPS: return "gps";
	case kRawData_ACC: return "acc";
	case kRawData_OBD: return "obd";
	default: return "unknown";
	}
}

int get_raw_array_index(int raw_type)
{
	switch (raw_type) {
	case kRawData_GPS: return kGps0Array;
	case kRawData_ACC: return kAcc0Array;
	case kRawData_OBD: return kObd0Array;
	default: return -1;
	}
}

int get_vdb_array_index(int data_type)
{
	switch (data_type) {
	case VDB_DATA_VIDEO: return kVideoArray;
	case VDB_DATA_JPEG: return kPictureArray;
	case RAW_DATA_GPS: return kGps0Array;
	case RAW_DATA_OBD: return kObd0Array;
	case RAW_DATA_ACC: return kAcc0Array;
	default: return -1;
	}
}

//-----------------------------------------------------------------------
//
//  clip_dir_t
//
//-----------------------------------------------------------------------

clip_dir_t::clip_dir_t(clip_dir_set_t *set, avf_time_t dirname, string_t *vdb_path)
{
	this->m_set = set;
	this->m_num_clips = 0;
	this->m_num_segs_0 = 0;
	this->m_num_segs_1 = 0;
	this->m_dirname = dirname;
	this->vdb_path = vdb_path->acquire();
}

clip_dir_t::~clip_dir_t()
{
	avf_safe_release(this->vdb_path);
}

string_t *clip_dir_t::CalcPathName(const char *subdir)
{
	local_time_name_t myname;
	local_time_to_name(m_dirname, PATH_SEP, myname);
	string_t *result = string_t::Add(vdb_path->string(), vdb_path->size(),
		myname.buf, myname.len, subdir, ::strlen(subdir), NULL, 0);
	return result;
}

string_t *clip_dir_t::CalcFileName(const char *subdir, local_time_name_t& lname)
{
	local_time_name_t myname;
	local_time_to_name(m_dirname, PATH_SEP, myname);
	string_t *result = string_t::Add(vdb_path->string(), vdb_path->size(),
		myname.buf, myname.len, subdir, ::strlen(subdir), lname.buf, lname.len);
	return result;
}

//-----------------------------------------------------------------------
//
//  clip_dir_set_t
//
//-----------------------------------------------------------------------
clip_dir_set_t::~clip_dir_set_t()
{
#ifdef AVF_DEBUG
	if (m_num_dirs != 0) {
		AVF_LOGW("clip_set: num_dirs=%d", m_num_dirs);
	}
	if (!list_is_empty(m_dir_list)) {
		AVF_LOGW("clip_set not empty");
	}
#endif
}

void clip_dir_set_t::Clear()
{
	list_head_t *node;
	list_head_t *next;
	list_for_each_del(node, next, m_dir_list) {
		clip_dir_t *cdir = clip_dir_t::from_node(node);
		avf_delete cdir;
	}

	Init();
}

clip_dir_t *clip_dir_set_t::FindDir(avf_time_t dirname)
{
	struct rb_node *n = m_cdir_set.rb_node;
	clip_dir_t *min_ge = NULL;

	while (n) {
		clip_dir_t *cdir = clip_dir_t::from_rbnode(n);
		if (dirname <= cdir->m_dirname) {
			min_ge = cdir;
			n = n->rb_left;
		} else {
			n = n->rb_right;
		}
	}

	if (min_ge && dirname == min_ge->m_dirname)
		return min_ge;

	return NULL;
}

clip_dir_t *clip_dir_set_t::GetAvail(avf_size_t max_segments)
{
	if (m_last_dir != NULL) {
		if (m_last_dir->SegmentsLessThan(max_segments))
			return m_last_dir;
	}

	list_head_t *node = m_dir_list.prev;
	if (node != &m_dir_list) {
		clip_dir_t *cdir = clip_dir_t::from_node(node);
		if (cdir->SegmentsLessThan(max_segments))
			return cdir;
	}

	return NULL;
}

clip_dir_t *clip_dir_set_t::AddClipDir(string_t *vdb_path, avf_time_t dirname, bool bCreate, bool *bExists)
{
	struct rb_node **p = &m_cdir_set.rb_node;
	struct rb_node *parent = NULL;
	while (*p) {
		parent = *p;
		clip_dir_t *tmp = clip_dir_t::from_rbnode(parent);

		if (dirname < tmp->m_dirname) {
			p = &parent->rb_left;
		} else if (dirname > tmp->m_dirname) {
			p = &parent->rb_right;
		} else {
			local_time_name_t lname;
			local_time_to_name(dirname, "", lname);
			AVF_LOGW("clipdir already exists: %s", lname.buf);
			if (bCreate) {
				m_last_dir = tmp;
			}
			if (bExists) {
				*bExists = true;
			}
			return tmp;
		}
	}

	clip_dir_t *cdir = avf_new clip_dir_t(this, dirname, vdb_path);

	list_add(&cdir->list_dir, m_dir_list);
	m_num_dirs++;

	rb_link_node(&cdir->rbnode, parent, p);
	rb_insert_color(&cdir->rbnode, &m_cdir_set);

	if (bCreate) {
		m_last_dir = cdir;
	}
	if (bExists) {
		*bExists = false;
	}

	return cdir;
}

void clip_dir_set_t::RemoveClipDir(clip_dir_t *cdir, bool bDirty)
{
	if (cdir == m_last_dir) {
		m_last_dir = NULL;
	}

	m_num_dirs--;
	mbDirty = bDirty;

	list_del(&cdir->list_dir);
	rb_erase(&cdir->rbnode, &m_cdir_set);

	avf_delete cdir;
}

clip_dir_t *clip_dir_set_t::GetLastDir()
{
	if (m_last_dir != NULL)
		return m_last_dir;

	list_head_t *node = m_dir_list.prev;
	if (node != &m_dir_list)
		return clip_dir_t::from_node(node);

	return NULL;
}

void clip_dir_set_t::Serialize(CMemBuffer *pmb)
{
	pmb->write_le_32(m_num_dirs);
	pmb->write_le_32(mbDirty);

	list_head_t *node;
	list_for_each(node, m_dir_list) {
		clip_dir_t *cdir = clip_dir_t::from_node(node);
		pmb->write_le_8(1);
		pmb->write_le_32(cdir->m_num_clips);
		pmb->write_le_32(cdir->m_num_segs_0);
		pmb->write_le_32(cdir->m_num_segs_1);
		pmb->write_le_32(cdir->m_dirname);
	}
	pmb->write_le_8(0);
}

//-----------------------------------------------------------------------
//
// misc
//
//-----------------------------------------------------------------------

#define FLAGS_MUTE_AUDIO	(1 << 0)
#define FLAGS_FULL_PATH		(1 << 1)
#define FLAGS_SPEC_TIME		(1 << 2)

void vdb_get_server_ip(CSocket *socket, char *server_ip)
{
#ifdef LINUX_SERVER
	::strcpy(server_ip, "127.0.0.1");
#else
	if (socket == NULL) {
		::strcpy(server_ip, "127.0.0.1");
	} else {
		socket->GetServerIP(server_ip, 20);	// TODO
	}
#endif
}

void vdb_format_playlist_path(CSocket *socket, char *path, clip_id_t plist_id)
{
	char server_ip[20];

	vdb_get_server_ip(socket, server_ip);

	::sprintf(path, "http://%s:%d/files/playlist/%d/", server_ip, VDB_HTTP_PORT, plist_id);
}

// clip_time_ms, length_ms, flags, param1, param2
#define M3U8_CLIP_FORMAT	LLD "-%d-%d-%d-%d"

// http://192.168.110.1:8085/address
// address:
//		[vdb/<vdb_id>]/<address1>
// 		address1:
//			<id>/<opt>/<filename>
//			id:
//				clip/<clip_type>/<clip_id>
//				playlist/<playlist_id>
//			opt:
//				%d (stream)
//				upload/%x (upload_opt)
// 			filename:
//				<clip_time_ms>-<length_ms>-<flags>-<param1>-<param2>.ext
//				ext:
//					ts|m3u8

int vdb_format_url(const char *server_ip, char *url,
	const char *vdb_id, int clip_type, clip_id_t clip_id,
	int stream, int upload_opt,	// upload_opt is valid if stream < 0
	int b_playlist, int url_type,
	avf_u64_t clip_time_ms, avf_u32_t length_ms,
	avf_u32_t param1, avf_u32_t param2)
{
	char *url_old = url;

	// http ip, port
	url += ::sprintf(url, "http://%s:%d/", server_ip, VDB_HTTP_PORT);

	// vdb_id
	if (vdb_id && vdb_id[0]) {
		url += ::sprintf(url, "vdb/%s/", vdb_id);
	}

	// path
	if (b_playlist) {
		url += ::sprintf(url, "playlist/%d/", clip_type);
	} else {
		url += ::sprintf(url, "clip/%d/%x/", clip_type, clip_id);
	}

	// opt
	if (stream >= 0) {
		url += ::sprintf(url, "%d/", stream);
	} else {
		url += ::sprintf(url, "upload/%x/", upload_opt);
	}

	int flags = 0;
	if (url_type & URL_MUTE_AUDIO) flags |= FLAGS_MUTE_AUDIO;
	if (url_type & URL_FULL_PATH) flags |= FLAGS_FULL_PATH;
	if (url_type & URL_SPEC_TIME) flags |= FLAGS_SPEC_TIME;

	// filename
	url += ::sprintf(url, M3U8_CLIP_FORMAT, clip_time_ms, length_ms,
		flags, param1, param2);

	// ext
	if (stream >= 0) {
		const char *ext;

		switch (url_type & _URL_TYPE_MASK) {
		default:
		case URL_TYPE_TS: ext = VIDEO_FILE_TS_EXT; break;
		case URL_TYPE_HLS: ext = HLS_FILE_EXT; break;
		case URL_TYPE_MP4: ext = VIDEO_FILE_MP4_EXT; break;
		}

		url += ::sprintf(url, "%s", ext);
	} else {
		url += ::sprintf(url, "%s", CLIP_FILE_EXT);
	}

	return url - url_old;
}

// address: item in m3u8
// 		<clip_time_ms>-<length_ms>-<flags>-<param1>-<param2>.ts (.m3u8)
// will be changed to address in url
int vdb_format_hls_item(char *url, bool bMuteAudio, bool bFixPTS,
	avf_u64_t clip_time_ms, avf_u32_t length_ms,
	avf_u32_t param1, avf_u32_t param2)
{
	char *url_old = url;

	int flags = 0;
	if (bMuteAudio) flags |= FLAGS_MUTE_AUDIO;
	if (bFixPTS) flags |= FLAGS_SPEC_TIME;

	url += ::sprintf(url, M3U8_CLIP_FORMAT, clip_time_ms, length_ms, flags, param1, param2);
	url += ::sprintf(url, VIDEO_FILE_TS_EXT);

	return url - url_old;
}

const char *vdb_parse_url_filename(const char *p, int *url_type,
	avf_u64_t *clip_time_ms, avf_u32_t *length_ms,
	int *b_mute_audio, int *b_full_path, int *b_spec_time,
	avf_u32_t *param1, avf_u32_t *param2)
{
	char *end;

	// clip_time_ms
	*clip_time_ms = ::strtoull(p, &end, 10);
	if (end == p) {
		AVF_LOGE("no clip_time_ms %s", p);
		return NULL;
	}
	p = end;
	if (*p == '-') p++;

	// length_ms
	*length_ms = ::strtoul(p, &end, 10);
	if (end == p) {
		AVF_LOGE("no length_ms %s", p);
		return NULL;
	}
	p = end;
	if (*p == '-') p++;

	// flags: mute_audio, full_path
	int flags = ::strtoul(p, &end, 10);
	if (end == p) {
		AVF_LOGE("no mute flags %s", p);
		return NULL;
	}
	p = end;
	if (*p == '-') p++;

	*b_mute_audio = (flags & FLAGS_MUTE_AUDIO) != 0;
	*b_full_path = (flags & FLAGS_FULL_PATH) != 0;
	*b_spec_time = (flags & FLAGS_SPEC_TIME) != 0;

	// param1
	*param1 = ::strtoul(p, &end, 10);
	if (end == p) {
		AVF_LOGE("no param1");
		return NULL;
	}
	p = end;
	if (*p == '-') p++;

	// param2
	*param2 = ::strtoul(p, &end, 10);
	if (end == p) {
		AVF_LOGE("no param2");
		return NULL;
	}
	p = end;
	if (*p == '-') p++;

	// url_type
	if (::strncasecmp(p, STR_WITH_SIZE(VIDEO_FILE_TS_EXT)) == 0) {
		*url_type = URL_TYPE_TS;
	} else if (::strncasecmp(p, STR_WITH_SIZE(HLS_FILE_EXT)) == 0) {
		*url_type = URL_TYPE_HLS;
	} else if (::strncasecmp(p, STR_WITH_SIZE(CLIP_FILE_EXT)) == 0) {
		*url_type = URL_TYPE_CLIP;
	} else if (::strncasecmp(p, STR_WITH_SIZE(VIDEO_FILE_MP4_EXT)) == 0) {
		*url_type = URL_TYPE_MP4;
	} else {
		AVF_LOGE("unknown file type");
		return NULL;
	}

	return p;
}

const char *vdb_parse_url(const char *url, char **pvdb_id, int *clip_type, clip_id_t *clip_id,
	int *stream, int *upload_opt, int *b_playlist, int *url_type, 
	avf_u64_t *clip_time_ms, avf_u32_t *length_ms, int *b_mute_audio, int *b_full_path, int *b_spec_time,
	avf_u32_t *param1, avf_u32_t *param2)
{
	const char *p = url;
	char *end;

	// release pvdb_id
	if (pvdb_id) {
		avf_safe_free(*pvdb_id);
	}

	if (::strncasecmp(p, "vdb/", 4) == 0) {
		p += 4;

		const char *pend = ::strchr(p, '/');
		if (pend == NULL) {
			AVF_LOGE("bad vdb_id");
			return NULL;
		}

		if (pvdb_id) {
			int size = pend - p;
			*pvdb_id = (char*)avf_malloc(size + 1);
			::memcpy(*pvdb_id, p, size);
			(*pvdb_id)[size] = 0;
		}

		p = pend + 1;
	}

	if (::strncasecmp(p, "clip/", 5) == 0) {
		p += 5;

		// clip_type
		*clip_type = ::strtoul(p, &end, 10);
		if (p == end) {
			AVF_LOGE("no clip_type %s", p);
			return NULL;
		}
		p = end;
		if (*p == '/') p++;

		// clip_id
		*clip_id = ::strtoul(p, &end, 16);
		if (p == end) {
			AVF_LOGE("no clip_id %s", p);
			return NULL;
		}
		p = end;
		if (*p == '/') p++;

		*b_playlist = 0;

	} else if (::strncasecmp(p, "playlist/", 9) == 0) {
		p += 9;

		// playlist_id == clip_type
		*clip_type = ::strtoul(p, &end, 10);
		if (p == end) {
			AVF_LOGE("no playlist_id %s", p);
			return NULL;
		}
		p = end;
		if (*p == '/') p++;

		// clip_id
		*clip_id = INVALID_CLIP_ID;

		*b_playlist = 1;

	} else {
		return url;
	}

	// stream/opt
	if (::strncasecmp(p, "upload/", 7) == 0) {
		p += 7;

		*stream = -1;
		*upload_opt = ::strtoul(p, &end, 16);
		if (end == p) {
			AVF_LOGE("no upload_opt %s", p);
			return NULL;
		}
		p = end;
		if (*p == '/') p++;

	} else {

		*stream = ::strtoul(p, &end, 10);
		if (end == p) {
			AVF_LOGE("no stream index %s", p);
			return NULL;
		}
		p = end;
		if (*p == '/') p++;

	}

	return vdb_parse_url_filename(p, url_type, clip_time_ms, length_ms,
		b_mute_audio, b_full_path, b_spec_time, param1, param2);
}

// for server
// http://192.168.110.1:8085/address
// address:
//		clips/<vdb_clip_id>/<stream>/<clip_time_ms>-<length_ms>-<mute>-<param1>-<param2>.m3u8
// for vdb server
void vdb_format_url_clips(char *url, avf_u64_t cdir_clip_id,
	int stream, avf_u64_t clip_time_ms, avf_u32_t length_ms,
	int b_mute_audio, avf_u32_t param1, avf_u32_t param2)
{
	::sprintf(url, "clips/" LLD "/%d/" M3U8_CLIP_FORMAT "%s",
		cdir_clip_id, stream, clip_time_ms, length_ms,
		!!b_mute_audio, param1, param2, HLS_FILE_EXT);
}

// for server
const char *vdb_parse_url_clips(const char *url,
	avf_u64_t *cdir_clip_id, int *stream, int *url_type,
	avf_u64_t *clip_time_ms, avf_u32_t *length_ms, 
	int *b_mute_audio, int *b_full_path, int *b_spec_time,
	avf_u32_t *param1, avf_u32_t *param2)
{
	const char *p = url;
	char *end;

	if (::strncasecmp(p, "clips/", 6) != 0)
		return NULL;
	p += 6;

	// cdir_clip_id
	*cdir_clip_id = ::strtoull(p, &end, 10);
	if (end == p)
		return NULL;
	p = end;
	if (*p == '/') p++;

	// stream
	*stream = ::strtoul(p, &end, 10);
	if (end == p)
		return NULL;
	p = end;
	if (*p == '/') p++;

	return vdb_parse_url_filename(p, url_type, clip_time_ms, length_ms,
		b_mute_audio, b_full_path, b_spec_time, param1, param2);
}



#define LOG_TAG "vdb_http_server"

#include <fcntl.h>

#include "avf_common.h"
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
#include "sys_io.h"
#include "buffer_io.h"
#include "avf_tcp_server.h"
#include "mem_stream.h"

#include "vdb_cmd.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_if.h"
#include "vdb.h"
#include "vdb_format.h"
#include "vdb_server.h"
#include "vdb_http_server.h"

#define RN	"\r\n"
#define BR	"<br/>"

#define BAD_REQUEST_STR \
	"HTTP/1.1 400 BAD REQUEST" RN \
	"Content-Type: text/html" RN \
	"Content-Length: 12" RN \
	RN \
	"BAD REQUEST!"

#define NOT_FOUND_STR \
	"HTTP/1.1 404 NOT FOUND" RN \
	"Content-Type: text/html" RN \
	"Content-Length: 15" RN \
	RN \
	"File NOT found!"

#define HTTP_200_OK_STR \
	"HTTP/1.1 200 OK" RN

#define HTTP_206_OK_STR \
	"HTTP/1.1 206 OK" RN

#define CONTENT_TYPE_M3U8_STR \
	"Content-Type: application/vnd.apple.mpegurl" RN

#define CONTENT_TYPE_TS_STR \
	"Content-Type: video/MP2T" RN

#define CONTENT_TYPE_MP4_STR \
	"Content-Type: video/mp4" RN

#define CONTENT_TYPE_APP_DATA \
	"Content-Type: application/octet-stream" RN

#define CONTENT_TYPE_JS \
	"Content-Type: application/javascript" RN

#define CONTENT_TYPE_CSS \
	"Content-Type: text/css" RN

#define CONTENT_TYPE_HTML \
	"Content-Type: text/html;charset=ISO-8859-1" RN

#define CONTENT_TYPE_JPG \
	"Content-Type: image/jpeg" RN

#define ACCEPT_RANGES_STR \
	"Accept-Ranges: bytes" RN

enum {
	DATA_TYPE_APP,
	DATA_TYPE_M3U8,
	DATA_TYPE_TS,
	DATA_TYPE_CLIP,
	DATA_TYPE_MP4,
	DATA_TYPE_HTML,
	DATA_TYPE_JPG,
	DATA_TYPE_JS,
	DATA_TYPE_CSS,
};

#define VHS_HOME	"/usr/local/bin/vhs/"

//-----------------------------------------------------------------------
//
//  CVdbHttpConnection
//
//-----------------------------------------------------------------------
CVdbHttpConnection* CVdbHttpConnection::Create(
	CVdbHttpServer *pServer, CSocket **ppSocket, 
	const vdb_set_s *vdb_set, void *context)
{
	CVdbHttpConnection *result = avf_new CVdbHttpConnection(
		pServer, ppSocket, vdb_set, context);
	if (result) {
		result->RunThread("vdb-http-conn");
	}
	CHECK_STATUS(result);
	return result;
}

CVdbHttpConnection::CVdbHttpConnection(
	CVdbHttpServer *pServer, CSocket **ppSocket, 
	const vdb_set_s *vdb_set, void *context):

	inherited(pServer, ppSocket),

	mp_vdb_set(vdb_set),
	mp_vdb_context(context),
	mp_vdb_id(NULL)
{
	// after 20s send KEEPALIVE for every 5s, 3 times
	mStatus = mpSocket->KeepAlive(20, 5, 3);
	mb_has_vhs = avf_file_exists(VHS_HOME);
}

CVdbHttpConnection::~CVdbHttpConnection()
{
	avf_safe_free(mp_vdb_id);
}

avf_status_t CVdbHttpConnection::ReadRequest()
{
	avf_u8_t *ptr = m_buffer;
	avf_uint_t len = 0;
	avf_uint_t remain = sizeof(m_buffer) - 1;	// trailing 0

	m_buffer_len = 0;
	m_buffer_pos = 0;
	m_buffer_remain = 0;

	while (1) {

		int nread;
		avf_status_t status = TCPReceiveAny(30*1000, (avf_u8_t*)ptr, remain, &nread);

		if (status != E_OK)
			return status;

		len += nread;
		ptr += nread;
		remain -= nread;

		if (len >= 4) {
			const char *pend = ::strstr((const char*)m_buffer, RN RN);
			if (pend) {
				m_buffer_len = pend - (const char*)m_buffer;
				m_buffer[pend - (const char*)m_buffer] = 0;
				m_buffer_pos = m_buffer_len + 4;
				m_buffer_remain = len - m_buffer_pos;
				return E_OK;
			}
		}

		if (remain == 0) {
			AVF_LOGE("too many request data");
			return E_ERROR;
		}
	}
}

const avf_u8_t *CVdbHttpConnection::NextLine(const avf_u8_t *ptr)
{
	const avf_u8_t *p = ptr;

	while (true) {
		if (p[0] == '\r' && p[1] == '\n') {
			p += 2;
			if (p[0] == '\r' && p[1] == '\n')
				return NULL;
			return p;
		}

		if (p[0] == 0)
			return NULL;

		p++;
	}
}

// 124-456
int CVdbHttpConnection::GetRange(const avf_u8_t *ptr, avf_u64_t *from, avf_u64_t *to)
{
	const char *p = (const char *)ptr;
	char *end;

	if (*p == '-') {
		p++;

		*to = ::strtoull(p, &end, 10);
		if (end == p)
			return -1;
		p = end;

		return RANGE_TO;
	}

	*from = ::strtoull(p, &end, 10);
	if (end == p)
		return -1;
	p = end;

	if (*p != '-')
		return -1;
	p++;

	if (*p < '0' || *p > '9')
		return RANGE_FROM;

	*to = ::strtoull(p, &end, 10);
	if (end == p)
		return -1;

	return RANGE_FROM_TO;
}

const char *CVdbHttpConnection::ParseRequest(request_t *request)
{
	const avf_u8_t *ptr = m_buffer;

#ifdef AVF_DEBUG
	char *line = ::strchr((char*)m_buffer, '\r');
	if (line) {
		*line = 0;
		AVF_LOGD(C_CYAN "%s" C_NONE, m_buffer);
		*line = '\r';
	}
#endif

	request->error = ERROR_BadRequest;
	if (::strncasecmp((const char*)ptr, "GET ", 4) == 0) {
		request->code = HTTP_GET;
		ptr += 4;
	} else if (::strncasecmp((const char*)ptr, "HEAD ", 5) == 0) {
		request->code = HTTP_HEAD;
		ptr += 5;
	} else if (::strncasecmp((const char *)ptr, "PUT ", 4) == 0) {
		request->code = HTTP_PUT;
		ptr += 4;
	} else if (::strncasecmp((const char *)ptr, "DELETE ", 7) == 0) {
		request->code = HTTP_DELETE;
		ptr += 7;
	} else {
		AVF_LOGE("GET/HEAD/PUT/DELETE needed");
		return NULL;
	}

	request->error = ERROR_NotFound;
	const char *p = (const char *)ptr;
	const char *result;

	if (*p == '/')
		p++;

	int clip_type;
	clip_id_t clip_id;
	int stream;
	int upload_opt;
	int b_playlist;
	int url_type;
	avf_u64_t clip_time_ms;
	avf_u32_t length_ms;
	int b_mute_audio;
	int b_full_path;
	int b_spec_time;
	avf_u32_t param1;
	avf_u32_t param2;
	bool bParsed = true;

	result = vdb_parse_url(p, &mp_vdb_id, &clip_type, &clip_id,
		&stream, &upload_opt, &b_playlist, &url_type,
		&clip_time_ms, &length_ms, &b_mute_audio, &b_full_path, &b_spec_time,
		&param1, &param2);
	if (result == NULL) {
		// there's error
		//return p;
		bParsed = false;
	} else if (result == p) {
		// got none
		//return p;
		bParsed = false;
	} else {
#ifdef AVF_DEBUG
		AVF_LOGD(C_CYAN "stream: %d, clip_time_ms: " LLD
			", length_ms: %d, mute_audio: %d" C_NONE,
			stream, clip_time_ms, length_ms, b_mute_audio);
#endif
	}

	if (bParsed) {
		request->range.clip_type = clip_type;
		request->range.ref_clip_id = clip_id;
		request->range.b_playlist = b_playlist;
		request->range._stream = stream;
		request->range.length_ms = length_ms;
		request->range.clip_time_ms = clip_time_ms;

		request->param1 = param1;
		request->param2 = param2;
		request->upload_opt = upload_opt;
		request->mute_audio = b_mute_audio;
		request->full_path = b_full_path;
		request->fix_pts = b_spec_time;
		request->url_type = url_type;
	}

	const char *kConnection = "Connection: ";
	const char *kKeepAlive = "keep-alive";
	const char *kRange = "Range: bytes=";
	const char *kContentLength = "Content-Length: ";

	request->error = ERROR_NotFound;

	while (true) {
		if ((ptr = NextLine(ptr)) == NULL)
			break;

		if (::strncasecmp((const char*)ptr, kConnection, 12) == 0) {
			ptr += 12;
			if (::strncasecmp((const char*)ptr, kKeepAlive, 10) == 0) {
				ptr += 10;
				request->keep_alive = 1;
			}
			continue;
		}

		if (::strncasecmp((const char*)ptr, kRange, 13) == 0) {
			AVF_LOGD("%s", ptr);
			ptr += 13;
			request->range_type = GetRange(ptr, &request->addr_from, &request->addr_to);
			if (request->range_type < 0) {
				AVF_LOGE("bad range");
				return NULL;
			}
			continue;
		}

		if (::strncasecmp((const char*)ptr, kContentLength, 16) == 0) {
			ptr += 16;
			char *end;
			request->content_length = ::strtoull((const char*)ptr, &end, 10);
			if (end == (const char*)ptr) {
				AVF_LOGE("bad content length");
				return NULL;
			}
			continue;
		}
	}

	request->error = ERROR_OK;

	return bParsed ? NULL : p;
}

avf_status_t CVdbHttpConnection::SendResponseHeader(request_t *request, avf_u64_t offset,
	avf_u64_t size, avf_u64_t file_size, int data_type)
{
	m_buffer_len = 0;

	// status line
	if (request == NULL || request->range_type == RANGE_NONE) {
		BufferString(STR_WITH_SIZE(HTTP_200_OK_STR));
	} else {
		BufferString(STR_WITH_SIZE(HTTP_206_OK_STR));
	}

	switch (data_type) {
	case DATA_TYPE_APP:
	case DATA_TYPE_CLIP:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_APP_DATA));
		break;

	case DATA_TYPE_M3U8:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_M3U8_STR));
		break;

	case DATA_TYPE_TS:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_TS_STR));
		BufferString(STR_WITH_SIZE(ACCEPT_RANGES_STR));
		break;

	case DATA_TYPE_MP4:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_MP4_STR));
		BufferString(STR_WITH_SIZE(ACCEPT_RANGES_STR));
		break;

	case DATA_TYPE_HTML:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_HTML));
		break;

	case DATA_TYPE_JPG:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_JPG));
		break;

	case DATA_TYPE_JS:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_JS));
		break;

	case DATA_TYPE_CSS:
		BufferString(STR_WITH_SIZE(CONTENT_TYPE_CSS));
		break;

	default:
		AVF_LOGE("bad data_type: %d", data_type);
		break;
	}

	int slen;

	if (request == NULL || request->range_type == RANGE_NONE) {
		char buffer[64];
		slen = ::sprintf(buffer, "Content-Length: " LLD RN, file_size);
		BufferString((const avf_u8_t*)buffer, slen);
	} else {
		char buffer[100];
		slen = ::sprintf(buffer, "Content-Range: bytes " LLD "-" LLD "/" LLD RN,
			offset, offset + size - 1, file_size);
		BufferString((const avf_u8_t*)buffer, slen);
	}

	BufferString(STR_WITH_SIZE(RN));

	m_buffer[m_buffer_len] = 0;
	//printf("%s\n", m_buffer);

	avf_status_t status = TCPSend(SEND_TIMEOUT, m_buffer, m_buffer_len);
	if (status != E_OK) {
		AVF_LOGD("TCPSend failed");
	}
	return status;
}

avf_status_t CVdbHttpConnection::ServiceHLS(request_t *request)
{
	CVDB *pvdb = GetVDB();
	if (pvdb == NULL)
		return E_FATAL;

	if (request->range_type != RANGE_NONE) {
		if (request->range_type != RANGE_FROM || request->addr_from != 0) {
			ReleaseVDB(pvdb);
			return E_ERROR;
		}
	}

	CMemBuffer *pmb = CMemBuffer::Create(1024);
	if (pmb == NULL) {
		ReleaseVDB(pvdb);
		return E_NOMEM;
	}

	char server_ip[20];
	vdb_get_server_ip(mpSocket, server_ip);

	avf_status_t status = pvdb->WritePlaylist(server_ip, &request->range, pmb, 
		request->mute_audio, request->full_path, request->fix_pts,
		request->param1, request->param2);
	if (status != E_OK) {
		AVF_LOGE("WritePlayList failed");
	} else {
		avf_u32_t size = pmb->GetTotalSize();
		status = SendResponseHeader(request, 0, size, size, DATA_TYPE_M3U8);
		if (status == E_OK) {
			status = SendMemBuffer(pmb);
		}
	}

//	printf("%s\n", pmb->GetFirstBlock()->GetMem());

	avf_safe_release(pmb);
	ReleaseVDB(pvdb);
	return status;
}

avf_status_t CVdbHttpConnection::SendMemBuffer(CMemBuffer *pmb)
{
	CMemBuffer::block_t *block = pmb->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		avf_status_t status = TCPSend(SEND_TIMEOUT, block->GetMem(), block->GetSize());
		if (status != E_OK) {
			AVF_LOGD("SendMemBuffer failed");
			return status;
		}
	}
	return E_OK;
}

avf_status_t CVdbHttpConnection::SendMemBuffer(CMemBuffer *pmb, int data_type)
{
	avf_u32_t size = pmb->GetTotalSize();
	avf_status_t status = SendResponseHeader(NULL, 0, size, size, data_type);
	if (status == E_OK) {
		status = SendMemBuffer(pmb);
	}
	return status;
}

avf_status_t CVdbHttpConnection::ServiceTS(request_t *request)
{
	IClipReader *reader = NULL;
	CVDB *pvdb = NULL;
	avf_u8_t *data = NULL;

	avf_status_t status;
	CVDB::clip_info_s clip_info;
	IVdbInfo::cliplist_info_s cliplist_info;
	avf_u64_t bytes_sent = 0;

	if ((pvdb = GetVDB()) == NULL)
		return E_FATAL;

	status = pvdb->GetRangeInfo(&request->range, &clip_info, &cliplist_info, 0);
	if (status != E_OK) {
		ReleaseVDB(pvdb);
		return status;
	}

	avf_u64_t total_size = clip_info.v_size[0];
	AVF_LOGD("service ts: " LLD " bytes", total_size);

	switch (request->range_type) {
	default:
	case RANGE_NONE:
		request->addr_from = 0;
		request->addr_to = total_size;
		break;
	case RANGE_FROM_TO:
		break;
	case RANGE_FROM:
		request->addr_to = total_size;
		break;
	case RANGE_TO:
		request->addr_from = 0;
		break;
	}

	// align to packet
	avf_u32_t ts_start_off = request->addr_from % TS_PACKET_SIZE;
	avf_u32_t ts_remain = request->addr_to % TS_PACKET_SIZE;
	if (ts_remain) {
		ts_remain = TS_PACKET_SIZE - ts_remain;
	}

	if (ts_start_off + ts_remain != 0) {
		AVF_LOGD("align to packet: %d+%d", ts_start_off, ts_remain);
	}

	avf_u64_t real_size = request->addr_to - request->addr_from;
	avf_u64_t ts_size = real_size + (ts_start_off + ts_remain);

	reader = pvdb->CreateClipReaderOfAddress(&clip_info,
		&cliplist_info, request->addr_from - ts_start_off,
		request->mute_audio, request->fix_pts, request->param1, 0);

	if (reader == NULL) {
		NotFound();
		status = E_ERROR;
		goto Done;
	}

	status = SendResponseHeader(request, request->addr_from, real_size, total_size, DATA_TYPE_TS);
	if (status != E_OK)
		goto Done;

	// content
	if (request->code == HTTP_HEAD)
		goto Done;

	if ((data = avf_malloc_bytes(FILE_BUFFER_SIZE)) == NULL) {
		status = E_NOMEM;
		goto Done;
	}

	while (ts_size > 0) {
		avf_u32_t toread = FILE_BUFFER_SIZE;
		if (toread > ts_size)
			toread = ts_size;

		int bytes = reader->ReadClip(data, toread);
		if (bytes <= 0) {
			status = bytes == 0 ? E_ERROR : (avf_status_t)bytes;
			break;
		}

		toread = bytes;
		avf_u32_t towrite = toread - ts_start_off;
		if (towrite > real_size)
			towrite = real_size;

		status = TCPSend(SEND_TIMEOUT, data + ts_start_off, towrite);
		if (status != E_OK) {
			AVF_LOGD("ServiceTS failed: %d", status);
			break;
		}

		bytes_sent += toread;

		ts_start_off = 0;
		real_size -= towrite;
		ts_size -= toread;
	}

	AVF_LOGD("sent " LLD " bytes, total " LLD ", remain: " LLD,
		bytes_sent, total_size, total_size - bytes_sent);

Done:
	avf_safe_release(reader);
	ReleaseVDB(pvdb);
	avf_safe_free(data);
	return status;
}

// http://90.0.0.30:8085/clip/1/127f/0/348000-90000-0-0-0.mp4
avf_status_t CVdbHttpConnection::ServiceMP4(request_t *request)
{
	AVF_LOGI("ServiceMP4");

	CVDB *pvdb = NULL;
	CISOReader *reader = NULL;
	avf_u8_t *data = NULL;
	IAVIO *io;

	avf_status_t status;
	CVDB::clip_info_s clip_info;
	IVdbInfo::cliplist_info_s cliplist_info;

	avf_u64_t total_size;
	avf_u64_t real_size;
	avf_u64_t remain;

	if ((pvdb = GetVDB()) == NULL)
		return E_FATAL;

	status = pvdb->GetRangeInfo(&request->range, &clip_info, &cliplist_info, 0);
	if (status != E_OK) {
		ReleaseVDB(pvdb);
		return status;
	}

	reader = pvdb->CreateISOReader(&clip_info, &cliplist_info);
	if (reader == NULL) {
		NotFound();
		status = E_ERROR;
		goto Done;
	}

	io = reader->CreateIO();
	if (io == NULL) {
		NotFound();
		status = E_ERROR;
		goto Done;
	}

	total_size = io->GetSize();

	switch (request->range_type) {
	default:
	case RANGE_NONE:
		request->addr_from = 0;
		request->addr_to = total_size;
		break;
	case RANGE_FROM_TO:
		break;
	case RANGE_FROM:
		request->addr_to = total_size;
		break;
	case RANGE_TO:
		request->addr_from = 0;
		break;
	}

	status = io->Seek(request->addr_from);
	if (status != E_OK)
		goto Done;

	real_size = request->addr_to - request->addr_from;
	AVF_LOGD("<<< total_size: " LLD ", " LLD " - " LLD, total_size, request->addr_from, request->addr_to);

	status = SendResponseHeader(request, request->addr_from, real_size, total_size, DATA_TYPE_MP4);
	if (status != E_OK)
		goto Done;

	// content
	if (request->code == HTTP_HEAD)
		goto Done;

	if ((data = avf_malloc_bytes(FILE_BUFFER_SIZE)) == NULL) {
		status = E_NOMEM;
		goto Done;
	}

	remain = real_size;
	while (remain > 0) {
		avf_u32_t toread = FILE_BUFFER_SIZE;
		if (toread > remain)
			toread = remain;

		int bytes = io->Read(data, toread);
		if (bytes != (int)toread) {
			AVF_LOGE("Read %d bytes failed", toread);
			status = E_IO;
			break;
		}

		status = TCPSend(SEND_TIMEOUT, data, toread);
		if (status != E_OK) {
			AVF_LOGD("ServiceMP4 failed %d", status);
			break;
		}

		remain -= toread;
	}

	AVF_LOGD("total: " LLD ", remain: " LLD " >>>", real_size, remain);
	status = E_OK;

Done:
	avf_safe_release(reader);
	ReleaseVDB(pvdb);
	avf_safe_free(data);
	return status;
}

avf_status_t CVdbHttpConnection::ServiceClip(request_t *request)
{
	if (request->range._stream >= 0 || request->upload_opt == 0) {
		AVF_LOGE("bad stream %d", request->range._stream);
		return E_ERROR;
	}

	if (request->range_type != RANGE_NONE) {
		AVF_LOGE("cannot send by range");
		return E_ERROR;
	}

	IClipReader *reader = NULL;
	CVDB *pvdb = NULL;
	avf_u8_t *data = NULL;
	avf_u64_t bytes_sent = 0;
	avf_status_t status;
	avf_u64_t total;

	if ((pvdb = GetVDB()) == NULL)
		return E_FATAL;

	reader = pvdb->CreateClipReaderOfTime(&request->range, false, false, 0, request->upload_opt);
	if (reader == NULL) {
		NotFound();
		status = E_ERROR;
		goto Done;
	}

	total = reader->GetSize();
	status = SendResponseHeader(request, request->addr_from, total, total, DATA_TYPE_CLIP);
	if (status != E_OK)
		goto Done;

	// content
	if (request->code == HTTP_HEAD)
		goto Done;

	if ((data = avf_malloc_bytes(FILE_BUFFER_SIZE)) == NULL) {
		status = E_NOMEM;
		goto Done;
	}

	while (true) {
		int bytes = reader->ReadClip(data, FILE_BUFFER_SIZE);

		if (bytes <= 0) {
			status = (avf_status_t)bytes;
			break;
		}

		if ((status = TCPSend(SEND_TIMEOUT, data, bytes)) != E_OK) {
			AVF_LOGD("ServiceTS failed: %d", status);
			break;
		}

		bytes_sent += bytes;
	}

	AVF_LOGD("total: " LLD ", sent: " LLD ", remain: " LLD,
		total, bytes_sent, total - bytes_sent);

Done:
	avf_safe_release(reader);
	ReleaseVDB(pvdb);
	avf_safe_free(data);
	return status;
}

void CVdbHttpConnection::ThreadLoop()
{
	while (1) {
		avf_status_t status = ReadRequest();
		if (status != E_OK)
			return;

		// test
		//printf("\n**tick: " LLD "\n", avf_get_curr_tick());
		//printf("%s", m_buffer);

		request_t request;
		::memset(&request, 0, sizeof(request));

		const char *url = ParseRequest(&request);
		if (url) {
			const char *p = url;
			while (true) {
				char c = *p;
				if (c == ' ' || c == '\r' || c == '\n' || c == 0)
					break;
				p++;
			}

			ap<string_t> name(url, p - url);

			CVDB *pvdb = GetVDB();
			if (pvdb) {
				ServiceFile(pvdb, &request, name);
				ReleaseVDB(pvdb);
			}

		} else {

			switch (request.error) {
			case ERROR_OK:
				break;

			case ERROR_BadRequest:
				AVF_LOGD("Bad request");
				BadRequest();
				return;

			default:
			case ERROR_NotFound:
				AVF_LOGD("Not found");
				NotFound();
				return;
			}

			m_buffer_len = 0;

			switch (request.url_type) {
			case URL_TYPE_HLS:
				status = ServiceHLS(&request);
				break;
			case URL_TYPE_TS:
				status = ServiceTS(&request);
				break;
			case URL_TYPE_CLIP:
				status = ServiceClip(&request);
				break;
			case URL_TYPE_MP4:
				status = ServiceMP4(&request);
				break;
			default:
				request.error = ERROR_NotFound;
				status = E_ERROR;
				break;
			}

			if (status != E_OK) {
				AVF_LOGD("ServiceRequest failed: %d", request.error);
				return;
			}
		}

		if (!request.keep_alive)
			return;
	}
}

void CVdbHttpConnection::StartScript(CMemBuffer *pmb)
{
	pmb->write_string(STR_WITH_SIZE("<script type=\"text/javascript\">" RN));
}

void CVdbHttpConnection::EndScript(CMemBuffer *pmb)
{
	pmb->write_string(STR_WITH_SIZE("</script>" RN));
}

void CVdbHttpConnection::WriteHeader(CMemBuffer *pmb)
{
	pmb->write_string(
		"<!DOCTYPE HTML>" RN
		"<html lang=\"en\">" RN
		"<head>" RN);
	if (mb_has_vhs) {
		pmb->write_string(
		"<link rel=\"stylesheet\" href=\"vhs/vhs.css\" type=\"text/css\" />" RN);
	}
	pmb->write_string(
		"</head>" RN
		"<body>" RN);
}

void CVdbHttpConnection::WriteNavBar(CMemBuffer *pmb)
{
	if (mb_has_vhs) {
		AppendFile(pmb, "nav_bar.html");
	}
}

void CVdbHttpConnection::WriteFooter(CMemBuffer *pmb)
{
	pmb->write_string("</body>" RN "</html>");
}

void CVdbHttpConnection::ConvertToAckMem(int rval, CMemBuffer *pmb, vdb_ack_mem_t& mem)
{
	if (rval >= 0) {
		mem.data = (uint8_t*)pmb->CollectData();
		mem.size = pmb->GetTotalSize();
	}
	pmb->Release();
}

void CVdbHttpConnection::FreeAckMem(vdb_ack_mem_t& mem)
{
	if (mem.data) {
		CMemBuffer::ReleaseData(mem.data);
		mem.data = NULL;
	}
}

void CVdbHttpConnection::GenerateSpaceInfo(CVDB *pvdb, CMemBuffer *pmb)
{
	avf_u64_t total_space;
	avf_u64_t used_space;
	avf_u64_t marked_space;
	avf_u64_t clip_space;
	int slen;

	if (pvdb->GetSpaceInfo(&total_space, &used_space, &marked_space, &clip_space) == E_OK && total_space) {
		char buffer[128];

		StartScript(pmb);

		slen = sprintf(buffer, "var vdb_total_space=" LLD ";" RN, total_space);
		pmb->write_string(buffer, slen);

		slen = sprintf(buffer, "var vdb_used_space=" LLD ";" RN, used_space);
		pmb->write_string(buffer, slen);

		slen = sprintf(buffer, "var vdb_marked_space=" LLD ";" RN, marked_space);
		pmb->write_string(buffer, slen);

		slen = sprintf(buffer, "var vdb_clip_space=" LLD ";" RN, clip_space);
		pmb->write_string(buffer, slen);

		AppendFile(pmb, "gen_space.js");

		EndScript(pmb);
	}
}

void CVdbHttpConnection::FetchClipSet(CVDB *vdb, int clip_type, vdb_ack_mem_t& mem)
{
	mem.data = NULL;
	mem.size = 0;

	CMemBuffer *pmb = CMemBuffer::Create(1*KB);
	if (pmb == NULL)
		return;

	bool bNoDelete;
	int rval = vdb->CmdGetClipSetInfo(clip_type,
		GET_CLIP_EXTRA|GET_CLIP_VDB_ID|/*GET_CLIP_DESC|*/GET_CLIP_ATTR|GET_CLIP_SIZE, true,
		pmb, &bNoDelete);

	ConvertToAckMem(rval, pmb, mem);
}

const uint8_t *CVdbHttpConnection::GenerateStreamAttr(CMemBuffer *pmb, char *buffer, const uint8_t *ptr)
{
	const uint8_t *p = ptr;
	int slen;

	int version = load_le_32(p); p += 4;
	slen = sprintf(buffer, "version:%d", version);
	pmb->write_string(buffer, slen);

	int video_coding = *p++;
	slen = sprintf(buffer, ",video_coding:%u", video_coding);
	pmb->write_string(buffer, slen);

	int video_framerate = *p++;
	slen = sprintf(buffer, ",video_framerate:%u", video_framerate);
	pmb->write_string(buffer, slen);

	int video_width = load_le_16(p); p += 2;
	slen = sprintf(buffer, ",video_width:%u", video_width);
	pmb->write_string(buffer, slen);

	int video_height = load_le_16(p); p += 2;
	slen = sprintf(buffer, ",video_height:%u", video_height);
	pmb->write_string(buffer, slen);

	int audio_coding = *p++;
	slen = sprintf(buffer, ",audio_coding:%u", audio_coding);
	pmb->write_string(buffer, slen);

	int audio_num_channels = *p++;
	slen = sprintf(buffer, ",audio_channels:%u", audio_num_channels);
	pmb->write_string(buffer, slen);

	int audio_sampling_freq = load_le_32(p); p += 4;
	slen = sprintf(buffer, ",audio_sampling_freq:%u}", audio_sampling_freq);
	pmb->write_string(buffer, slen);

	return p;
}

const uint8_t *CVdbHttpConnection::GenerateOneClip(CMemBuffer *pmb, int index, char *buffer, const uint8_t *ptr)
{
	int slen;

	slen = sprintf(buffer, "vdb_clip[%d]={", index);
	pmb->write_string(buffer, slen);

	const uint8_t *p = ptr;

	uint32_t clip_id = load_le_32(p); p += 4;
	slen = sprintf(buffer, "clip_id:\"%x\"", clip_id);
	pmb->write_string(buffer, slen);

	uint32_t clip_date = load_le_32(p); p += 4;
	slen = sprintf(buffer, ",date:%u", clip_date);
	pmb->write_string(buffer, slen);

	uint32_t clip_duration_ms = load_le_32(p); p += 4;
	slen = sprintf(buffer, ",duration_ms:%u", clip_duration_ms);
	pmb->write_string(buffer, slen);

	uint64_t start_time_ms = load_le_32(p) | ((uint64_t)load_le_32(p + 4) << 32); p += 8;
	slen = sprintf(buffer, ",start_time_ms:" LLD, start_time_ms);
	pmb->write_string(buffer, slen);

	int num_streams = load_le_16(p); p += 2;
	int flags = load_le_16(p); p += 2;

	if (num_streams >= 1) {
		pmb->write_string(STR_WITH_SIZE(",stream0:{"));
		p = GenerateStreamAttr(pmb, buffer, p);
		if (num_streams >= 2) {
			pmb->write_string(STR_WITH_SIZE(",stream1:{"));
			p = GenerateStreamAttr(pmb, buffer, p);
		} else {
			pmb->write_string(STR_WITH_SIZE(",stream1:null"));
		}
	} else {
		pmb->write_string(STR_WITH_SIZE(",stream0:null"));
		pmb->write_string(STR_WITH_SIZE(",stream1:null"));
	}

	int clip_type = load_le_32(p); p += 4;
	slen = sprintf(buffer, ",clip_type:%d", clip_type);
	pmb->write_string(buffer, slen);

	uint32_t extra = load_le_32(p); p += 4;
	const uint8_t *pnext = p + extra;

	if (flags & GET_CLIP_EXTRA) {
		pmb->write_string(STR_WITH_SIZE(",uuid:\""));
		pmb->write_string((const char*)p, 36); p += 36;
		pmb->write_string(STR_WITH_SIZE("\""));

		p += 4;	// ref_clip_date

		int gmtoff = load_le_32(p); p += 4;
		slen = sprintf(buffer, ",gmtoff:%d", gmtoff);
		pmb->write_string(buffer, slen);

		uint32_t real_clip_id = load_le_32(p); p += 4;
		slen = sprintf(buffer, ",real_clip_id:\"%x\"", real_clip_id);
		pmb->write_string(buffer, slen);
	} else {
		pmb->write_string(STR_WITH_SIZE(",uuid:\"\""));
		pmb->write_string(STR_WITH_SIZE(",gmtoff:0"));
		pmb->write_string(STR_WITH_SIZE(",real_clip_id:\"0\""));
	}

	if (flags & GET_CLIP_DESC) {
		for (;;) {
			uint32_t fcc = load_le_32(p); p += 4;
			if (fcc == 0)
				break;
			uint32_t data_size = load_le_32(p); p += 4;
			p += data_size;
			if (data_size & 3) {
				p += 4 - (data_size & 3);
			}
		}
	}

	if (flags & GET_CLIP_ATTR) {
		uint32_t clip_attr = load_le_32(p); p += 4;
		slen = sprintf(buffer, ",clip_attr:%u", clip_attr);
		pmb->write_string(buffer, slen);
	} else {
		pmb->write_string(STR_WITH_SIZE(",clip_attr:0"));
	}

	if (flags & GET_CLIP_SIZE) {
		uint64_t clip_size = load_le_32(p) | ((uint64_t)load_le_32(p + 4) << 32); p += 8;
		slen = sprintf(buffer, ",clip_size:" LLD, clip_size);
		pmb->write_string(buffer, slen);
	} else {
		pmb->write_string(STR_WITH_SIZE(",clip_size:0"));
	}

	pmb->write_string(STR_WITH_SIZE("};" RN));

	return pnext;
}

void CVdbHttpConnection::GenerateAllClips(CVDB *vdb, CMemBuffer *pmb)
{
	vdb_ack_mem_t buffered_cs;
	vdb_ack_mem_t marked_cs;
	int slen;

	FetchClipSet(vdb, CLIP_TYPE_BUFFER, buffered_cs);
	FetchClipSet(vdb, CLIP_TYPE_MARKED, marked_cs);

	const uint8_t *p_buffered = buffered_cs.data;
	const uint8_t *p_marked = marked_cs.data;

	// vdb_ack_GetClipSetInfoEx_t
	uint32_t num_buffered = p_buffered ? load_le_32(p_buffered + 4) : 0;
	uint32_t num_marked = p_marked ? load_le_32(p_marked + 4) : 0;

	StartScript(pmb);

	char buffer[128];
	slen = sprintf(buffer, "var vdb_clip=new Array(%d);" RN, num_buffered + num_marked);
	pmb->write_string(buffer, slen);

	p_buffered += 16;
	for (uint32_t i = 0; i < num_buffered; i++) {
		p_buffered = GenerateOneClip(pmb, i, buffer, p_buffered);
	}

	p_marked += 16;
	for (uint32_t i = 0; i < num_marked; i++) {
		p_marked = GenerateOneClip(pmb, i + num_buffered, buffer, p_marked);
	}

	AppendFile(pmb, "gen_clip.js");

	EndScript(pmb);

	FreeAckMem(buffered_cs);
	FreeAckMem(marked_cs);
}

// todo - handle error
void CVdbHttpConnection::GenerateClipPoster(CVDB *vdb, CMemBuffer *pmb, const char *ptr)
{
	const char *p = ptr;
	char *pend;

	int clip_type = ::strtol(p, &pend, 10);
	if (p == pend) {
		return;
	}
	p = pend;
	if (*p == '-') p++;

	uint32_t clip_id = ::strtoul(p, &pend, 16);
	if (p == pend) {
		return;
	}
	p = pend;
	if (*p == '-') p++;

	uint64_t clip_time_ms = ::strtoull(p, &pend, 10);
	if (p == pend) {
		return;
	}
	p = pend;
	if (::strncmp(p, ".jpg", 4)) {
		return;
	}

	vdb_cmd_GetIndexPicture_t cmd;
	::memset(&cmd, 0, sizeof(cmd));

	cmd.clip_type = clip_type;
	cmd.clip_id = clip_id;
	cmd.user_data = 0;
	cmd.clip_time_ms_lo = U64_LO(clip_time_ms);
	cmd.clip_time_ms_hi = U64_HI(clip_time_ms);

	avf_status_t status = vdb->CmdGetIndexPicture(&cmd, pmb, true, false);
	if (status != E_OK) {
		return;
	}
}

// todo - handle error
void CVdbHttpConnection::GenerateClipPlaybackPage(CVDB *vdb, CMemBuffer *pmb, const char *ptr)
{
	const char *p = ptr;
	char *pend;

	int clip_type = ::strtol(p, &pend, 10);
	if (p == pend) {
		return;
	}
	p = pend;
	if (*p == '-') p++;

	uint32_t clip_id = ::strtoul(p, &pend, 16);
	if (p == pend) {
		return;
	}
	p = pend;
	if (*p == '-') p++;

	int stream = ::strtoul(p, &pend, 10);
	if (p == pend) {
		return;
	}
	p = pend;
	if (*p == '-') p++;

	uint64_t clip_time_ms = ::strtoull(p, &pend, 10);
	if (p == pend) {
		return;
	}
	p = pend;
	if (*p == '-') p++;

	uint32_t clip_duration_ms = ::strtoul(p, &pend, 10);
	if (p == pend) {
		return;
	}

	char server_ip[20];
	vdb_get_server_ip(mpSocket, server_ip);

	char url[200];
	vdb_format_url(server_ip, url, mp_vdb_id, clip_type, clip_id, stream, 0, 0,
		URL_TYPE_HLS, clip_time_ms, clip_duration_ms, 0, 0);

	pmb->write_string(STR_WITH_SIZE("<video src=\""));
	pmb->write_string(url);
	pmb->write_string(STR_WITH_SIZE("\" controls=\"controls\" autoplay=\"autoplay\">"));
	pmb->write_string("Your browser does not support the video tag");
	pmb->write_string(STR_WITH_SIZE("</video>"));
}

// http://192.168.110.1:8085/<address>
// address
//		vdb.dat
//		index.html
//
//		clips.html
//		playlists.html
//
//		clip-poster/<clip_type>-<clip_id>-<time_ms>.jpg
//		clip-playback/<clip_type>-<clip_id>-<stream>-<start_time_ms>.html
//		clip-page/<gen_space.js>
//		vhs/<filename>
//
//		files/playlist/%d/<filename>
//
// code: GET/HEAD/PUT/DELETE
//

void CVdbHttpConnection::ServiceFile(CVDB *pvdb, request_t *request, ap<string_t>& name)
{
	const char *ptr = name->string();

	AVF_LOGD("ServiceFile " C_CYAN "%s" C_NONE, name->string());

	if (::strcmp(ptr, "vdb.dat") == 0) {
		CMemBuffer *pmb = CMemBuffer::Create(1024);

		pvdb->Serialize(pmb);

		SendMemBuffer(pmb, DATA_TYPE_APP);
		pmb->Release();

		return;
	}

	if (name->size() == 0 || ::strcmp(ptr, "index.html") == 0 ||
			::strcmp(ptr, "index.htm") == 0) {

		CMemBuffer *pmb = CMemBuffer::Create(512);

		WriteHeader(pmb);
		WriteNavBar(pmb);

		if (mb_has_vhs) {
			StartScript(pmb);

			char buffer[128];
			int slen = sprintf(buffer, "var preview_addr=\"http://%s:8081\";" RN, GetIP());
			pmb->write_string(buffer, slen);
			AppendFile(pmb, "preview.js");

			EndScript(pmb);
		} else {
			pmb->write_string("<img src=\"http://");
			pmb->write_string(GetIP());
			pmb->write_string(":8081\" alt=\"preview\"/>" RN);
		}

		WriteFooter(pmb);

		SendMemBuffer(pmb, DATA_TYPE_HTML);
		pmb->Release();

		return;
	}

	if (::strcmp(ptr, "clips.html") == 0) {

		CMemBuffer *pmb = CMemBuffer::Create(512);

		WriteHeader(pmb);
		WriteNavBar(pmb);

		GenerateSpaceInfo(pvdb, pmb);
		GenerateAllClips(pvdb, pmb);

		WriteFooter(pmb);

		SendMemBuffer(pmb, DATA_TYPE_HTML);
		pmb->Release();

		return;
	}

	if (::strncmp(ptr, "clip-poster/", 12) == 0) {
		CMemBuffer *pmb = CMemBuffer::Create(16);

		GenerateClipPoster(pvdb, pmb, ptr + 12);

		SendMemBuffer(pmb, DATA_TYPE_JPG);
		pmb->Release();

		return;
	}

	if (::strncmp(ptr, "clip-playback/", 14) == 0) {
		CMemBuffer *pmb = CMemBuffer::Create(1024);

		GenerateClipPlaybackPage(pvdb, pmb, ptr + 14);

		SendMemBuffer(pmb, DATA_TYPE_HTML);
		pmb->Release();

		return;
	}

	if (::strncmp(ptr, "vhs/", 4) == 0) {

		CMemBuffer *pmb = CMemBuffer::Create(16);

		if (::strstr(ptr + 4, "..") == NULL) {
			AppendFile(pmb, ptr + 4);
		}

		SendMemBuffer(pmb, DATA_TYPE_CSS);	// TODO
		pmb->Release();

		return;
	}

	if (::strcmp(ptr, "playlists.html") == 0) {

		CMemBuffer *pmb = CMemBuffer::Create(512);

		WriteHeader(pmb);
		WriteNavBar(pmb);

		pmb->write_string("All Playlists");

		WriteFooter(pmb);

		SendMemBuffer(pmb, DATA_TYPE_HTML);
		pmb->Release();

		return;
	}

	if (::strncasecmp(ptr, "files/", 6) == 0) {
		ptr += 6;

		if (::strncasecmp(ptr, "playlist/", 9) == 0) {
			ServicePlaylistFile(pvdb, request, ptr + 9);
		} else {
			NotFound();
		}

		return;
	}

	NotFound();

	return;
}

void CVdbHttpConnection::AppendFile(CMemBuffer *pmb, const char *filename)
{
	ap<string_t> fullname(VHS_HOME, filename);

	int fd = avf_open_file(fullname->string(), O_RDONLY, 0);
	if (fd < 0) {
		//AVF_LOGD("cannot open %s", fullname->string());
		return;
	}

	int size = (int)::lseek(fd, 0, SEEK_END);
	::lseek(fd, 0, SEEK_SET);

	char *buffer = (char*)pmb->Alloc(size);
	if (buffer == NULL) {
		avf_close_file(fd);
		return;
	}

	int rval = ::read(fd, buffer, size);
	if (rval != size) {
		AVF_LOGD("read %s failed, size: %d, return: %d", fullname->string(), size, rval);
	}

	avf_close_file(fd);
}

// GET/HEAD/PUT/DELETE
void CVdbHttpConnection::ServicePlaylistFile(CVDB *pvdb, request_t *request, const char *name)
{
	ap<string_t> filename(pvdb->GetVdbRoot(), PLAYLIST_PATH, name);

	switch (request->code) {
	case HTTP_GET:
	case HTTP_HEAD:
		GetFile(request, filename);
		break;

	case HTTP_PUT:
		PutFile(request, filename);
		break;

	case HTTP_DELETE: {
			// 300/*
			avf_u32_t id;
			if (::sscanf(name, "%d/*", &id) == 1) {
				DeleteAllFiles(pvdb, request, id);
			} else {
				DeleteFile(request, filename);
			}
		}
		break;

	default:
		BadRequest();
		break;
	}
}

void CVdbHttpConnection::GetFile(request_t *request, ap<string_t>& filename)
{
	IAVIO *io = CSysIO::Create();
	avf_u8_t *data = NULL;
	playlist_file_info_t info;
	avf_u64_t bytes_sent;
	avf_u64_t bytes_remain;

	AVF_LOGW("GetFile: " C_GREEN "%s" C_NONE, filename->string());

	if (io->OpenRead(filename->string()) != E_OK)
		goto Error;

	if (io->Seek(io->GetSize() - sizeof(info), IAVIO::kSeekSet) != E_OK)
		goto Error;

	if (io->Read(&info, sizeof(info)) != (int)sizeof(info))
		goto Error;

	io->Seek(0, IAVIO::kSeekSet);

	if (SendResponseHeader(NULL, 0, info.filesize, info.filesize, DATA_TYPE_APP) != E_OK)
		goto Error2;

	// send file
	if ((data = avf_malloc_bytes(FILE_BUFFER_SIZE)) == NULL)
		goto Error2;

	bytes_remain = info.filesize;
	while (bytes_remain > 0) {
		avf_u32_t tosend = MIN(FILE_BUFFER_SIZE, bytes_remain);
		if (io->Read(data, tosend) != (int)tosend) {
			AVF_LOGE("read failed");
			goto Error2;
		}

		avf_status_t status = TCPSend(SEND_TIMEOUT, data, tosend);
		if (status != E_OK)
			goto Error2;

		bytes_sent += tosend;
		bytes_remain -= tosend;
	}

	AVF_LOGD("sent " C_GREEN "%s" C_NONE ", total " LLD " bytes", filename->string(), info.filesize);

	avf_safe_free(data);
	avf_safe_release(io);
	return;

Error:
	avf_safe_release(io);
	NotFound();
	return;

Error2:
	avf_safe_free(data);
	avf_safe_release(io);
	return;
}

void CVdbHttpConnection::PutFile(request_t *request, ap<string_t>& filename)
{
	AVF_LOGD("PutFile " C_GREEN "%s" C_NONE, filename->string());

	if (!avf_mkdir_for_file(filename->string())) {
		AVF_LOGW("failed to mkdir %s", filename->string());
		return;
	}

	IAVIO *io = CBufferIO::Create();
	avf_u8_t *buffer = NULL;
	avf_u64_t remain = request->content_length;
	playlist_file_info_t info;
	avf_size_t pad_size;
	IBlockIO *block_io;

	if (io->CreateFile(filename->string()) != E_OK) {
		AVF_LOGW("cannot create %s", filename->string());
		goto Done;
	}

	if ((buffer = avf_malloc_bytes(FILE_BUFFER_SIZE)) == NULL) {
		goto Done;
	}

	while (remain > 0) {
		avf_size_t tocopy = MIN(FILE_BUFFER_SIZE, remain);

		if (m_buffer_remain > 0) {
			AVF_LOGD("copy %d bytes from buffer", m_buffer_remain);
			tocopy = MIN(tocopy, m_buffer_remain);
			::memcpy(buffer, m_buffer + m_buffer_pos, tocopy);
			m_buffer_pos += tocopy;
			m_buffer_remain -= tocopy;
		} else {
			avf_status_t status = TCPReceive(100*1000, buffer, tocopy);
			if (status != E_OK) {
				BadRequest();
				break;
			}
		}

		if (io->Write(buffer, tocopy) != (int)tocopy) {
			AVF_LOGE("write failed");
			BadRequest();
			break;
		}

		remain -= tocopy;
	}

	if (remain == 0) {
		AVF_LOGD("put file done: " C_GREEN "%s" C_NONE, filename->string());
		OK();
	}

	if ((block_io = IBlockIO::GetInterfaceFrom(io)) != NULL) {
		IBlockIO::block_desc_s desc;

		if (block_io->_GetBlockDesc(desc) != E_OK) {
			AVF_LOGE("_GetBlockDesc failed");
			goto Done;
		}

		pad_size = 0;
		if (desc.align_size > 0) {
			if (desc.file_remain >= sizeof(info)) {
				pad_size = desc.file_remain - sizeof(info);
			} else {
				avf_size_t total = desc.file_remain + desc.align_size;
				while (total < sizeof(info))
					total += desc.align_size;
				pad_size = total - sizeof(info);
			}
		}

		::memset(&info, 0, sizeof(info));
		info.filesize = io->GetSize();

		if (pad_size > 0) {
			io->Write(NULL, pad_size);
		}

		io->Write(&info, sizeof(info));
	}

Done:
	avf_safe_free(buffer);
	avf_safe_release(io);
}

void CVdbHttpConnection::DeleteFile(request_t *request, ap<string_t>& filename)
{
	AVF_LOGD("DeleteFile " C_GREEN "%s" C_NONE, filename->string());
//	if (bDelAll) {
//		avf_size_t num_files;
//		avf_rm_dir_recursive(filename->string(), &num_files, false);
//	} else {
		avf_remove_file(filename->string(), true);
//	}
	OK();
}

void CVdbHttpConnection::DeleteAllFiles(CVDB *pvdb, request_t *request, avf_u32_t id)
{
	char buffer[16];
	::sprintf(buffer, "%d/", id);
	ap<string_t> path(pvdb->GetVdbRoot(), PLAYLIST_PATH, buffer);
	avf_size_t num_files;
	AVF_LOGD("DeleteAllFiles: " C_GREEN "%s" C_NONE, path->string());
	avf_rm_dir_recursive(path->string(), &num_files, false);
	OK();
}

const char *CVdbHttpConnection::ParseClipRequest(const char *url, clip_request_t *request)
{
	const char *p = (const char *)url;
	char *end;
	avf_u32_t v32;
	avf_u64_t v64;

	request->code = CLIP_NONE;

	// dirname
	v32 = ::strtoul(p, &end, 16);
	if (end == p) {
		AVF_LOGE("no dirname %s", p);
		return p;
	}
	request->dirname = v32;
	p = end;
	if (*p == '-') p++;

	// clip_id
	v32 = ::strtoul(p, &end, 16);
	if (end == p) {
		AVF_LOGE("no clip_id %s", p);
		return p;
	}
	request->clip_id = v32;
	p = end;
	if (*p == '-') p++;

	if (*p == '/') p++;

	if (*p == 0 || ::strcmp(p, "index.html") == 0) {
		request->code = CLIP_INDEX;
		return NULL;
	}

	if (::strcmp(p, "poster.html") == 0) {
		request->code = CLIP_POSTER;
		return NULL;
	}

	// clip_time_ms
	v64 = ::strtoull(p, &end, 10);
	if (end == p) {
		AVF_LOGE("expect clip_time_ms %s", p);
		return p;
	}
	request->clip_time_ms = v64;
	p = end;

	if (::strcmp(p, ".jpg") != 0) {
		AVF_LOGE("only support .jpg %s", p);
		return p;
	}

	request->code = CLIP_PICTURE;
	return NULL;
}

void CVdbHttpConnection::OK()
{
	AVF_LOGD("send HTTP OK");
	TCPSend(SEND_TIMEOUT, (const avf_u8_t*)STR_WITH_SIZE(HTTP_200_OK_STR));
}

void CVdbHttpConnection::BadRequest()
{
	AVF_LOGD("send HTTP Bad Request");
	TCPSend(SEND_TIMEOUT, (const avf_u8_t*)STR_WITH_SIZE(BAD_REQUEST_STR));
}

void CVdbHttpConnection::NotFound()
{
	AVF_LOGD("send HTTP Not Found");
	TCPSend(SEND_TIMEOUT, (const avf_u8_t*)STR_WITH_SIZE(NOT_FOUND_STR));
}

void CVdbHttpConnection::BufferString(const avf_u8_t *str, int len)
{
	if (len < 0) {
		len = ::strlen((const char*)str);
	}
	::memcpy(m_buffer + m_buffer_len, str, len);
	m_buffer_len += len;
	AVF_ASSERT(m_buffer_len < sizeof(m_buffer));
}

//-----------------------------------------------------------------------
//
//  CVdbHttpServer
//
//-----------------------------------------------------------------------
CVdbHttpServer* CVdbHttpServer::Create(const vdb_set_s *vdb_set, void *context)
{
	CVdbHttpServer *result = avf_new CVdbHttpServer(vdb_set, context);
	CHECK_STATUS(result);
	return result;
}

CVdbHttpServer::CVdbHttpServer(const vdb_set_s *vdb_set, void *context):
	inherited("VdbHttpServer")
{
	m_vdb_set = *vdb_set;
	mp_vdb_context = context;
}


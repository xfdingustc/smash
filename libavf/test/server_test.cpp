
#define LOG_TAG	"server_test"

#include <getopt.h>
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

#include "vdb_api.h"

#include "avf_tcp_server.h"
#include "test_common.cpp"

#ifdef LINUX_OS
#define VDB_PATH	"/tmp/mmcblk0p1/camera/"
#else
#define VDB_PATH	"camera/"
#endif

static VDB *g_vdb;

static void upload_file(IAVIO *io)
{
	if (VDB_StartRecord(g_vdb, 0, 0) != E_OK)
		return;

	avf_u8_t *buffer = avf_malloc_bytes(4*1024);
	if (buffer == NULL) {
		AVF_LOGE("no memory");
		goto End;
	}

	while (true) {
		int rval = io->Read(buffer, 4*1024);
		if (rval <= 0) {
			if (rval < 0) {
				AVF_LOGP("read");
			}
			break;
		}

		if (VDB_AddDataEx(g_vdb, buffer, rval, 0) < 0)
			break;
	}

	avf_free(buffer);

End:
	VDB_StopRecord(g_vdb);
}

// defined in vdb.h
struct clip_list_s {
	clip_list_s *next;
	string_t *display_name;
	avf_u32_t clip_dir;
	avf_u32_t clip_id;
	void ReleaseAll();
};

static void menu_all_clips_info(void)
{
	clip_list_s *info = VDB_GetAllClipsInfo(g_vdb);
	if (info == NULL) {
		AVF_LOGE("VDB_GetAllClipsInfo failed");
		return;
	}

	for (clip_list_s *tmp = info; tmp; tmp = tmp->next) {
		AVF_LOGD("%s: dir=0x%x, id=%d", tmp->display_name->string(), tmp->clip_dir, tmp->clip_id);
	}

	info->ReleaseAll();
}

static void menu_upload_clip_file(void)
{
	char filename[256];

	if (!get_input("Input upload filename: ", with_size(filename)))
		return;

	IAVIO *io = CSysIO::Create();
	if (io == NULL)
		return;

	if (io->OpenRead(filename) != E_OK) {
		AVF_LOGP("cannot open %s", filename);
		io->Release();
		return;
	}

	upload_file(io);

	io->Release();
}

static void menu_read_raw_data(void)
{
	vdbs_clip_info_t info;
	vdb_clip_id_t clip_id = MAKE_VDB_CLIP_ID(0x173efc5e, 390003806);

	if (VDB_GetClipInfo(g_vdb, clip_id, &info) < 0) {
		AVF_LOGE("VDB_GetClipInfo failed");
		return;
	}

	AVF_LOGD("clip_data: %d, gmtoff: %d, duration: %d, start_time_ms: " LLD ", num_streams: %d",
		info.clip_date, info.gmtoff, info.clip_duration_ms, info.clip_start_time_ms, info.num_streams);

	uint64_t clip_end_ms = info.clip_start_time_ms + info.clip_duration_ms;

	uint64_t clip_time_ms = info.clip_start_time_ms;
	while (1) {
		if (clip_time_ms >= clip_end_ms)
			break;

		uint32_t length_ms = 3000;	// 3 seconds
		if (clip_time_ms + length_ms > clip_end_ms) {
			length_ms = clip_end_ms - clip_time_ms;
		}

		vdb_raw_data_t rdata;
		if (VDB_ReadRawData(g_vdb, clip_id, RAW_DATA_ACC, clip_time_ms, length_ms, &rdata) < 0) {
			AVF_LOGE("VDB_ReadRawData failed");
			break;
		}

		AVF_LOGE("=== " LLD ", %d: size: %d ===", clip_time_ms, length_ms, rdata.data_size);
		avf_dump_mem(rdata.data, rdata.data_size);

		clip_time_ms += length_ms;
	}
}

static const menu_t g_main_menu[] = {
	{"All Clips Info", menu_all_clips_info},
	{"Upload Clip File...", menu_upload_clip_file},
	{"Test Read Raw Data...", menu_read_raw_data},
};

static void vdb_callback(void *ctx, int msg, void *msg_data)
{
	switch (msg) {
	case VDB_CB_ClipCreated:
		AVF_LOGD("VDB_CB_ClipCreated");
		break;

	case VDB_CB_ClipFinished:
		AVF_LOGD("VDB_CB_ClipFinished");
		break;

	case VDB_CB_ClipRemoved:
		AVF_LOGD("VDB_CB_ClipRemoved");
		break;

	default:
		AVF_LOGW("unknown callback msg %d", msg);
		break;
	}
}

int main(int argc, char **argv)
{
	// create vdb
	avf_safe_mkdir(VDB_PATH);
	if ((g_vdb = VDB_Open(VDB_PATH, 1)) == NULL) {
		AVF_LOGE("cannot open vdb: %s", VDB_PATH);
		return -1;
	}
	VDB_SetCallback(g_vdb, vdb_callback, NULL, "");

	RUN_MENU("Main", g_main_menu);

	VDB_Close(g_vdb);

	// for debug
	avf_mem_info();
	avf_file_info();
	avf_socket_info();

	return 0;
}


/*
class CUploadConnection;
class CUploadServer;

static CUploadServer *pserver;

//-----------------------------------------------------------------------
//
//	tcp connection
//
//-----------------------------------------------------------------------
class CUploadConnection: public CTCPConnection
{
	typedef CTCPConnection inherited;

public:
	static CUploadConnection* Create(CUploadServer *pServer, CSocket *pSocket);

private:
	CUploadConnection(CUploadServer *pServer, CSocket *pSocket);
	virtual ~CUploadConnection();

protected:
	virtual void ThreadLoop();
};

//-----------------------------------------------------------------------
//
//	tcp server
//
//-----------------------------------------------------------------------
class CUploadServer: public CTCPServer
{
	typedef CTCPServer inherited;
	friend class CUploadConnection;

public:
	static CUploadServer* Create();
	avf_status_t Start();

private:
	CUploadServer(): inherited("UploadServer") {}
	virtual ~CUploadServer() {
		StopServer();
	}

protected:
	virtual CTCPConnection *CreateConnection(CSocket *pSocket, bool *pbRelease);
};

//-----------------------------------------------------------------------
//
//	tcp connection
//
//-----------------------------------------------------------------------

CUploadConnection* CUploadConnection::Create(CUploadServer *pServer, CSocket *pSocket)
{
	CUploadConnection *result = avf_new CUploadConnection(pServer, pSocket);
	if (result) {
		result->RunThread("upload-conn");
	}
	CHECK_STATUS(result);
	return result;
}

CUploadConnection::CUploadConnection(CUploadServer *pServer, CSocket *pSocket):
	inherited(pServer, pSocket)
{
}

CUploadConnection::~CUploadConnection()
{
}

void CUploadConnection::ThreadLoop()
{
	server_cmd_t cmd;
	while (true) {
		avf_status_t status = mpSocketEvent->TCPReceive(mpSocket, 30*1000, (avf_u8_t*)&cmd, sizeof(cmd));
		if (status != E_OK)
			break;

		switch (cmd.cmd_code) {
		case SERVER_CMD_StartRecord:
			printf("StartRecord\n");
			VDB_StartRecord(pvdb, cmd.u.StartRecord.has_video, cmd.u.StartRecord.has_picture);
			break;

		case SERVER_CMD_StopRecord:
			printf("StopRecord\n");
			VDB_StopRecord(pvdb);
			break;

		case SERVER_CMD_AddData: {
				printf("AddData\n");
				avf_uint_t size = cmd.u.AddData.data_size;
				void *data = malloc(size);
				status = mpSocketEvent->TCPReceive(mpSocket, 30*1000, (avf_u8_t*)data, size);
				if (status != E_OK) {
					free(data);
					return;
				}
				VDB_AddData(pvdb, (const avf_u8_t*)data, size);
				free(data);
			}
			break;

		default:
			break;
		}
	}
}

//-----------------------------------------------------------------------
//
//	tcp server
//
//-----------------------------------------------------------------------

CUploadServer* CUploadServer::Create()
{
	CUploadServer *result = avf_new CUploadServer();
	CHECK_STATUS(result);
	return result;
}

avf_status_t CUploadServer::Start()
{
	printf("run server at port %d\n", VDB_SERVER_PORT);
	return RunServer(VDB_SERVER_PORT);
}

CTCPConnection *CUploadServer::CreateConnection(CSocket *pSocket, bool *pbRelease)
{
	return CUploadConnection::Create(this, pSocket);
}

//-----------------------------------------------------------------------
//
//	server test
//
//-----------------------------------------------------------------------

#define RUN_MENU(_name, _menu) \
	run_menu(_name, _menu, sizeof(_menu) / sizeof(_menu[0]))

static void menu_no_action()
{
}

static const menu_t g_vdb_menu[] =
{
	{"No action", menu_no_action},
};

static void vdb_callback(void *ctx, int msg, void *msg_data)
{
}

int main(int argc, char **argv)
{
	// create VDB
	avf_safe_mkdir(VDB_PATH);
	if ((pvdb = VDB_Open(VDB_PATH, 1)) == NULL) {
		printf("cannot open vdb\n");
		return -1;
	}

	VDB_SetCallback(pvdb, vdb_callback, NULL, "");

	pserver = CUploadServer::Create();
	pserver->Start();

	RUN_MENU("VDB", g_vdb_menu);
	avf_safe_release(pserver);

	VDB_Close(pvdb);

	// for debug
	avf_mem_info();
	avf_file_info();
	avf_socket_info();

	return 0;
}
*/


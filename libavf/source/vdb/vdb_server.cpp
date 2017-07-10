
#define LOG_TAG "vdb_server"

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
#include "mem_stream.h"

#include "vdb_cmd.h"
#include "vdb_file_list.h"
#include "vdb_clip.h"
#include "vdb_util.h"
#include "vdb_if.h"
#include "vdb.h"
#include "vdb_server.h"

#ifdef LINUX_OS
#define Z_LIB
#include "zlib.h"
#else
#undef Z_LIB
#endif

#if defined(ANDROID_OS)

#define VDB_OS			OS_ANDROID
#define VDB_EXTRA_FLAG	0

#elif defined(IOS_OS)

#define VDB_OS			OS_IOS
#define VDB_EXTRA_FLAG	0

#elif defined(WIN32_OS)

#define VDB_OS			OS_WINDOWS
#define VDB_EXTRA_FLAG	VDB_HAS_ID

#elif defined(MAC_OS)

#define VDB_OS			OS_MAC
#define VDB_EXTRA_FLAG	VDB_HAS_ID

#else

#define VDB_OS			OS_LINUX
#define VDB_EXTRA_FLAG	0

#endif

//-----------------------------------------------------------------------
//
//  CVDBConnection
//
//-----------------------------------------------------------------------
CVDBConnection *CVDBConnection::Create(CVDBServer *pServer, CSocket **ppSocket,
		const vdb_set_s *vdb_set, void *context)
{
	CVDBConnection *result = avf_new CVDBConnection(pServer, ppSocket, vdb_set, context);
	if (result) {
		result->RunThread("vdb-conn");
	}
	CHECK_STATUS(result);
	return result;
}

CVDBConnection::CVDBConnection(CVDBServer *pServer, CSocket **ppSocket,
		const vdb_set_s *vdb_set, void *context):
	inherited(pServer, ppSocket),
	m_seqid(0),
	mp_vdb_set(vdb_set),
	mp_vdb_context(context),
	mRawDataFlags(0),
	m_hls_seg_length(0),	// default
	mpMemBuffer(NULL),
	mpData(NULL)
{
	CREATE_OBJ(mpMemBuffer = CMemBuffer::Create(4*KB));
}

CVDBConnection::~CVDBConnection()
{
	avf_safe_release(mpMemBuffer);
	avf_safe_release(mpData);
}

void CVDBConnection::ThreadLoop()
{
	avf_status_t status;

	mp_vdb_id = NULL;

	// send the first ack: version info
	vdb_cmd_header_t *cmd_header = (vdb_cmd_header_t*)m_cmd_buffer;
	cmd_header->cmd_code = VDB_CMD_GetVersionInfo;
	cmd_header->cmd_flags = 0;
	cmd_header->cmd_tag = 0;
	cmd_header->user1 = 0;
	cmd_header->user2 = 0;

	if ((status = ProcessCmd()) != E_OK)
		return;

//	AVF_LOGI("sent VDB_CMD_GetVersionInfo to socket %d", mpSocket->GetSocket());

	while (true) {
		AVF_LOGD("WaitCmd");

		status = TCPReceive(-1, with_size(m_cmd_buffer));
		if (status != E_OK)
			break;

		if ((status = ProcessCmd()) != E_OK)
			break;
	}
}

CVDB *CVDBConnection::GetVDB(const avf_u8_t *base, const avf_u8_t *ptr, int line)
{
#if VDB_EXTRA_FLAG
	// vdb_id_t
	avf_size_t id_size = load_le_16(ptr);
	ptr += 4;	// +reserved
	if (id_size == 0) {
		mp_vdb_id = NULL;
	} else {
		if (ptr + id_size > base + VDB_CMD_SIZE) {
			AVF_LOGE("bad vdb id_size: %d", id_size);
			mp_vdb_id = NULL;
		} else {
			mp_vdb_id = (char*)ptr;
			mp_vdb_id[id_size - 1] = 0;	// trailing 0
		}
	}
#endif
	mp_vdb = mp_vdb_set->get_vdb(mp_vdb_context, mp_vdb_id);
	if (mp_vdb == NULL) {
		AVF_LOGE("get_vdb(%s) failed, line %d", mp_vdb_id, line);
	}
	return mp_vdb;
}

avf_status_t CVDBConnection::WriteVdbId(CMemBuffer *pmb)
{
#if VDB_EXTRA_FLAG
	// vdb_id_t
	if (mp_vdb_id == NULL) {
		pmb->write_le_16(0);	// id_size
		pmb->write_le_16(0);	// reserved
	} else {
		pmb->write_string_aligned(mp_vdb_id);
	}
#endif
	return E_OK;
}

#define GET_VDB_ID(_cmd_type) \
	if (GetVDB(m_cmd_buffer, m_cmd_buffer + sizeof(_cmd_type), __LINE__) == NULL) { \
		status = E_ERROR; \
	} else {

#define WRITE_VDB_ID(_pmb) \
		if (status == E_OK) \
			status = WriteVdbId(_pmb); \
	}

avf_status_t CVDBConnection::ProcessCmd()
{
	avf_status_t status;
	bool b_write_pmb = true;

	avf_safe_release(mpData);

	mpMemBuffer->Clear();
	mpAck = (vdb_ack_t*)mpMemBuffer->Alloc(sizeof(vdb_ack_t));

	mp_vdb = NULL;

	avf_uint_t cmd_code = load_le_16(m_cmd_buffer);
	// avf_uint_t cmd_flags = load_le_16(m_cmd_buffer + 2);

	AVF_LOGD("ProcessCmd: %d", cmd_code);

	switch (cmd_code) {
	case VDB_CMD_GetVersionInfo:
		// vdb_ack_GetVersionInfo_t
		mpMemBuffer->write_le_16(VDB_VERSION_MAJOR);
		mpMemBuffer->write_le_16(VDB_VERSION_MINOR);
		mpMemBuffer->write_le_16(VDB_OS);
		mpMemBuffer->write_le_16(VDB_EXTRA_FLAG);
		status = E_OK;
		break;

	case VDB_CMD_GetClipSetInfo: {
			bool bNoDelete;
			GET_VDB_ID(vdb_cmd_GetClipSetInfo_t)
				vdb_cmd_GetClipSetInfo_t *this_cmd = (vdb_cmd_GetClipSetInfo_t*)m_cmd_buffer;
				status = mp_vdb->CmdGetClipSetInfo(this_cmd->clip_type, 0, false, mpMemBuffer, &bNoDelete);
			WRITE_VDB_ID(mpMemBuffer);
			if (status == E_OK) {
				mpMemBuffer->write_le_32(bNoDelete ? 1 : 0);
			}
		}
		break;

	case VDB_CMD_GetIndexPicture:
		GET_VDB_ID(vdb_cmd_GetIndexPicture_t)
			vdb_cmd_GetIndexPicture_t *this_cmd = (vdb_cmd_GetIndexPicture_t*)m_cmd_buffer;
			//if (VDB_EXTRA_FLAG && load_le_16(m_cmd_buffer + 2) == 1) {
			if (VDB_EXTRA_FLAG && this_cmd->header.cmd_flags == 1) {
				status = HandleGetClipPoster(this_cmd);
			} else {
				status = mp_vdb->CmdGetIndexPicture(this_cmd, mpMemBuffer, false, false);
			}
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetIFrame:
		GET_VDB_ID(vdb_cmd_GetIFrame_t)
		status = mp_vdb->CmdGetIFrame((vdb_cmd_GetIFrame_t*)m_cmd_buffer, mpMemBuffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetPlaybackUrl:
		GET_VDB_ID(vdb_cmd_GetPlaybackUrl_t)
		status = HandleGetPlaybackUrl(mp_vdb, (vdb_cmd_GetPlaybackUrl_t*)m_cmd_buffer, 0);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetDownloadUrl:
		GET_VDB_ID(vdb_cmd_GetDownloadUrl_t)
		status = HandleGetDownloadUrl(mp_vdb, (vdb_cmd_GetDownloadUrl_t*)m_cmd_buffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_MarkClip:
		GET_VDB_ID(vdb_cmd_MarkClip_t)
			avf_u32_t error = 0;
			int clip_type;
			clip_id_t clip_id;
			status = mp_vdb->CmdMarkClip((vdb_cmd_MarkClip_t*)m_cmd_buffer, &error, &clip_type, &clip_id);
			mpMemBuffer->write_le_32(error);
#if VDB_EXTRA_FLAG
			if (status == E_OK && error == 0) {
				if (mp_vdb_set->on_clip_created) {
					mp_vdb_set->on_clip_created(mp_vdb_context, mp_vdb_id, clip_type, clip_id);
				}
			}
#endif
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_DeleteClip:
		GET_VDB_ID(vdb_cmd_DeleteClip_t)
			avf_u32_t error = 0;
			vdb_cmd_DeleteClip_t *this_cmd = (vdb_cmd_DeleteClip_t*)m_cmd_buffer;
			status = mp_vdb->CmdDeleteClip(this_cmd, &error);
			mpMemBuffer->write_le_32(error);
#if VDB_EXTRA_FLAG
			if (status == E_OK && error == 0) {
				if (mp_vdb_set->on_clip_deleted) {
					mp_vdb_set->on_clip_deleted(mp_vdb_context, mp_vdb_id, this_cmd->clip_type, this_cmd->clip_id);
				}
			}
#endif
		WRITE_VDB_ID(mpMemBuffer)
		break;

	case VDB_CMD_GetRawData:
		GET_VDB_ID(vdb_cmd_GetRawData_t)
		status = mp_vdb->CmdGetRawData((vdb_cmd_GetRawData_t*)m_cmd_buffer, mpMemBuffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_SetRawDataOption: {
			vdb_cmd_SetRawDataOption_t *param = (vdb_cmd_SetRawDataOption_t*)m_cmd_buffer;
			mRawDataFlags = param->data_types;
		}
		status = E_OK;
		break;

	case VDB_CMD_GetRawDataBlock:
		GET_VDB_ID(vdb_cmd_GetRawDataBlock_t)
		vdb_cmd_GetRawDataBlock_t *pcmd = (vdb_cmd_GetRawDataBlock_t*)m_cmd_buffer;
		if ((pcmd->data_options & DATA_OPTION_NO_DATA) == 0) {
			mpData = CMemBuffer::Create(4*KB);
		}
		status = mp_vdb->CmdGetRawDataBlock(pcmd, mpMemBuffer, mpData, 0);
		WRITE_VDB_ID(mpData ? mpData : mpMemBuffer);
		break;

	case VDB_CMD_GetRawDataBlockEx:
		GET_VDB_ID(vdb_cmd_GetRawDataBlockEx_t)
		vdb_cmd_GetRawDataBlockEx_t *pcmd = (vdb_cmd_GetRawDataBlockEx_t*)m_cmd_buffer;
		if ((pcmd->data_options & DATA_OPTION_NO_DATA) == 0) {
			mpData = CMemBuffer::Create(4*KB);
		}
		status = mp_vdb->CmdGetRawDataBlock((vdb_cmd_GetRawDataBlock_t*)m_cmd_buffer, mpMemBuffer,
			mpData, pcmd->interval_ms);
		WRITE_VDB_ID(mpData ? mpData : mpMemBuffer);
		break;

	case VDB_CMD_GetDownloadUrlEx:
		GET_VDB_ID(vdb_cmd_GetDownloadUrlEx_t)
		status = HandleGetDownloadUrlEx(mp_vdb, (vdb_cmd_GetDownloadUrlEx_t*)m_cmd_buffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetAllPlaylists:
		GET_VDB_ID(vdb_cmd_GetAllPlaylists_t)
		status = mp_vdb->CmdGetAllPlaylists((vdb_cmd_GetAllPlaylists_t*)m_cmd_buffer, mpMemBuffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetPlaylistIndexPicture:
		GET_VDB_ID(vdb_cmd_GetPlaylistIndexPicture_t)
		status = mp_vdb->CmdGetPlaylistIndexPicture((vdb_cmd_GetPlaylistIndexPicture_t*)m_cmd_buffer, mpMemBuffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_ClearPlaylist:
		GET_VDB_ID(vdb_cmd_ClearPlaylist_t)
			avf_u32_t error = 0;
			status = mp_vdb->CmdClearPlaylist((vdb_cmd_ClearPlaylist_t*)m_cmd_buffer, &error);
			mpMemBuffer->write_le_32(error);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_InsertClip:
		GET_VDB_ID(vdb_cmd_InsertClip_t)
			avf_u32_t error = 0;
			status = mp_vdb->CmdInsertClip((vdb_cmd_InsertClip_t*)m_cmd_buffer, true, &error);
			mpMemBuffer->write_le_32(error);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_MoveClip:
		GET_VDB_ID(vdb_cmd_MoveClip_t)
			avf_u32_t error = 0;
			status = mp_vdb->CmdMoveClip((vdb_cmd_MoveClip_t*)m_cmd_buffer, &error);
			mpMemBuffer->write_le_32(error);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetPlaylistPlaybackUrl:
		GET_VDB_ID(vdb_cmd_GetPlaylistPlaybackUrl_t)
		status = HandleGetPlaylistPlaybackUrl(mp_vdb, (vdb_cmd_GetPlaylistPlaybackUrl_t*)m_cmd_buffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetUploadUrl:
		GET_VDB_ID(vdb_cmd_GetUploadUrl_t)
		status = HandleGetUploadUrl(mp_vdb, (vdb_cmd_GetUploadUrl_t*)m_cmd_buffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_SetClipAttr:
		GET_VDB_ID(vdb_cmd_SetClipAttr_t)
		status = mp_vdb->CmdSetClipAttr((vdb_cmd_SetClipAttr_t*)m_cmd_buffer, mpMemBuffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetClipExtent:
		GET_VDB_ID(vdb_cmd_GetClipExtent_t)
		status = mp_vdb->CmdGetClipExtent((vdb_cmd_GetClipExtent_t*)m_cmd_buffer, mpMemBuffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_SetOptions:
		GET_VDB_ID(vdb_cmd_SetOptions_t)
			vdb_cmd_SetOptions_t *this_cmd = (vdb_cmd_SetOptions_t*)m_cmd_buffer;
			switch (this_cmd->option) {
			case VDB_OPTION_HLS_SEGMENT_LENGTH:
				m_hls_seg_length = this_cmd->params[0];
				status = E_OK;
				break;
			default:
				status = E_ERROR;
				break;
			}
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetSpaceInfo:
		GET_VDB_ID(vdb_cmd_GetSpaceInfo_t)
			avf_u64_t total_space;
			avf_u64_t used_space;
			avf_u64_t marked_space;
			avf_u64_t clip_space;
			status = mp_vdb->GetSpaceInfo(&total_space, &used_space, &marked_space, &clip_space);
			if (status == E_OK) {
				// vdb_ack_GetSpaceInfo_t
				mpMemBuffer->write_le_64(total_space);
				mpMemBuffer->write_le_64(used_space);
				mpMemBuffer->write_le_64(marked_space);
				mpMemBuffer->write_le_64(clip_space);
				mpMemBuffer->write_zero(4*4);
			}
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_SetClipExtent:
		GET_VDB_ID(vdb_cmd_SetClipExtent_t)
			status = mp_vdb->CmdSetClipExtent((vdb_cmd_SetClipExtent_t*)m_cmd_buffer);
			mpMemBuffer->write_le_32(status);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetClipSetInfoEx: {
			bool bNoDelete;
			GET_VDB_ID(vdb_cmd_GetClipSetInfoEx_t)
				vdb_cmd_GetClipSetInfoEx_t *this_cmd = (vdb_cmd_GetClipSetInfoEx_t*)m_cmd_buffer;
				status = mp_vdb->CmdGetClipSetInfo(this_cmd->inherited.clip_type, this_cmd->flags, true, mpMemBuffer, &bNoDelete);
			WRITE_VDB_ID(mpMemBuffer);
			if (status == E_OK) {
				mpMemBuffer->write_le_8(bNoDelete ? 1 : 0);
			}
		}
		break;

	case VDB_CMD_GetAllClipSetInfo:
		status = HandleGetAllClipSetInfo();
		break;

	case VDB_CMD_GetClipInfo:
		GET_VDB_ID(vdb_cmd_GetClipInfo_t)
			vdb_cmd_GetClipInfo_t *this_cmd = (vdb_cmd_GetClipInfo_t*)m_cmd_buffer;
			status = mp_vdb->CmdGetClipInfo(this_cmd->clip_type, this_cmd->clip_id, this_cmd->flags, true, mpMemBuffer);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetPlaybackUrlEx:
		GET_VDB_ID(vdb_cmd_GetPlaybackUrlEx_t);
		vdb_cmd_GetPlaybackUrlEx_t *this_cmd = (vdb_cmd_GetPlaybackUrlEx_t*)m_cmd_buffer;
		status = HandleGetPlaybackUrl(mp_vdb, &this_cmd->inherited, this_cmd->length_ms);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetPictureList: {
			CPictureList *piclist = GetPictureList();
			if (piclist == NULL) {
				status = E_ERROR;
			} else {
				int32_t gmt_off;
				avf_get_sys_time(&gmt_off);
				vdb_cmd_GetPictureList_t *this_cmd = (vdb_cmd_GetPictureList_t*)m_cmd_buffer;
				// vdb_ack_GetPictureList_t
				mpMemBuffer->write_le_32(this_cmd->options);
				mpMemBuffer->write_le_32(0);	// reserved1
				mpMemBuffer->write_le_32(0);	// reserved2
				mpMemBuffer->write_le_32(gmt_off);
				piclist->GetPictureList(mpMemBuffer);
				status = E_OK;
			}
		}
		break;

	case VDB_CMD_ReadPicture: {
			CPictureList *piclist = GetPictureList();
			if (piclist == NULL) {
				status = E_ERROR;
			} else {
				vdb_cmd_ReadPicture_t *this_cmd = (vdb_cmd_ReadPicture_t*)m_cmd_buffer;
				vdb_picture_name_t *pname = (vdb_picture_name_t*)(this_cmd + 1);
				const char *picname = GetPictureFileName(pname);
				if (picname == NULL) {
					status = E_ERROR;
				} else {
					status = ReadPictureData(this_cmd, piclist, picname);
					b_write_pmb = false;
				}
			}
		}
		break;

	case VDB_CMD_RemovePicture: {
			CPictureList *piclist = GetPictureList();
			if (piclist == NULL) {
				status = E_ERROR;
			} else {
				vdb_cmd_RemovePicture_t *this_cmd = (vdb_cmd_RemovePicture_t*)m_cmd_buffer;
				vdb_picture_name_t *pname = (vdb_picture_name_t*)(this_cmd + 1);
				const char *picture_name = GetPictureFileName(pname);
				if (picture_name == NULL) {
					status = E_ERROR;
				} else {
					// vdb_ack_RemovePicture_t
					string_t *picname = string_t::CreateFrom(picture_name);
					status = piclist->RemovePicture(picname);
					if (status == E_OK) {
						CVDBServer *pServer = (CVDBServer*)mpServer;
						pServer->PictureRemoved(picname);
					}
					mpMemBuffer->write_le_32(status);
					status = E_OK;
				}
			}
		}
		break;

	case VDB_CMD_CreatePlaylist:
		GET_VDB_ID(vdb_cmd_CreatePlaylist_t)
			avf_u32_t playlist_id;
			status = mp_vdb->CmdCreatePlaylist(&playlist_id);
			// vdb_ack_CreatePlaylist_t
			mpMemBuffer->write_le_32(status);
			mpMemBuffer->write_le_32(playlist_id);
			mpMemBuffer->write_le_32(0);
			mpMemBuffer->write_le_32(0);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_DeletePlaylist:
		GET_VDB_ID(vdb_cmd_DeletePlaylist_t)
			vdb_cmd_DeletePlaylist_t *this_cmd = (vdb_cmd_DeletePlaylist_t*)m_cmd_buffer;
			status = mp_vdb->CmdDeletePlaylist(this_cmd->list_id);
			// vdb_ack_DeletePlaylist_t
			mpMemBuffer->write_le_32(status);
			mpMemBuffer->write_le_32(this_cmd->list_id);
			mpMemBuffer->write_le_32(0);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_InsertClipEx:
		GET_VDB_ID(vdb_cmd_InsertClipEx_t)
			avf_u32_t error = 0;
			vdb_cmd_InsertClipEx_t *this_cmd = (vdb_cmd_InsertClipEx_t*)m_cmd_buffer;
			status = mp_vdb->CmdInsertClip(&this_cmd->inherited, this_cmd->check_stream_attr, &error);
			mpMemBuffer->write_le_32(error);
		WRITE_VDB_ID(mpMemBuffer);
		break;

	case VDB_CMD_GetPlaylistPath:
		GET_VDB_ID(vdb_cmd_GetPlaylistPath_t)
			char buffer[200];
			vdb_cmd_GetPlaylistPath_t *this_cmd = (vdb_cmd_GetPlaylistPath_t*)m_cmd_buffer;
			if (!mp_vdb->PlaylistExists(this_cmd->list_id)) {
				mpMemBuffer->write_le_32(-1);
				status = E_ERROR;
			} else {
				vdb_format_playlist_path(mpSocket, buffer, this_cmd->list_id);
				int len = ::strlen(buffer) + 1;
				mpMemBuffer->write_le_32(0);
				mpMemBuffer->write_le_32(len);
				mpMemBuffer->write_string(buffer, len);
				status = E_OK;
			}
		WRITE_VDB_ID(mpMemBuffer);
		break;

	default:
		AVF_LOGW("unknown command received: %d", cmd_code);
		status = E_ERROR;
		break;
	}

	if (b_write_pmb) {
		AUTO_LOCK(mWriteMutex);
		status = WriteAckLocked((const vdb_cmd_header_t*)m_cmd_buffer, status);
	}

	avf_safe_release(mpData);
	ReleaseVDB();

	return status;
}

CPictureList *CVDBConnection::GetPictureList()
{
	CVDBServer *pServer = static_cast<CVDBServer*>(mpServer);
	CPictureList *piclist = pServer->GetPictureList();
	if (piclist == NULL) {
		AVF_LOGD("picture list not created");
		return NULL;
	}
	return piclist;
}

const char *CVDBConnection::GetPictureFileName(vdb_picture_name_t *pname)
{
	if (pname->filename_size == 0 || pname->filename_size > 100) {
		AVF_LOGD("bad filename_size %d", pname->filename_size);
		return NULL;
	}
	const char *picname = (const char *)(pname + 1);
	if (picname[pname->filename_size - 1] != 0) {
		AVF_LOGD("bad filename");
		return NULL;
	}
	return picname;
}

#define PICBUF_SIZE	(8*KB)

avf_status_t CVDBConnection::ReadPictureData(vdb_cmd_ReadPicture_t *cmd, CPictureList *piclist, const char *picname)
{
	AUTO_LOCK(mWriteMutex);

	avf_status_t status;
	avf_u8_t *buffer = avf_malloc_bytes(PICBUF_SIZE);

	// vdb_ack_ReadPicture_t
	vdb_ack_t *ack = (vdb_ack_t*)buffer;
	ack->magic = VDB_ACK_MAGIC;
	ack->seqid = m_seqid++;
	ack->user1 = cmd->header.user1;
	ack->user2 = cmd->header.user2;
	ack->cmd_code = cmd->header.cmd_code;
	ack->cmd_flags = cmd->header.cmd_flags & VDB_CMD_FLAGS;
	ack->cmd_tag = cmd->header.cmd_tag;

	ack->ret_code = E_OK;	// should use vdb_ack_ReadPicture_t.result_ok
	ack->extra_bytes = 0;	// fixed later

	// vdb_ack_ReadPicture_t
	avf_u8_t *ptr = (avf_u8_t*)(ack + 1);
	SAVE_LE_32(ptr, cmd->options);
	SAVE_LE_32(ptr, 0);	// reserved1
	SAVE_LE_32(ptr, 0);	// reserved2

	status = piclist->ReadPicture(picname, cmd, ack,
		buffer, PICBUF_SIZE, ptr - buffer,
		(cmd->options & PIC_OPT_THUMBNAIL) != 0,
		mpSocketEvent, mpSocket, 10*1000, true);

	avf_free(buffer);
	return status;
}

avf_status_t CVDBConnection::HandleGetAllClipSetInfo()
{
	if (mp_vdb_set->get_all_clips == NULL) {
		AVF_LOGE("get_all_clips is NULL");
		return E_ERROR;
	}

	uint32_t num_clips = 0;
	vdb_clip_desc_t *clips = NULL;

	if (mp_vdb_set->get_all_clips(mp_vdb_context, &num_clips, &clips) < 0) {
		AVF_LOGE("get_all_clips failed");
		return E_ERROR;
	}

	// vdb_ack_GetAllClipSetInfo_t
	mpMemBuffer->write_le_32(num_clips);

	for (uint32_t i = 0; i < num_clips; i++) {
		vdb_clip_desc_t *desc = clips + i;

		// vdb_clip_info_t
		mpMemBuffer->write_le_32(desc->clip_id);
		mpMemBuffer->write_le_32(desc->clip_date);
		mpMemBuffer->write_le_32(desc->clip_duration_ms);
		mpMemBuffer->write_le_64(desc->clip_start_time);
		mpMemBuffer->write_le_16(desc->num_streams);
		mpMemBuffer->write_le_16(GET_CLIP_VDB_ID);
		if (desc->num_streams > 0) {
			mpMemBuffer->write_mem((const avf_u8_t*)(desc->stream_attr + 0), sizeof(avf_stream_attr_t));
			if (desc->num_streams > 1) {
				mpMemBuffer->write_mem((const avf_u8_t*)(desc->stream_attr + 1), sizeof(avf_stream_attr_t));
			}
		}

		// vdb_clip_info_ex_t
		mpMemBuffer->write_le_32(desc->clip_type);
		avf_u32_t *p_extra_size = (avf_u32_t*)mpMemBuffer->Alloc(4);
		avf_size_t total = mpMemBuffer->write_string_aligned(desc->vdb_id);
		*p_extra_size = total;
	}

	mp_vdb_set->release_clips(mp_vdb_context, num_clips, clips);

	return E_OK;
}

avf_status_t CVDBConnection::HandleGetClipPoster(vdb_cmd_GetIndexPicture_t *cmd)
{
	if (mp_vdb_set->get_clip_poster == NULL) {
		AVF_LOGE("no get_clip_poster");
		return E_ERROR;
	}

	uint32_t size = 0;
	void *data = NULL;
	if (mp_vdb_set->get_clip_poster(mp_vdb_context, mp_vdb_id, cmd->clip_type, cmd->clip_id, &size, &data) != 0) {
		AVF_LOGE("get clip poster failed");
		return E_ERROR;
	}

	// vdb_cmd_GetIndexPicture_t
	mpMemBuffer->write_le_32(cmd->clip_type);
	mpMemBuffer->write_le_32(cmd->clip_id);
	mpMemBuffer->write_le_32(0);
	mpMemBuffer->write_le_32(cmd->user_data);
	mpMemBuffer->write_le_32(cmd->clip_time_ms_lo);	// requested
	mpMemBuffer->write_le_32(cmd->clip_time_ms_hi);	// requested
	mpMemBuffer->write_le_32(cmd->clip_time_ms_lo);	// real
	mpMemBuffer->write_le_32(cmd->clip_time_ms_hi);	// real
	mpMemBuffer->write_le_32(0);	// duration
	mpMemBuffer->write_le_32(size);	// picture_size
	mpMemBuffer->write_mem((const avf_u8_t*)data, size);

	mp_vdb_set->release_clip_poster(mp_vdb_context, mp_vdb_id, size, data);

	return E_OK;
}

avf_status_t CVDBConnection::HandleGetPlaybackUrl(CVDB *pvdb, vdb_cmd_GetPlaybackUrl_t *cmd, uint32_t length_ms)
{
	IVdbInfo::range_s range;
	IVdbInfo::clip_info_s clip_info;

	range.clip_type = cmd->clip_type;
	range.ref_clip_id = cmd->clip_id;
	range._stream = cmd->stream;
	range.b_playlist = 0;
	range.clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);
	range.length_ms = length_ms;

	if (range.length_ms == 0 || range.length_ms > MAX_PLAYBACK_LENGTH) {
		AVF_LOGDW("limit playback length to %d min, requested: %d",
			MAX_PLAYBACK_LENGTH/(60*1000), range.length_ms / (60*1000));
		range.length_ms = MAX_PLAYBACK_LENGTH;
	}

	avf_status_t status = pvdb->GetRangeInfo(&range, &clip_info, NULL, 0);
	if (status != E_OK)
		return status;

	char server_ip[20];
	vdb_get_server_ip(mpSocket, server_ip);

	char url[200];
	vdb_format_url(server_ip, url, mp_vdb_id, cmd->clip_type, cmd->clip_id, cmd->stream, 0, range.b_playlist,
		cmd->url_type, clip_info.clip_time_ms, clip_info._length_ms, m_hls_seg_length, 0);

	AVF_LOGD(C_GREEN "playback url is %s" C_NONE, url);

	// vdb_ack_GetPlaybackUrl_t
	CMemBuffer *pmb = mpMemBuffer;
	pmb->write_le_32(cmd->clip_type);
	pmb->write_le_32(cmd->clip_id);
	pmb->write_le_32(cmd->stream);
	pmb->write_le_32(cmd->url_type);
	pmb->write_le_64(clip_info.clip_time_ms);
	pmb->write_le_32(clip_info._length_ms);
	pmb->write_le_32(0);		// has_more
	int len = ::strlen(url) + 1;
	pmb->write_le_32(len);
	pmb->write_string(url, len);

	return E_OK;
}

avf_status_t CVDBConnection::HandleGetPlaylistPlaybackUrl(CVDB *pvdb, vdb_cmd_GetPlaylistPlaybackUrl_t *cmd)
{
	IVdbInfo::range_s range;
	IVdbInfo::clip_info_s clip_info;

	range.clip_type = cmd->list_type;
	range.ref_clip_id = INVALID_CLIP_ID;
	range._stream = cmd->stream;
	range.b_playlist = 1;
	range.length_ms = MAX_PLAYBACK_LENGTH;
	range.clip_time_ms = cmd->playlist_start_ms;

	avf_status_t status = pvdb->GetRangeInfo(&range, &clip_info, NULL, 0);
	if (status != E_OK)
		return status;

	char server_ip[20];
	vdb_get_server_ip(mpSocket, server_ip);

	char url[200];
	vdb_format_url(server_ip, url, mp_vdb_id, cmd->list_type, 0, cmd->stream, 0, range.b_playlist,
		cmd->url_type, clip_info.clip_time_ms, clip_info._length_ms, m_hls_seg_length, 0);

	AVF_LOGD(C_GREEN "playback url is %s" C_NONE, url);

	// vdb_ack_GetPlaylistPlaybackUrl_t
	CMemBuffer *pmb = mpMemBuffer;
	pmb->write_le_32(cmd->list_type);
	pmb->write_le_32(clip_info.clip_time_ms);
	pmb->write_le_32(cmd->stream);
	pmb->write_le_32(cmd->url_type);
	///
	pmb->write_le_32(clip_info._length_ms);
	pmb->write_le_32(0);		// has_more
	int len = ::strlen(url) + 1;
	pmb->write_le_32(len);
	pmb->write_string(url, len);

	return E_OK;
}

avf_status_t CVDBConnection::HandleGetUploadUrl(CVDB *pvdb, vdb_cmd_GetUploadUrl_t *cmd)
{
	IVdbInfo::range_s range;
	IVdbInfo::clip_info_s clip_info;

	range.clip_type = cmd->clip_type;
	range.ref_clip_id = cmd->clip_id;
	range._stream = -1;
	range.b_playlist = cmd->b_playlist;
	range.length_ms = cmd->length_ms;
	range.clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);

	// upload_opt=0: do not need size info now
	avf_status_t status = pvdb->GetRangeInfo(&range, &clip_info, NULL, cmd->upload_opt);
	if (status != E_OK)
		return status;

	char server_ip[20];
	vdb_get_server_ip(mpSocket, server_ip);

	char url[200];
	vdb_format_url(server_ip, url, mp_vdb_id, cmd->clip_type, cmd->clip_id, -1, cmd->upload_opt,
		cmd->b_playlist, 0, clip_info.clip_time_ms, clip_info._length_ms, 0, 0);

	AVF_LOGD(C_GREEN "upload url is %s" C_NONE, url);

	// vdb_ack_GetUploadUrl_t
	CMemBuffer *pmb = mpMemBuffer;
	pmb->write_le_32(cmd->b_playlist);
	pmb->write_le_32(cmd->clip_type);
	pmb->write_le_32(cmd->clip_id);
	pmb->write_le_64(clip_info.clip_time_ms);
	pmb->write_le_32(clip_info._length_ms);
	pmb->write_le_32(cmd->upload_opt);
	pmb->write_le_32(cmd->reserved1);
	pmb->write_le_32(cmd->reserved2);

	int len = ::strlen(url) + 1;
	pmb->write_le_32(len);
	pmb->write_string(url, len);

	return E_OK;
}

void CVDBConnection::WriteDownloadUrl(int clip_type, avf_u32_t clip_id,
	CVDB::clip_info_s *clip_info, int stream, 
	int b_playlist, int b_mute_audio, int url_type)
{
	char server_ip[20];
	vdb_get_server_ip(mpSocket, server_ip);

	char url[200];
	vdb_format_url(server_ip, url, mp_vdb_id, clip_type, clip_id, stream, 0, b_playlist,
		b_mute_audio ? (url_type|URL_MUTE_AUDIO) : url_type,
		clip_info->clip_time_ms, clip_info->_length_ms, 0, 0);

	AVF_LOGD("download url[%d]: " C_YELLOW "%s" C_NONE, stream, url);

	CMemBuffer *pmb = mpMemBuffer;
	pmb->write_le_32(clip_info->clip_date);
	pmb->write_le_64(clip_info->clip_time_ms);
	pmb->write_le_32(clip_info->_length_ms);
	pmb->write_le_64(clip_info->v_size[0]);
	int len = ::strlen(url) + 1;
	pmb->write_le_32(len);
	pmb->write_string(url, len);
}

avf_status_t CVDBConnection::GetDownloadIndexPicture(CVDB *pvdb, int clip_type, avf_u32_t clip_id,
	avf_u32_t clip_time_ms_lo, avf_u32_t clip_time_ms_hi, bool bPlaylist)
{
	vdb_cmd_GetIndexPicture_t icmd;
	icmd.clip_type = clip_type;
	icmd.clip_id = clip_id;
	icmd.user_data = 0;
	icmd.clip_time_ms_lo = clip_time_ms_lo;
	icmd.clip_time_ms_hi = clip_time_ms_hi;
	return pvdb->CmdGetIndexPicture(&icmd, mpMemBuffer, true, bPlaylist, true);
}

// obsolete
avf_status_t CVDBConnection::HandleGetDownloadUrl(CVDB *pvdb, vdb_cmd_GetDownloadUrl_t *cmd)
{
	avf_status_t status;
	IVdbInfo::range_s range;
	CVDB::clip_info_s clip_info;

	range.clip_type = cmd->clip_type;
	range.ref_clip_id = cmd->clip_id;
	range._stream = STREAM_MAIN;
	range.b_playlist = 0;
	range.length_ms = cmd->clip_length_ms;
	range.clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);

	status = pvdb->GetRangeInfo(&range, &clip_info, NULL, 0);
	if (status != E_OK)
		return status;

	//vdb_ack_GetDownloadUrl_t
	CMemBuffer *pmb = mpMemBuffer;
	pmb->write_le_32(cmd->clip_type);
	pmb->write_le_32(cmd->clip_id);
	WriteDownloadUrl(cmd->clip_type, cmd->clip_id, &clip_info, range._stream, range.b_playlist, 0, URL_TYPE_TS);

	// index picture
	status = GetDownloadIndexPicture(pvdb, cmd->clip_type, cmd->clip_id,
		cmd->clip_time_ms_lo, cmd->clip_time_ms_hi, range.b_playlist);
	if (status != E_OK)
		return status;

	return E_OK;
}

avf_status_t CVDBConnection::HandleDownloadStreamUrl(CVDB *pvdb, vdb_cmd_GetDownloadUrlEx_t *cmd,
	int stream, bool bDownloadPlaylist, uint32_t *download_opt)
{
	IVdbInfo::range_s range;
	CVDB::clip_info_s clip_info;
	avf_status_t status;

	range.clip_type = cmd->clip_type;
	range.ref_clip_id = cmd->clip_id;
	range._stream = stream;
	range.b_playlist = bDownloadPlaylist;
	range.length_ms = cmd->clip_length_ms;
	range.clip_time_ms = MK_U64(cmd->clip_time_ms_lo, cmd->clip_time_ms_hi);

	status = pvdb->GetRangeInfo(&range, &clip_info, NULL, 0);
	if (status != E_OK)
		return status;

	int wants_mp4 = cmd->download_opt & ((stream == STREAM_MAIN) ? DOWNLOAD_OPT_MAIN_MP4 : DOWNLOAD_OPT_SUB_MP4);
	int has_mp4 = ((stream == STREAM_MAIN) ? clip_info.video_type[0] : clip_info.video_type[1]) == VIDEO_TYPE_MP4;
	int url_type = (wants_mp4 && has_mp4) ? URL_TYPE_MP4 : URL_TYPE_TS;

	WriteDownloadUrl(cmd->clip_type, cmd->clip_id, &clip_info, stream, range.b_playlist,
		(cmd->download_opt&URL_MUTE_AUDIO) != 0, url_type);

	*download_opt |= (stream == STREAM_MAIN) ? DOWNLOAD_OPT_MAIN_STREAM : DOWNLOAD_OPT_SUB_STREAM_1;

	return E_OK;
}

avf_status_t CVDBConnection::HandleGetDownloadUrlEx(CVDB *pvdb, vdb_cmd_GetDownloadUrlEx_t *cmd)
{
	CMemBuffer *pmb = mpMemBuffer;
	avf_status_t status;

	// vdb_ack_GetDownloadUrlEx_t
	pmb->write_le_32(cmd->clip_type);
	pmb->write_le_32(cmd->clip_id);
	avf_u8_t *p_download_opt = pmb->Alloc(sizeof(avf_u32_t));
	bool bDownloadPlaylist = (cmd->download_opt & DOWNLOAD_OPT_PLAYLIST) != 0;
	avf_u32_t download_opt = bDownloadPlaylist ? DOWNLOAD_OPT_PLAYLIST : 0;

	// main stream
	if (cmd->download_opt & DOWNLOAD_OPT_MAIN_STREAM) {
		HandleDownloadStreamUrl(pvdb, cmd, STREAM_MAIN, bDownloadPlaylist, &download_opt);
	}

	// sub stream 1
	if (cmd->download_opt & DOWNLOAD_OPT_SUB_STREAM_1) {
		HandleDownloadStreamUrl(pvdb, cmd, STREAM_SUB_1, bDownloadPlaylist, &download_opt);
	}

	// index picture
	if (cmd->download_opt & DOWNLOAD_OPT_INDEX_PICT) {
		status = GetDownloadIndexPicture(pvdb, cmd->clip_type, cmd->clip_id,
			cmd->clip_time_ms_lo, cmd->clip_time_ms_hi, bDownloadPlaylist);
		if (status == E_OK) {
			download_opt |= DOWNLOAD_OPT_INDEX_PICT;
		}
	}

	// mute audio
	download_opt |= cmd->download_opt & DOWNLOAD_OPT_MUTE_AUDIO;

	if (download_opt == 0) {
		AVF_LOGE("HandleGetDownloadUrlEx failed");
		return E_ERROR;
	}

	*(avf_u32_t*)p_download_opt = download_opt;
	return E_OK;
}

avf_status_t CVDBConnection::BroadcastMsg(void *buf, int size, const avf_u8_t *p_vdb_id, int len)
{
	AUTO_LOCK(mWriteMutex);

	vdb_ack_t *header = (vdb_ack_t*)buf;
	header->seqid = m_seqid++;

	avf_status_t status = WriteBufferLocked((avf_u8_t *)buf, size, false);
	if (status != E_OK)
		return status;

	if (p_vdb_id) {
		status = WriteBufferLocked(p_vdb_id, len, false);
		if (status != E_OK)
			return status;
	}

	if (size < VDB_ACK_SIZE) {
		return WritePaddingLocked(VDB_ACK_SIZE - size, false);
	}

	return E_OK;
}

avf_status_t CVDBConnection::BroadcastRawData(vdb_msg_RawData_t *param, raw_data_t *raw,
	int size, const avf_u8_t *p_vdb_id, int len)
{
	AUTO_LOCK(mWriteMutex);

	param->header.seqid = m_seqid++;

	avf_status_t status = WriteBufferLocked((avf_u8_t*)param, sizeof(*param), false);
	if (status != E_OK)
		return status;

	status = WriteBufferLocked(raw->GetData(), raw->GetSize(), false);
	if (status != E_OK)
		return status;

	if (p_vdb_id) {
		status = WriteBufferLocked(p_vdb_id, len, false);
		if (status != E_OK)
			return status;
	}

	if (size < VDB_ACK_SIZE) {
		return WritePaddingLocked(VDB_ACK_SIZE - size, false);
	}

	return E_OK;
}

avf_status_t CVDBConnection::BroadcastPictureRemoved(string_t *picname)
{
	AUTO_LOCK(mWriteMutex);

	avf_u8_t buffer[VDB_ACK_SIZE];
	vdb_ack_t *ack = (vdb_ack_t*)buffer;

	ack->magic = VDB_ACK_MAGIC;
	ack->seqid = m_seqid++;
	ack->user1 = 0;
	ack->user2 = 0;
	ack->cmd_code = VDB_MSG_PictureRemoved;
	ack->cmd_flags = 0;
	ack->cmd_tag = 0;

	ack->ret_code = E_OK;
	ack->extra_bytes = 0;	// assume picname is not too long

	// vdb_msg_PictureRemoved_t
	avf_u8_t *ptr = (avf_u8_t*)(ack + 1);
	avf_size_t picname_size = picname->size() + 1;
	if (buffer + VDB_ACK_SIZE < ptr + 4 + ROUND_UP(picname_size, 4)) {
		AVF_LOGD("picture name too long %d", picname_size);
		return E_ERROR;
	}

	SAVE_LE_32(ptr, picname_size);
	::memcpy(ptr, picname->string(), picname_size);
	ptr += picname_size;
	::memset(ptr, 0, buffer + VDB_ACK_SIZE - ptr);

	return WriteBufferLocked(buffer, sizeof(buffer), false);
}

avf_status_t CVDBConnection::BroadcastPictureTaken(string_t *picname, avf_u32_t pic_date)
{
	// TODO:
	return E_OK;
}

avf_status_t CVDBConnection::BroadcastSpaceInfo(vdb_space_info_t *info)
{
	// TODO:
	return E_OK;
}

avf_status_t CVDBConnection::DeflateMemBufferLocked(void *_stream, CMemBuffer *from, int last,
	CMemBuffer *to, avf_u8_t **pptr, avf_size_t *remain_bytes)
{
#ifdef Z_LIB
	avf_u8_t *ptr = *pptr;
	avf_size_t room = *remain_bytes;
	avf_size_t block_size = to->GetBlockSize();

	z_stream *stream = (z_stream*)_stream;

	CMemBuffer::block_t *block = from->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		stream->next_in = block->GetMem();
		stream->avail_in = block->GetSize();

		int flush = last && block->GetNext() == NULL;

		for (;;) {
			if (room == 0) {
				ptr = to->AllocBlock();
				room = block_size;
			}

			stream->next_out = ptr;
			stream->avail_out = room;

			int ret = deflate(stream, flush);
			AVF_ASSERT(ret != Z_STREAM_ERROR);

			to->SetCurrBlockSize(block_size - stream->avail_out);
			ptr += room - stream->avail_out;
			room = stream->avail_out;

			if (room > 0)
				break;
		}

		AVF_ASSERT(stream->avail_in == 0);
	}

	*pptr = ptr;
	*remain_bytes = room;

	return E_OK;
#else
	return E_ERROR;
#endif
}

avf_status_t CVDBConnection::InflateMemBufferLocked(void *_stream, CMemBuffer *from, CMemBuffer *to)
{
#ifdef Z_LIB
	z_stream *stream = (z_stream*)_stream;

	avf_u8_t *ptr = NULL;
	avf_size_t room = 0;
	avf_size_t block_size = to->GetBlockSize();

	CMemBuffer::block_t *block = from->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		stream->next_in = block->GetMem();
		stream->avail_in = block->GetSize();

		for (;;) {
			if (room == 0) {
				ptr = to->AllocBlock();
				room = block_size;
			}

			stream->next_out = ptr;
			stream->avail_out = room;

			int ret = inflate(stream, Z_NO_FLUSH);
			AVF_ASSERT(ret != Z_STREAM_ERROR);

			to->SetCurrBlockSize(block_size - stream->avail_out);
			ptr += room - stream->avail_out;
			room = stream->avail_out;

			if (room > 0)
				break;
		}
	}

	return E_OK;
#else
	return E_ERROR;
#endif
}

avf_status_t CVDBConnection::DeflateAckLocked()
{
#ifdef Z_LIB
	CMemBuffer *pmb = CMemBuffer::Create(4*KB);

	z_stream stream;

	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	int ret = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
	AVF_ASSERT(ret == Z_OK);

	avf_u8_t *ptr = NULL;
	avf_size_t room = 0;

	DeflateMemBufferLocked((void*)&stream, mpMemBuffer, mpData == NULL, pmb, &ptr, &room);
	if (mpData) {
		DeflateMemBufferLocked((void*)&stream, mpData, 1, pmb, &ptr, &room);
	}

	deflateEnd(&stream);

	avf_safe_release(mpMemBuffer);
	avf_safe_release(mpData);
	mpMemBuffer = pmb;

	return E_OK;
#else
	return E_ERROR;
#endif
}

avf_status_t CVDBConnection::InflateAckLocked()
{
#ifdef Z_LIB
	CMemBuffer *pmb = CMemBuffer::Create(4*KB);

	z_stream stream;

	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = 0;
	stream.next_in = Z_NULL;
	int ret = inflateInit(&stream);
	AVF_ASSERT(ret == Z_OK);

	InflateMemBufferLocked((void*)&stream, mpMemBuffer, pmb);

	inflateEnd(&stream);

	avf_safe_release(mpMemBuffer);
	avf_safe_release(mpData);
	mpMemBuffer = pmb;

	return E_OK;
#else
	return E_ERROR;
#endif
}

avf_status_t CVDBConnection::WriteAckLocked(const vdb_cmd_header_t *cmd, avf_status_t ret_code)
{
	if (cmd->cmd_flags & VDB_CMD_DEFLATE) {
		DeflateAckLocked();
	} else {
//		AVF_LOGI("deflate & inflate");
//		DeflateAckLocked();
//		InflateAckLocked();
	}

	avf_u32_t size_ack = mpMemBuffer->GetTotalSize();
	if (mpData) {
		size_ack += mpData->GetTotalSize();
	}

	if (size_ack == 0 || ret_code != E_OK) {
		return WriteBaseAckLocked(cmd, ret_code);
	}

	// ack header - already reserved
	mpAck->magic = VDB_ACK_MAGIC;
	mpAck->seqid = m_seqid++;
	mpAck->user1 = cmd->user1;
	mpAck->user2 = cmd->user2;
	mpAck->cmd_code = cmd->cmd_code;
	mpAck->cmd_flags = cmd->cmd_flags & VDB_CMD_FLAGS;
	mpAck->cmd_tag = cmd->cmd_tag;

	mpAck->ret_code = ret_code;
	mpAck->extra_bytes = size_ack > VDB_ACK_SIZE ? (size_ack - VDB_ACK_SIZE) : 0;

	// all buffer blocks
	CMemBuffer::block_t *block = mpMemBuffer->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		avf_status_t status = WriteBufferLocked(block->GetMem(), block->GetSize(), true);
		if (status != E_OK)
			return status;
	}

	// data
	if (mpData) {
		block = mpData->GetFirstBlock();
		for (; block; block = block->GetNext()) {
			avf_status_t status = WriteBufferLocked(block->GetMem(), block->GetSize(), true);
			if (status != E_OK)
				return status;
		}
	}

	// padding 0
	if (size_ack < VDB_ACK_SIZE) {
		return WritePaddingLocked(VDB_ACK_SIZE - size_ack, true);
	}

	return E_OK;
}

avf_status_t CVDBConnection::WriteBaseAckLocked(const vdb_cmd_header_t *cmd, avf_status_t ret_code)
{
	vdb_ack_t ack;

	ack.magic = VDB_ACK_MAGIC;
	ack.seqid = m_seqid++;
	ack.user1 = cmd->user1;
	ack.user2 = cmd->user2;
	ack.cmd_code = cmd->cmd_code;
	ack.cmd_flags = 0;
	ack.cmd_tag = cmd->cmd_tag;

	ack.ret_code = ret_code;
	ack.extra_bytes = 0;

	avf_status_t status = WriteBufferLocked((avf_u8_t*)&ack, sizeof(ack), true);
	if (status != E_OK)
		return status;

	if (sizeof(ack) < VDB_ACK_SIZE) {
		return WritePaddingLocked(VDB_ACK_SIZE - sizeof(ack), true);
	}

	return E_OK;
}

avf_status_t CVDBConnection::WritePaddingLocked(avf_uint_t size, bool bCheckClose)
{
	avf_u8_t buffer[VDB_ACK_SIZE];
	::memset(buffer, 0, size);
	return WriteBufferLocked(buffer, size, bCheckClose);
}

avf_status_t CVDBConnection::WriteBufferLocked(const avf_u8_t *buffer, avf_uint_t size, bool bCheckClose)
{
	avf_status_t status = TCPSend(5*1000, buffer, size, bCheckClose);
	if (status != E_OK) {
		AVF_LOGD("WriteBufferLocked failed: %d, bCheckClose: %d", status, bCheckClose);
	}
	return status;
}

//-----------------------------------------------------------------------
//
//  CVDBServer
//
//-----------------------------------------------------------------------
CVDBServer* CVDBServer::Create(const vdb_set_s *vdb_set, void *context)
{
	CVDBServer *result = avf_new CVDBServer(vdb_set, context);
	CHECK_STATUS(result);
	return result;
}

CVDBServer::CVDBServer(const vdb_set_s *vdb_set, void *context):
	inherited("VdbServer"),
	mpMsgQueue(NULL),
	mpThread(NULL),
	mpPictureList(NULL)
{
	m_vdb_set = *vdb_set;
	mp_vdb_context = context;
	CREATE_OBJ(mpMsgQueue = CMsgQueue::Create("vdb-server", sizeof(cmd_t), 4));
}

void *CVDBServer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IVDBBroadcast)
		return static_cast<IVDBBroadcast*>(this);
	return inherited::GetInterface(refiid);
}

CVDBServer::~CVDBServer()
{
	// first interrupt the msg q so the thread won't read anymore
	if (mpMsgQueue) {
		mpMsgQueue->Interrupt();
	}

	// stop server and all its sockets, so the thread will be forced to return
	StopServer();

	if (mpThread) {
		mpThread->Release();
	}

	avf_safe_release(mpPictureList);

	// TODO: should release memory hold by msgs
	avf_safe_release(mpMsgQueue);
}

CPictureList *CVDBServer::GetPictureList()
{
	AUTO_LOCK(mMutex);
	return mpPictureList;
}

CPictureList *CVDBServer::CreatePictureList(const char *picture_path)
{
	AUTO_LOCK(mMutex);
	if (mpPictureList == NULL) {
		string_t *pic_path = string_t::CreateFrom(picture_path);
		mpPictureList = CPictureList::Create(pic_path);
		if (mpPictureList) {
			mpPictureList->SetPictureListener(0, this);
		}
		pic_path->Release();
		return mpPictureList;
	}
	return NULL;
}

avf_status_t CVDBServer::Start()
{
	avf_status_t status;

	if (mpThread == NULL) {
		mpThread = CThread::Create("vdb-server", ThreadEntry, this);
		if (mpThread == NULL)
			return E_NOMEM;
	}

	if ((status = RunServer(VDB_CMD_PORT)) != E_OK) {
		return status;
	}

	return E_OK;
}

void CVDBServer::ServerLoop()
{
	cmd_t cmd;

	while (true) {

		if (!mpMsgQueue->GetMsgInterruptible(&cmd, sizeof(cmd)))
			break;

		switch (cmd.cmd_code) {
		case INFO_VdbReady: {
				vdb_msg_VdbReady_t msg;
				msg.status = cmd.u.VdbReady.status;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_VdbReady, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_VdbUnmounted: {
				vdb_msg_VdbUnmounted_t msg;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_VdbUnmounted, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_ClipInfo: {
				vdb_msg_MarkLiveClipInfo_t msg;
				msg.super.action = cmd.u.ClipInfo.action;
				msg.super.flags = cmd.u.ClipInfo.clip_attr;
				msg.super.list_pos = cmd.u.ClipInfo.list_pos;
				///
				msg.super.clip_type = cmd.u.ClipInfo.clip_type;
				msg.super.clip_id = cmd.u.ClipInfo.clip_id;
				msg.super.clip_date = cmd.u.ClipInfo.clip_date;
				msg.super.clip_duration_ms = cmd.u.ClipInfo.clip_duration_ms;
				msg.super.clip_start_time_ms_lo = U64_LO(cmd.u.ClipInfo.clip_start_time_ms);
				msg.super.clip_start_time_ms_hi = U64_HI(cmd.u.ClipInfo.clip_start_time_ms);
				//
				msg.super.num_streams = cmd.u.ClipInfo.num_streams;
				msg.super.reserved = 0;
				::memcpy(&msg.super.stream_info, &cmd.u.ClipInfo.stream_info, sizeof(avf_stream_attr_s)*MAX_VDB_STREAMS);
				///

				if (cmd.u.ClipInfo.mark_info_valid == 0) {
					BroadcastMsg(cmd.vdb_id, VDB_MSG_ClipInfo, (void*)&msg.super, sizeof(msg.super));
				} else {
					msg.flags = 0;
					msg.delay_ms = cmd.u.ClipInfo.delay_ms;
					msg.before_live_ms = cmd.u.ClipInfo.before_live_ms;
					msg.after_live_ms = cmd.u.ClipInfo.after_live_ms;
					BroadcastMsg(cmd.vdb_id, VDB_MSG_MarkLiveClipInfo, (void*)&msg, sizeof(msg));
				}
			}
			break;

		case INFO_ClipRemoved: {
				vdb_msg_ClipRemoved_t msg;
				msg.clip_type = cmd.u.ClipRemoved.clip_type;
				msg.clip_id = cmd.u.ClipRemoved.clip_id;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_ClipRemoved, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_BufferSpaceLow: {
				vdb_msg_BufferSpaceLow_t msg;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_BufferSpaceLow, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_BufferFull: {
				vdb_msg_BufferFull_t msg;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_BufferFull, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_SendRawData: {
				BroadcastRawData(cmd.vdb_id, cmd.u.SendRawData.raw);
				cmd.u.SendRawData.raw->Release();
			}
			break;

		case INFO_PlaylistCleared: {
				vdb_msg_PlaylistCleared_t msg;
				msg.list_type = cmd.u.PlaylistParam.list_type;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_PlaylistCleared, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_PlaylistCreated: {
				vdb_msg_PlaylistCreated_t msg;
				msg.list_type = cmd.u.PlaylistParam.list_type;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_PlaylistCreated, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_PlaylistDeleted: {
				vdb_msg_PlaylistDeleted_t msg;
				msg.list_type = cmd.u.PlaylistParam.list_type;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_PlaylistDeleted, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_RefreshPictureList: {
				vdb_msg_RefreshPictureList_t msg;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_RefreshPictureList, (void*)&msg, sizeof(msg));
			}
			break;

		case INFO_PictureTaken: {
				string_t *picname = cmd.u.PictureTaken.picname;
				BroadcastPictureTaken(picname, cmd.u.PictureTaken.pic_date);
				picname->Release();
			}
			break;

		case INFO_PictureRemoved: {
				string_t *picname = cmd.u.PictureRemoved.picname;
				BroadcastPictureRemoved(picname);
				picname->Release();
			}
			break;

		case INFO_SpaceInfo: {
				BroadcastSpaceInfo(cmd.vdb_id, 
					cmd.u.SpaceInfo.total_bytes,
					cmd.u.SpaceInfo.used_bytes,
					cmd.u.SpaceInfo.marked_bytes);
			}
			break;

		case INFO_ClipAttrChanged: {
				vdb_msg_ClipAttr_t msg;
				msg.clip_type = cmd.u.ClipAttrChanged.clip_type;
				msg.clip_id = cmd.u.ClipAttrChanged.clip_id;
				msg.clip_attr = cmd.u.ClipAttrChanged.clip_attr;
				BroadcastMsg(cmd.vdb_id, VDB_MSG_CLIP_ATTR, (void*)&msg, sizeof(msg));
			}
			break;

		default:
			break;
		}

		avf_safe_release(cmd.vdb_id);
	}
}

void CVDBServer::ThreadEntry(void *p)
{
	((CVDBServer*)p)->ServerLoop();
}

void CVDBServer::CopyVdbId(string_t *vdb_id, avf_u8_t **p_vdb_id, int *len)
{
	*len = vdb_id->size() + 1;
	if (*len > (int)ARRAY_SIZE(m_vdb_id_buf)) {
		AVF_LOGW("vdb_id for msg too long %d", *len);
		*len = 0;
	} else {
		*p_vdb_id = m_vdb_id_buf;
		::memcpy(m_vdb_id_buf, vdb_id->string(), *len);
		if (*len % 4 != 0) {
			int align = 4 - (*len % 4);
			avf_u8_t *ptr = m_vdb_id_buf + *len;
			for (int i = 0; i < align; i++) {
				ptr[i] = 0;
			}
			*len += align;
		}
	}
}

void CVDBServer::BroadcastMsg(string_t *vdb_id, int code, void *buf, int size)
{
	AUTO_LOCK(mMutex);

	if (mpFirstConn == NULL)
		return;

	vdb_ack_t *header = (vdb_ack_t*)buf;

	header->magic = VDB_ACK_MAGIC;
	header->seqid = 0;	// fixed later
	header->user1 = 0;
	header->user2 = 0;
	header->cmd_code = code;
	header->cmd_flags = 0;
	header->cmd_tag = 0;

	// apend vdb_id
	avf_u8_t *p_vdb_id = NULL;
	int len = 0;
	if (VDB_EXTRA_FLAG && vdb_id) {
		CopyVdbId(vdb_id, &p_vdb_id, &len);
		size += len;
	}

	header->ret_code = E_OK;
	header->extra_bytes = (size > VDB_MSG_SIZE) ? (size - VDB_MSG_SIZE) : 0;

	// TODO - on connection can block others
	CTCPConnection *conn;
	for (conn = mpFirstConn; conn; conn = conn->_GetNext()) {
		CVDBConnection *connection = static_cast<CVDBConnection*>(conn);
		connection->BroadcastMsg(buf, size, p_vdb_id, len);
	}
}

void CVDBServer::BroadcastRawData(string_t *vdb_id, raw_data_t *raw)
{
	AUTO_LOCK(mMutex);

	if (mpFirstConn == NULL)
		return;

	int data_type;
	switch (raw->GetFCC()) {
	case IRecordObserver::GPS_DATA:
		data_type = kRawData_GPS;
		break;

	case IRecordObserver::ACC_DATA:
		data_type = kRawData_ACC;
		break;

	case IRecordObserver::OBD_DATA:
		data_type = kRawData_OBD;
		break;

	default:
		AVF_LOGE("unknown raw data: 0x%x", raw->GetFCC());
		return;
	}

	vdb_msg_RawData_t param;
	avf_size_t size = sizeof(param) + raw->GetSize();

	param.header.magic = VDB_ACK_MAGIC;
	param.header.seqid = 0;	// fixed later
	param.header.user1 = 0;
	param.header.user2 = 0;
	param.header.cmd_code = VDB_MSG_RawData;
	param.header.cmd_flags = 0;
	param.header.cmd_tag = 0;

	// apend vdb_id
	avf_u8_t *p_vdb_id = NULL;
	int len = 0;
	if (VDB_EXTRA_FLAG && vdb_id) {
		CopyVdbId(vdb_id, &p_vdb_id, &len);
		size += len;
	}

	param.header.ret_code = E_OK;
	param.header.extra_bytes = (size > VDB_MSG_SIZE) ? (size - VDB_MSG_SIZE) : 0;

	param.data_type = data_type;
	param.data_size = raw->GetSize();

	// TODO - one connection can block others
	CTCPConnection *conn;
	for (conn = mpFirstConn; conn; conn = conn->_GetNext()) {
		CVDBConnection *connection = static_cast<CVDBConnection*>(conn);
		if (connection->mRawDataFlags & (1 << data_type)) {
			connection->BroadcastRawData(&param, raw, size, p_vdb_id, len);
		}
	}
}

void CVDBServer::BroadcastPictureTaken(string_t *picname, avf_u32_t pic_date)
{
	AUTO_LOCK(mMutex);
	CTCPConnection *conn;
	for (conn = mpFirstConn; conn; conn = conn->_GetNext()) {
		CVDBConnection *connection = static_cast<CVDBConnection*>(conn);
		connection->BroadcastPictureTaken(picname, pic_date);
	}
}

void CVDBServer::BroadcastPictureRemoved(string_t *picname)
{
	AUTO_LOCK(mMutex);
	CTCPConnection *conn;
	for (conn = mpFirstConn; conn; conn = conn->_GetNext()) {
		CVDBConnection *connection = static_cast<CVDBConnection*>(conn);
		connection->BroadcastPictureRemoved(picname);
	}
}

void CVDBServer::BroadcastSpaceInfo(string_t *vdb_id, avf_u64_t total_bytes, avf_u64_t used_bytes, avf_u64_t marked_bytes)
{
	vdb_space_info_t info;

	::memset(&info, 0, sizeof(info));
	info.total_space_lo = U64_LO(total_bytes);
	info.total_space_hi = U64_HI(total_bytes);
	info.used_space_lo = U64_LO(used_bytes);
	info.used_space_hi = U64_HI(used_bytes);
	info.protected_space_lo = U64_LO(marked_bytes);
	info.protected_space_hi = U64_HI(marked_bytes);

	AUTO_LOCK(mMutex);
	CTCPConnection *conn;
	for (conn = mpFirstConn; conn; conn = conn->_GetNext()) {
		CVDBConnection *connection = static_cast<CVDBConnection*>(conn);
		connection->BroadcastSpaceInfo(&info);
	}
}

avf_status_t CVDBServer::RefreshPictureList()
{
	AVF_LOGD(C_CYAN "RefreshPictureList" C_NONE);
	cmd_t cmd(INFO_RefreshPictureList, NULL);
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::PictureTaken(string_t *picname, avf_u32_t pic_date)
{
	AVF_LOGD(C_YELLOW "PictureTaken: %s" C_NONE, picname->string());
	cmd_t cmd(INFO_PictureTaken, NULL);
	cmd.u.PictureTaken.picname = picname->acquire();
	cmd.u.PictureTaken.pic_date = pic_date;
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::PictureRemoved(string_t *picname)
{
	AVF_LOGD(C_YELLOW "PictureRemoved: %s" C_NONE, picname->string());
	cmd_t cmd(INFO_PictureRemoved, NULL);
	cmd.u.PictureRemoved.picname = picname->acquire();
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::VdbReady(string_t *vdb_id, int status)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(INFO_VdbReady, vdb_id);
	cmd.u.VdbReady.status = status;
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::VdbUnmounted(string_t *vdb_id)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(INFO_VdbUnmounted, vdb_id);
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::ClipInfo(string_t *vdb_id, int action, ref_clip_t *ref_clip, mark_info_s *mark_info)
{
	if (GetNumConnections() <= 0)
		return E_OK;

	cmd_t cmd(INFO_ClipInfo, vdb_id);
	//AVF_LOGD("ClipInfo: %d", action);

	cmd.u.ClipInfo.action = action;
	cmd.u.ClipInfo.clip_attr = ref_clip->clip_attr;
	cmd.u.ClipInfo.list_pos = ref_clip->plist_index;
	///
	cmd.u.ClipInfo.clip_type = ref_clip->ref_clip_type;
	cmd.u.ClipInfo.clip_id = ref_clip->ref_clip_id;
	cmd.u.ClipInfo.clip_date = ref_clip->ref_clip_date;
	cmd.u.ClipInfo.clip_duration_ms = ref_clip->GetDurationLong();	// TODO
	cmd.u.ClipInfo.clip_start_time_ms = ref_clip->start_time_ms;
	///

	vdb_clip_t *clip = ref_clip->clip;
	cmd.u.ClipInfo.num_streams = clip->m_num_segs_1 > 0 ? 2 : 1;

	avf_stream_attr_s *si = cmd.u.ClipInfo.stream_info + 0;
	*si = clip->GetStreamAttr(0);

	si = cmd.u.ClipInfo.stream_info + 1;
	if (cmd.u.ClipInfo.num_streams > 1) {
		*si = clip->GetStreamAttr(1);
	} else {
		::memset(si, 0, sizeof(avf_stream_attr_s));
	}

	if (mark_info == NULL) {
		cmd.u.ClipInfo.mark_info_valid = 0;
	} else {
		cmd.u.ClipInfo.mark_info_valid = 1;
		cmd.u.ClipInfo.delay_ms = mark_info->delay_ms;
		cmd.u.ClipInfo.before_live_ms = mark_info->before_live_ms;
		cmd.u.ClipInfo.after_live_ms = mark_info->after_live_ms;
	}

	///
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::ReportSpaceInfo(string_t *vdb_id, avf_u64_t total_bytes, avf_u64_t used_bytes, avf_u64_t marked_bytes)
{
	AVF_LOGV("SpaceInfo, total: " C_CYAN "%d M" C_NONE
		", used: " C_CYAN "%d M (%d%%)" C_NONE
		", marked: " C_CYAN "%d M (%d%%)" C_NONE,
		(int)(total_bytes / 1000000),
		(int)(used_bytes / 1000000), (int)(used_bytes * 100 / total_bytes),
		(int)(marked_bytes / 1000000), (int)(marked_bytes * 100 / total_bytes));

	if (GetNumConnections() <= 0)
		return E_OK;

	cmd_t cmd(INFO_SpaceInfo, vdb_id);

	cmd.u.SpaceInfo.total_bytes = total_bytes;
	cmd.u.SpaceInfo.used_bytes = used_bytes;
	cmd.u.SpaceInfo.marked_bytes = marked_bytes;

	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::ClipRemoved(string_t *vdb_id, avf_u32_t clip_type, avf_u32_t clip_id)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(INFO_ClipRemoved, vdb_id);
	cmd.u.ClipRemoved.clip_type = clip_type;
	cmd.u.ClipRemoved.clip_id = clip_id;
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::BufferSpaceLow(string_t *vdb_id)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(INFO_BufferSpaceLow, vdb_id);
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::BufferFull(string_t *vdb_id)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(INFO_BufferFull, vdb_id);
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::SendRawData(string_t *vdb_id, raw_data_t *raw)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(INFO_SendRawData, vdb_id);
	cmd.u.SendRawData.raw = raw->acquire();
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::PlaylistAction(string_t *vdb_id, int action, avf_u32_t list_type)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(action, vdb_id);
	cmd.u.PlaylistParam.list_type = list_type;
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

avf_status_t CVDBServer::PlaylistCleared(string_t *vdb_id, avf_u32_t list_type)
{
	return PlaylistAction(vdb_id, INFO_PlaylistCleared, list_type);
}

avf_status_t CVDBServer::PlaylistCreated(string_t *vdb_id, avf_u32_t list_type)
{
	return PlaylistAction(vdb_id, INFO_PlaylistCreated, list_type);
}

avf_status_t CVDBServer::PlaylistDeleted(string_t *vdb_id, avf_u32_t list_type)
{
	return PlaylistAction(vdb_id, INFO_PlaylistDeleted, list_type);
}

avf_status_t CVDBServer::ClipAttrChanged(string_t *vdb_id, avf_u32_t clip_type, avf_u32_t clip_id, avf_u32_t clip_attr)
{
	if (GetNumConnections() <= 0)
		return E_OK;
	cmd_t cmd(INFO_ClipAttrChanged, vdb_id);
	cmd.u.ClipAttrChanged.clip_type = clip_type;
	cmd.u.ClipAttrChanged.clip_id = clip_id;
	cmd.u.ClipAttrChanged.clip_attr = clip_attr;
	return mpMsgQueue->PostMsg(&cmd, sizeof(cmd));
}

void CVDBServer::OnListScaned(int total)
{
	AVF_LOGD("OnListScanned");
	RefreshPictureList();
}

void CVDBServer::OnListCleared()
{
	AVF_LOGD("OnListCleared");
	RefreshPictureList();
}

void CVDBServer::OnPictureTaken(string_t *picname, avf_u32_t pic_date)
{
	AVF_LOGD("OnPictureTaken %s", picname->string());
	PictureTaken(picname, pic_date);
}

void CVDBServer::OnPictureRemoved(string_t *picname)
{
	AVF_LOGD("OnPictureRemoved %s", picname->string());
	PictureRemoved(picname);
}


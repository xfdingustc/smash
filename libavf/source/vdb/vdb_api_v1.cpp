

typedef struct ch_info_s {
	bool mbRecording;
	int ch;
	struct {
		avf_u64_t seg_start_pts;
		avf_u32_t duration;	// 90k
		avf_u32_t fsize;
		IAVIO *pVideoIO;	// valid when recording
	} stream[2];
	gps_range_t m_gps_range;
} ch_info_t;

struct VDB_s {
	struct rb_node rbnode;
	avf_uint_t refcount;
	avf_hash_val_t hash_val;
	char *path;
	CVDB *pVDB;

	CMutex mMutex;

	vdb_cb m_cb;
	void *m_ctx;
	camera_id_t m_camera_id;

	ch_info_t m_ch[2];

	static INLINE VDB_s* from_node(struct rb_node *node) {
		return container_of(node, VDB_s, rbnode);
	}
};

static void init_ch(ch_info_t *ci, int index)
{
	ci->mbRecording = false;
	ci->ch = index;

	ci->stream[0].seg_start_pts = 0;
	ci->stream[0].duration = 0;
	ci->stream[0].fsize = 0;
	ci->stream[0].pVideoIO = NULL;

	ci->stream[1].seg_start_pts = 0;
	ci->stream[1].duration = 0;
	ci->stream[1].fsize = 0;
	ci->stream[1].pVideoIO = NULL;

	::memset(&ci->m_gps_range, 0, sizeof(ci->m_gps_range));
}

extern "C" VDB *vdb_Open(const char *path, int create, bool *bExists)
{
	avf_hash_val_t hash_val = avf_calc_hash((const avf_u8_t*)path);

	struct rb_node **p = &g_vdb_tree.rb_node;
	struct rb_node *parent = NULL;
	VDB *vdb;

	while (*p) {
		parent = *p;
		vdb = VDB::from_node(parent);

		if (hash_val < vdb->hash_val) {
			p = &parent->rb_left;
		} else if (hash_val > vdb->hash_val) {
			p = &parent->rb_right;
		} else {
			int cmp = ::strcmp(path, vdb->path);
			if (cmp < 0) {
				p = &parent->rb_left;
			} else if (cmp > 0) {
				p = &parent->rb_right;
			} else {
				vdb->refcount++;
				*bExists = true;
				return vdb;
			}
		}
	}

	if (!avf_file_exists(path) && !create) {
		AVF_LOGE("VDB_Open: %s does not exist", path);
		return NULL;
	}

	CVDB *pVDB = CVDB::Create(NULL, NULL, true);
	if (pVDB == NULL) {
		AVF_LOGE("CVDB::Create() failed");
		return NULL;
	}

	vdb = avf_new VDB_s();
	vdb->refcount = 1;
	vdb->hash_val = hash_val;
	vdb->path = avf_strdup(path);
	vdb->pVDB = pVDB;

	init_ch(vdb->m_ch + 0, 0);
	init_ch(vdb->m_ch + 1, 1);

	vdb->m_cb = NULL;
	vdb->m_ctx = NULL;
	::memset(&vdb->m_camera_id, 0, sizeof(vdb->m_camera_id));

	rb_link_node(&vdb->rbnode, parent, p);
	rb_insert_color(&vdb->rbnode, &g_vdb_tree);

	*bExists = false;
	return vdb;
}

extern "C" void vdb_Close(VDB *vdb)
{
	rb_erase(&vdb->rbnode, &g_vdb_tree);
	vdb->pVDB->Release();
	avf_free(vdb->path);
	avf_delete vdb;
}

extern "C" VDB *VDB_Open(const char *path, int create)
{
	AUTO_LOCK(g_vdb_lock);

	bool bExists = false;
	VDB *vdb = vdb_Open(path, create, &bExists);
	if (!bExists) {
		vdb->pVDB->Load(path, true, NULL);
	}
	return vdb;
}

INLINE ch_info_t *get_ch_info(VDB *vdb, int ch)
{
	return ch == 0 ? vdb->m_ch + 0 : vdb->m_ch + 1;
}

extern "C" void vdb_EndSegment(VDB *vdb, ch_info_t *ci, int stream, bool bLast)
{
	if (ci->stream[stream].pVideoIO) {
		vdb->pVDB->FinishSegment(ci->stream[stream].duration/90, ci->stream[stream].fsize,
			stream, bLast, ci->ch);
		ci->stream[stream].duration = 0;
		ci->stream[stream].fsize = 0;
	}
}

extern "C" void vdb_StopRecord(VDB *vdb, int ch)
{
	ch_info_t *ci = get_ch_info(vdb, ch);

	if (!ci->mbRecording)
		return;

	ci->mbRecording = false;

	// stream 0
	vdb_EndSegment(vdb, ci, 0, true);
	avf_safe_release(ci->stream[0].pVideoIO);

	// stream 1
	vdb_EndSegment(vdb, ci, 1, true);
	avf_safe_release(ci->stream[1].pVideoIO);

	CVDB::vdb_record_info_t info;
	vdb->pVDB->StopRecord(&info, ch);

	if (info.valid && vdb->m_cb) {
		if (info.removed) {
			// the clip is empty and removed
			VDB_ClipRemovedMsg_t msg;
			GET_CAMID(msg.camera_id, vdb);
			msg.clip_id = info.ref_clip_id;
			vdb->m_cb(vdb->m_ctx, VDB_CB_ClipRemoved, &msg);
		} else {
			VDB_ClipFinishedMsg_t msg;
			GET_CAMID(msg.camera_id, vdb);
			msg.clip_id = MAKE_VDB_CLIP_ID(info.dirname, info.ref_clip_id);
			msg.clip_date = info.ref_clip_date;
			msg.gmtoff = info.gmtoff;
			msg.duration_ms = info.duration_ms;
			msg.gps_range = ci->m_gps_range;
			vdb->m_cb(vdb->m_ctx, VDB_CB_ClipFinished, &msg);
		}
	}
}

extern "C" void VDB_Close(VDB *vdb)
{
	AUTO_LOCK(g_vdb_lock);

	if (--vdb->refcount == 0) {
		vdb_StopRecord(vdb, 0);
		vdb_StopRecord(vdb, 1);
		vdb->pVDB->Unload();
		vdb_Close(vdb);
	}
}

extern "C" void VDB_SetCallback(VDB *vdb, vdb_cb cb, void *ctx, const char *camera_id)
{
	AUTO_LOCK(vdb->mMutex);

	vdb->m_cb = cb;
	vdb->m_ctx = ctx;
	::strncpy(vdb->m_camera_id, camera_id, sizeof(camera_id_t));
}

int vdb_StartRecord(VDB *vdb, int has_video, int has_picture, int ch)
{
	ch_info_t *ci = get_ch_info(vdb, ch);

	if (ci->mbRecording) {
		AVF_LOGE("vdb_StartRecording: already recording");
		return -1;
	}

	CVDB::vdb_record_info_t info;
	avf_status_t status = vdb->pVDB->StartRecord(&info, ch);
	if (status != E_OK) {
		return -1;
	}

	if (vdb->m_cb) {
		VDB_ClipCreatedMsg_t msg;
		GET_CAMID(msg.camera_id, vdb);
		msg.clip_id = info.ref_clip_id;
		msg.clip_date = info.ref_clip_date;
		msg.gmtoff = info.gmtoff;
		vdb->m_cb(vdb->m_ctx, VDB_CB_ClipCreated, &msg);
	}

	ci->m_gps_range.range_is_valid = 0;
	ci->mbRecording = true;

	return 0;
}

extern "C" int VDB_StartRecordEx(VDB *vdb, int has_video, int has_picture, int ch)
{
	AUTO_LOCK(vdb->mMutex);
	return vdb_StartRecord(vdb, has_video, has_picture, ch);
}

extern "C"int VDB_StopRecordEx(VDB *vdb, int ch)
{
	AUTO_LOCK(vdb->mMutex);
	vdb_StopRecord(vdb, ch);
	return 0;
}

extern "C" int vdb_add_data_l(VDB *vdb, ch_info_t *ci, upload_header_t *header)
{
	int stream = header->stream ? 1 : 0;
	int ch = ci->ch;

	switch (header->data_type) {
	case VDB_DATA_VIDEO: {
			bool bNewSeg = false;
			CVDB *pVDB = vdb->pVDB;
			avf_stream_attr_s *stream_attr = (avf_stream_attr_s*)(header + 1);

			if (ci->stream[stream].pVideoIO == NULL) {

				pVDB->InitVideoStream(stream_attr, stream, ch);

				ci->stream[stream].seg_start_pts = header->timestamp;
				ci->stream[stream].duration = 0;
				ci->stream[stream].fsize = 0;
				ci->stream[stream].pVideoIO = CBufferIO::Create();
				bNewSeg = true;

			} else {

				if (header->timestamp >= ci->stream[stream].seg_start_pts + 10*60*_90kHZ) {
					ci->stream[stream].pVideoIO->Close();
					vdb_EndSegment(vdb, ci, stream, false);
					bNewSeg = true;
				}

			}

			if (bNewSeg) {
				string_t *video_name;
				string_t *picture_name;
				avf_time_t video_gen_time;
				avf_time_t picture_gen_time;

				video_name = pVDB->CreateVideoFileName(&video_gen_time, stream, false, ch);
				picture_name = pVDB->CreatePictureFileName(&picture_gen_time, stream, false, ch);

				pVDB->StartVideo(video_gen_time, stream, ch);
				pVDB->StartPicture(picture_gen_time, stream, ch);

				ci->stream[stream].pVideoIO->CreateFile(video_name->string());
				ci->stream[stream].seg_start_pts = header->timestamp;

				video_name->Release();
				picture_name->Release();
			}

			avf_uint_t size = header->data_size - sizeof(avf_stream_attr_s);
			ci->stream[stream].pVideoIO->Write(stream_attr + 1, size);
			pVDB->AddVideo((header->timestamp - ci->stream[stream].seg_start_pts)/90, 1000,
				ci->stream[stream].fsize, stream, ch);

			ci->stream[stream].duration += header->duration;
			ci->stream[stream].fsize += size;
		}
		break;

	case VDB_DATA_JPEG: {
			CBuffer buffer;
			buffer.mpNext = NULL;
			buffer.mpData = (avf_u8_t*)(header + 1);
			buffer.mSize = header->data_size;
			buffer.m_offset = 0;
			buffer.m_data_size = buffer.mSize;
			buffer.m_dts = buffer.m_pts = header->timestamp;
			buffer.m_duration = 0;	// todo
			buffer.m_timescale = _90kHZ;
			vdb->pVDB->AddPicture(&buffer, 0, ch);	// stream 0
		}
		break;

	case RAW_DATA_GPS:
	case RAW_DATA_OBD:
	case RAW_DATA_ACC: {
			raw_data_t *raw = NULL;
			avf_u32_t fcc = 0;

			if (header->data_type == RAW_DATA_GPS) {
				gps_raw_data_v3_t *data = (gps_raw_data_v3_t*)(header + 1);

				if (!ci->m_gps_range.range_is_valid) {
					ci->m_gps_range.latitude_min = data->latitude;
					ci->m_gps_range.latitude_max = data->latitude;

					ci->m_gps_range.longitude_min = data->longitude;
					ci->m_gps_range.longitude_max = data->longitude;

					ci->m_gps_range.altitude_min = data->altitude;
					ci->m_gps_range.altitude_max = data->altitude;
				} else {
					if (ci->m_gps_range.latitude_min > data->latitude)
						ci->m_gps_range.latitude_min = data->latitude;
					if (ci->m_gps_range.latitude_max < data->latitude)
						ci->m_gps_range.latitude_max = data->latitude;

					if (ci->m_gps_range.longitude_min > data->longitude)
						ci->m_gps_range.longitude_min = data->longitude;
					if (ci->m_gps_range.longitude_max < data->longitude)
						ci->m_gps_range.longitude_max = data->longitude;

					if (ci->m_gps_range.altitude_min > data->altitude)
						ci->m_gps_range.altitude_min = data->altitude;
					if (ci->m_gps_range.altitude_max < data->altitude)
						ci->m_gps_range.altitude_max = data->altitude;
				}
			}

			switch (header->data_type) {
			case RAW_DATA_GPS: fcc = IRecordObserver::GPS_DATA; break;
			case RAW_DATA_OBD: fcc = IRecordObserver::OBD_DATA; break;
			case RAW_DATA_ACC: fcc = IRecordObserver::ACC_DATA; break;
			default: break;
			}

			if (fcc) {
				raw = avf_alloc_raw_data(header->data_size, fcc);
				::memcpy(raw->GetData(), header + 1, header->data_size);

				vdb->pVDB->AddRawData(header->timestamp, raw, 0, ch); // stream 0
				raw->Release();
			}
		}
		break;

	default:
		AVF_LOGE("VDB_AddData: unknown type: 0x%x", header->data_type);
		return -1;
	}

	return 0;
}

extern "C" int VDB_AddDataEx(VDB *vdb, const uint8_t *data, int size, int ch)
{
	AUTO_LOCK(vdb->mMutex);

	ch_info_t *ci = get_ch_info(vdb, ch);

	if (!ci->mbRecording) {
		AVF_LOGE("VDB_AddData: not recording");
		return -1;
	}

	bool has_video = false;
	bool has_picture = false;
	avf_size_t live_length = vdb->pVDB->s_GetLiveClipLength(has_video, has_picture, ch);
	if (live_length >= 3600*1000) {
		vdb_StopRecord(vdb, ch);
		vdb_StartRecord(vdb, has_video, has_picture, ch);
	}

	if (!ci->mbRecording) {
		AVF_LOGE("VDB_AddData: not recording(2)");
		return -1;
	}

	upload_header_t *header = (upload_header_t*)data;
	while (header->data_type != __RAW_DATA_END) {
		// check version
		if (header->extra.version != UPLOAD_VERSION) {
			AVF_LOGE("VDB_AddData: version should be %d", UPLOAD_VERSION);
			return -1;
		}

		int rval = vdb_add_data_l(vdb, ci, header);
		if (rval < 0)
			return rval;

		header = go_next_header(header);
	}

	return 0;
}



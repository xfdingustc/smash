
struct ch_info_s
{
	int const ch;
	IClipWriter *mpClipWriter;

	ch_info_s(int _ch);
	~ch_info_s();
};

ch_info_s::ch_info_s(int _ch):
	ch(_ch),
	mpClipWriter(NULL)
{
}

ch_info_s::~ch_info_s()
{
	avf_safe_release(mpClipWriter);
}

struct VDB_s
{
	struct rb_node rbnode;
	avf_int_t refcount;

	avf_hash_val_t hash_val;	// hash of path
	char *path;

	CMutex mMutex;
	CVDB *const pVDB;

	vdb_cb m_cb;
	void *m_ctx;
	camera_id_t m_camera_id;

	// one for live, another for uploading
	ch_info_s m_ch_0;
	ch_info_s m_ch_1;

	gps_range_t gps_range;

	VDB_s(CVDB *_pVDB): pVDB(_pVDB), m_ch_0(0), m_ch_1(1) {
		::memset(&gps_range, 0, sizeof(gps_range));
	}

	static INLINE VDB_s* from_node(struct rb_node *node) {
		return container_of(node, VDB_s, rbnode);
	}

	INLINE ch_info_s *get_ch_info(int ch) {
		if ((unsigned)ch >= 2) {
			AVF_LOGE("bad ch %d", ch);
			return NULL;
		}
		return ch == 0 ? &m_ch_0 : &m_ch_1;
	}
};

int vdb_stop_record_l(VDB *vdb, int ch);

extern "C" VDB *vdb_open_l(const char *path, int create, bool *bExisted)
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
				*bExisted = true;
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

	vdb = avf_new VDB_s(pVDB);
	vdb->refcount = 1;

	vdb->hash_val = hash_val;
	vdb->path = avf_strdup(path);

	vdb->m_cb = NULL;
	vdb->m_ctx = NULL;
	::memset(&vdb->m_camera_id, 0, sizeof(vdb->m_camera_id));

	rb_link_node(&vdb->rbnode, parent, p);
	rb_insert_color(&vdb->rbnode, &g_vdb_tree);

	*bExisted = false;
	return vdb;
}

// open/create and load vdb
VDB *VDB_Open(const char *path, int create)
{
	AUTO_LOCK(g_vdb_lock);

	bool bExisted = false;
	VDB *vdb = vdb_open_l(path, create, &bExisted);
	if (vdb && !bExisted) {
		vdb->pVDB->Load(NULL, path, true, NULL);
	}

	return vdb;
}

extern "C" void vdb_close_l(VDB *vdb)
{
	vdb->pVDB->Release();

	rb_erase(&vdb->rbnode, &g_vdb_tree);
	avf_free(vdb->path);

	avf_delete vdb;
}

// unload and close vdb
void VDB_Close(VDB *vdb)
{
	AUTO_LOCK(g_vdb_lock);

	if (--vdb->refcount == 0) {
		AUTO_LOCK(vdb->mMutex);
		vdb_stop_record_l(vdb, 0);
		vdb_stop_record_l(vdb, 1);
	} else {
		return;
	}

	vdb->pVDB->Unload();
	vdb_close_l(vdb);
}

void VDB_SetCallback(VDB *vdb, vdb_cb cb, void *ctx, const char *camera_id)
{
	AUTO_LOCK(vdb->mMutex);
	vdb->m_cb = cb;
	vdb->m_ctx = ctx;
	::strncpy(vdb->m_camera_id, camera_id, sizeof(camera_id_t));
}

void vdb_clip_writer_cb(void *context, int cbid, void *param, int size)
{
	VDB *vdb = (VDB*)context;
	switch (cbid) {
	case IClipWriter::CB_ID_StartRecord:
		AVF_LOGD("CB_ID_StartRecord");
		if (vdb->m_cb) {
			CVDB::vdb_record_info_t *info = (CVDB::vdb_record_info_t*)param;
			VDB_ClipCreatedMsg_t msg;
			GET_CAMID(msg.camera_id, vdb);
			msg.clip_id = info->ref_clip_id;
			msg.clip_date = info->ref_clip_date;
			msg.gmtoff = info->gmtoff;
			vdb->m_cb(vdb->m_ctx, VDB_CB_ClipCreated, &msg);
		}
		break;
	case IClipWriter::CB_ID_StopRecord:
		AVF_LOGD("CB_ID_StopRecord");
		if (vdb->m_cb) {
			CVDB::vdb_record_info_t *info = (CVDB::vdb_record_info_t*)param;
			if (info->removed) {
				// the clip is empty and removed
				VDB_ClipRemovedMsg_t msg;
				GET_CAMID(msg.camera_id, vdb);
				msg.clip_id = MAKE_VDB_CLIP_ID(info->dirname, info->ref_clip_id);
				vdb->m_cb(vdb->m_ctx, VDB_CB_ClipRemoved, &msg);
			} else {
				VDB_ClipFinishedMsg_t msg;
				GET_CAMID(msg.camera_id, vdb);
				msg.clip_id = MAKE_VDB_CLIP_ID(info->dirname, info->ref_clip_id);
				msg.clip_date = info->ref_clip_date;
				msg.gmtoff = info->gmtoff;
				msg.duration_ms = info->duration_ms;
				msg.gps_range = vdb->gps_range;
				vdb->m_cb(vdb->m_ctx, VDB_CB_ClipFinished, &msg);
			}
		}
		break;
	case IClipWriter::CB_ID_GPS_DATA:
		if (size == sizeof(gps_raw_data_v3_t)) {
			gps_raw_data_v3_t *data = (gps_raw_data_v3_t*)param;
			if (vdb->gps_range.range_is_valid) {
				vdb->gps_range.latitude_min = data->latitude;
				vdb->gps_range.latitude_max = data->latitude;

				vdb->gps_range.longitude_min = data->longitude;
				vdb->gps_range.longitude_max = data->longitude;

				vdb->gps_range.altitude_min = data->altitude;
				vdb->gps_range.altitude_max = data->altitude;
			} else {
				if (vdb->gps_range.latitude_min > data->latitude)
					vdb->gps_range.latitude_min = data->latitude;
				if (vdb->gps_range.latitude_max < data->latitude)
					vdb->gps_range.latitude_max = data->latitude;

				if (vdb->gps_range.longitude_min > data->longitude)
					vdb->gps_range.longitude_min = data->longitude;
				if (vdb->gps_range.longitude_max < data->longitude)
					vdb->gps_range.longitude_max = data->longitude;

				if (vdb->gps_range.altitude_min > data->altitude)
					vdb->gps_range.altitude_min = data->altitude;
				if (vdb->gps_range.altitude_max < data->altitude)
					vdb->gps_range.altitude_max = data->altitude;
			}
		}
		break;
	default:
		AVF_LOGW("unknown cbid: %d", cbid);
		break;
	}
}

int VDB_StartRecordEx(VDB *vdb, int has_video, int has_picture, int ch)
{
	AUTO_LOCK(vdb->mMutex);

	ch_info_s *ci = vdb->get_ch_info(ch);
	if (ci == NULL)
		return E_ERROR;

	if (ci->mpClipWriter) {
		AVF_LOGE("ch %d already recording", ch);
		return E_ERROR;
	}

	ci->mpClipWriter = vdb->pVDB->CreateClipWriter();
	if (ci->mpClipWriter == NULL) {
		AVF_LOGE("cannot create clip writer");
		return E_ERROR;
	}

	ci->mpClipWriter->SetCallback(vdb_clip_writer_cb, (void*)vdb);

	avf_status_t status = ci->mpClipWriter->StartRecord(ch);
	if (status != E_OK) {
		avf_safe_release(ci->mpClipWriter);
		return status;
	}

	return E_OK;
}

int vdb_stop_record_l(VDB *vdb, int ch)
{
	ch_info_s *ci = vdb->get_ch_info(ch);
	if (ci == NULL)
		return E_ERROR;

	if (ci->mpClipWriter) {
		ci->mpClipWriter->StopRecord();
		avf_safe_release(ci->mpClipWriter);
	}

	return E_OK;
}

int VDB_StopRecordEx(VDB *vdb, int ch)
{
	AUTO_LOCK(vdb->mMutex);
	return vdb_stop_record_l(vdb, ch);
}

int VDB_AddDataEx(VDB *vdb, const uint8_t *data, int size, int ch)
{
	AUTO_LOCK(vdb->mMutex);

	ch_info_s *ci = vdb->get_ch_info(ch);
	if (ci == NULL)
		return E_ERROR;

	if (ci->mpClipWriter == NULL) {
		AVF_LOGE("not recording");
		return E_ERROR;
	}

	return ci->mpClipWriter->WriteData(data, size);
}


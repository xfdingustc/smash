
#include "mms_if.h"

#define MMS_RTSP_PATH		"/tmp/mms-1/"
#define MMS_RTSP_STREAM		1

class CCameraControl:
	public CObject,
	public IActiveObject,
	public ICameraControl
{
	typedef CObject inherited;
	typedef IActiveObject AO;
	DEFINE_INTERFACE;

public:
	static CCameraControl *Create(CCameraState *pCameraState);

private:
	CCameraControl(CCameraState *pCameraState);
	virtual ~CCameraControl();

public:
	enum {
		STATE_IDLE,
		STATE_PREVIEW,
		STATE_RECORDING,
		STATE_ERROR,
	};

// IActiveObject
public:
	virtual const char *GetAOName() { return "camcontrol"; }
	virtual void AOLoop();
	virtual bool AOCmdProc(const CMD& cmd, bool bInAOLoop);

// ICameraControl
public:
	virtual avf_status_t StartRecord() {
		return CmdSetState(STATE_RECORDING);
	}

	virtual avf_status_t StopRecord() {
		return CmdSetState(STATE_PREVIEW);
	}

	virtual avf_status_t EnterIdle() {
		return CmdSetState(STATE_IDLE);
	}

	virtual avf_status_t ChangeVideoResolution(int resolution) {
		for (int i = 0; i < (int)ARRAY_SIZE(g_video_config); i++) {
			if ((int)g_video_config[i].std_resolution == resolution) {
				AVF_LOGI(C_YELLOW "change video resolution to %s" C_NONE,
					g_video_config[i].name);
				return CmdChangeVideoConfig(i);
			}
		}
		AVF_LOGE("video resolution %d not found", resolution);
		return E_ERROR;
	}

	virtual avf_status_t ChangeVideoQuality(int quality) {
		if (quality >= 0 && quality < Video_Quality_num) {
			AVF_LOGI(C_YELLOW "change video quality to %d" C_NONE, quality);
			return CmdChangeVideoQuality((enum Video_Quality)quality);
		}
		AVF_LOGE("video quality %d is invalid", quality);
		return E_ERROR;
	}

	virtual avf_status_t ChangeRecMode(int value) {
		return Post(CMD_ChangeRecMode, value);
	}

	virtual avf_status_t ChangeColorMode(int value) {
		return Post(CMD_ChangeColorMode, value);
	}

	virtual avf_status_t ChangeOverlayFlags(int value) {
		return Post(CMD_ChangeOverlayFlags, value);
	}

	virtual avf_status_t ChangeCameraName(string_t *name) {
		return Post(CMD_ChangeCameraName, (avf_intptr_t)name->acquire());
	}

	virtual avf_status_t NetworkSyncTime(uint32_t time, int timezone) {
		return Post(CMD_NetworkChangeTime, time, timezone);
	}

	virtual avf_status_t SetStillCaptureMode(bool bStillMode) {
		return CmdSetStillCaptureMode(bStillMode);
	}

	virtual avf_status_t StartStillCapture(bool bOneShot) {
		return CmdStillCapture(bOneShot ? 
			STILLCAP_ACTION_SINGLE : STILLCAP_ACTION_BURST);
	}

	virtual avf_status_t StopStillCapture() {
		return CmdStillCapture(STILLCAP_ACTION_STOP);
	}

	virtual avf_status_t WantIdle() {
		return Post(CMD_WantIdle);
	}

	virtual avf_status_t WantPreview() {
		return Post(CMD_WantPreview);
	}

	virtual avf_status_t ChangeLCDBrightness(int value) {
		return Post(CMD_ChangeLCDBrightness, value);
	}

	virtual avf_status_t FlipLCDVideo(int flip) {
		return Post(CMD_FlipLCDVideo, flip);
	}

	virtual avf_status_t FlipLCDOSD(int flip) {
		return Post(CMD_FlipLCDOSD, flip);
	}

public:
	avf_status_t CmdSetState(int state) {
		return Post(CMD_SetState, state);
	}

	avf_status_t CmdChangeVideoConfig(int index) {
		return Post(CMD_ChangeVideoConfig, index);
	}

	avf_status_t CmdChangeVideoQuality(enum Video_Quality quality) {
		return Post(CMD_ChangeVideoQuality, quality);
	}

	avf_status_t CmdChangePreviewConfig(int index) {
		return Post(CMD_ChangePreviewConfig, index);
	}

	avf_status_t CmdChangeSecondVideoConfig(int index) {
		return Post(CMD_ChangeSecondVideoConfig, index);
	}

	avf_status_t CmdSetStillCaptureMode(bool bStillMode) {
		return Post(CMD_SetStillCaptureMode, bStillMode);
	}

	avf_status_t CmdStillCapture(int action) {
		return Post(CMD_StillCapture, action);
	}

	avf_status_t CmdSetCaptureRaw(int raw) {
		return Post(CMD_CaptureRaw, raw);
	}

	avf_status_t CmdUnmountTF() {
		return Post(CMD_UnmountTF);
	}

	avf_status_t CmdMountTF() {
		return Post(CMD_MountTF);
	}

	avf_status_t CmdMarkLiveClip(int before_live_ms, int after_live_ms) {
		return Post(CMD_MarkLiveClip, before_live_ms, after_live_ms);
	}

	avf_status_t CmdStopMarkLiveClip() {
		return Post(CMD_StopMarkLiveClip);
	}

	avf_status_t CmdSetSceneData(raw_data_t *raw) {
		raw->AddRef();
		return Post(CMD_SetSceneData, (avf_intptr_t)raw);
	}

	avf_status_t CmdSetMirrorHorz(bool bToggle, bool bMirrorHorz) {
		return Post(CMD_SetMirrorHorz, bToggle, bMirrorHorz);
	}

	avf_status_t CmdSetMirrorVert(bool bToggle, bool bMirrorVert) {
		return Post(CMD_SetMirrorVert, bToggle, bMirrorVert);
	}

	avf_status_t CmdChangeLCDVideo(int lb) {
		return Post(CMD_ChangeLCDVideo, lb, 0);
	}

	avf_status_t CmdChangeLCDVideo(int xoff, int yoff, int video_width, int video_height) {
		video_rect_t *rect = avf_malloc_type(video_rect_t);
		rect->xoff = xoff;
		rect->yoff = yoff;
		rect->width = video_width;
		rect->height = video_height;
		return Post(CMD_ChangeLCDVideo_r, (avf_intptr_t)rect);
	}

	avf_status_t NotifyTFStateChanged() {
		return Post(MSG_TFStateChanged);
	}

	int GetLCDBrightness() {
		int brightness = -1;
		if (avf_display_get_brightness(mpMedia, VOUT_0, VOUT_TYPE_MIPI, &brightness) < 0) {
			AVF_LOGE("avf_display_get_brightness failed");
		}
		return brightness;
	}

private:
	typedef struct video_rect_s {
		int xoff;
		int yoff;
		int width;
		int height;
	} video_rect_t;

private:
	static void avf_media_event_callback(int type, void *pself);
	static void avf_media_on_disconnected_callback(int error, void *pself);

	int set_config_string(const char *key, int id, const char *value) {
		return avf_media_set_config_string(mpMedia, key, id,value);
	}

	int set_config_int32(const char *key, int id, int32_t value) {
		return avf_media_set_config_int32(mpMedia, key, id, value);
	}

	int set_config_bool(const char *key, int id, int value) {
		return avf_media_set_config_bool(mpMedia, key, id, value);
	}

	int get_record_flag(bool bRecord) {
		if (bRecord) {
			return opt_nowrite ? 2 : 1;
		} else {
			return 0;
		}
	}

private:
	enum {
		// cmds
		CMD_Null,
		CMD_SetState,
		CMD_ChangeVideoConfig,
		CMD_ChangeVideoQuality,
		CMD_ChangePreviewConfig,
		CMD_ChangeSecondVideoConfig,
		CMD_ChangeRecMode,
		CMD_ChangeColorMode,
		CMD_ChangeOverlayFlags,
		CMD_ChangeCameraName,
		CMD_NetworkChangeTime,
		CMD_SetStillCaptureMode,
		CMD_StillCapture,
		CMD_CaptureRaw,
		CMD_UnmountTF,
		CMD_MountTF,
		CMD_MarkLiveClip,
		CMD_StopMarkLiveClip,
		CMD_SetSceneData,
		CMD_SetMirrorHorz,
		CMD_SetMirrorVert,
		CMD_ChangeLCDVideo,
		CMD_ChangeLCDVideo_r,
		CMD_WantIdle,
		CMD_WantPreview,
		CMD_ChangeLCDBrightness,
		CMD_FlipLCDVideo,
		CMD_FlipLCDOSD,

		// msgs
		MSG_AvfServerError,

		MSG_MediaStateChanged,
		MSG_TFStateChanged,
		MSG_BufferSpaceFull,	// tf is full
		MSG_VdbStateChanged,
		MSG_DiskError,
		MSG_DiskSlow,

		MSG_StillCaptureStateChanged,
		MSG_PictureTaken,
		MSG_PictureListChanged,

		MSG_MMS_Mode1,
		MSG_MMS_Mode2,
	};

	typedef struct cmd_msg_s {
		int code;
		avf_intptr_t p1;
		avf_intptr_t p2;

		cmd_msg_s() {
			this->code = 0;
			this->p1 = 0;
			this->p2 = 0;
		}

		cmd_msg_s(int code, avf_intptr_t p1, avf_intptr_t p2) {
			this->code = code;
			this->p1 = p1;
			this->p2 = p2;
		}

	} cmd_msg_t;

	avf_status_t Post(cmd_msg_t& cmd) {
		return mpCmdMsgQ->PostMsg(&cmd, sizeof(cmd));
	}

	avf_status_t Post(int code) {
		cmd_msg_t cmd(code, 0, 0);
		return Post(cmd);
	}

	avf_status_t Post(int code, avf_intptr_t p1) {
		cmd_msg_t cmd(code, p1, 0);
		return Post(cmd);
	}

	avf_status_t Post(int code, avf_intptr_t p1, avf_intptr_t p2) {
		cmd_msg_t cmd(code, p1, p2);
		return Post(cmd);
	}

	void ProcessCmdMsg(cmd_msg_t& cmdmsg);
	int OpenCamera();
	int CloseCamera();
	int RunCamera(bool bRecord);
	int StopCamera();

	int SetOverlay();
	bool ShouldEnableSecondVideo();
	int ConfigCameraParams(bool bRecord);

private:
	void StartRTSP();
	void StopRTSP();

private:
	int DoEnterIdle(bool bTry);
	int DoEnterPreview(bool bTry);
	int DoStartRecord();

	void EnterIdleIfError() {
		if (mState == STATE_ERROR) {
			DoEnterIdle(false);
		}
	}

	int CheckSpaceForRecord();

	int DoChangeVideoConfig(int index);
	int DoChangeVideoQuality(enum Video_Quality quality);
	int DoChangePreviewConfig(int index);
	int DoChangeSecondVideoConfig(int index);

	int DoChangeRecMode(int value);
	int DoChangeColorMode(int value);
	int DoChangeLCDBrightness(int value);
	int DoFlipLCDVideo(int flip);
	int DoFlipLCDOSD(int flip);
	int DoChangeOverlayFlags(int value);
	int DoChangeCameraName(string_t *name);
	int DoNetworkChangeTime(uint32_t time, int timezone);

	int DoSetStillCaptureMode(bool bStillMode);
	int DoStillCapture(int action);
	int DoSetCaptureRaw(int raw);
	int DoUnmountTF();
	int DoMountTF();
	int DoMarkLiveClip(int before_live_ms, int after_live_ms);
	int DoStopMarkLiveClip();
	int DoSetSceneData(raw_data_t *raw);
	int DoSetMirrorHorz(int toggle, int mirror_horz);
	int DoSetMirrorVert(int toggle, int mirror_vert);
	int ShowLCDInfo(const char *msg);
	int DoChangeLCDVideo(int lb);
	int DoChangeLCDVideo(video_rect_t *r);

	int ConfigChanged(bool bSwitchStillMode = false);
	bool FetchVdbSpace(bool bPrint);

private:
	void HandleMediaStateChanged();
	void HandleTFStateChanged();
	void HandleBufferSpaceFull();
	void HandleVdbStateChanged(bool bPrint);
	void HandleDiskError();
	void HandleDiskSlow();
	void ConfigVin(const char *video_mode, const char *fps, int bits, int hdr_mode, int enc_mode);
	void ConfigVout();
	const vout_video_config_t *GetLCDVideoConfig();

private:
	void HandleStillCaptureStateChanged();
	void HandlePictureTaken();
	void UpdateTotalNumPictures();
	void HandleMMsMode1();

private:
	void RecordModeChanged();
	void RecordStateChanged(enum State_Rec state);
	void VideoResolutionChanged();
	void VideoQualityChanged();
	void StorageStateChanged();
	void StorageSpaceInfoChanged(bool bFetch);
	void SetAutoDelFlag();
	void SetNoDeleteFlag();

private:
	CCameraState *mpCameraState;
	CWorkQueue *mpWorkQ;
	CMsgQueue *mpCmdMsgQ;
	avf_media_t *mpMedia;
	int mState;
	///
	bool mbTFReady;
	bool mbVdbReady;
	///
	int mVideoConfig;		// main video config index
	enum Video_Quality mVideoQuality;
	int mOverlayFlags;
	bool mbStillMode;
	bool mbIdle;	// init state be IDLE
	int mPreviewConfig;	// mjpeg preview config index
	int mSecVideoConfig;	// second video config index
	int m_mirror_horz;
	int m_mirror_vert;
	int m_flip_horz;
	int m_flip_vert;
	int m_anti_flicker;
	int m_osd_flip_horz;
	int m_osd_flip_vert;
	///
	bool mb_auto_record;
	bool mb_auto_delete;
	bool mb_no_delete;
	bool mb_raw;
	///
	bool mb_force_enc_mode;
	int m_enc_mode;
	///
	avf_vdb_info_t mVdbInfo;
	///
#ifdef RTSP
	IMMSource *mp_rtsp_source;
	IMMRtspServer *mp_rtsp_server;
#endif
	int dummy;
};

//============================================================

CCameraControl* CCameraControl::Create(CCameraState *pCameraState)
{
	CCameraControl *result = avf_new CCameraControl(pCameraState);
	CHECK_STATUS(result);
	return result;
}

CCameraControl::CCameraControl(CCameraState *pCameraState):

	mpCameraState(pCameraState),
	mpWorkQ(NULL),
	mpCmdMsgQ(NULL),
	mpMedia(NULL),
	mState(STATE_IDLE),

	mbTFReady(false),
	mbVdbReady(false),

	mVideoQuality(Video_Quality_Super),
	mOverlayFlags(opt_overlay_flags),
	mbStillMode(opt_still_mode),
	mbIdle(opt_idle_state),

	m_mirror_horz(opt_mirror_horz),
	m_mirror_vert(opt_mirror_vert),
	m_flip_horz(opt_flip_horz),
	m_flip_vert(opt_flip_vert),
	m_anti_flicker(opt_anti_flicker),
	m_osd_flip_horz(opt_osd_flip_horz),
	m_osd_flip_vert(opt_osd_flip_horz),

	mb_auto_record(opt_auto_record),
	mb_auto_delete(opt_auto_delete),
	mb_no_delete(opt_no_delete),
	mb_raw(opt_raw),

	mb_force_enc_mode(opt_force_enc_mode),
	m_enc_mode(opt_enc_mode),
#ifdef RTSP
	mp_rtsp_source(NULL),
	mp_rtsp_server(NULL),
#endif
	dummy(0)
{
	// -------------------------------------------------------
	// preview config
	// -------------------------------------------------------
	if (opt_preview_config >= 0) {
		mPreviewConfig = opt_preview_config;
	} else {
		mPreviewConfig = 0;	// default
	}

	// -------------------------------------------------------
	// video config & second video config
	// -------------------------------------------------------
#ifdef ARCH_S2L

	if (opt_video_config >= 0) {
		mVideoConfig = opt_video_config;
	} else {
		mVideoConfig = find_video_config("1080p60");
	}

	if (opt_sec_video_config >= 0) {
		mSecVideoConfig = opt_sec_video_config;
	} else {
		mSecVideoConfig = 0;	// default
	}

#else

	if (opt_video_config >= 0) {
		mVideoConfig = opt_video_config;
	} else {
		mVideoConfig = find_video_config("1080p30");
	}

	if (opt_sec_video_config >= 0) {
		mSecVideoConfig = opt_sec_video_config;
	} else {
		mSecVideoConfig = find_sec_video_config("512x288", opt_config);
	}

#endif

	// must be created before avf_media_create(), because it may post msgs immediately
	CREATE_OBJ(mpWorkQ = CWorkQueue::Create(static_cast<IActiveObject*>(this)));
	CREATE_OBJ(mpCmdMsgQ = CMsgQueue::Create("CmdMsgQ", sizeof(avf_msg_t), 4));
	mpWorkQ->AttachMsgQueue(mpCmdMsgQ, CMsgQueue::Q_INPUT);

#ifdef RTSP
	CREATE_OBJ(mp_rtsp_source = mms_create_source(MMS_RTSP_PATH, IMMSource::MODE_TS));
	CREATE_OBJ(mp_rtsp_server = mms_create_rtsp_server(mp_rtsp_source));
	if ((mStatus = mp_rtsp_server->Start()) != E_OK) {
		return;
	}
	avf_remove_dir_recursive(MMS_RTSP_PATH, false);
	avf_safe_mkdir(MMS_RTSP_PATH);
#endif

	mpMedia = avf_media_create_ex(avf_media_event_callback, avf_media_on_disconnected_callback, (void*)this);
	if (mpMedia == NULL) {
		AVF_LOGE("avf_media_create() failed, make sure avf_media_server is started.");
		mStatus = E_ERROR;
		return;
	}

	// vdb.fs
	avf_media_set_config_string(mpMedia, NAME_VDB_FS, AVF_GLOBAL_CONFIG, opt_vdbfs);

	// logo
	avf_media_set_config_int32(mpMedia, NAME_INTERNAL_LOGO, AVF_GLOBAL_CONFIG|0, !opt_nologo);		// main stream
	avf_media_set_config_int32(mpMedia, NAME_INTERNAL_LOGO, AVF_GLOBAL_CONFIG|1, !opt_nologo);		// secondary stream
	avf_media_set_config_int32(mpMedia, NAME_INTERNAL_LOGO, AVF_GLOBAL_CONFIG|2, !opt_nologo);		// preview stream

#ifdef BOARD_HELLFIRE
	// init picture list service
	char buffer[REG_STR_MAX];
	sprintf(buffer, "%s/DCIM/", opt_vdbfs);
	avf_media_enable_picture_service(mpMedia, buffer);
#endif

	pCameraState->SetRawMode(mb_raw);

	mpWorkQ->Run();
}

CCameraControl::~CCameraControl()
{
	if (mpWorkQ) {
		// stop the workQ first, so when destorying avf media,
		// we won't process any ipc requests
		mpWorkQ->Stop();
	}

	// destroy the media object and wait its thread finished,
	// so we can safely release other objects
	if (mpMedia) {
		DoEnterIdle(false);
		avf_media_destroy(mpMedia);
	}

#ifdef RTSP
	if (mp_rtsp_source) {
		mp_rtsp_source->Close();
	}
	if (mp_rtsp_server) {
		mp_rtsp_server->Stop();
		mp_rtsp_server->Release();
	}
	if (mp_rtsp_source) {
		mp_rtsp_source->Release();
	}
#endif

	// clear the cmd/msg Q and detach it
	if (mpWorkQ) {
		if (mpCmdMsgQ) {
			mpCmdMsgQ->ClearMsgQ(NULL, NULL);
			mpWorkQ->DetachMsgQueue(mpCmdMsgQ);
		}
	}

	avf_safe_release(mpCmdMsgQ);
	avf_safe_release(mpWorkQ);
}

void *CCameraControl::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IActiveObject)
		return static_cast<IActiveObject*>(this);
	return inherited::GetInterface(refiid);
}

void CCameraControl::AOLoop()
{
	mpWorkQ->ReplyCmd(E_OK);

	// if set clock mode when camera config changes.
	avf_media_set_config_bool(mpMedia, "config.clockmode.enable", AVF_GLOBAL_CONFIG, opt_setclock);

	// open HDMI or LCD
	ConfigVout();

	if (!mbIdle) {
		DoEnterPreview(false);
	}

	// init ccam state
	RecordModeChanged();
	VideoResolutionChanged();
	VideoQualityChanged();

	// init ccam state
	switch (mState) {
	default:
	case STATE_IDLE:
	case STATE_PREVIEW:
		RecordStateChanged(State_Rec_stopped);
		break;
	case STATE_RECORDING:
		RecordStateChanged(State_Rec_recording);
		break;
	case STATE_ERROR:
		RecordStateChanged(State_Rec_error);
		break;
	}

	HandleTFStateChanged();
	HandleVdbStateChanged(false);

	if (opt_nowrite && mb_auto_record) {
		AVF_LOGI(C_YELLOW "auto start record" C_NONE);
		DoStartRecord();
	}

	while (true) {
		CMsgHub::MsgResult result;
		if (!mpWorkQ->WaitMsg(result, CMsgQueue::Q_CMD | CMsgQueue::Q_INPUT, 5*1000)) {
			if (mbVdbReady) {
				// broadcast space info every 5 second
				StorageSpaceInfoChanged(true);
			}
			continue;
		}

		if (result.flag >= 0) {
			// no event flags are used
			continue;
		}

		if (result.pMsgQ) {

			if (mpWorkQ->IsCmdQ(result.pMsgQ)) {

				AO::CMD cmd;
				if (mpWorkQ->PeekCmd(cmd)) {
					if (!AOCmdProc(cmd, true)) {
						// returning false means stop AOLoop
						return;
					}
				}

			} else if (result.pMsgQ == mpCmdMsgQ) {

				cmd_msg_t cmdmsg;
				if (mpCmdMsgQ->PeekMsg((void*)&cmdmsg, sizeof(cmdmsg))) {
					ProcessCmdMsg(cmdmsg);
				}

			} else {
				AVF_LOGE("unknown msg queue: %s", result.pMsgQ->GetName());
			}
		}
	}
}

bool CCameraControl::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	switch (cmd.code) {
	case AO::CMD_STOP:
		mpWorkQ->ReplyCmd(E_OK);
		return false;
	default:
		mpWorkQ->ReplyCmd(E_UNKNOWN);
		return true;
	}
}

const vout_video_config_t *CCameraControl::GetLCDVideoConfig()
{
	const vout_table_t *pVoutTable = find_vout_table_item(opt_lcd_mode);
	if (pVoutTable == NULL) {
		AVF_LOGE("no such mode %d", opt_lcd_mode);
		return NULL;
	}

	const vout_video_config_t *pVideoConfig = NULL;
	if (opt_video_letterboxed) {
		// find video config according to aspect ratio
		int ar = g_video_config[mVideoConfig].aspect_ratio;
		pVideoConfig = pVoutTable->video_config_lb + ar;
	} else {
		pVideoConfig = pVoutTable->video_config_ps;
	}

	return pVideoConfig;
}

void CCameraControl::ConfigVout()
{
	if (opt_hdmi_mode == -2 && opt_lcd_mode == -2) {
		// do nothing
		return;
	}

	// enter IDLE so we can setup VOUT
	avf_display_set_mode(mpMedia, MODE_IDLE);

	int flip = 0;
	if (m_flip_horz) {
		flip = m_flip_vert ? VOUT_FLIP_HV : VOUT_FLIP_H;
	} else {
		flip = m_flip_horz ? VOUT_FLIP_V : VOUT_FLIP_NORMAL;
	}
	int osd_flip = 0;
	//S2L only support VOUT_FLIP_HV & VOUT_FLIP_NORMAL
	if (m_osd_flip_horz) {
		osd_flip = m_osd_flip_vert ? VOUT_FLIP_HV : VOUT_FLIP_H;
	} else {
		osd_flip = m_osd_flip_horz ? VOUT_FLIP_V : VOUT_FLIP_NORMAL;
	}

	if (opt_hdmi_mode >= 0) {
		vout_config_t config = {0};
		config.vout_id = VOUT_1;
		config.vout_type = VOUT_TYPE_HDMI;
		config.vout_mode = opt_hdmi_mode;
		config.enable_video = 1;
		config.video_flip = flip;
		config.osd_flip = osd_flip;
		config.fb_id = opt_fb_id;
		avf_display_set_vout(mpMedia, &config, NULL);
	} else if (opt_hdmi_mode == -1) {
		avf_display_disable_vout(mpMedia, VOUT_1);
	}

	if (opt_lcd_mode >= 0) {
		vout_config_t config = {0};
		config.vout_id = VOUT_0;
		config.vout_type = VOUT_TYPE_MIPI;//VOUT_TYPE_DIGITAL;
		config.vout_mode = opt_lcd_mode;
		config.enable_video = 1;
		config.video_flip = flip;
		config.osd_flip = osd_flip;
		config.fb_id = opt_fb_id;

		const vout_video_config_t *pVideoConfig = GetLCDVideoConfig();
		if (pVideoConfig != NULL) {
			config.video_size_flag = 1;
			config.video_width = pVideoConfig->video_width;
			config.video_height = pVideoConfig->video_height;
			config.video_offset_flag = 1;
			config.video_offset_x = pVideoConfig->video_off_x;
			config.video_offset_y = pVideoConfig->video_off_y;
		}

		avf_display_set_vout(mpMedia, &config, NULL);
		avf_display_set_brightness(mpMedia, VOUT_0, VOUT_TYPE_MIPI, 
			mpCameraState->GetLCDBrightness());

	} else if (opt_lcd_mode == -1) {
		avf_display_disable_vout(mpMedia, VOUT_0);
	}
}

void CCameraControl::avf_media_event_callback(int type, void *pself)
{
	CCameraControl *self = (CCameraControl*)pself;
	switch (type) {
	case AVF_CAMERA_EventStatusChanged:
		self->Post(MSG_MediaStateChanged);
		break;

//	case AVF_CAMERA_EventFileStart:
//		break;
//	case AVF_CAMERA_EventFileEnd:
//		break;
//	case AVF_CAMERA_EventBufferOverrun:
//		break;
//	case AVF_CAMERA_EventBufferOverflow:
//		break;
//	case AVF_CAMERA_EventDoneSaving:
//		break;
//	case AVF_CAMERA_EventDoneSaveNextPicture:
//		break;
//	case AVF_CAMERA_EventBufferSpaceLow:
//		break;

	case AVF_CAMERA_EventBufferSpaceFull:
		self->Post(MSG_BufferSpaceFull);
		break;
	case AVF_CAMERA_EventVdbStateChanged:
		self->Post(MSG_VdbStateChanged);
		break;

//	case AVF_CAMERA_EventUploadVideo:
//		break;
//	case AVF_CAMERA_EventUploadPicture:
//		break;
//	case AVF_CAMERA_EventUploadRaw:
//		break;

	case AVF_CAMERA_EventDiskError:
		self->Post(MSG_DiskError);
		break;

	case AVF_CAMERA_EventDiskSlow:
		self->Post(MSG_DiskSlow);
		break;

	case AVF_CAMERA_EventStillCaptureStateChanged:
		self->Post(MSG_StillCaptureStateChanged);
		break;
	case AVF_CAMERA_EventPictureTaken:
		self->Post(MSG_PictureTaken);
		break;
	case AVF_CAMERA_EventPictureListChanged:
		self->Post(MSG_PictureListChanged);
		break;

	case AVF_CAMERA_EventMmsMode1:
		self->Post(MSG_MMS_Mode1);
		break;

	case AVF_CAMERA_EventMmsMode2:
		self->Post(MSG_MMS_Mode2);
		break;

	default:
		break;
	}
}

void CCameraControl::avf_media_on_disconnected_callback(int error, void *pself)
{
	AVF_LOGW("disconnected from avf_media_server, error: %d", error);
	CCameraControl *self = (CCameraControl*)pself;
	self->Post(MSG_AvfServerError, error);
}

void CCameraControl::ProcessCmdMsg(cmd_msg_t& cmdmsg)
{
	switch (cmdmsg.code) {
	case CMD_SetState:
		switch (cmdmsg.p1) {
		case STATE_IDLE: DoEnterIdle(false); break;
		case STATE_PREVIEW: DoEnterPreview(false); break;
		case STATE_RECORDING: DoStartRecord(); break;
		case STATE_ERROR: DoEnterIdle(false); break;
		}
		break;

	case CMD_ChangeVideoConfig: DoChangeVideoConfig(cmdmsg.p1); break;
	case CMD_ChangeVideoQuality: DoChangeVideoQuality((enum Video_Quality)cmdmsg.p1); break;
	case CMD_ChangePreviewConfig: DoChangePreviewConfig(cmdmsg.p1); break;
	case CMD_ChangeSecondVideoConfig: DoChangeSecondVideoConfig(cmdmsg.p1); break;
	case CMD_ChangeRecMode: DoChangeRecMode(cmdmsg.p1); break;
	case CMD_ChangeColorMode: DoChangeColorMode(cmdmsg.p1); break;
	case CMD_ChangeOverlayFlags: DoChangeOverlayFlags(cmdmsg.p1); break;

	case CMD_ChangeCameraName: {
			string_t *name = (string_t*)cmdmsg.p1;
			DoChangeCameraName(name);
			name->Release();
		}
		break;

	case CMD_NetworkChangeTime:
		DoNetworkChangeTime((uint32_t)cmdmsg.p1, (int)cmdmsg.p2);
		break;

	case CMD_SetStillCaptureMode: DoSetStillCaptureMode(cmdmsg.p1); break;
	case CMD_StillCapture: DoStillCapture(cmdmsg.p1); break;
	case CMD_CaptureRaw: DoSetCaptureRaw(cmdmsg.p1); break;
	case CMD_UnmountTF: DoUnmountTF(); break;
	case CMD_MountTF: DoMountTF(); break;
	case CMD_MarkLiveClip: DoMarkLiveClip(cmdmsg.p1, cmdmsg.p2); break;
	case CMD_StopMarkLiveClip: DoStopMarkLiveClip(); break;
	case CMD_SetSceneData: {
			raw_data_t *raw = (raw_data_t*)cmdmsg.p1;
			DoSetSceneData(raw);
			raw->Release();
		}
		break;
	case CMD_SetMirrorHorz: DoSetMirrorHorz(cmdmsg.p1, cmdmsg.p2); break;
	case CMD_SetMirrorVert: DoSetMirrorVert(cmdmsg.p1, cmdmsg.p2); break;
	case CMD_ChangeLCDVideo: DoChangeLCDVideo(cmdmsg.p1); break;
	case CMD_ChangeLCDVideo_r: DoChangeLCDVideo((video_rect_t*)cmdmsg.p1); break;
	case CMD_WantIdle: DoEnterIdle(true); break;
	case CMD_WantPreview: DoEnterPreview(true); break;
	case CMD_ChangeLCDBrightness: DoChangeLCDBrightness(cmdmsg.p1); break;
	case CMD_FlipLCDVideo: DoFlipLCDVideo(cmdmsg.p1); break;
	case CMD_FlipLCDOSD: DoFlipLCDOSD(cmdmsg.p1); break;

	case MSG_AvfServerError:
		pthread_kill(main_thread_id, SIGQUIT);
		break;

	case MSG_MediaStateChanged: HandleMediaStateChanged(); break;
	case MSG_TFStateChanged: HandleTFStateChanged();break;
	case MSG_BufferSpaceFull: HandleBufferSpaceFull(); break;
	case MSG_VdbStateChanged: HandleVdbStateChanged(true); break;
	case MSG_DiskError: HandleDiskError(); break;
	case MSG_DiskSlow: HandleDiskSlow(); break;

	case MSG_StillCaptureStateChanged: HandleStillCaptureStateChanged(); break;
	case MSG_PictureTaken: HandlePictureTaken(); break;
	case MSG_PictureListChanged: UpdateTotalNumPictures(); break;

	case MSG_MMS_Mode1: HandleMMsMode1(); break;

	default:
		break;
	}
}

void CCameraControl::ConfigVin(const char *video_mode, const char *fps, int bits, int hdr_mode, int enc_mode)
{
	char buffer[128];

	if (opt_vin_video_mode[0]) {
		video_mode = opt_vin_video_mode;
	}

	if (mb_force_enc_mode) {
		enc_mode = m_enc_mode;
	}

	if (opt_bits > 0) {
		bits = opt_bits;
	}

	if (opt_force_hdr) {
		hdr_mode = opt_hdr;
	}

	// video_mode:fps:mirror_horz:mirror_vert:anti_flicker;
	// bits:hdr_mode:enc_mode;
	// exposure
	snprintf(buffer, sizeof(buffer), "%s:%s:" "%d:%d:%d;" "%d:%d:%d;" "%d",
		video_mode, fps, m_mirror_horz, m_mirror_vert, m_anti_flicker, bits, hdr_mode, enc_mode, opt_exposure);

	set_config_string("config.vin", 0, buffer);
}

// =======================================================================
// imx178
// 3072x2048P		60		RGB	10bits	4:3	AUTO	rev[0]
// 2592x1944P		30		RGB	14bits	4:3	AUTO	rev[0]
// 3072x2048P		60		RGB	10bits	4:3	AUTO	rev[0]
// 3072x2048P		30		RGB	14bits	4:3	AUTO	rev[0]
// 2560x2048P		30		RGB	14bits	4:3	AUTO	rev[0]
// 3072x1728P		60		RGB	12bits	16:9	AUTO	rev[0]
// 3072x1728P		30		RGB	14bits	16:9	AUTO	rev[0]
// 1280x720P		120		RGB	12bits	16:9	AUTO	rev[0]
// 1920x1080P		120		RGB	12bits	16:9	AUTO	rev[0]
// 2048x2048P		30		RGB	14bits	1:1	AUTO	rev[0]
// =======================================================================

#define VIDEO_VIN_IMX178_FULL		"3072x2048"
#define VIDEO_VIN_IMX178_16_9		"3072x1728"
#define VIDEO_VIN_IMX178_4_3		"2592x1944"
#define VIDEO_VIN_IMX178_1_1		"2048x2048"

#define VIDEO_VIN_IMX124_4_3		"2048x1536"
#define VIDEO_VIN_IMX124_16_9		"1920x1080"
#define VIDEO_VIN_IMX124_FULL		VIDEO_VIN_IMX124_4_3

// idle -> open, or recording -> open
int CCameraControl::OpenCamera()
{
	int rval;
	char tmp_prop[AGCMD_PROPERTY_SIZE];

#ifdef ARCH_S2L
	agcmd_property_get(prebuild_board_vin0, tmp_prop, "imx178");
#else	//ARCH_A5S
	agcmd_property_get(prebuild_board_vin0, tmp_prop, "mt9t002");
#endif

	if (mState == STATE_IDLE) {
		rval = avf_camera_set_media_source(mpMedia, NULL, opt_conf);
		// workaround: after set_media_source(), all config will be cleared
		SetAutoDelFlag();
		SetNoDeleteFlag();

		// set file format
		if (opt_format[0]) {
			set_config_string("config.MediaWriter.format", 0, opt_format);
			set_config_string("config.MediaWriter.format", 1, opt_format);
		}

		if (rval < 0) {
			AVF_LOGE("avf_camera_set_media_source(%s) failed %d", opt_conf, rval);
			return rval;
		}
		AVF_LOGI("avf_camera_set_media_source(%s)", opt_conf);
	}

	// if state is PREVIEW or RECORDING, this will stop the camera
	if ((rval = avf_camera_open(mpMedia, 1)) < 0) {
		AVF_LOGE("avf_camera_open() failed %d", rval);
		return rval;
	}

	// video_mode:fps:mirror_horz:mirror_vert:anti_flicker;bits:hdr_mode:enc_mode

	if (mbStillMode) {
		if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
			ConfigVin(VIDEO_VIN_IMX178_FULL, "20", 14, 0, 4);
		} else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
			ConfigVin(VIDEO_VIN_IMX124_FULL, "30", 12, 0, 4);
		} else {
			AVF_LOGE("Do not support sensor: %s", tmp_prop);
		}
	} else {
		const video_config_t *pVideoConfig = g_video_config + mVideoConfig;

		// -------------------------------
		// 120 fps
		// -------------------------------
		if (pVideoConfig->framerate == 120) {

			if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
				//ConfigVin("720p", "120", 12, 0, 4);
				ConfigVin("720p", "120", 12, 0, 4);
			} else {
				AVF_LOGE("Do not support sensor: %s", tmp_prop);
			}

		// -------------------------------
		// 60 fps
		// -------------------------------
		} else if (pVideoConfig->framerate == 60) {

#ifdef ARCH_S2L
			if (pVideoConfig->aspect_ratio == AR_4_3) {
				if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
					ConfigVin(VIDEO_VIN_IMX178_4_3, "60", 12, 0, 4);
				} else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
					ConfigVin(VIDEO_VIN_IMX124_4_3, "60", 12, 0, 4);
				} else {
					AVF_LOGE("Do not support sensor: %s", tmp_prop);
				}
			} else {
				if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
					if (pVideoConfig->width == 1280 && pVideoConfig->height == 720) {
						ConfigVin("720p", "60", 12, 0, 4);
					} else {
						int imx178_encode_mode = 4;
#ifdef BOARD_HACHI
{
						int insane_mode;
						char insane_prop[AGCMD_PROPERTY_SIZE];

						agcmd_property_get("temp.earlyboot.insane", insane_prop, "0");
						insane_mode = strtoul(insane_prop, NULL, 0);
						if (insane_mode == 0) {
							imx178_encode_mode = 0;
						}
}
#endif
						ConfigVin(VIDEO_VIN_IMX178_16_9, "60", 12, 0, imx178_encode_mode);
					}
				} else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
					ConfigVin(VIDEO_VIN_IMX124_16_9, "60", 12, 0, 4);
				} else if (strncasecmp(tmp_prop, "ov4689", strlen("ov4689")) == 0) {
					ConfigVin("2688x1512", "60", 10,0, 0);
				} else {
					AVF_LOGE("Do not support sensor: %s", tmp_prop);
				}
			}
#else
			// a5s: 60 is actually 59.94
			ConfigVin("1920x1080", "59.94", 10, 0, 4);
#endif

		// -------------------------------
		// 30 fps
		// -------------------------------
		} else if (pVideoConfig->framerate == 30) {

#ifdef ARCH_S2L
			if (pVideoConfig->aspect_ratio == AR_4_3) {
				if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
					ConfigVin(VIDEO_VIN_IMX178_4_3, "30", 14, 0, 4);
				} else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
					ConfigVin(VIDEO_VIN_IMX124_4_3, "30", 12, 0, 4);
				} else {
					AVF_LOGE("Do not support sensor: %s", tmp_prop);
				}
			} else {
				if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
					if (pVideoConfig->width == 1280 && pVideoConfig->height == 720) {
						//ConfigVin("720p", "60", 12, 0, 4);
						ConfigVin("720p", "30", 12, 0, 4);
					} else if (pVideoConfig->width == 2048 && pVideoConfig->height == 2048) {
						ConfigVin(VIDEO_VIN_IMX178_1_1, "30", 14, 0, 4);
					} else {
						ConfigVin(VIDEO_VIN_IMX178_16_9, "30", 12, 0, 4);
					}
				} else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
					if (pVideoConfig->width == 1536 && pVideoConfig->height == 1536) {
						ConfigVin(VIDEO_VIN_IMX124_FULL, "30", 12, 0, 4);
					} else {
						ConfigVin(VIDEO_VIN_IMX124_16_9, "30", 12, 0, 4);
					}
				} else if (strncasecmp(tmp_prop, "ov4689", strlen("ov4689")) == 0) {
					if (opt_force_enc_mode) {
						if (opt_enc_mode == 5) {
							ConfigVin("2688x1520", "30", 10, 1, 5);
						} else {
							ConfigVin("2688x1512", "30", 10, 0, 0);
						}
					} else {
						ConfigVin("2688x1512", "30", 10, 0, 0);
					}
				} else {
					AVF_LOGE("Do not support sensor: %s", tmp_prop);
				}
			}
#else
			// a5s: 30 is actually 29.97
			ConfigVin("auto", "29.97", 14, 0, 4);
#endif

		// -------------------------------
		// 25 fps, for calib
		// -------------------------------
		} else if (pVideoConfig->framerate == 25) {
#ifdef ARCH_S2L
			if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
				ConfigVin(VIDEO_VIN_IMX178_FULL, "25", 14, 0, 0);
			} else if (strncasecmp(tmp_prop, "imx124", strlen("imx124")) == 0) {
				ConfigVin(VIDEO_VIN_IMX124_FULL, "25", 12, 0, 4);
			} else if (strncasecmp(tmp_prop, "ov4689", strlen("ov4689")) == 0) {
				ConfigVin("2688x1512", "25", 10, 0, 0);
			} else {
				AVF_LOGE("Do not support sensor: %s", tmp_prop);
			}
#else
			AVF_LOGE("unknown framerate %d", pVideoConfig->framerate);
#endif

		} else {
			AVF_LOGE("unknown framerate %d", pVideoConfig->framerate);
			return -1;
		}
	}

	return 0;
}

int CCameraControl::CloseCamera()
{
	int rval;

	if ((rval = avf_camera_close(mpMedia, 1)) < 0) {
		AVF_LOGE("avf_camera_close() failed %d", rval);
		// TODO: error handling
	}

	return rval;
}

int CCameraControl::RunCamera(bool bRecord)
{
	int rval;

	if ((rval = ConfigCameraParams(bRecord)) < 0)
		return rval;

	// enable bt
	set_config_bool("config.obd.enable", 0, 1);

	// start rtsp streaming
	if (bRecord) {
		StartRTSP();
	}

	if ((rval = avf_camera_run(mpMedia, true)) < 0) {
		AVF_LOGE("avf_camera_run() failed %d", rval);
		return rval;
	}

	if (mbStillMode) {
		avf_camera_still_capture(mpMedia, STILLCAP_ACTION_SET_RAW, mb_raw);
	}

	return 0;
}

int CCameraControl::StopCamera()
{
	int rval;

	StopRTSP();

	if ((rval = avf_camera_stop(mpMedia, 1)) < 0) {
		AVF_LOGE("avf_camera_stop() failed %d", rval);
		return rval;
	}

	return 0;
}

int CCameraControl::SetOverlay()
{
	string_t *prefix = mpCameraState->GetCameraName();
	char name[128];

	const video_config_t *pVideoConfig = g_video_config + mVideoConfig;
	snprintf(name, sizeof(name), "%s %s", prefix->string(), pVideoConfig->name);
	name[sizeof(name)-1] = 0;

	set_config_string("config.subtitle.prefix", 0, name);
	set_config_int32("config.subtitle.flags", 0, mOverlayFlags);
	prefix->Release();

	return 0;
}

bool CCameraControl::ShouldEnableSecondVideo()
{
	if (mbStillMode) {
		// still mode has no 2nd video
		return false;
	}

	const sec_video_config_t *pSecConfig = get_sec_video_config_i(opt_config, mSecVideoConfig);
	if (pSecConfig->width == 0 || pSecConfig->height == 0) {
		// 2nd video is off
		return false;
	}

	const video_config_t *pVideoConfig = g_video_config + mVideoConfig;
	if (pVideoConfig->width <= 720) {
		// main video is too small
		return false;
	}

	return true;
}

// before RunCamera()
int CCameraControl::ConfigCameraParams(bool bRecord)
{
	char buffer[256];

	//---------------------------------------------------------
	// set clock
	//---------------------------------------------------------
	//if (opt_setclock) {
	const char *clock_mode = "";
	if (mbStillMode) {
		clock_mode = "still";
	} else {
		clock_mode = g_video_config[mVideoConfig].clock_mode;
	}
	set_config_string("config.clockmode", 0, clock_mode);
	//}

	//---------------------------------------------------------
	// video config
	//---------------------------------------------------------
	const char *overlay = OVERLAY_NONE;
	if (mbStillMode) {
		if (bRecord) {
			AVF_LOGE("cannot record in still mode");
			return -1;
		}
		// set width and height as 0: use full vin size
		// set jpeg quality to 95
		snprintf(buffer, sizeof(buffer), "still:0x0x95");
	} else {
		const video_config_t *pVideoConfig = g_video_config + mVideoConfig;
		if (bRecord) {
			int bitrate = pVideoConfig->bitrate;
			snprintf(buffer, sizeof(buffer), "h264:%dx%d:%d:%d:%d",
				pVideoConfig->width, pVideoConfig->height,
				bitrate, bitrate, bitrate);
			overlay = pVideoConfig->overlay;
		} else {
			// just preview, video stream is disabled
			// width, height must be set anyway
			snprintf(buffer, sizeof(buffer), "none:%dx%d",
				pVideoConfig->width, pVideoConfig->height);
		}
	}

	set_config_string("config.video", 0, buffer);
	set_config_string("config.video.overlay", 0, overlay);
	set_config_int32("config.record", 0, get_record_flag(bRecord));

	//---------------------------------------------------------
	// preview config
	//---------------------------------------------------------

	int preview_config = mPreviewConfig;

#ifdef ARCH_A5S
	if (bRecord) {
		// A5s cannot have 1024x576 jpeg when recording
		if (preview_config == find_preview_config("1024x576", opt_config)) {
			preview_config = find_preview_config("512x288", opt_config);
		}
	}
#endif

	const preview_config_t *pPreviewConfig = get_preview_config_i(opt_config, preview_config);
	const preview_size_info_t *size_info;
	int preview_height;

	if (mbStillMode) {
		// in still mode, set preview height to 0, it will be calculated by avf
		// to be proportional to the still picture
		size_info = &pPreviewConfig->size_16_9;
		preview_height = 0;
	} else {
		// select preview size according to video aspect ratio
		switch (g_video_config[mVideoConfig].aspect_ratio) {
		case AR_4_3:
			size_info = &pPreviewConfig->size_4_3;
			break;
		case AR_3_2:
			size_info = &pPreviewConfig->size_3_2;
			break;
		case AR_1_1:
			size_info = &pPreviewConfig->size_1_1;
			break;
		case AR_16_9:
		default:
			size_info = &pPreviewConfig->size_16_9;
			break;
		}
		preview_height = size_info->height;
	}

	if (size_info->width == 0) {
		snprintf(buffer, sizeof(buffer), "none");
	} else {
		snprintf(buffer, sizeof(buffer), "mjpeg:%dx%dx%d",
			size_info->width, preview_height,
			pPreviewConfig->quality);
	}

	set_config_string("config.video", 1, buffer);
	set_config_string("config.video.overlay", 1, mbStillMode ? OVERLAY_NONE : size_info->overlay);

	//---------------------------------------------------------
	// second video config
	//---------------------------------------------------------

	if (!ShouldEnableSecondVideo()) {
		if (mbStillMode) {
			// for still capture thumbnail
			set_config_string("config.video", 2, "mjpeg:352x240x85");
		} else {
			set_config_string("config.video", 2, "none");
		}
		set_config_int32("config.record", 1, 0);

	} else {

		const sec_video_config_t *pSecConfig = get_sec_video_config_i(opt_config, mSecVideoConfig);
		if (bRecord) {
			int bitrate = pSecConfig->bitrate;
			snprintf(buffer, sizeof(buffer), "h264:%dx%d:%d:%d:%d",
				pSecConfig->width, pSecConfig->height,
				bitrate, bitrate, bitrate);
		} else {
			// just preview. but width, height must be set anyway
			snprintf(buffer, sizeof(buffer), "none:%dx%d",
				pSecConfig->width, pSecConfig->height);
		}

		set_config_string("config.video", 2, buffer);
		set_config_string("config.video.overlay", 2,
			bRecord && !mbStillMode ? pSecConfig->overlay : OVERLAY_NONE);
		set_config_int32("config.record", 1, get_record_flag(bRecord));
	}

	//---------------------------------------------------------
	// frame rate config
	//---------------------------------------------------------

	if (mbStillMode) {
		set_config_string("config.video.framerate", 0, "1:1");
		set_config_string("config.video.framerate", 1, "1:1");
		set_config_string("config.video.framerate", 2, "1:1");
	} else {
		const video_config_t *pVideoConfig = g_video_config + mVideoConfig;

		if (pVideoConfig->framerate == 120) {

			set_config_string("config.video.framerate", 0, "1:1");	// video: 120

			if (opt_config == 1) {
				set_config_string("config.video.framerate", 1, "1:120");
			} else {
				set_config_string("config.video.framerate", 1,
					bRecord ? "1:8" : "1:4");	// preview: 15 or 30
			}

			set_config_string("config.video.framerate", 2, "1:4");	// 2nd video: 30

		} else if (pVideoConfig->framerate == 60) {

			set_config_string("config.video.framerate", 0, "1:1");	// video: 60

			if (opt_config == 1) {
				set_config_string("config.video.framerate", 1, "1:60");
			} else {
				set_config_string("config.video.framerate", 1, 
					bRecord ? "1:4" : "1:2");	// preview: 15 or 30
			}

			set_config_string("config.video.framerate", 2, "1:2");	// 2nd video: 30

		} else if (pVideoConfig->framerate == 30) {

			set_config_string("config.video.framerate", 0, "1:1");	// video: 30

			if (opt_config == 1) {
				set_config_string("config.video.framerate", 1, "1:30");
			} else {
				set_config_string("config.video.framerate", 1, 
					bRecord ? "1:2" : "1:1");	// preview: 15 or 30
			}

			set_config_string("config.video.framerate", 2, "1:1");	// 2nd video: 30

		} else if (pVideoConfig->framerate == 25) {

			set_config_string("config.video.framerate", 0, "1:1");	// video: 25

			if (opt_config == 1) {
				set_config_string("config.video.framerate", 1, "1:25");
			} else {
				set_config_string("config.video.framerate", 1, "1:1");	// preview: 25
			}

			set_config_string("config.video.framerate", 2, "1:1");	// 2nd video: 25

		} else {
			AVF_LOGE("unknown framerate %d", pVideoConfig->framerate);
			return -1;
		}
	}

	SetOverlay();

	return 0;
}

void CCameraControl::StartRTSP()
{
	if (opt_rtsp) {
		avf_media_set_config_string(mpMedia, "config.mms.path.mode1", MMS_RTSP_STREAM, MMS_RTSP_PATH);
		avf_media_set_config_int32(mpMedia, "config.mms.opt.mode1", MMS_RTSP_STREAM, UPLOAD_OPT_ALL);
	}
}

void CCameraControl::StopRTSP()
{
	if (opt_rtsp) {
		avf_media_set_config_int32(mpMedia, "config.mms.opt.mode1", MMS_RTSP_STREAM, 0);
	}
}

int CCameraControl::DoEnterIdle(bool bTry)
{
	switch (mState) {
	case STATE_IDLE:
		return 0;

	case STATE_PREVIEW:
		CloseCamera();
		break;

	case STATE_RECORDING:
		if (bTry) {
			return 0;
		}
		CloseCamera();
		break;

	case STATE_ERROR:
		CloseCamera();
		break;

	default:
		return -1;
	}

	mState = STATE_IDLE;
	return 0;
}

int CCameraControl::DoEnterPreview(bool bTry)
{
	int rval;

	EnterIdleIfError();

	switch (mState) {
	case STATE_IDLE:	// idle -> preview
		if ((rval = OpenCamera()) < 0) {
			goto Error;
		}
		if ((rval = RunCamera(false)) < 0) {
			goto Error;
		}
		mState = STATE_PREVIEW;
		break;

	case STATE_PREVIEW:
		break;

	case STATE_RECORDING:
		if (!bTry) {
			// recording -> preview
			if (mbStillMode) {
				AVF_LOGW("still mode");
				return -1;
			}
			RecordStateChanged(State_Rec_stopping);
			if ((rval = StopCamera()) < 0) {
				goto Error;
			}
			if ((rval = RunCamera(false)) < 0) {
				goto Error;
			}
			mState = STATE_PREVIEW;
			RecordStateChanged(State_Rec_stopped);
		}
		break;

	default:
		return -1;
	}

	return 0;

Error:
	mState = STATE_ERROR;
	RecordStateChanged(State_Rec_error);
	return rval;
}

int CCameraControl::CheckSpaceForRecord()
{
	if (!mbTFReady) {
		AVF_LOGW("no tf, cannot start record");
		mpCameraState->NotifyRecordError(Error_code_notfcard);
		return -1;
	}

	if (!mbVdbReady) {
		AVF_LOGW("vdb is not ready, cannot start record");
		return -1;
	}

	// remove some old files if space is little (auto delete mode)
	if (mb_auto_delete) {
		if (avf_media_free_vdb_space(mpMedia, &mVdbInfo) < 0) {
			// unexpected
			AVF_LOGE("avf_media_free_vdb_space failed");
			mpCameraState->NotifyRecordError(Error_code_tfFull);
			return -1;
		}
	}

	// check space
	int enough_space = mb_auto_delete ?
		mVdbInfo.enough_space_autodel : mVdbInfo.enough_space;
	if (!enough_space) {
		AVF_LOGE("no enough space to start record");
		mpCameraState->NotifyRecordError(Error_code_tfFull);
		return -1;
	}

	return 0;
}

int CCameraControl::DoStartRecord()
{
	int rval;

	if (mbStillMode) {
		AVF_LOGW("cannot record video in still mode");
		return -1;
	}

	if (!opt_nowrite && CheckSpaceForRecord() < 0)
		return -1;

	EnterIdleIfError();

	switch (mState) {
	case STATE_IDLE:
	case STATE_PREVIEW:

		RecordStateChanged(State_Rec_starting);

		if (mState == STATE_IDLE) {
			if ((rval = OpenCamera()) < 0) {
				goto Error;
			}
		} else {
			if ((rval = StopCamera()) < 0) {
				goto Error;
			}
		}

		if ((rval = RunCamera(true)) < 0) {
			goto Error;
		}

		mState = STATE_RECORDING;
		RecordStateChanged(State_Rec_recording);
		break;

	case STATE_RECORDING:
		return 0;

	default:
		return -1;
	}

	return 0;

Error:
	mState = STATE_ERROR;
	RecordStateChanged(State_Rec_error);
	return rval;
}

int CCameraControl::ConfigChanged(bool bSwitchStillMode)
{
	int rval;

	EnterIdleIfError();

	switch (mState) {
	case STATE_IDLE:
		return 0;

	case STATE_PREVIEW:
	case STATE_RECORDING:
		if (bSwitchStillMode) {
			RecordStateChanged(State_Rec_switching);
		} else {
			RecordStateChanged(State_Rec_stopping);
		}
		if ((rval = OpenCamera()) < 0) {
			goto Error;
		}
		if ((rval = RunCamera(false)) < 0) {
			goto Error;
		}
		mState = STATE_PREVIEW;
		RecordStateChanged(State_Rec_stopped);
		break;

	default:
		return -1;
	}

	return 0;

Error:
	mState = STATE_ERROR;
	RecordStateChanged(State_Rec_error);
	return rval;
}

int CCameraControl::DoChangeVideoConfig(int index)
{
	int rval;

	if (index == mVideoConfig)
		return 0;

	mVideoConfig = index;

	if ((rval = ConfigChanged()) < 0)
		return rval;

	VideoResolutionChanged();

	return 0;
}

int CCameraControl::DoChangeVideoQuality(enum Video_Quality quality)
{
	if (mVideoQuality != quality) {
		mVideoQuality = quality;
		// TODO: implementation
		VideoQualityChanged();
	}
	return 0;
}

int CCameraControl::DoChangePreviewConfig(int index)
{
	int rval;

	if (index == mPreviewConfig)
		return 0;

	mPreviewConfig = index;

	if ((rval = ConfigChanged()) < 0)
		return rval;

	return 0;
}

int CCameraControl::DoChangeSecondVideoConfig(int index)
{
	int rval;

	if (index == mSecVideoConfig)
		return 0;

	mSecVideoConfig = index;

	if ((rval = ConfigChanged()) < 0)
		return rval;

	return 0;
}

int CCameraControl::DoChangeRecMode(int value)
{
	bool b_auto_record;
	bool b_auto_delete;

	switch (value) {
	case Rec_Mode_Manual:
		b_auto_record = false;
		b_auto_delete = false;
		break;

	case Rec_Mode_AutoStart:
		b_auto_record = true;
		b_auto_delete = false;
		break;

	case Rec_Mode_Manual_circle:
		b_auto_record = false;
		b_auto_delete = true;
		break;

	case Rec_Mode_AutoStart_circle:
		b_auto_record = true;
		b_auto_delete = true;
		break;

	default:
		AVF_LOGE("unknown rec mode %d", value);
		return -1;
	}

	if (mb_auto_record != b_auto_record || mb_auto_delete != b_auto_delete) {
		mb_auto_record = b_auto_record;
		mb_auto_delete = b_auto_delete;
		SetAutoDelFlag();
		RecordModeChanged();
	}

	return 0;
}

int CCameraControl::DoChangeColorMode(int value)
{
	if (value < 0 || value >= Color_Mode_num) {
		AVF_LOGE("bad color mode %d", value);
		return -1;
	}
	// TODO: implementation
	mpCameraState->ColorModeChanged((enum Color_Mode)value);
	return 0;
}

int CCameraControl::DoChangeLCDBrightness(int value)
{
	if (value < 0 || value > 255) {
		AVF_LOGE("bad lcd brightness %d", value);
		return -1;
	}

	if (avf_display_set_brightness(mpMedia, VOUT_0, VOUT_TYPE_MIPI, value) < 0) {
		AVF_LOGE("avf_display_set_brightness failed");
		return -1;
	}

	mpCameraState->LCDBrightnessChanged(value);
	return 0;
}

int CCameraControl::DoFlipLCDVideo(int flip)
{
	if (avf_display_flip_vout_video(mpMedia, VOUT_0, VOUT_TYPE_MIPI, flip) < 0) {
		AVF_LOGE("avf_display_flip_vout_video failed");
		return -1;
	}

	mpCameraState->LCDVideoFlipChanged(flip);
	return 0;
}

int CCameraControl::DoFlipLCDOSD(int flip)
{
	if (avf_display_flip_vout_osd(mpMedia, VOUT_0, VOUT_TYPE_MIPI, flip) < 0) {
		AVF_LOGE("avf_display_flip_vout_osd failed");
		return -1;
	}

	mpCameraState->LCDOSDFlipChanged(flip);
	return 0;
}

int CCameraControl::DoChangeOverlayFlags(int value)
{
	if (mOverlayFlags == value)
		return 0;

	mOverlayFlags = value;

	if (mState == STATE_PREVIEW || mState == STATE_RECORDING) {
		SetOverlay();
	}

	mpCameraState->OverlayFlagsChanged(value);
	return 0;
}

int CCameraControl::DoChangeCameraName(string_t *name)
{
	if (mpCameraState->CameraNameChanged(name)) {
		if (mState == STATE_PREVIEW || mState == STATE_RECORDING) {
			SetOverlay();
		}
	}
	return 0;
}

void set_rtc(time_t utc)
{
	int fd;
	int rval;

	if ((fd = avf_open_file("/dev/rtc", O_WRONLY, 0)) >= 0)
		goto OK;

	if ((fd = avf_open_file("/dev/rtc0", O_WRONLY, 0)) >= 0)
		goto OK;

	if ((fd = avf_open_file("/dev/misc/rtc", O_WRONLY, 0)) >= 0)
		goto OK;

	AVF_LOGE("cannot open rtc device");
	return;

OK:
	struct tm sys_tm;
	struct tm *psys_tm;

	psys_tm = ::gmtime_r(&utc, &sys_tm);
	if (psys_tm != &sys_tm) {
		AVF_LOGE("gmtime_r failed");
		goto Done;
	}

	if ((rval = ::ioctl(fd, RTC_SET_TIME, &sys_tm)) < 0) {
		AVF_LOGP("RTC_SET_TIME");
		goto Done;
	}

Done:
	avf_close_file(fd);
	return;
}

int CCameraControl::DoNetworkChangeTime(uint32_t time, int timezone)
{
	struct timeval tv = {0};
	struct timezone tz = {0};

	tv.tv_sec = time;
	tv.tv_usec = 0;

	tz.tz_minuteswest = timezone * 60;
	tz.tz_dsttime = 0;

	::settimeofday(&tv, &tz);
	set_rtc(time);

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%sGMT%+d",
		"/usr/share/zoneinfo/TS/", timezone);

	avf_remove_file("/castle/localtime", false);
	avf_symlink(buffer, "/castle/localtime");

	return 0;
}

int CCameraControl::DoSetStillCaptureMode(bool bStillMode)
{
	int rval;

	if (mbStillMode == bStillMode)
		return 0;

#ifndef ARCH_S2L
	if (bStillMode) {
		AVF_LOGE("a5s does not have still mode");
		return -1;
	}
#endif

	mbStillMode = bStillMode;

	if ((rval = ConfigChanged(true)) < 0)
		return rval;

	return 0;
}

int CCameraControl::DoStillCapture(int action)
{
	if (mState != STATE_PREVIEW && mState != STATE_RECORDING) {
		AVF_LOGE("DoStillCapture: not in preview/recording state: %d", mState);
		return -1;
	}

/*
	if (!mbStillMode) {
		AVF_LOGE("DoStillCapture: not still mode");
		return -1;
	}
*/

	if (action == STILLCAP_ACTION_SINGLE || action == STILLCAP_ACTION_BURST) {
		if (CheckSpaceForRecord() < 0)
			return -1;
	}

	if (avf_camera_still_capture(mpMedia, action, 0) < 0)
		return -1;

	if (action == STILLCAP_ACTION_SINGLE || action == STILLCAP_ACTION_BURST) {
		mpCameraState->NotifyStillCaptureStarted(action == STILLCAP_ACTION_SINGLE);
	}

	return 0;
}

int CCameraControl::DoSetCaptureRaw(int raw)
{
	if (mb_raw != raw) {
		mb_raw = raw;
		if (mbStillMode) {
			avf_camera_still_capture(mpMedia, STILLCAP_ACTION_SET_RAW, mb_raw);
		}
		mpCameraState->SetRawMode(mb_raw);
	}
	return 0;
}

int CCameraControl::DoUnmountTF()
{
	// stop record first
	DoEnterPreview(false);

	// unload vdb
	// note: if vdb is loading now, it will send AVF_CAMERA_EventVdbStateChanged, 
	// then MSG_VdbStateChanged is posted to msg Q.
	// So when processing MSG_VdbStateChanged in HandleVdbStateChanged(),
	// vdb state should still be checked.
	// Note: after avf_media_storage_ready() returns, the vdb has been unloaded.
	avf_media_storage_ready(mpMedia, 0);

	mbTFReady = false;
	mbVdbReady = false;

	return 0;
}

int CCameraControl::DoMountTF()
{
	HandleTFStateChanged();
	return 0;
}

int CCameraControl::DoMarkLiveClip(int before_live_ms, int after_live_ms)
{
	if (mState != STATE_RECORDING) {
		AVF_LOGE("not recording state");
		return E_ERROR;
	}

	//return avf_camera_mark_live_clip(mpMedia, 0, before_live_ms, after_live_ms);
	return avf_camera_mark_live_clip_ex(mpMedia, 0, before_live_ms, after_live_ms, 1);
}

int CCameraControl::DoStopMarkLiveClip()
{
	return avf_camera_cancel_mark_live_clip(mpMedia);
}

int CCameraControl::DoSetSceneData(raw_data_t *raw)
{
	avf_mark_live_info_t info;
	if (avf_camera_get_mark_live_clip_info(mpMedia, &info) < 0) {
		AVF_LOGE("avf_camera_get_mark_live_clip_info failed");
		return -1;
	}

	if (avf_camera_set_clip_scene_data(mpMedia, CLIP_TYPE_MARKED, info.clip_id, raw->GetData(), raw->GetSize()) < 0) {
		AVF_LOGE("avf_camera_set_clip_scene_data failed");
		return -1;
	}

	return 0;
}

int CCameraControl::DoSetMirrorHorz(int toggle, int mirror_horz)
{
	m_mirror_horz = toggle ? !m_mirror_horz : mirror_horz;
	return ConfigChanged(false);
}

int CCameraControl::DoSetMirrorVert(int toggle, int mirror_vert)
{
	m_mirror_vert = toggle ? !m_mirror_vert : mirror_vert;
	return ConfigChanged(false);
}

int CCameraControl::ShowLCDInfo(const char *msg)
{
	avf_vout_info_t info;

	if (avf_display_get_vout_info(mpMedia, VOUT_0, VOUT_TYPE_MIPI, &info) < 0) {
		AVF_LOGE("avf_camera_get_vout_info failed");
		return -1;
	}

	AVF_LOGI(C_YELLOW "%s" C_NONE, msg);
	AVF_LOGI("    enabled: %d, vout_width: %d, vout_height: %d",
		info.enabled, info.vout_width, info.vout_height);
	AVF_LOGI("    video_width: %d, video_height: %d, video_xoff: %d, video_yoff: %d",
		info.video_width, info.video_height, info.video_xoff, info.video_yoff);

	return 0;
}

int CCameraControl::DoChangeLCDVideo(int lb)
{
//	if (lb == opt_video_letterboxed) {
//		AVF_LOGI("not changed");
//		return 0;
//	}

	ShowLCDInfo("current");

	opt_video_letterboxed = lb;

	const vout_video_config_t *pVideoConfig = GetLCDVideoConfig();
	char buffer[64];

	sprintf(buffer, "lcd:%d,%d,%d,%d",
		pVideoConfig->video_off_x, pVideoConfig->video_off_y,
		pVideoConfig->video_width, pVideoConfig->video_height);

	if (avf_camera_change_vout_video(mpMedia, buffer) < 0)
		return -1;

	ShowLCDInfo("new");

	return 0;
}

int CCameraControl::DoChangeLCDVideo(video_rect_t *r)
{
	ShowLCDInfo("current");

	char buffer[64];

	sprintf(buffer, "lcd:%d,%d,%d,%d", r->xoff, r->yoff, r->width, r->height);
	avf_free(r);

	if (avf_camera_change_vout_video(mpMedia, buffer) < 0)
		return -1;

	ShowLCDInfo("new");

	return 0;
}

void CCameraControl::HandleMediaStateChanged()
{
	int state = avf_media_get_state(mpMedia);
	switch (state) {
	case AVF_MEDIA_STATE_IDLE:
		break;
	case AVF_MEDIA_STATE_STOPPED:
		break;
	case AVF_MEDIA_STATE_RUNNING:
		break;
	case AVF_MEDIA_STATE_ERROR:
		break;
	}
}

void CCameraControl::HandleTFStateChanged()
{
	bool bTFReady = avf_file_exists(opt_vdbfs);
	AVF_LOGI(C_YELLOW "%s ready: %d" C_NONE, opt_vdbfs, bTFReady);
	if (bTFReady != mbTFReady) {
		mbTFReady = bTFReady;
		if (!bTFReady) {
			// TODO: stop record if it is recording
		}
		StorageStateChanged();
		SetNoDeleteFlag();
		// call vdb to load or unload
		// when done, it will send msg AVF_CAMERA_EventVdbStateChanged
		int rval = avf_media_storage_ready(mpMedia, (int)bTFReady);
		if (rval < 0) {
			AVF_LOGE("avf_media_storage_ready() failed %d", rval);
		}
	}
}

void CCameraControl::HandleBufferSpaceFull()
{
	if (mState == STATE_RECORDING) {
		AVF_LOGW("Buffer Space Full");
		DoEnterPreview(false);
	}
}

bool CCameraControl::FetchVdbSpace(bool bPrint)
{
	int rval;

	if ((rval = avf_media_get_vdb_info(mpMedia, &mVdbInfo)) < 0) {
		AVF_LOGW("avf_media_get_vdb_info() failed %d", rval);
		return false;
	}

	if (0) {
	//if (bPrint) {
		AVF_LOGI("vdb state changed");
		AVF_LOGI("  total_space: " C_GREEN "%d MB" C_NONE, mVdbInfo.total_space_mb);
		AVF_LOGI("  disk_space: " C_GREEN "%d MB" C_NONE, mVdbInfo.disk_space_mb);
		AVF_LOGI("  file_space_mb: " C_GREEN "%d MB" C_NONE, mVdbInfo.file_space_mb);
		AVF_LOGI("  vdb_ready: " C_GREEN "%d" C_NONE, mVdbInfo.vdb_ready);
		AVF_LOGI("  enough_space: " C_GREEN "%d" C_NONE, mVdbInfo.enough_space);
		AVF_LOGI("  enough_space_autodel: " C_GREEN "%d" C_NONE, mVdbInfo.enough_space_autodel);
	}

	if (0) {
		avf_space_info_t si;
		rval = avf_media_get_space_info(mpMedia, &si);
		if (rval == 0) {
			AVF_LOGI("total_space: " LLD ", used_space: " LLD ", marked_space: " LLD ", clip_space: " LLD,
				si.total_space, si.used_space, si.marked_space, si.clip_space);
		} else {
			AVF_LOGW("avf_media_get_space_info failed");
		}
	}

	return true;
}

void CCameraControl::HandleVdbStateChanged(bool bPrint)
{
	if (FetchVdbSpace(bPrint) < 0)
		return;

	bool bVdbReady = mVdbInfo.vdb_ready != 0;
	if (bVdbReady != mbVdbReady) {
		mbVdbReady = bVdbReady;
		StorageStateChanged();
	}

	// TODO: auto record will only run for the first time vdb ready
	if (mbVdbReady) {
		StorageSpaceInfoChanged(false);
		if (mb_auto_record) {
			AVF_LOGD(C_YELLOW "vdb ready, start record" C_NONE);
			DoStartRecord();
		}
	}
}

void CCameraControl::HandleDiskError()
{
	AVF_LOGW("HandleDiskError");
	StopRecord();
}

void CCameraControl::HandleDiskSlow()
{
	AVF_LOGW("== disk is slow ==");
}

void CCameraControl::HandleStillCaptureStateChanged()
{
	still_capture_info_t info;
	if (avf_camera_get_still_capture_info(mpMedia, &info) < 0) {
		AVF_LOGE("avf_camera_get_still_capture_info failed");
		return;
	}
	mpCameraState->StillCaptureStateChanged(info.stillcap_state, info.last_error);
}

void CCameraControl::HandlePictureTaken()
{
	still_capture_info_t info;
	if (avf_camera_get_still_capture_info(mpMedia, &info) < 0) {
		AVF_LOGE("avf_camera_get_still_capture_info failed");
		return;
	}
	mpCameraState->PictureTaken(info.num_pictures, info.burst_ticks);
}

void CCameraControl::UpdateTotalNumPictures()
{
	int total = avf_camera_get_num_pictures(mpMedia);
	mpCameraState->TotalNumPicturesChanged(total);
}

void CCameraControl::HandleMMsMode1()
{
#ifdef RTSP
	if (mp_rtsp_source) {
		uint32_t buf[3];
		int rval = avf_media_get_mem(mpMedia, "state.mms.mode1", MMS_RTSP_STREAM, (uint8_t*)buf, sizeof(buf));
		if (rval == 0) {
			mp_rtsp_source->UpdateWrite(buf[0], buf[1], buf[2]);
		}
	}
#endif
}

void CCameraControl::RecordStateChanged(enum State_Rec state)
{
	mpCameraState->RecStateChanged(state, mbStillMode);
}

void CCameraControl::RecordModeChanged()
{
	mpCameraState->RecordModeChanged(mb_auto_record, mb_auto_delete);
}

void CCameraControl::VideoResolutionChanged()
{
	mpCameraState->VideoResolutionChanged(g_video_config[mVideoConfig].std_resolution);
}

void CCameraControl::VideoQualityChanged()
{
	mpCameraState->VideoQualityChanged(mVideoQuality);
}

void CCameraControl::StorageStateChanged()
{
	enum State_storage state;
	if (!mbTFReady) {
		state = State_storage_noStorage;
	} else {
		if (!mbVdbReady)
			state = State_storage_loading;
		else
			state = State_storage_ready;
	}
	mpCameraState->StorageStateChanged(state);
}

void CCameraControl::StorageSpaceInfoChanged(bool bFetch)
{
	if (bFetch) {
		if (FetchVdbSpace(false) < 0)
			return;
	}

	uint64_t total_space = 0;
	uint64_t free_space = 0;

	if (mbVdbReady) {
		total_space = mVdbInfo.total_space_mb;
		free_space = mVdbInfo.disk_space_mb + mVdbInfo.file_space_mb;
		total_space <<= 20;
		free_space <<= 20;
	}

	mpCameraState->StorageSpaceInfoChanged(total_space, free_space);
}

void CCameraControl::SetAutoDelFlag()
{
	set_config_bool("config.vdb.autodel", 0, mb_auto_delete);
	set_config_bool("config.vdb.autodel", AVF_GLOBAL_CONFIG, mb_auto_delete);
}

void CCameraControl::SetNoDeleteFlag()
{
	set_config_bool("config.vdb.nodelete", 0, mb_no_delete);
	set_config_bool("config.vdb.nodelete", AVF_GLOBAL_CONFIG, mb_no_delete);
}




//{3760E70B-628B-475D-B9D0-63EC162DD47A}
AVF_DEFINE_IID(IID_ICameraStateListener,
0x3760E70B, 0x628B, 0x475D, 0xB9, 0xD0, 0x63, 0xEC, 0x16, 0x2D, 0xD4, 0x7A);

//-----------------------------------------------------------------------
//
// ICameraStateListener
//
//-----------------------------------------------------------------------
class ICameraStateListener: public IInterface
{
public:
	DECLARE_IF(ICameraStateListener, IID_ICameraStateListener);

public:
	enum {
		Null,
		RecState,
		MicState,
		PowerBatteryState,
		StorageState,
		StorageSpaceInfo,
		GpsState,
		VideoResolution,
		VideoQuality,
		OverlayFlags,
		RecordMode,
		ColorMode,
		CameraName,
		TotalNumPictures,
		StillCaptureState,
		Raw,
		LCDBrightness,
	};

public:
	virtual void OnStateChanged(int which) = 0;
	virtual void OnRecordError(enum Rec_Error_code code) = 0;
	virtual void OnStillCaptureStarted(int one_shot) = 0;
};

//-----------------------------------------------------------------------
//
//  CCameraState
//
//-----------------------------------------------------------------------

class CCameraState: public CObject
{
public:
	static CCameraState *Create();

public:
	avf_status_t RegisterOnStateChangedListener(ICameraStateListener *listener);
	avf_status_t UnRegisterOnStateChangedListener(ICameraStateListener *listener);

public:
	void RecStateChanged(enum State_Rec state, int is_still);
	void RecDurationChanged(int duration);

	void MicStateChanged(enum State_Mic state, int vol);

	void PowerStateChanged(enum State_power state);
	void BatteryStateChanged(const char *state);

	void StorageStateChanged(enum State_storage state);
	void StorageSpaceInfoChanged(uint64_t total_space, uint64_t free_space);

	void GPSStateChanged(enum State_GPS state);

	void VideoResolutionChanged(enum Video_Resolution resolution);
	void VideoQualityChanged(enum Video_Quality quality);
	void OverlayFlagsChanged(int value);

	void RecordModeChanged(enum Rec_Mode mode);
	void RecordModeChanged(int auto_record, int auto_del);
	void ColorModeChanged(enum Color_Mode mode);
	void LCDBrightnessChanged(int brightness);
	void LCDVideoFlipChanged(int flip);
	void LCDOSDFlipChanged(int flip);

	int CameraNameChanged(string_t *name);

	void TotalNumPicturesChanged(int total);
	void StillCaptureStateChanged(int stillcap_state, int last_error);
	void PictureTaken(int num_pictures, int burst_ticks);
	void SetRawMode(int raw);
	const char *GetStillCapStateName(int stillcap_state);

	void NotifyRecordError(enum Rec_Error_code code);
	void NotifyStillCaptureStarted(int one_shot);

public:
	void GetRecState(enum State_Rec *state, int *is_still) {
		AUTO_LOCK(mMutex);
		*state = m_rec_state;
		*is_still = m_is_still;
	}

	int GetRecDuration();

	void GetMicState(enum State_Mic *mic_state, int *mic_vol) {
		AUTO_LOCK(mMutex);
		*mic_state = m_mic_state;
		*mic_vol = m_mic_vol;
	}

	void GetPowerState(enum State_power *power, string_t **p_battery) {
		AUTO_LOCK(mMutex);
		*power = m_power_state;
		*p_battery = m_battery_state ? 
			m_battery_state->acquire() : string_t::CreateFrom("Unknown");
	}

	enum State_storage GetStorageState() {
		return m_storage_state;
	}

	void GetStorageSpaceInfo(uint64_t *total_space, uint64_t *free_space) {
		AUTO_LOCK(mMutex);
		*total_space = m_total_space;
		*free_space = m_free_space;
	}

	enum State_GPS GetGpsState() {
		return m_gps_state;
	}

	enum Video_Resolution GetVideoResolution() {
		return m_video_resolution;
	}

	enum Video_Quality GetVideoQuality() {
		return m_video_quality;
	}

	int GetOverlayFlags() {
		return m_overlay_flags;
	}

	enum Rec_Mode GetRecMode() {
		return m_rec_mode;
	}

	enum Color_Mode GetColorMode() {
		return m_color_mode;
	}

	int GetTotalNumPictures() {
		return m_total_pictures;
	}

	void GetStillCapInfo(int *stillcap_state, int *num_pictures, int *burst_ticks) {
		AUTO_LOCK(mMutex);
		*stillcap_state = m_stillcap_state;
		*num_pictures = m_num_pictures;
		*burst_ticks = m_burst_ticks;
	}

	string_t *GetCameraName() {
		AUTO_LOCK(mMutex);
		return mp_camera_name->acquire();
	}

	enum State_Cam_Mode GetCameraMode() {
		return m_cam_mode;
	}

	int GetRawMode() {
		return m_raw;
	}

	int GetLCDBrightness() {
		return m_lcd_brightness;
	}

private:
	CCameraState();
	virtual ~CCameraState();

private:
	void OnStateChangedL(int which);
	void OnRecordErrorL(enum Rec_Error_code code);
	void OnStillCaptureStartedL(int one_shot);

private:
	CMutex mMutex;
	ICameraStateListener *mpListeners[4];

private:
	enum State_Cam_Mode m_cam_mode;

	int m_is_still;
	enum State_Rec m_rec_state;
	avf_u64_t m_start_rec_tick;

	enum State_Mic m_mic_state;
	int m_mic_vol;

	enum State_power m_power_state;
	string_t *m_battery_state;

	enum State_storage m_storage_state;
	uint64_t m_total_space;
	uint64_t m_free_space;

	enum State_GPS m_gps_state;

	enum Video_Resolution m_video_resolution;
	enum Video_Quality m_video_quality;
	int m_overlay_flags;

	enum Rec_Mode m_rec_mode;
	enum Color_Mode m_color_mode;

	string_t *mp_camera_name;

	int m_total_pictures;	// total pictures on tf card
	int m_stillcap_state;
	int m_stillcap_last_error;
	int m_num_pictures;	// num pictures taken this turn
	int m_burst_ticks;
	int m_raw;

	int m_lcd_brightness;	// -1 if not available
};

//-----------------------------------------------------------------------
//
//  CCameraState
//
//-----------------------------------------------------------------------

CCameraState *CCameraState::Create()
{
	CCameraState *result = avf_new CCameraState();
	CHECK_STATUS(result);
	return result;
}

CCameraState::CCameraState()
{
	SET_ARRAY_NULL(mpListeners);

	m_cam_mode = State_Cam_Mode_Record;

	m_is_still = 0;
	m_rec_state = State_Rec_stopped;

	m_mic_state = State_Mic_ON;
	m_mic_vol = 5;

	m_power_state = State_power_yes;
	m_battery_state = string_t::CreateFrom("Not charging");

	m_storage_state = State_storage_noStorage;
	m_total_space = 0;
	m_free_space = 0;

	m_gps_state = State_GPS_on;

	m_video_resolution = Video_Resolution_1080p30;
	m_video_quality = Video_Quality_Super;
	m_overlay_flags = 0x0f;

	m_rec_mode = Rec_Mode_Manual;
	m_color_mode = Color_Mode_NORMAL;

#ifdef ARCH_S2L
	mp_camera_name = string_t::CreateFrom("S2L");
#else
	mp_camera_name = string_t::CreateFrom("A5S");
#endif

	m_total_pictures = 0;
	m_stillcap_state = STILLCAP_STATE_IDLE;
	m_stillcap_last_error = 0;
	m_num_pictures = 0;
	m_burst_ticks = 0;
	m_raw = 0;

	m_lcd_brightness = 255;
}

CCameraState::~CCameraState()
{
	avf_safe_release(m_battery_state);
	avf_safe_release(mp_camera_name);
}

avf_status_t CCameraState::RegisterOnStateChangedListener(ICameraStateListener *listener)
{
	AUTO_LOCK(mMutex);
	for (int i = 0; i < (int)ARRAY_SIZE(mpListeners); i++) {
		if (mpListeners[i] == NULL) {
			mpListeners[i] = listener;
			return E_OK;
		}
	}
	return E_ERROR;
}

avf_status_t CCameraState::UnRegisterOnStateChangedListener(ICameraStateListener *listener)
{
	AUTO_LOCK(mMutex);
	for (int i = 0; i < (int)ARRAY_SIZE(mpListeners); i++) {
		if (mpListeners[i] == listener) {
			mpListeners[i] = NULL;
			return E_OK;
		}
	}
	return E_ERROR;
}

void CCameraState::OnStateChangedL(int which)
{
	for (int i = 0; i < (int)ARRAY_SIZE(mpListeners); i++) {
		if (mpListeners[i] != NULL) {
			mpListeners[i]->OnStateChanged(which);
		}
	}
}

void CCameraState::OnRecordErrorL(enum Rec_Error_code code)
{
	for (int i = 0; i < (int)ARRAY_SIZE(mpListeners); i++) {
		if (mpListeners[i] != NULL) {
			mpListeners[i]->OnRecordError(code);
		}
	}
}

void CCameraState::OnStillCaptureStartedL(int one_shot)
{
	for (int i = 0; i < (int)ARRAY_SIZE(mpListeners); i++) {
		if (mpListeners[i] != NULL) {
			mpListeners[i]->OnStillCaptureStarted(one_shot);
		}
	}
}

void CCameraState::RecStateChanged(enum State_Rec state, int is_still)
{
	AUTO_LOCK(mMutex);
	if (m_rec_state != state || m_is_still != is_still) {
		m_rec_state = state;
		m_is_still = is_still;
		if (state == State_Rec_recording) {
			m_start_rec_tick = avf_get_curr_tick();
		}
		OnStateChangedL(ICameraStateListener::RecState);
	}
}

int CCameraState::GetRecDuration()
{
	AUTO_LOCK(mMutex);

	if (!m_is_still) {
		if (m_rec_state == State_Rec_recording || m_rec_state == State_Rec_stopping) {
			avf_u64_t tick = avf_get_curr_tick();
			return (int)((tick - m_start_rec_tick) / 1000);
		}
	}

	return 0;
}

void CCameraState::MicStateChanged(enum State_Mic state, int vol)
{
	AUTO_LOCK(mMutex);
	if (m_mic_state != state || m_mic_vol != vol) {
		m_mic_state = state;
		m_mic_vol = vol;
		OnStateChangedL(ICameraStateListener::MicState);
	}
}

void CCameraState::PowerStateChanged(enum State_power state)
{
	AUTO_LOCK(mMutex);
	if (m_power_state != state) {
		m_power_state = state;
		OnStateChangedL(ICameraStateListener::PowerBatteryState);
	}
}

void CCameraState::BatteryStateChanged(const char *state)
{
	AUTO_LOCK(mMutex);
	if (m_battery_state == NULL || ::strcmp(m_battery_state->string(), state) != 0) {
		avf_safe_release(m_battery_state);
		m_battery_state = string_t::CreateFrom(state);
		OnStateChangedL(ICameraStateListener::PowerBatteryState);
	}
}

void CCameraState::StorageStateChanged(enum State_storage state)
{
	AUTO_LOCK(mMutex);
	if (m_storage_state != state) {
		m_storage_state = state;
		OnStateChangedL(ICameraStateListener::StorageState);
	}
}

void CCameraState::StorageSpaceInfoChanged(uint64_t total_space, uint64_t free_space)
{
	AUTO_LOCK(mMutex);
	if (m_total_space != total_space || m_free_space != free_space) {
		m_total_space = total_space;
		m_free_space = free_space;
		OnStateChangedL(ICameraStateListener::StorageSpaceInfo);
	}
}

void CCameraState::GPSStateChanged(enum State_GPS state)
{
	AUTO_LOCK(mMutex);
	if (m_gps_state != state) {
		m_gps_state = state;
		OnStateChangedL(ICameraStateListener::GpsState);
	}
}

void CCameraState::VideoResolutionChanged(enum Video_Resolution resolution)
{
	AUTO_LOCK(mMutex);
	if (m_video_resolution != resolution) {
		m_video_resolution = resolution;
		OnStateChangedL(ICameraStateListener::VideoResolution);
	}
}

void CCameraState::VideoQualityChanged(enum Video_Quality quality)
{
	AUTO_LOCK(mMutex);
	if (m_video_quality != quality) {
		m_video_quality = quality;
		OnStateChangedL(ICameraStateListener::VideoQuality);
	}
}

void CCameraState::OverlayFlagsChanged(int value)
{
	AUTO_LOCK(mMutex);
	if (m_overlay_flags != value) {
		m_overlay_flags = value;
		OnStateChangedL(ICameraStateListener::OverlayFlags);
	}
}

void CCameraState::RecordModeChanged(enum Rec_Mode mode)
{
	AUTO_LOCK(mMutex);
	if (m_rec_mode != mode) {
		m_rec_mode = mode;
		OnStateChangedL(ICameraStateListener::RecordMode);
	}
}

void CCameraState::RecordModeChanged(int auto_record, int auto_del)
{
	enum Rec_Mode mode;
	if (auto_record) {
		mode = auto_del ? Rec_Mode_AutoStart_circle : Rec_Mode_AutoStart;
	} else {
		mode = auto_del ? Rec_Mode_Manual_circle : Rec_Mode_Manual;
	}
	RecordModeChanged(mode);
}

void CCameraState::ColorModeChanged(enum Color_Mode mode)
{
	AUTO_LOCK(mMutex);
	if (m_color_mode != mode) {
		m_color_mode = mode;
		OnStateChangedL(ICameraStateListener::ColorMode);
	}
}

void CCameraState::LCDBrightnessChanged(int brightness)
{
	AUTO_LOCK(mMutex);
	if (m_lcd_brightness != brightness) {
		m_lcd_brightness = brightness;
		OnStateChangedL(ICameraStateListener::LCDBrightness);
	}
}

void CCameraState::LCDVideoFlipChanged(int flip)
{
	// TODO
}

void CCameraState::LCDOSDFlipChanged(int flip)
{
	// TODO
}

int CCameraState::CameraNameChanged(string_t *name)
{
	AUTO_LOCK(mMutex);
	if (::strcmp(mp_camera_name->string(), name->string())) {
		avf_assign(mp_camera_name, name);
		OnStateChangedL(ICameraStateListener::CameraName);
		return 1;
	}
	return 0;
}

void CCameraState::TotalNumPicturesChanged(int total)
{
	AUTO_LOCK(mMutex);
	if (m_total_pictures != total) {
		m_total_pictures = total;
		AVF_LOGD("total pictures: %d", total);
		OnStateChangedL(ICameraStateListener::TotalNumPictures);
	}
}

const char *CCameraState::GetStillCapStateName(int stillcap_state)
{
	switch (stillcap_state) {
	case STILLCAP_STATE_IDLE: return "Idle";
	case STILLCAP_STATE_SINGLE: return "Single";
	case STILLCAP_STATE_BURST: return "Burst";
	case STILLCAP_STATE_STOPPING: return "Stopping";
	default: return "Unknown";
	}
}

void CCameraState::StillCaptureStateChanged(int stillcap_state, int last_error)
{
	AUTO_LOCK(mMutex);
	if (m_stillcap_state != stillcap_state || m_stillcap_last_error != last_error) {
		m_stillcap_state = stillcap_state;
		m_stillcap_last_error = last_error;
		AVF_LOGD("stillcap state changed to " C_BROWN "%s" C_NONE ", error %d",
			GetStillCapStateName(stillcap_state), last_error);
		OnStateChangedL(ICameraStateListener::StillCaptureState);
	}
}

void CCameraState::PictureTaken(int num_pictures, int burst_ticks)
{
	AUTO_LOCK(mMutex);
	if (m_num_pictures != num_pictures || m_burst_ticks != burst_ticks) {
		m_num_pictures = num_pictures;
		m_burst_ticks = burst_ticks;
		AVF_LOGD("stillcap picture taken, %d pictures, ticks %d", num_pictures, burst_ticks);
		OnStateChangedL(ICameraStateListener::StillCaptureState);
	}
}

void CCameraState::SetRawMode(int raw)
{
	AUTO_LOCK(mMutex);
	m_raw = raw;
	OnStateChangedL(ICameraStateListener::Raw);
}

void CCameraState::NotifyRecordError(enum Rec_Error_code code)
{
	AUTO_LOCK(mMutex);
	OnRecordErrorL(code);
}

void CCameraState::NotifyStillCaptureStarted(int one_shot)
{
	AUTO_LOCK(mMutex);
	OnStillCaptureStartedL(one_shot);
}


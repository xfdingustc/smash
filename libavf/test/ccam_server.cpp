

#include "avf_socket.h"
#include "avf_tcp_server.h"
#include "mem_stream.h"
#include "ccam_cmd.h"
#include "oxml.h"

class CCamConnection;
class CCamServer;

//{776293B9-9A94-4B8A-9BAC-BDC75552C262}
AVF_DEFINE_IID(IID_ICameraControl,
0x776293B9, 0x9A94, 0x4B8A, 0x9B, 0xAC, 0xBD, 0xC7, 0x55, 0x52, 0xC2, 0x62);

//-----------------------------------------------------------------------
//
//  ICameraControl
//
//-----------------------------------------------------------------------

class ICameraControl: public IInterface
{
public:
	DECLARE_IF(ICameraControl, IID_ICameraControl);

public:
	virtual avf_status_t StartRecord() = 0;
	virtual avf_status_t StopRecord() = 0;
	virtual avf_status_t EnterIdle() = 0;
	virtual avf_status_t ChangeVideoResolution(int resolution) = 0;
	virtual avf_status_t ChangeVideoQuality(int quality) = 0;
	virtual avf_status_t ChangeRecMode(int value) = 0;
	virtual avf_status_t ChangeColorMode(int value) = 0;
	virtual avf_status_t ChangeOverlayFlags(int value) = 0;
	virtual avf_status_t ChangeCameraName(string_t *name) = 0;
	virtual avf_status_t NetworkSyncTime(uint32_t time, int timezone) = 0;
	virtual avf_status_t SetStillCaptureMode(bool bStillMode) = 0;
	virtual avf_status_t StartStillCapture(bool bOneShot) = 0;
	virtual avf_status_t StopStillCapture() = 0;
	virtual avf_status_t WantIdle() = 0;
	virtual avf_status_t WantPreview() = 0;
	virtual avf_status_t ChangeLCDBrightness(int value) = 0;
	virtual avf_status_t FlipLCDVideo(int flip) = 0;
};

//-----------------------------------------------------------------------
//
//  CCamConnection
//
//-----------------------------------------------------------------------

class CCamConnection: public CTCPConnection
{
	typedef CTCPConnection inherited;

	enum {
		BASE_CMD_SIZE = 128,
	};

public:
	static CCamConnection *Create(CCamServer *pServer, CSocket **ppSocket, 
		CCameraState *pCameraState, ICameraControl *pCameraControl);

public:
	void StateChanged(int which, bool bCheckClose);
	void RecordError(enum Rec_Error_code code, bool bCheckClose);
	void StillCaptureStarted(int one_shot, bool bCheckClose);

private:
	CCamConnection(CCamServer *pServer, CSocket **ppSocket,
		CCameraState *pCameraState, ICameraControl *pCameraControl);
	virtual ~CCamConnection();

protected:
	virtual void ThreadLoop();

private:
	avf_status_t ReadCmd();
	avf_status_t ProcessCmd();

private:
	avf_status_t ProcessCamDomainCmd(int cmd);
	avf_status_t ProcessRecDomainCmd(int cmd);

private:
	avf_status_t ResizeBuffer(avf_size_t size);

private:
	avf_status_t SendRecState(bool bCheckClose);
	avf_status_t SendRecDuration(bool bCheckClose);
	avf_status_t SendMicState(bool bCheckClose);
	avf_status_t SendPowerBatteryState(bool bCheckClose);
	avf_status_t SendBatteryInfo(bool bCheckClose);
	avf_status_t SendStorageState(bool bCheckClose);
	avf_status_t SendStorageSpaceInfo(bool bCheckClose);
	avf_status_t SendVideoResolutionList(bool bCheckClose);
	avf_status_t SendVideoResolution(bool bCheckClose);
	avf_status_t SendVideoQualityList(bool bCheckClose);
	avf_status_t SendVideoQuality(bool bCheckClose);
	avf_status_t SendOverlayFlags(bool bCheckClose);
	avf_status_t SendRecModeList(bool bCheckClose);
	avf_status_t SendRecordMode(bool bCheckClose);
	avf_status_t SendColorModeList(bool bCheckClose);
	avf_status_t SendColorMode(bool bCheckClose);
	avf_status_t SendLCDBrightness(bool bCheckClose);

	avf_status_t SendServiceVersion(bool bCheckClose);
	avf_status_t SendCameraName(bool bCheckClose);
	avf_status_t SendCameraMode(bool bCheckClose);
	avf_status_t SendGPSState(bool bCheckClose);

	avf_status_t SendFwVersion(bool bCheckClose);
	avf_status_t SendAllInfo(bool bCheckClose);

	avf_status_t SendTotalNumPictures(bool bCheckClose);
	avf_status_t SendStillCaptureState(bool bCheckClose);
	avf_status_t SendRawMode(bool bCheckClose);

	avf_status_t SendBtIsSupported(bool bCheckClose);
	avf_status_t SendBtIsEnabled(bool bCheckClose);

private:
	avf_status_t ActionStartRecord();
	avf_status_t ActionStopRecord();
	avf_status_t ActionChangeVideoResolution();
	avf_status_t ActionChangeVideoQuality();
	avf_status_t ActionSetRecMode();
	avf_status_t ActionSetColorMode();
	avf_status_t ActionSetOverlayFlags();
	avf_status_t ActionSetCameraName();
	avf_status_t ActionSetMic();
	avf_status_t ActionNetworkSyncTime();
	avf_status_t ActionSetStillMode();
	avf_status_t ActionStartStillCapture();
	avf_status_t ActionStopStillCapture();
	avf_status_t ActionWantIdle();
	avf_status_t ActionWantPreview();

private:
	avf_status_t SendResult(int domain, int msg, const char *p1, const char *p2, bool bCheckClose);
	avf_status_t SendIntResult(int domain, int msg, int p1, bool bCheckClose);
	avf_status_t SendIntResult(int domain, int msg, int p1, int p2, bool bCheckClose);

private:
	avf_u8_t *mp_cmd_buffer;
	avf_size_t m_cmd_buffer_size;
	CXmlDoc *mp_cmd_doc;
	const char *mp_act;	// points to mp_cmd_doc
	const char *mp_p1;	// points to mp_cmd_doc
	const char *mp_p2;	// points to mp_cmd_doc
	CCameraState *mpCameraState;
	ICameraControl *mpCameraControl;
	CMutex mWriteMutex;
	CMemBuffer *mpMemBuffer;
};

//-----------------------------------------------------------------------
//
//  CCamServer
//
//-----------------------------------------------------------------------

class CCamServer: public CTCPServer, public ICameraStateListener
{
	typedef CTCPServer inherited;
	DEFINE_INTERFACE;

public:
	static CCamServer *Create(CCameraState *pCameraState, ICameraControl *pCameraControl);
	avf_status_t Run(int port) {
		return RunServer(port);
	}

private:
	CCamServer(CCameraState *pCameraState, ICameraControl *pCameraControl);
	virtual ~CCamServer();
	void RunThread();

private:
	static void ThreadEntry(void *p) {
		((CCamServer*)p)->ThreadLoop();
	}
	void ThreadLoop();

// ICameraStateListener
public:
	virtual void OnStateChanged(int which) {
		PostMsg(MSG_StateChanged, which, 0);
	}
	virtual void OnRecordError(enum Rec_Error_code code) {
		PostMsg(MSG_RecordError, code, 0);
	}
	virtual void OnStillCaptureStarted(int one_shot) {
		PostMsg(MSG_StillCaptureStarted, one_shot, 0);
	}

protected:
	virtual CTCPConnection *CreateConnection(CSocket **ppSocket) {
		return CCamConnection::Create(this, ppSocket, mpCameraState, mpCameraControl);
	}

private:
	enum {
		MSG_None = 0,
		MSG_StateChanged,
		MSG_RecordError,
		MSG_StillCaptureStarted,
	};

	struct msg_s {
		int code;
		avf_intptr_t p1;
		avf_intptr_t p2;
	};

private:
	void BroadcastMsg(msg_s& msg);
	void PostMsg(int code, int p1, int p2);

private:
	CCameraState *mpCameraState;
	ICameraControl *mpCameraControl;
	CThread *mpThread;
	CMsgQueue *mpMsgQueue;
};

//-----------------------------------------------------------------------
//
//  CCamConnection
//
//-----------------------------------------------------------------------

CCamConnection *CCamConnection::Create(CCamServer *pServer, CSocket **ppSocket, 
		CCameraState *pCameraState, ICameraControl *pCameraControl)
{
	CCamConnection *result = avf_new CCamConnection(pServer, ppSocket, 
		pCameraState, pCameraControl);
	if (result) {
		result->RunThread("ccam-conn");
	}
	CHECK_STATUS(result);
	return result;
}

CCamConnection::CCamConnection(CCamServer *pServer, CSocket **ppSocket, 
		CCameraState *pCameraState, ICameraControl *pCameraControl):
	inherited(pServer, ppSocket),
	mp_cmd_buffer(NULL),
	m_cmd_buffer_size(0),
	mp_cmd_doc(NULL),
	mpCameraState(pCameraState),
	mpCameraControl(pCameraControl),
	mpMemBuffer(NULL)
{
	CREATE_OBJ(mpMemBuffer = CMemBuffer::Create(1*1024));
}

CCamConnection::~CCamConnection()
{
	avf_safe_release(mp_cmd_doc);
	avf_safe_free(mp_cmd_buffer);
	avf_safe_release(mpMemBuffer);
}

void CCamConnection::ThreadLoop()
{
	avf_status_t status;

	m_cmd_buffer_size = 1024;
	if ((mp_cmd_buffer = avf_malloc_bytes(m_cmd_buffer_size)) == NULL)
		return;

	mp_cmd_doc = CXmlDoc::Create();
	if (mp_cmd_doc == NULL)
		return;

	while (true) {
		mp_act = NULL;

		if ((status = ReadCmd()) != E_OK)
			break;

		if (mp_act) {
			ProcessCmd();
		}
	}
}

avf_status_t CCamConnection::ResizeBuffer(avf_size_t size)
{
	if (size <= m_cmd_buffer_size)
		return E_OK;

	avf_u8_t *new_buffer = avf_malloc_bytes(size);
	if (new_buffer == NULL) {
		return E_NOMEM;
	}

	::memcpy(new_buffer, mp_cmd_buffer, BASE_CMD_SIZE);

	avf_u8_t *tmp = mp_cmd_buffer;
	mp_cmd_buffer = new_buffer;
	avf_free(tmp);

	m_cmd_buffer_size = size;

	return E_OK;
}

avf_status_t CCamConnection::ReadCmd()
{
	avf_status_t status;

	status = TCPReceive(-1, mp_cmd_buffer, BASE_CMD_SIZE);
	if (status != E_OK)
		return status;

	//avf_size_t length = load_be_32(mp_cmd_buffer);
	avf_size_t appended = load_be_32(mp_cmd_buffer + 4);
	if (appended > 0) {
		status = ResizeBuffer(BASE_CMD_SIZE + appended);
		if (status != E_OK)
			return status;
		status = TCPReceive(10*1000, mp_cmd_buffer + BASE_CMD_SIZE, appended);
		if (status != E_OK)
			return status;
	}

	mp_cmd_doc->Clear();
	if (!mp_cmd_doc->Parse(mp_cmd_buffer + 8)) {
		AVF_LOGW("doc has error");
		return E_OK;
	}

	CXmlElement *elem = mp_cmd_doc->GetElementByName(ELEM_ROOT, "cmd");
	if (elem == NULL) {
		AVF_LOGW("no ccev");
		return E_OK;
	}

	CXmlAttribute *act = mp_cmd_doc->GetAttributeByName(elem, "act");
	if (act == NULL) {
		AVF_LOGW("no act");
		return E_OK;
	}

	CXmlAttribute *p1 = mp_cmd_doc->GetAttributeByName(elem, "p1");
	if (act == NULL) {
		AVF_LOGW("no p1");
		return E_OK;
	}

	CXmlAttribute *p2 = mp_cmd_doc->GetAttributeByName(elem, "p2");
	if (act == NULL) {
		AVF_LOGW("no p2");
		return E_OK;
	}

	mp_act = act->Value();
	mp_p1 = p1->Value();
	if (mp_p1 == NULL) mp_p1 = "";
	mp_p2 = p2->Value();
	if (mp_p2 == NULL) mp_p2 = "";

	return E_OK;
}

avf_status_t CCamConnection::ProcessCmd()
{
	int domain;
	int cmd;

	if (::sscanf(mp_act, "ECMD%d.%d", &domain, &cmd) != 2) {
		AVF_LOGW("bad act %s", mp_act);
		return E_OK;
	}

	//AVF_LOGI("cmd %d.%d", domain, cmd);

	switch (domain) {
	case CMD_Domain_cam: return ProcessCamDomainCmd(cmd);
	case CMD_Domain_rec: return ProcessRecDomainCmd(cmd);
	default: AVF_LOGW("unknown domain %d", domain); return E_OK;
	}
}

void CCamConnection::StateChanged(int which, bool bCheckClose)
{
	switch (which) {
	case ICameraStateListener::RecState: SendRecState(bCheckClose); break;
	case ICameraStateListener::MicState: SendMicState(bCheckClose); break;
	case ICameraStateListener::PowerBatteryState: SendPowerBatteryState(bCheckClose); break;
	case ICameraStateListener::StorageState: SendStorageState(bCheckClose); break;
	case ICameraStateListener::StorageSpaceInfo: SendStorageSpaceInfo(bCheckClose); break;
	case ICameraStateListener::GpsState: SendGPSState(bCheckClose); break;
	case ICameraStateListener::VideoResolution: SendVideoResolution(bCheckClose); break;
	case ICameraStateListener::VideoQuality: SendVideoQuality(bCheckClose); break;
	case ICameraStateListener::OverlayFlags: SendOverlayFlags(bCheckClose); break;
	case ICameraStateListener::RecordMode: SendRecordMode(bCheckClose); break;
	case ICameraStateListener::ColorMode: SendColorMode(bCheckClose); break;
	case ICameraStateListener::CameraName: SendCameraName(bCheckClose); break;
	case ICameraStateListener::TotalNumPictures: SendTotalNumPictures(bCheckClose); break;
	case ICameraStateListener::StillCaptureState: SendStillCaptureState(bCheckClose); break;
	case ICameraStateListener::Raw: SendRawMode(bCheckClose); break;
	case ICameraStateListener::LCDBrightness: SendLCDBrightness(bCheckClose); break;
	default: AVF_LOGW("StateChanged(%d) not handled", which); break;
	}
}

void CCamConnection::RecordError(enum Rec_Error_code code, bool bCheckClose)
{
	SendIntResult(CMD_Domain_cam, CMD_Rec_error, code, bCheckClose);
}

void CCamConnection::StillCaptureStarted(int one_shot, bool bCheckClose)
{
	SendIntResult(CMD_Domain_rec, CMD_Rec_StartStillCapture, one_shot, bCheckClose);
}

avf_status_t CCamConnection::SendRecState(bool bCheckClose)
{
	enum State_Rec state;
	int is_still;
	mpCameraState->GetRecState(&state, &is_still);
	return SendIntResult(CMD_Domain_cam, CMD_Cam_get_State_result, state, is_still, bCheckClose);
}

avf_status_t CCamConnection::SendRecDuration(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_cam, CMD_Cam_get_time_result,
		mpCameraState->GetRecDuration(), bCheckClose);
}

avf_status_t CCamConnection::SendMicState(bool bCheckClose)
{
	enum State_Mic state;
	int vol;
	mpCameraState->GetMicState(&state, &vol);
	return SendIntResult(CMD_Domain_cam, CMD_Cam_msg_Mic_infor, state, vol, bCheckClose);
}

avf_status_t CCamConnection::SendPowerBatteryState(bool bCheckClose)
{
	enum State_power power;
	string_t *battery;
	mpCameraState->GetPowerState(&power, &battery);
	char p2[16];
	sprintf(p2, "%d", power);
	avf_status_t status = SendResult(CMD_Domain_cam, CMD_Cam_msg_power_infor,
		battery->string(), p2, bCheckClose);
	battery->Release();
	return status;
}

avf_status_t CCamConnection::SendBatteryInfo(bool bCheckClose)
{
	// TODO: read from device
	return SendResult(CMD_Domain_cam, CMD_Cam_msg_Battery_infor, "3467", "100", bCheckClose);
}

avf_status_t CCamConnection::SendStorageState(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_cam, CMD_Cam_msg_Storage_infor,
		mpCameraState->GetStorageState(), bCheckClose);
}

avf_status_t CCamConnection::SendStorageSpaceInfo(bool bCheckClose)
{
	uint64_t total_space;
	uint64_t free_space;
	mpCameraState->GetStorageSpaceInfo(&total_space, &free_space);
	char p1[32];
	char p2[32];
	sprintf(p1, LLD, total_space);
	sprintf(p2, LLD, free_space);
	return SendResult(CMD_Domain_cam, CMD_Cam_msg_StorageSpace_infor, p1, p2, bCheckClose);
}

avf_status_t CCamConnection::SendVideoResolutionList(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_List_Resolutions,
#ifdef ARCH_S2L
		(1<<Video_Resolution_1080p60) |
		(1<<Video_Resolution_720p120) |
		(1<<Video_Resolution_Still) |
		(1<<Video_Resolution_QXGAp30) |
#endif
		(1<<Video_Resolution_1080p30) |
		(1<<Video_Resolution_720p30) |
		(1<<Video_Resolution_720p60) |
		(1<<Video_Resolution_480p30),
		bCheckClose
	);
}

avf_status_t CCamConnection::SendVideoResolution(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_get_Resolution,	
		mpCameraState->GetVideoResolution(), bCheckClose);
}

avf_status_t CCamConnection::SendVideoQualityList(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_List_Qualities,
		(1<<Video_Quality_Super) | (1<<Video_Quality_HI) |
		(1<<Video_Quality_Mid) | (1<<Video_Quality_LOW), bCheckClose);
}

avf_status_t CCamConnection::SendVideoQuality(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_get_Quality,
		mpCameraState->GetVideoQuality(), bCheckClose);
}

avf_status_t CCamConnection::SendOverlayFlags(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_getOverlayState,
		mpCameraState->GetOverlayFlags(), bCheckClose);
}

avf_status_t CCamConnection::SendRecModeList(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_List_RecModes,
		(1<<Rec_Mode_Manual) | (1<<Rec_Mode_AutoStart) |
		(1<<Rec_Mode_Manual_circle) | (1<<Rec_Mode_AutoStart_circle), bCheckClose);
}

avf_status_t CCamConnection::SendRecordMode(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_get_RecMode,
		mpCameraState->GetRecMode(), bCheckClose);
}

avf_status_t CCamConnection::SendColorModeList(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_List_ColorModes,
		(1<<Color_Mode_NORMAL) | (1<<Color_Mode_SPORT) |
		(1<<Color_Mode_CARDV) | (1<<Color_Mode_SCENE), bCheckClose);
}

avf_status_t CCamConnection::SendColorMode(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_rec, CMD_Rec_get_ColorMode,
		mpCameraState->GetColorMode(), bCheckClose);
}

avf_status_t CCamConnection::SendLCDBrightness(bool bCheckClose)
{
	//	TODO
	return E_OK;
}

avf_status_t CCamConnection::SendServiceVersion(bool bCheckClose)
{
	return SendResult(CMD_Domain_cam, CMD_Cam_getAPIVersion, SERVICE_VERSION, NULL, bCheckClose);
}

avf_status_t CCamConnection::SendCameraName(bool bCheckClose)
{
	// TODO: string in xml
	string_t *name = mpCameraState->GetCameraName();
	avf_status_t status = SendResult(CMD_Domain_cam, CMD_Cam_get_Name_result, 
		name->string(), NULL, bCheckClose);
	name->Release();
	return status;
}

avf_status_t CCamConnection::SendTotalNumPictures(bool bCheckClose)
{
	int total = mpCameraState->GetTotalNumPictures();
	return SendIntResult(CMD_Domain_rec, MSG_Rec_TotalPictures, total, bCheckClose);
}

avf_status_t CCamConnection::SendStillCaptureState(bool bCheckClose)
{
	int stillcap_state;
	int num_pictures;
	int burst_ticks;
	mpCameraState->GetStillCapInfo(&stillcap_state, &num_pictures, &burst_ticks);
	//AVF_LOGD(C_GREEN "capturing: %d, num_pic: %d" C_NONE, capturing, num_pictures);
	return SendIntResult(CMD_Domain_rec, MSG_Rec_StillPictureState, 
		(burst_ticks << 4) | stillcap_state, num_pictures, bCheckClose);
}

avf_status_t CCamConnection::SendRawMode(bool bCheckClose)
{
	int raw = mpCameraState->GetRawMode();
	return SendResult(CMD_Domain_rec, MSG_Rec_RawMode, raw ? "1" : "0", NULL, bCheckClose);
}

avf_status_t CCamConnection::SendCameraMode(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_cam, CMD_Cam_getMode_result,
		mpCameraState->GetCameraMode(), bCheckClose);
}

avf_status_t CCamConnection::SendGPSState(bool bCheckClose)
{
	return SendIntResult(CMD_Domain_cam, CMD_Cam_msg_GPS_infor,
		mpCameraState->GetGpsState(), bCheckClose);
}

avf_status_t CCamConnection::SendFwVersion(bool bCheckClose)
{
	return SendResult(CMD_Domain_cam, CMD_fw_getVersion,
		"B4994C354DBB0067DS0001CA@agb_diablo", "1.0.236120.3344.647", bCheckClose);
}

avf_status_t CCamConnection::SendBtIsSupported(bool bCheckClose)
{
	return SendResult(CMD_Domain_cam, CMD_CAM_BT_isSupported, "0", NULL, bCheckClose);
}

avf_status_t CCamConnection::SendBtIsEnabled(bool bCheckClose)
{
	return SendResult(CMD_Domain_cam, CMD_CAM_BT_isEnabled, "0", NULL, bCheckClose);
}

avf_status_t CCamConnection::SendAllInfo(bool bCheckClose)
{
	avf_status_t status;

	if ((status = SendMicState(bCheckClose)) != E_OK)
		return status;

	if ((status = SendPowerBatteryState(bCheckClose)) != E_OK)
		return status;

	if ((status = SendBatteryInfo(bCheckClose)) != E_OK)
		return status;

	if ((status = SendStorageState(bCheckClose)) != E_OK)
		return status;

	if ((status = SendStorageSpaceInfo(bCheckClose)) != E_OK)
		return status;

	if ((status = SendGPSState(bCheckClose)) != E_OK)
		return status;

	return E_OK;
}

#define XML_HEAD \
	"<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>" \
	"<ccev><cmd act=\"ECMD"

#define XML_P1 \
	"\" p1=\""

#define XML_P2 \
	"\" p2=\""

#define XML_TAIL \
	"\" /></ccev>"

// <ccev><cmd act="ECMD0.2" p1="xxx" p2="xxx" /></ccev>
avf_status_t CCamConnection::SendResult(int domain, int msg, const char *p1, const char *p2, bool bCheckClose)
{
	AUTO_LOCK(mWriteMutex);
	char buffer[64];

	mpMemBuffer->Clear();

	avf_u8_t *p_length = mpMemBuffer->Alloc(4);
	avf_u8_t *p_appended = mpMemBuffer->Alloc(4);

	mpMemBuffer->write_mem((const avf_u8_t*)STR_WITH_SIZE(XML_HEAD));

	sprintf(buffer, "%d.%d", domain, msg);
	mpMemBuffer->write_string(buffer);

	mpMemBuffer->write_mem((const avf_u8_t*)STR_WITH_SIZE(XML_P1));
	if (p1) {
		mpMemBuffer->write_string(p1);
	}

	mpMemBuffer->write_mem((const avf_u8_t*)STR_WITH_SIZE(XML_P2));
	if (p2) {
		mpMemBuffer->write_string(p2);
	}

	mpMemBuffer->write_mem((const avf_u8_t*)STR_WITH_SIZE(XML_TAIL));
	mpMemBuffer->write_le_8(0);

	int total = mpMemBuffer->GetTotalSize();
	if (total <= BASE_CMD_SIZE) {
		SAVE_LE_32(p_length, BASE_CMD_SIZE);
		SAVE_LE_32(p_appended, 0);
	} else {
		SAVE_LE_32(p_length, total);
		SAVE_LE_32(p_appended, total - BASE_CMD_SIZE);
	}

	CMemBuffer::block_t *block = mpMemBuffer->GetFirstBlock();

//#if AVF_DEBUG
//	printf("%s\n", block->GetMem() + 8);
//#endif

	for (; block; block = block->GetNext()) {
		avf_status_t status = TCPSend(10*1000, block->GetMem(), block->GetSize());
		if (status != E_OK)
			return status;
	}

	if (total < BASE_CMD_SIZE) {
		avf_u8_t buf[BASE_CMD_SIZE];
		int appended = BASE_CMD_SIZE - total;
		::memset(buf, 0, appended);
		avf_status_t status = TCPSend(10*1000, buf, appended);
		if (status != E_OK)
			return status;
	}

	return E_OK;
}

avf_status_t CCamConnection::SendIntResult(int domain, int msg, int p1, bool bCheckClose)
{
	char buffer[16];
	sprintf(buffer, "%d", p1);
	return SendResult(domain, msg, buffer, NULL, bCheckClose);
}

avf_status_t CCamConnection::SendIntResult(int domain, int msg, int p1, int p2, bool bCheckClose)
{
	char buf1[16];
	char buf2[16];
	sprintf(buf1, "%d", p1);
	sprintf(buf2, "%d", p2);
	return SendResult(domain, msg, buf1, buf2, bCheckClose);
}

avf_status_t CCamConnection::ActionStartRecord()
{
	AVF_LOGD(C_CYAN "ActionStartRecord" C_NONE);
	enum State_Rec state;
	int is_still;
	mpCameraState->GetRecState(&state, &is_still);
	if (!is_still && state == State_Rec_recording) {
		return mpCameraControl->StopRecord();
	} else {
		return mpCameraControl->StartRecord();
	}
}

avf_status_t CCamConnection::ActionStopRecord()
{
	AVF_LOGD(C_CYAN "ActionStopRecord" C_NONE);
	return mpCameraControl->StopRecord();
}

avf_status_t CCamConnection::ActionChangeVideoResolution()
{
	AVF_LOGD(C_CYAN "ActionChangeVideoResolution %s" C_NONE, mp_p1);
	int index = ::atoi(mp_p1);
	return mpCameraControl->ChangeVideoResolution(index);
}

avf_status_t CCamConnection::ActionChangeVideoQuality()
{
	AVF_LOGD(C_CYAN "ActionChangeVideoQuality %s" C_NONE, mp_p1);
	int index = ::atoi(mp_p1);
	return mpCameraControl->ChangeVideoQuality(index);
}

avf_status_t CCamConnection::ActionSetRecMode()
{
	AVF_LOGD(C_CYAN "ActionSetRecMode %s" C_NONE, mp_p1);
	int value = ::atoi(mp_p1);
	return mpCameraControl->ChangeRecMode(value);
}

avf_status_t CCamConnection::ActionSetColorMode()
{
	AVF_LOGD(C_CYAN "ActionSetColorMode %s" C_NONE, mp_p1);
	int value = ::atoi(mp_p1);
	return mpCameraControl->ChangeColorMode(value);
}

avf_status_t CCamConnection::ActionSetOverlayFlags()
{
	AVF_LOGD(C_CYAN "ActionSetOverlayFlags %s" C_NONE, mp_p1);
	int value = ::atoi(mp_p1);
	return mpCameraControl->ChangeOverlayFlags(value);
}

avf_status_t CCamConnection::ActionSetCameraName()
{
	AVF_LOGD(C_CYAN "ActionSetCameraName %s" C_NONE, mp_p1);
	string_t *name = string_t::CreateFrom(mp_p1);
	avf_status_t status = mpCameraControl->ChangeCameraName(name);
	name->Release();
	return status;
}

avf_status_t CCamConnection::ActionSetMic()
{
	AVF_LOGD(C_CYAN "ActionSetMic %s, %s" C_NONE, mp_p1, mp_p2);
	int state = ::atoi(mp_p1);
	int vol = ::atoi(mp_p2);
	// TODO: implementation
	mpCameraState->MicStateChanged((enum State_Mic)state, vol);
	return E_OK;
}

avf_status_t CCamConnection::ActionNetworkSyncTime()
{
	AVF_LOGD(C_CYAN "ActionNetworkSyncTime %s, %s" C_NONE, mp_p1, mp_p2);
	uint32_t time = ::atoi(mp_p1);
	int timezone = ::atoi(mp_p2);
	return mpCameraControl->NetworkSyncTime(time, timezone);
}

avf_status_t CCamConnection::ActionSetStillMode()
{
	AVF_LOGD(C_CYAN "ActionSetStillMode %s" C_NONE, mp_p1);
	bool bStillMode = ::atoi(mp_p1) != 0;
	return mpCameraControl->SetStillCaptureMode(bStillMode);
}

avf_status_t CCamConnection::ActionStartStillCapture()
{
	AVF_LOGD(C_CYAN "ActionStartStillCapture %s" C_NONE, mp_p1);
	bool bOneShot = ::atoi(mp_p1) != 0;
	return mpCameraControl->StartStillCapture(bOneShot);
}

avf_status_t CCamConnection::ActionStopStillCapture()
{
	AVF_LOGD(C_CYAN "ActionStopStillCapture" C_NONE);
	return mpCameraControl->StopStillCapture();
}

avf_status_t CCamConnection::ActionWantIdle()
{
	AVF_LOGD(C_CYAN "ActionWantIdle" C_NONE);
	return mpCameraControl->WantIdle();
}

avf_status_t CCamConnection::ActionWantPreview()
{
	AVF_LOGD(C_CYAN "ActionWantPreview" C_NONE);
	return mpCameraControl->WantPreview();
}

#define NOIMPL(_name) \
	do { \
		AVF_LOGW("%s is not implemented", _name); \
	} while (0)

avf_status_t CCamConnection::ProcessCamDomainCmd(int cmd)
{
	bool bCheckClose = true;

	switch (cmd) {
	case CMD_Cam_getMode: SendCameraMode(bCheckClose); break;
	case CMD_Cam_getMode_result: NOIMPL("CMD_Cam_getMode_result"); break;
	case CMD_Cam_getAPIVersion: SendServiceVersion(bCheckClose); break;
	case CMD_Cam_isAPISupported: NOIMPL("CMD_Cam_isAPISupported"); break;
	case CMD_Cam_get_Name: SendCameraName(bCheckClose); break;
	case CMD_Cam_get_Name_result: SendCameraName(bCheckClose); break;
	case CMD_Cam_set_Name: ActionSetCameraName(); break;
	case CMD_Cam_set_Name_result: NOIMPL("CMD_Cam_set_Name_result"); break;
	case CMD_Cam_get_State: SendRecState(bCheckClose); break;
	case CMD_Cam_get_State_result: SendRecState(bCheckClose);	 break;
	case CMD_Cam_start_rec: ActionStartRecord(); break;
	case CMD_Cam_stop_rec: ActionStopRecord(); break;
	case CMD_Cam_get_time: SendRecDuration(bCheckClose); break;
	case CMD_Cam_get_time_result: SendRecDuration(bCheckClose); break;
	case CMD_Cam_get_getAllInfor: SendAllInfo(bCheckClose); break;
	case CMD_Cam_get_getStorageInfor: SendStorageState(bCheckClose); break;
	case CMD_Cam_msg_Storage_infor: SendStorageState(bCheckClose); break;
	case CMD_Cam_msg_StorageSpace_infor: SendStorageSpaceInfo(bCheckClose); break;
	case CMD_Cam_msg_Battery_infor: SendBatteryInfo(bCheckClose); break;
	case CMD_Cam_msg_power_infor: SendPowerBatteryState(bCheckClose); break;
	case CMD_Cam_msg_BT_infor: NOIMPL("CMD_Cam_msg_BT_infor"); break;
	case CMD_Cam_msg_GPS_infor: SendGPSState(bCheckClose); break;
	case CMD_Cam_msg_Internet_infor: NOIMPL("CMD_Cam_msg_Internet_infor"); break;
	case CMD_Cam_msg_Mic_infor: SendMicState(bCheckClose); break;
	case CMD_Cam_set_StreamSize: NOIMPL("CMD_Cam_set_StreamSize"); break;
	case CMD_Cam_PowerOff: NOIMPL("CMD_Cam_PowerOff"); break;
	case CMD_Cam_Reboot: NOIMPL("CMD_Cam_Reboot"); break;
	case CMD_Network_GetWLanMode: NOIMPL("CMD_Network_GetWLanMode"); break;
	case CMD_Network_GetHostNum: NOIMPL("CMD_Network_GetHostNum"); break;
	case CMD_Network_GetHostInfor: NOIMPL("CMD_Network_GetHostInfor"); break;
	case CMD_Network_AddHost: NOIMPL("CMD_Network_AddHost"); break;
	case CMD_Network_RmvHost: NOIMPL("CMD_Network_RmvHost"); break;
	case CMD_Network_ConnectHost: NOIMPL("CMD_Network_ConnectHost"); break;
	case CMD_Network_Synctime: ActionNetworkSyncTime(); break;
	case CMD_Network_GetDevicetime: NOIMPL("CMD_Network_GetDevicetime"); break;
	case CMD_Rec_error: NOIMPL("CMD_Rec_error"); break;
	case CMD_audio_setMic: ActionSetMic(); break;
	case CMD_audio_getMicState: SendMicState(bCheckClose); break;
	case CMD_fw_getVersion: SendFwVersion(bCheckClose); break;
	case CMD_fw_newVersion: NOIMPL("CMD_fw_newVersion"); break;
	case CMD_fw_doUpgrade: NOIMPL("CMD_fw_doUpgrade"); break;
	case CMD_CAM_BT_isSupported: SendBtIsSupported(bCheckClose); break;
	case CMD_CAM_BT_isEnabled: SendBtIsEnabled(bCheckClose); break;
	case CMD_CAM_BT_Enable: NOIMPL("CMD_CAM_BT_Enable"); break;
	case CMD_CAM_BT_getDEVStatus: NOIMPL("CMD_CAM_BT_getDEVStatus"); break;
	case CMD_CAM_BT_getHostNum: NOIMPL("CMD_CAM_BT_getHostNum"); break;
	case CMD_CAM_BT_getHostInfor: NOIMPL("CMD_CAM_BT_getHostInfor"); break;
	case CMD_CAM_BT_doScan: NOIMPL("CMD_CAM_BT_doScan"); break;
	case CMD_CAM_BT_doBind: NOIMPL("CMD_CAM_BT_doBind"); break;
	case CMD_CAM_BT_doUnBind: NOIMPL("CMD_CAM_BT_doUnBind"); break;
	case CMD_CAM_BT_setOBDTypes: NOIMPL("CMD_CAM_BT_setOBDTypes"); break;
	case CMD_CAM_BT_RESERVED: NOIMPL("CMD_CAM_BT_RESERVED"); break;
	case CMD_CAM_WantIdle: ActionWantIdle(); break;
	case CMD_CAM_WantPreview: ActionWantPreview(); break;
	default: AVF_LOGW("cmd %d.%d is not implemented", CMD_Domain_cam, cmd); break;
	}
	return E_OK;
}

avf_status_t CCamConnection::ProcessRecDomainCmd(int cmd)
{
	bool bCheckClose = true;

	switch (cmd) {
	case CMD_Rec_Start: NOIMPL("CMD_Rec_Start"); break;
	case CMD_Rec_Stop: NOIMPL("CMD_Rec_Stop"); break;
	case CMD_Rec_List_Resolutions: SendVideoResolutionList(bCheckClose); break;
	case CMD_Rec_Set_Resolution: ActionChangeVideoResolution(); break;
	case CMD_Rec_get_Resolution: SendVideoResolution(bCheckClose); break;
	case CMD_Rec_List_Qualities: SendVideoQualityList(bCheckClose); break;
	case CMD_Rec_Set_Quality: ActionChangeVideoQuality(); break;
	case CMD_Rec_get_Quality: SendVideoQuality(bCheckClose); break;
	case CMD_Rec_List_RecModes: SendRecModeList(bCheckClose); break;
	case CMD_Rec_Set_RecMode: ActionSetRecMode(); break;
	case CMD_Rec_get_RecMode: SendRecordMode(bCheckClose); break;
	case CMD_Rec_List_ColorModes: SendColorModeList(bCheckClose); break;
	case CMD_Rec_Set_ColorMode: ActionSetColorMode(); break;
	case CMD_Rec_get_ColorMode: SendColorMode(bCheckClose); break;
	case CMD_Rec_List_SegLens: NOIMPL("CMD_Rec_List_SegLens"); break;
	case CMD_Rec_Set_SegLen: NOIMPL("CMD_Rec_Set_SegLen"); break;
	case CMD_Rec_get_SegLen: NOIMPL("CMD_Rec_get_SegLen"); break;
	case CMD_Rec_get_State: NOIMPL("CMD_Rec_get_State"); break;
	case EVT_Rec_state_change: NOIMPL("EVT_Rec_state_change"); break;
	case CMD_Rec_getTime: NOIMPL("CMD_Rec_getTime"); break;
	case CMD_Rec_getTime_result: NOIMPL("CMD_Rec_getTime_result"); break;
	case CMD_Rec_setDualStream: NOIMPL("CMD_Rec_setDualStream"); break;
	case CMD_Rec_getDualStreamState: NOIMPL("CMD_Rec_getDualStreamState"); break;
	case CMD_Rec_setOverlay: ActionSetOverlayFlags(); break;
	case CMD_Rec_getOverlayState: SendOverlayFlags(bCheckClose); break;
	case CMD_Rec_SetStillMode: ActionSetStillMode(); break;
	case CMD_Rec_StartStillCapture: ActionStartStillCapture(); break;
	case CMD_Rec_StopStillCapture: ActionStopStillCapture(); break;
	default: AVF_LOGW("cmd %d.%d is not implemented", CMD_Domain_rec, cmd); break;
	}
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CCamServer
//
//-----------------------------------------------------------------------

CCamServer *CCamServer::Create(CCameraState *pCameraState, ICameraControl *pCameraControl)
{
	CCamServer *result = avf_new CCamServer(pCameraState, pCameraControl);
	if (result) {
		result->RunThread();
	}
	CHECK_STATUS(result);
	return result;
}

CCamServer::CCamServer(CCameraState *pCameraState, ICameraControl *pCameraControl):
	inherited("ccam-server"),
	mpCameraState(pCameraState),
	mpCameraControl(pCameraControl),
	mpThread(NULL),
	mpMsgQueue(NULL)
{
	CREATE_OBJ(mpMsgQueue = CMsgQueue::Create("ccam-server", sizeof(msg_s), 8));
	mpCameraState->RegisterOnStateChangedListener(this);
}

CCamServer::~CCamServer()
{
	// interrupt the msg q so the thread won't read any more
	if (mpMsgQueue) {
		mpMsgQueue->Interrupt();
	}

	if (mpThread) {
		// stop server and sockets so the thread won't block
		StopServer();
		avf_safe_release(mpThread);
	}

	mpCameraState->UnRegisterOnStateChangedListener(this);

	avf_safe_release(mpMsgQueue);
}

void CCamServer::RunThread()
{
	CREATE_OBJ(mpThread = CThread::Create("ccam-server", ThreadEntry, this));
}

void *CCamServer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ICameraStateListener)
		return static_cast<ICameraStateListener*>(this);
	return inherited::GetInterface(refiid);
}

void CCamServer::PostMsg(int code, int p1, int p2)
{
	msg_s msg;
	msg.code = code;
	msg.p1 = p1;
	msg.p2 = p2;
	mpMsgQueue->PostMsg(&msg, sizeof(msg));
}

void CCamServer::ThreadLoop()
{
	msg_s msg;
	while (mpMsgQueue->GetMsgInterruptible(&msg, sizeof(msg))) {
		BroadcastMsg(msg);
	}
}

void CCamServer::BroadcastMsg(msg_s& msg)
{
	AUTO_LOCK(mMutex);
	CTCPConnection *conn;
	for (conn = mpFirstConn; conn; conn = conn->_GetNext()) {
		CCamConnection *connection = static_cast<CCamConnection*>(conn);
		switch (msg.code) {
		case MSG_StateChanged:
			connection->StateChanged(msg.p1, false);
			break;
		case MSG_RecordError:
			connection->RecordError((enum Rec_Error_code)msg.p1, false);
			break;
		case MSG_StillCaptureStarted:
			connection->StillCaptureStarted(msg.p1, false);
			break;
		default:
			break;
		}
	}
}


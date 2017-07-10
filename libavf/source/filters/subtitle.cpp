
#define LOG_TAG "subtitle"

#include <limits.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "filter_if.h"
#include "avf_media_format.h"
#include "avf_config.h"

#include "avio.h"
#include "sys_io.h"
#include "mem_stream.h"
#include "avf_util.h"

#include "subtitle.h"

#ifdef ARCH_S2L
#include "aglib.h"
#endif

#ifdef INCLUDE_GPS
#include "gps_dev.cpp"
#endif

#ifdef INCLUDE_IIO
#include "iio_dev.cpp"
#endif

#ifdef INCLUDE_OBD
#include "obd_dev.cpp"
#endif

#include "raw_reg.cpp"

//-----------------------------------------------------------------------
//
//  CRawThreadHelper
//
//-----------------------------------------------------------------------

CRawThreadHelper* CRawThreadHelper::Create(CSubtitleFilter *pOwner, int id, const char *pThreadName)
{
	CRawThreadHelper *result = avf_new CRawThreadHelper(pOwner, id, pThreadName);
	CHECK_STATUS(result);
	return result;
}

CRawThreadHelper::CRawThreadHelper(CSubtitleFilter *pOwner, int id, const char *pThreadName):
	mpOwner(pOwner),
	mID(id),
	mpThreadName(pThreadName),
	mbRunning(false),
	mpThread(NULL)
{
}

CRawThreadHelper::~CRawThreadHelper()
{
	StopThread();
	ReleaseThread();
}

avf_status_t CRawThreadHelper::StartThread(int interval)
{
	AVF_ASSERT(mpThread == NULL);

	mbRunning = true;
	mInterval = interval;

	mpThread = CThread::Create(mpThreadName, ThreadEntry, (void*)this);
	if (mpThread == NULL) {
		AVF_LOGP("cannot create thread: %s", mpThreadName);
		mbRunning = false;
		return E_ERROR;
	}

	mpThread->SetThreadPriority(CThread::PRIO_MANAGER);
	AVF_LOGD("Start obd thread");

	return E_OK;
}

void CRawThreadHelper::StopThread()
{
	if (mbRunning) {
		AUTO_LOCK(mMutex);
		mbRunning = false;
		mSem.Wakeup();
	}
}

void CRawThreadHelper::ReleaseThread()
{
	avf_safe_release(mpThread);
}

void CRawThreadHelper::ThreadEntry(void *p)
{
	CRawThreadHelper *self = (CRawThreadHelper*)p;
	self->ThreadLoop();
}

void CRawThreadHelper::ThreadLoop()
{
	avf_u64_t next_tick = avf_get_curr_tick();

	while (mbRunning) {
		mpOwner->ThreadTriggered(mID);

		avf_u64_t curr_tick = avf_get_curr_tick();
		next_tick += mInterval;

		avf_uint_t sleep_ms = 0;
		if (curr_tick < next_tick) {
			sleep_ms = (avf_uint_t)(next_tick - curr_tick);
		}
		if (sleep_ms < (avf_uint_t)mInterval / 2) {
			sleep_ms = (avf_uint_t)mInterval / 2;
		}

		{
			AUTO_LOCK(mMutex);
			if (mbRunning) {
				if (mSem.TimedWait(mMutex, sleep_ms)) {
					AVF_LOGD(C_YELLOW "%s thread interrupted" C_NONE, mpThreadName);
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------
//
//  CSubtitleFilter
//
//-----------------------------------------------------------------------
IFilter* CSubtitleFilter::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CSubtitleFilter *result = avf_new CSubtitleFilter(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

// default is true
bool CSubtitleFilter::GetConfig(CConfigNode *pFilterConfig, const char *name)
{
	CConfigNode *node = pFilterConfig->FindChild(name, 0);
	return node && ::atoi(node->GetValue()) ? true : false;
}

CSubtitleFilter::CSubtitleFilter(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),

	mpTimerManager(NULL),

	mbTimerSet(false),
	mbLastIioInfoValid(false),
	mpLastObdData(NULL),
	m_gps_rval(-1),	// no info

#ifdef INCLUDE_GPS
	mbEnableGps(false),
#ifdef SYNC_GPS
	mpGpsThread(NULL),
#endif
#endif

#ifdef INCLUDE_OBD
	mbEnableObd(false),
	mbObdOpen(false),
	mNumPids(0),
#ifdef SYNC_OBD
	mpObdThread(NULL),
#endif
#endif

#ifdef INCLUDE_IIO
	mbEnableIio(false),
#endif

	mpBufferPool(NULL),
	mpObserver(NULL),

	mpGpsRawCache(NULL),
	mpAccRawCache(NULL),

	mb_uploading(false),
	mp_upload_mb(NULL)
{
	SET_ARRAY_NULL(mpOutput);

	CREATE_OBJ(mpTimerManager = ITimerManager::GetInterfaceFrom(mpEngine));

	mpObserver = GetRecordObserver();

#ifdef INCLUDE_GPS
	mbEnableGps = GetConfig(pFilterConfig, "gps");
	if (mbEnableGps) {
		mpGpsRawCache = CRawDataCache::Create();
		gps_dev_open();
#ifdef SYNC_GPS
		CREATE_OBJ(mpGpsThread = CRawThreadHelper::Create(this, TIMER_GPS, "readgps"));
#endif
	}
#endif

#ifdef INCLUDE_IIO
	mbEnableIio = GetConfig(pFilterConfig, "iio");
	if (mbEnableIio) {
		mpAccRawCache = CRawDataCache::Create();
		iio_dev_open();
	}
#endif

#ifdef INCLUDE_OBD
	mbEnableObd = GetConfig(pFilterConfig, "obd");
	if (mbEnableObd) {
#ifdef SYNC_OBD
		CREATE_OBJ(mpObdThread = CRawThreadHelper::Create(this, TIMER_OBD, "readobd"));
#endif
		// open later
	}
#endif

	for (int i = 0; ; i++) {
		CConfigNode *node = pFilterConfig->FindChild("reg", i);
		if (node == NULL)
			break;

		if (mpRawReg == NULL && (mpRawReg = CRawReg::Create(this)) == NULL) {
			mStatus = E_ERROR;
			return;
		}

		const char *reg_str = node->GetValue();
		mpRawReg->AddEntryByConfig(reg_str);
	}
}

CSubtitleFilter::~CSubtitleFilter()
{
	SAFE_RELEASE_ARRAY(mpOutput);

	avf_safe_release(mpLastObdData);

	avf_safe_release(mpBufferPool);
	avf_safe_release(mpGpsRawCache);
	avf_safe_release(mpAccRawCache);
	avf_safe_release(mp_upload_mb);

	avf_safe_release(mpRawReg);

#ifdef INCLUDE_GPS
	if (mbEnableGps) {
		gps_dev_close();
	}
#ifdef SYNC_GPS
	avf_safe_release(mpGpsThread);
#endif
#endif

#ifdef INCLUDE_IIO
	if (mbEnableIio) {
		iio_dev_close();
	}
#endif

#ifdef INCLUDE_OBD
	if (mbObdOpen) {
		obd_dev_close();
		mbObdOpen = false;
	}
#ifdef SYNC_OBD
	avf_safe_release(mpObdThread);
#endif
#endif
}

void *CSubtitleFilter::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ITimerTarget)
		return static_cast<ITimerTarget*>(this);
	if (refiid == IID_ISourceFilter)
		return static_cast<ISourceFilter*>(this);
	if (refiid == IID_ISubtitleInput)
		return static_cast<ISubtitleInput*>(this);
	return inherited::GetInterface(refiid);
}

avf_status_t CSubtitleFilter::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	for (avf_size_t i = 0; i < NUM_OUTPUT; i++) {
		CSubtitleOutput *pPin = avf_new CSubtitleOutput(this);
		if (pPin == NULL || pPin->Status() != E_OK) {
			avf_safe_release(pPin);
			return E_NOMEM;
		}
		mpOutput[i] = pPin;
	}

	mpBufferPool = CDynamicBufferPool::CreateNonBlock("SubtBP", NULL, 2);
	if (mpBufferPool == NULL)
		return E_NOMEM;

	return E_OK;
}

avf_status_t CSubtitleFilter::RunFilter()
{
	mbLastIioInfoValid = false;

	ClearObdData();
	ClearGpsData();

#ifdef INCLUDE_OBD
	mNumPids = 0;

	if (mbEnableObd) {
		char buffer[REG_STR_MAX];
		mpEngine->ReadRegString(NAME_OBD_MASK, 0, buffer, VALUE_OBD_MASK);
		char *p = buffer;
		if (::strcmp(p, "all") == 0) {
			mNumPids = -1;
		} else {
			while (*p) {
				char *pend;

				avf_u32_t v = ::strtoul(p, &pend, 16);
				if (pend == p)
					break;
				p = pend;

				if (mNumPids < (int)ARRAY_SIZE(mPidList)) {
					mPidList[mNumPids] = v;
					mNumPids++;
				} else {
					AVF_LOGW("too many PIDs");
				}

				if (*p == ',') {
					p++;
				}
			}
		}
	}
#endif

	mLastTick = mStartTick = avf_get_curr_tick();
	mbVinSent = false;

	mSubtitleInterval = mpEngine->ReadRegInt32(NAME_SUBTITLE_INTERVAL, 0, VALUE_SUBTITLE_INTERVAL);
	SetTimer(TIMER_OSD, mSubtitleInterval);

	mGpsInterval = 0;
	mAccInterval = 0;
	mObdInterval = 0;

#ifdef INCLUDE_GPS
	if (mbEnableGps) {
		mGpsInterval = mpEngine->ReadRegInt32(NAME_GPS_INTERVAL, 0, VALUE_GPS_INTERVAL);
#ifdef SYNC_GPS
		mpGpsThread->StartThread(mGpsInterval);
#else
		SetTimer(TIMER_GPS, mGpsInterval);
#endif
	}
#endif

#ifdef INCLUDE_IIO
	if (mbEnableIio) {
		mAccInterval = mpEngine->ReadRegInt32(NAME_ACC_INTERVAL, 0, VALUE_ACC_INTERVAL);
		SetTimer(TIMER_ACC, mAccInterval);
	}
#endif

#ifdef INCLUDE_OBD
	if (mbEnableObd) {
		mObdInterval = mpEngine->ReadRegInt32(NAME_OBD_INTERVAL, 0, VALUE_OBD_INTERVAL);
#ifdef SYNC_OBD
		mpObdThread->StartThread(mObdInterval);
#else
		SetTimer(TIMER_OBD, mObdInterval);
#endif
	}
#endif

	if (mpRawReg) {
		mpRawReg->StartThread();
	}

	SendFirstSubtitle();

	if (mpObserver) {
		mpObserver->PostRawDataFreq(mGpsInterval, mAccInterval, mObdInterval);
	}

	mUploadInterval = mpEngine->ReadRegInt32(NAME_UPLOAD_INTERVAL_RAW, 0, VALUE_UPLOAD_INTERVAL_RAW);
	SetTimer(TIMER_UPLOAD, mUploadInterval);

	mbTimerSet = true;

	return E_OK;
}

avf_status_t CSubtitleFilter::StopFilter()
{
	if (mbTimerSet) {
		mbTimerSet = false;

		KillTimer(TIMER_OSD);

#ifdef INCLUDE_GPS
		if (mbEnableGps) {
#ifdef SYNC_GPS
			mpGpsThread->StopThread();
#else
			KillTimer(TIMER_GPS);
#endif
		}
#endif

#ifdef INCLUDE_IIO
		if (mbEnableIio) {
			KillTimer(TIMER_ACC);
		}
#endif

#ifdef INCLUDE_OBD
		if (mbEnableObd) {
#ifdef SYNC_OBD
			mpObdThread->StopThread();
#else
			KillTimer(TIMER_OBD);
#endif
		}
#endif

		if (mpRawReg) {
			mpRawReg->StopThread();
		}

		KillTimer(TIMER_UPLOAD);
	}

#ifdef INCLUDE_GPS
#ifdef SYNC_GPS
	if (mpGpsThread) {
		mpGpsThread->ReleaseThread();
	}
#endif
#endif

#ifdef INCLUDE_OBD
#ifdef SYNC_OBD
	if (mpObdThread) {
		mpObdThread->ReleaseThread();
	}
#endif
#endif

	if (mpRawReg) {
		mpRawReg->ReleaseThread();
	}

	AUTO_LOCK(mUploadMutex);
	EndUploadingLocked(true);

	return E_OK;
}

IPin* CSubtitleFilter::GetOutputPin(avf_uint_t index)
{
	return (index >= NUM_OUTPUT) ? NULL : mpOutput[index];
}

avf_status_t CSubtitleFilter::StopSourceFilter()
{
	PostEosMsg();

	avf_u64_t tick = GetTickCount();
	for (avf_size_t i = 0; i < NUM_OUTPUT; i++) {
		SendSubtitle(NULL, 0, 0, i, tick);
	}

	CBuffer *pBuffer = GetBuffer(NULL, 0);
	if (pBuffer) {
		pBuffer->SetEOS();
		for (avf_size_t i = 0; i < NUM_OUTPUT; i++) {
			pBuffer->AddRef();
			mpOutput[i]->SetEOS();
			mpOutput[i]->PostBuffer(pBuffer);
		}
		pBuffer->Release();
	}

	return E_OK;
}

CBuffer *CSubtitleFilter::GetBuffer(CMediaFormat *pFormat, avf_size_t size)
{
	CBuffer *pBuffer;
	return mpBufferPool->AllocBuffer(pBuffer, pFormat, size) ? pBuffer : NULL;
}

avf_status_t CSubtitleFilter::SendSubtitle(const avf_u8_t *content,
	avf_size_t size, avf_uint_t duration_ms, avf_size_t pin_index, avf_u64_t ticks)
{
	CSubtitleOutput *pOutput = mpOutput[pin_index];
	CBuffer *pBuffer;

	pBuffer = GetBuffer(pOutput->GetUsingMediaFormat(), size);
	if (pBuffer == NULL)
		return E_NOMEM;

	if (size) {
		::memcpy(pBuffer->mpData, content, size);
	}

	pBuffer->mType = CBuffer::DATA;
	pBuffer->mFlags = CBuffer::F_KEYFRAME;

	//pBuffer->mpData 
	//pBuffer->mSize

	pBuffer->m_offset = 0;
	pBuffer->m_data_size = size;

	pBuffer->m_dts = ticks;
	pBuffer->m_pts = ticks;
	pBuffer->m_duration = 0;	//duration_ms;

	pOutput->PostBuffer(pBuffer);

	return E_OK;
}

avf_status_t CSubtitleFilter::SendFirstSubtitle()
{
#ifndef SYNC_GPS
	CheckGPS(0);
#endif

	//CheckAcc();

#ifndef SYNC_OBD
	CheckObd(0);
#endif

	CheckUpload();

	// todo - no gps/obd yet
	GenerateOSD();

	return E_OK;
}

void CSubtitleFilter::SetObdData(raw_data_t *raw)
{
	AUTO_LOCK(mMutex);
	avf_assign(mpLastObdData, raw);
}

void CSubtitleFilter::ClearGpsData()
{
	AUTO_LOCK(mMutex);
	m_gps_rval = -1;	// info not valid
	gps_dev_clear();
}

int CSubtitleFilter::GetLocalTime(char *buffer, int buffer_size)
{
	if (buffer_size <= 0)
		return 0;

	time_t local_time = ::time(NULL);
	struct tm local_tm;
	avf_local_time(&local_time, &local_tm);

	char reg[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_SUBTITLE_TIMEFMT, 0, reg, VALUE_SUBTITLE_TIMEFMT);

	char buf[64];
	::strftime(with_size(buf), reg, &local_tm);

	return avf_snprintf(buffer, buffer_size, "%s", buf);
}

// debug Hellfire power
int CSubtitleFilter::GetPowerInfo(char *buffer, int offset, int size)
{
#ifdef ARCH_S2L
	int ret_val;
	int tmp;
	float tmp_f;
	char str[512];

	if (offset < size) {
		ret_val = aglib_read_file_int("/sys/class/power_supply/BQ27421/capacity", &tmp);
		if (ret_val == 0) {
			offset += avf_snprintf((buffer + offset), (size - offset), "%d%% ", tmp);
		}
	}
	if (offset < size) {
		memset(str, 0, sizeof(str));
		ret_val = aglib_read_file_buffer("/sys/class/power_supply/BQ27421/capacity_level", str, sizeof(str));
		if (ret_val > 0) {
			str[ret_val - 1] = 0;
			offset += avf_snprintf((buffer + offset), (size - offset), "%s ", str);
		}
	}
	if (offset < size) {
		ret_val = aglib_read_file_int("/sys/class/power_supply/BQ27421/temp", &tmp);
		if (ret_val == 0) {
			tmp_f = tmp;
			tmp_f /= 10;
			offset += avf_snprintf((buffer + offset), (size - offset), "%.1fC ", tmp_f);
		}
	}
	if (offset < size) {
		ret_val = aglib_read_file_int("/sys/class/power_supply/BQ27421/current_avg", &tmp);
		if (ret_val == 0) {
			offset += avf_snprintf((buffer + offset), (size - offset), "%dmA ", tmp/1000);
		}
	}
	if (offset < size) {
		ret_val = aglib_read_file_int("/sys/class/power_supply/BQ27421/voltage_now", &tmp);
		if (ret_val == 0) {
			offset += avf_snprintf((buffer + offset), (size - offset), "%dmV ", tmp/1000);
		}
	}
	if (offset < size) {
		ret_val = aglib_read_file_int("/sys/class/power_supply/BQ27421/charge_now", &tmp);
		if (ret_val == 0) {
			offset += avf_snprintf((buffer + offset), (size - offset), "%dmAh ", tmp/1000);
		}
	}
#if 0
	if (offset < size) {
		memset(str, 0, sizeof(str));
		ret_val = aglib_read_file_buffer("/debug/ts_bq27421/regs", str, sizeof(str));
		if (ret_val > 0) {
			str[ret_val - 1] = 0;
			offset += avf_snprintf((buffer + offset), (size - offset), "\n%s ", str);
		}
	}
#endif
	if (offset < size) {
		aglib_read_file_int("/sys/bus/iio/devices/iio:device0/in_temp", &tmp);
		tmp_f = tmp;
		ret_val = aglib_read_file_buffer("/sys/bus/iio/devices/iio:device0/name", str, sizeof(str));
		if (ret_val > 0) {
			str[ret_val - 1] = 0;
			offset += avf_snprintf((buffer + offset), (size - offset), "%s:%.2fC ", str, tmp_f);
		}
	}
	if (offset < size) {
		aglib_read_file_int("/sys/bus/iio/devices/iio:device1/in_temp_fix", &tmp);
		tmp_f = tmp;
		tmp_f /= 100;
		ret_val = aglib_read_file_buffer("/sys/bus/iio/devices/iio:device1/name", str, sizeof(str));
		if (ret_val > 0) {
			str[ret_val - 1] = 0;
			offset += avf_snprintf((buffer + offset), (size - offset), "%s:%.2fC ", str, tmp_f);
		}
	}
	if (offset < size) {
		memset(str, 0, sizeof(str));
		ret_val = aglib_read_file_buffer("/proc/loadavg", str, sizeof(str));
		if (ret_val > 0) {
			str[ret_val - 1] = 0;
			offset += avf_snprintf((buffer + offset), (size - offset), "%s ", str);
		}
	}
#endif
	return offset;
}

// debug accel
int CSubtitleFilter::GetAccelInfo(char *buffer, int offset, int size)
{
#ifdef ARCH_S2L
//	int ret_val;
//	iio_info_t info;
//	int data_size;

	static int32_t accel_x_min = INT32_MAX, accel_x_max = INT32_MIN;
	static int32_t accel_y_min = INT32_MAX, accel_y_max = INT32_MIN;
	static int32_t accel_z_min = INT32_MAX, accel_z_max = INT32_MIN;

	if (!mbLastIioInfoValid) {
		return offset;
	}

//	ret_val = iio_dev_read(&info, &data_size);
//	if (ret_val) {
//		return offset;
//	}

	iio_info_t info = mLastIioInfo;

	if (accel_x_min > info.accel_x) {
		accel_x_min = info.accel_x;
	}
	if (accel_x_max < info.accel_x) {
		accel_x_max = info.accel_x;
	}
	if (accel_y_min > info.accel_y) {
		accel_y_min = info.accel_y;
	}
	if (accel_y_max < info.accel_y) {
		accel_y_max = info.accel_y;
	}
	if (accel_z_min > info.accel_z) {
		accel_z_min = info.accel_z;
	}
	if (accel_z_max < info.accel_z) {
		accel_z_max = info.accel_z;
	}
	if (offset < size) {
		offset += avf_snprintf((buffer + offset), (size - offset),
			"Accel[%d,%d,%d]:[%d,%d,%d]:[%d,%d,%d] "
			"Gyro[%d,%d,%d] Magn[%d,%d,%d] "
			"Euler[%d,%d,%d] Quaternion[%d,%d,%d,%d] Pressure[%d]",
			info.accel_x, info.accel_y, info.accel_z,
			accel_x_min, accel_y_min, accel_z_min,
			accel_x_max, accel_y_max, accel_z_max,
			info.gyro_x, info.gyro_y, info.gyro_z,
			info.magn_x, info.magn_y, info.magn_z,
			info.euler_heading, info.euler_roll, info.euler_pitch,
			info.quaternion_w, info.quaternion_x, info.quaternion_y,
			info.quaternion_z, info.pressure);
	}
#endif
	return offset;
}

extern const uint8_t g_pid_data_size[];

int CSubtitleFilter::GetObdInfo(char *buffer, int offset, int size)
{
	AUTO_LOCK(mMutex);

	if (mpLastObdData == NULL) {
		return offset;
	}

	avf_u8_t *data = mpLastObdData->GetData();

	if (*data++ != OBD_VERSION_2) {
		return offset;
	}

	for (;;) {
		avf_uint_t pid = *data++;
		if (pid == 0)
			break;

		avf_uint_t pid_value;
		avf_uint_t pid_size = g_pid_data_size[pid];

		if (pid_size == 1) {
			pid_value = *data++;
		} else if (pid_size == 2) {
			pid_value = data[0] | (data[1] << 8);
			data += 2;
		} else if (pid_size == 4) {
			pid_value = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
			data += 4;
		} else {
			continue;
		}

		offset += avf_snprintf(buffer + offset, size - offset, "PID%02X[0x%x] ", pid, pid_value);
	}

	return offset;
}

int CSubtitleFilter::GetGPSInfo(char *buffer, int offset, int size)
{
	AUTO_LOCK(mMutex);

	int ret_val = gps_dev_get_satellite(buffer + offset, size - offset);

	if (ret_val > 0) {
		offset += ret_val;
	}

	return offset;
}

avf_u64_t CSubtitleFilter::GetTickCount()
{
	avf_u64_t tick = avf_get_curr_tick();
//	if (tick - mLastTick > 20*1000) {
//		AVF_LOGE("curr tick: " LLD ", last tick: " LLD, tick, mLastTick);
//	}
	mLastTick = tick;
	return mLastTick - mStartTick;
}

int CSubtitleFilter::GetOtherOsdInfo(char *buffer, int offset, int size, char *reg)
{
	mpEngine->ReadRegString(NAME_DBG_SHOWOSD, IRegistry::GLOBAL, reg, VALUE_DBG_SHOWOSD);
	if (reg[0]) {
		if (::strstr(reg, "power")) {
			offset = GetPowerInfo(buffer, offset, size);
		}
		if (::strstr(reg, "accel")) {
			offset = GetAccelInfo(buffer, offset, size);
		}
		if (::strstr(reg, "obd")) {
			offset = GetObdInfo(buffer, offset, size);
		}
		if (::strstr(reg, "gps")) {
			offset = GetGPSInfo(buffer, offset, size);
		}
	}
	return offset;
}

void CSubtitleFilter::SendGPS(int pin_index, avf_u64_t tick)
{
	char gps_buffer[1024];
	char *const buffer = gps_buffer;
	int size = sizeof(gps_buffer) - 1;
	char reg[REG_STR_MAX];
	int offset = 0;
	int flags;

	// "config.subtitle.flags"
	//	0x01 - display prefix (e.g. camera name)
	//	0x02 - display time info
	//	0x04 - display gps location
	//	0x08 - display speed

	buffer[0] = 0;
	if (pin_index == OUTPUT_GPS) {
		flags = 0x01 | 0x02 | 0x04 | 0x08;
	} else {
		flags = mpEngine->ReadRegInt32(NAME_SUBTITLE_FLAGS, 0, VALUE_SUBTITLE_FLAGS);
	}

	// prefix
	if (flags & 0x01) {
		mpEngine->ReadRegString(NAME_SUBTITLE_PREFIX, 0, reg, VALUE_SUBTITLE_PREFIX);
		if (reg[0] != 0) {
			offset += avf_snprintf(buffer + offset, size - offset, "%s ", reg);
		}
	}

	if (pin_index == OUTPUT_OSD && mpEngine->ReadRegBool(NAME_SUBTITLE_SHOW_GPS, 0, VALUE_SUBTITLE_SHOW_GPS)) {

		// show satellite info on osd overlay
		AUTO_LOCK(mMutex);
		gps_dev_get_satellite(buffer + offset, size - offset);

	} else {

		// time
		if (flags & 0x02) {
			mpEngine->ReadRegString(NAME_SUBTITLE_TIME, 0, reg, VALUE_SUBTITLE_TIME);
			if (reg[0] != 0) {
				offset += avf_snprintf(buffer + offset, size - offset, "%s ", reg);
			} else {
				offset += GetLocalTime(buffer + offset, size - offset);
				if (offset + 1 < size) {
					buffer[offset] = ' ';
					buffer[offset + 1] = 0;
					offset++;
				}
			}

			if (pin_index == OUTPUT_OSD) {
				offset = GetOtherOsdInfo(buffer, offset, size, reg);
			}
		}

		AUTO_LOCK(mMutex);
		if (m_gps_rval >= 0) {

			// position
			if (flags & 0x04) {
				offset += avf_snprintf(buffer + offset, size - offset, "%s ", m_gps_latlng);
			}

			// speed
			if (flags & 0x08) {
				if (m_gps_speed >= 0) {
					int slimit = mpEngine->ReadRegInt32(NAME_SUBTITLE_SLIMIT, 0, VALUE_SUBTITLE_SLIMIT);
					if (slimit < 0 || m_gps_speed <= slimit) {
						offset += avf_snprintf(buffer + offset, size - offset, "%d km/h", (int)m_gps_speed);
					} else {
						mpEngine->ReadRegString(NAME_SUBTITLE_SLIMIT_STR, 0, reg, VALUE_SUBTITLE_SLIMIT_STR);
						offset += avf_snprintf(buffer + offset, size - offset, "%s ", reg);
					}
				}
			}

		} else {

			// no GPS info, read from user
			mpEngine->ReadRegString(NAME_SUBTITLE_GPS_INFO, 0, reg, VALUE_SUBTITLE_GPS_INFO);
			if (reg[0] != 0) {
				offset += avf_snprintf(buffer + offset, size - offset, "%s", reg);
			}

		}
	}

	buffer[sizeof(gps_buffer)-1] = 0;
	SendSubtitle((const avf_u8_t*)buffer, ::strlen(buffer), 0, pin_index, tick);
}

void CSubtitleFilter::GenerateOSD()
{
	if (mpEngine->ReadRegBool(NAME_SUBTITLE_ENABLE, 0, VALUE_SUBTITLE_ENABLE)) {
		SendGPS(OUTPUT_OSD, GetTickCount());
	}
}

void CSubtitleFilter::CheckGPS(int b_sync)
{
#ifdef INCLUDE_GPS
	if (!mbEnableGps)
		return;

	gps_data_t data = {0};
	char reg[REG_STR_MAX];

	int min_sat;
	int min_snr;
	char *end_ptr;
	int accuracy;

	avf_u64_t tick = GetTickCount();

	mpEngine->ReadRegString(NAME_GPS_SATSNR, 0, reg, VALUE_GPS_SATSNR);
	min_sat = ::strtol(reg, &end_ptr, 10);
	if (end_ptr == reg) {
		min_sat = 0;
		min_snr = 0;
	} else {
		char *p = end_ptr;
		if (*p == ',')
			p++;
		min_snr = ::strtol(p, &end_ptr, 10);
		if (end_ptr == p) {
			min_sat = 0;
			min_snr = 0;
		}
	}

	accuracy = mpEngine->ReadRegInt32(NAME_GPS_ACCURACY, 0, VALUE_GPS_ACCURACY);

	gps_dev_update(b_sync, mMutex, min_sat, min_snr, accuracy);

	{
		AUTO_LOCK(mMutex);
		m_gps_rval = gps_dev_read_data(with_size(m_gps_latlng), &data, &m_gps_speed, &m_gps_num_svs);
		mpEngine->WriteRegInt32(NAME_STATUS_GPS, 0, m_gps_rval >= 0 ? 0 : m_gps_rval);
		mpEngine->WriteRegInt32(NAME_STATUS_GPS_NUM_SVS, 0, m_gps_num_svs);
	}

	SendGPS(OUTPUT_GPS, tick);

	if (mpObserver && data.flags) {
		raw_data_t *raw = mpGpsRawCache->AllocRawData(sizeof(data), IRecordObserver::GPS_DATA);
		raw->ts_ms = tick;
		::memcpy(raw->GetData(), &data, sizeof(data));
		mpObserver->AddRawData(raw, 0);
		raw->Release();
	}

	if (data.flags != 0) {
		SaveDataForUpload(NAME_UPLOAD_GPS, VALUE_UPLOAD_GPS, tick,
			RAW_DATA_GPS, (const avf_u8_t*)&data, sizeof(data));
	}
#endif
}

void CSubtitleFilter::CheckAcc()
{
	mbLastIioInfoValid = false;	// clear

#ifdef INCLUDE_IIO
	if (!mbEnableIio)
		return;

	iio_info_t info;
	int data_size;
	char buffer[256];

	avf_u64_t tick = GetTickCount();

	if (iio_dev_read(&info, &data_size) >= 0) {

		// save it to show on OSD overlay
		mLastIioInfo = info;
		mbLastIioInfoValid = true;

		sprintf(buffer, "%d,%d,%d", info.accel_x, info.accel_y, info.accel_z);
		SendSubtitle((const avf_u8_t*)buffer, ::strlen(buffer), 0, OUTPUT_ACCEL, tick);

		// send to vdb
		if (mpObserver) {
			raw_data_t *raw = mpAccRawCache->AllocRawData(data_size, IRecordObserver::ACC_DATA);
			raw->ts_ms = tick;
			::memcpy(raw->GetData(), &info, data_size);
			mpObserver->AddRawData(raw, 0);
			raw->Release();
		}

		SaveDataForUpload(NAME_UPLOAD_ACC, VALUE_UPLOAD_ACC, tick,
			RAW_DATA_ACC, (const avf_u8_t*)&info, sizeof(info));
	}
#endif
}

void CSubtitleFilter::CheckObd(int b_sync)
{
#ifdef INCLUDE_OBD
	if (!mbEnableObd)
		return;

	// set this will dynamically enable/disable obd
	// obd is disabled by default, untill app enable it when obd is connected
	bool bOpen = mpEngine->ReadRegBool(NAME_OBD_ENABLE, 0, VALUE_OBD_ENABLE);

	if (!mbObdOpen) {
		// not open
		if (bOpen) {
			// request to open
			if (obd_dev_open() < 0) {
				AVF_LOGE("obd_dev_open failed");
				return;
			}
			mbObdOpen = true;
			AVF_LOGD("obd_dev_open");
			ClearObdData();
		} else {
			return;
		}
	} else {
		// open
		if (!bOpen) {
			// request to close
			obd_dev_close();
			mbObdOpen = false;
			AVF_LOGD("obd_dev_close");
			ClearObdData();
			return;
		}
	}

	avf_u64_t tick = GetTickCount();

	if (obd_dev_read(b_sync) < 0) {
		ClearObdData();
		return;
	}

	if (!mbVinSent && mpObserver) {
		raw_data_t *vin = obd_dev_get_vin(IRecordObserver::VIN_DATA);
		if (vin) {
			//AVF_LOGD("VIN: " C_YELLOW "%s" C_NONE, vin->GetData());
			char reg[REG_STR_MAX];
			mpEngine->ReadRegString(NAME_VIN_MASK, 0, reg, VALUE_VIN_MASK);
			if (reg[0]) {
				char *mask = reg;
				char *p = (char*)vin->GetData();
				for (; *mask && *p; mask++, p++) {
					if (*mask == '-') {
						*p = '-';
					}
				}
				for (; *p; p++) {
					*p = '-';
				}
				AVF_LOGI("VIN: " C_YELLOW "%s" C_NONE, vin->GetData());
				mpObserver->SetConfigData(vin);
			}
			vin->Release();
			mbVinSent = true;
		}
	}

	raw_data_t *raw = obd_dev_get_data(mPidList, mNumPids, IRecordObserver::OBD_DATA);
	if (raw == NULL)
		return;

	raw->ts_ms = tick;

	// save it to show on osd overlay
	SetObdData(raw);

	if (mpObserver) {
		mpObserver->AddRawData(raw, 0);
	}

	SaveDataForUpload(NAME_UPLOAD_OBD, VALUE_UPLOAD_OBD, tick,
		RAW_DATA_OBD, raw->GetData(), raw->GetSize());

	raw->Release();
#endif
}

void CSubtitleFilter::SaveDataForUpload(const char *name, int value, avf_u64_t tick,
	int data_type, const avf_u8_t *data, avf_size_t size)
{
	AUTO_LOCK(mUploadMutex);

	if (mp_upload_mb && mpEngine->ReadRegBool(name, 0, value)) {
		upload_header_v2_t header;
		header.u_data_type = data_type;
		header.u_data_size = size;
		header.u_timestamp = tick - m_upload_start_tick;
		header.u_stream = 0;
		header.u_duration = 0;
		header.u_version = UPLOAD_VERSION_v2;
		header.u_flags = 0;
		header.u_param1 = 0;
		header.u_param2 = 0;
		mp_upload_mb->write_mem((const avf_u8_t *)&header, sizeof(header));
		mp_upload_mb->write_mem(data, size);
	}
}

void CSubtitleFilter::CheckUpload()
{
	AUTO_LOCK(mUploadMutex);

	if (mpEngine->ReadRegBool(NAME_UPLOAD_RAW, 0, VALUE_UPLOAD_RAW)) {

		if (mb_uploading) {
			// continue uploading: start a new file
			EndUploadingLocked(false);
			StartUploadingLocked();
		} else {
			// start uploading
			m_upload_counter = 1;
			m_upload_start_tick = GetTickCount();
			StartUploadingLocked();
		}

	} else {

		if (mb_uploading) {
			// stop uploading
			EndUploadingLocked(true);
		} else {
			// do nothing
		}

	}
}

// this is called from the media control thread
avf_status_t CSubtitleFilter::TimerTriggered(avf_enum_t id)
{
	if (!mbTimerSet) {
		AVF_LOGD("timer stopped");
		return E_UNKNOWN;
	}

	switch (id) {
	case TIMER_OSD:
		GenerateOSD();
		break;

#ifdef INCLUDE_GPS
	case TIMER_GPS:
		CheckGPS(0);
		break;
#endif

#ifdef INCLUDE_IIO
	case TIMER_ACC:
		CheckAcc();
		break;
#endif

#ifdef INCLUDE_OBD
	case TIMER_OBD:
		CheckObd(0);
		break;
#endif

	case TIMER_UPLOAD:
		CheckUpload();
		break;

	default:
		AVF_LOGD("Unknown timer");
		break;
	}

	return E_OK;
}

// called from CRawThreadHelper thread
void CSubtitleFilter::ThreadTriggered(int id)
{
	switch (id) {
#ifdef INCLUDE_GPS
	case TIMER_GPS:
		CheckGPS(1);
		break;
#endif

#ifdef INCLUDE_IIO
	case TIMER_ACC:
		break;
#endif

#ifdef INCLUDE_OBD
	case TIMER_OBD:
		CheckObd(1);
		break;
#endif

	default:
		AVF_LOGD("unknown thread id %d", id);
		break;
	}
}

// called from reg thread
void CSubtitleFilter::OnRawData(uint32_t fcc, const avf_u8_t *data, int data_size, uint64_t ts)
{
	if (data_size > 0) {
		raw_data_t *raw = avf_alloc_raw_data(data_size, fcc);
		if (raw) {
			memcpy(raw->GetData(), data, data_size);
			raw->ts_ms = ts - mStartTick;
			if (mpObserver) {
				mpObserver->AddRawDataEx(raw);
			}
			raw->Release();
		}
	}
}

void CSubtitleFilter::GetUploadingFilename(char *buffer, int len, avf_uint_t index)
{
	char path[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_UPLOAD_PATH_RAW, 0, path, VALUE_UPLOAD_PATH_RAW);
	::avf_snprintf(buffer, len, "%s%d.raw", path, index);
	buffer[len-1] = 0;
}

void CSubtitleFilter::SaveToFileLocked()
{
	IAVIO *io = CSysIO::Create();

	char filename[256];
	GetUploadingFilename(with_size(filename), m_upload_counter);

	if (io->CreateFile(filename) != E_OK) {
		AVF_LOGE("cannot create %s", filename);
		io->Release();
		return;
	}

	CMemBuffer::block_t *block = mp_upload_mb->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		io->Write(block->GetMem(), block->GetSize());
	}

	upload_header_v2_t header;
	set_upload_end_header_v2(&header, mp_upload_mb->GetTotalSize());
	io->Write(&header, sizeof(header));

	io->Release();

	mp_upload_mb->Clear();

	// notify app
	mpEngine->WriteRegString(NAME_UPLOAD_RAW_PREV, 0, filename);
	mpEngine->WriteRegInt32(NAME_UPLOAD_RAW_INDEX, 0, m_upload_counter);
	PostFilterMsg(IEngine::MSG_UPLOAD_RAW);
}

void CSubtitleFilter::StartUploadingLocked()
{
	if (mp_upload_mb == NULL) {
		mp_upload_mb = CMemBuffer::Create(4*KB);
	}
	mb_uploading = true;
}

void CSubtitleFilter::EndUploadingLocked(bool bLast)
{
	if (mp_upload_mb) {
		SaveToFileLocked();
		m_upload_counter++;

		// remove oldest one
		if (m_upload_counter > 10) {
			char filename[256];
			GetUploadingFilename(with_size(filename), m_upload_counter - 10);
			avf_remove_file(filename, false);
		}

		if (bLast) {
			avf_safe_release(mp_upload_mb);
			mb_uploading = false;
			/// todo - write end flag
		}
	}
}

//-----------------------------------------------------------------------
//
//  CSubtitleOutput
//
//-----------------------------------------------------------------------
CSubtitleOutput::CSubtitleOutput(CSubtitleFilter *pFilter):
	inherited(pFilter, 0, "SubtitleOutput"),
	mpSubtitleFilter(pFilter)
{
}

CSubtitleOutput::~CSubtitleOutput()
{
}

CMediaFormat* CSubtitleOutput::QueryMediaFormat()
{
	CSubtitleFormat *pFormat = avf_alloc_media_format(CSubtitleFormat);

	if (pFormat) {
		pFormat->mMediaType = MEDIA_TYPE_SUBTITLE;
		pFormat->mFormatType = MF_SubtitleFormat;
		pFormat->mFrameDuration = 0;
		pFormat->mTimeScale = 1000;	// in ms
	}

	return pFormat;
}

IBufferPool* CSubtitleOutput::QueryBufferPool()
{
	IBufferPool *pBufferPool = mpSubtitleFilter->mpBufferPool;
	AVF_ASSERT(pBufferPool);
	return pBufferPool->acquire();
}



#ifndef __SUBTITLE_H__
#define __SUBTITLE_H__

#ifndef CONFIG_NO_GPS
#define INCLUDE_GPS
#else
#undef INCLUDE_GPS
#endif

#ifndef CONFIG_NO_IIO
#define INCLUDE_IIO
#else
#undef INCLUDE_IIO
#endif

#ifndef CONFIG_NO_OBD
#define INCLUDE_OBD
#else
#undef INCLUDE_OBD
#endif

#ifdef BOARD_HACHI
#define SYNC_GPS	// thread + blocking
#define SYNC_OBD	// thread + blocking
#endif

struct CConfigNode;
class CSubtitleFilter;
class CSubtitleOutput;
class CRawReg;

//-----------------------------------------------------------------------
//
//  CRawThreadHelper
//
//-----------------------------------------------------------------------
class CRawThreadHelper: public CObject
{
public:
	static CRawThreadHelper* Create(CSubtitleFilter *pOwner, int id, const char *pThreadName);

private:
	CRawThreadHelper(CSubtitleFilter *pOwner, int id, const char *pThreadName);
	~CRawThreadHelper();

public:
	avf_status_t StartThread(int interval);
	void StopThread();
	void ReleaseThread();

private:
	static void ThreadEntry(void *p);
	void ThreadLoop();

private:
	CSubtitleFilter *mpOwner;
	int mID;
	const char *mpThreadName;
	int mInterval;	// ms

	CMutex mMutex;
	CSemaphore mSem;

	bool mbRunning;
	CThread *mpThread;
};

//-----------------------------------------------------------------------
//
//  CSubtitleFilter
//
//-----------------------------------------------------------------------
class CSubtitleFilter:
	public CFilter, 
	public ISourceFilter,
	public ISubtitleInput,
	public ITimerTarget
{
	typedef CFilter inherited;
	friend class CRawThreadHelper;
	friend class CSubtitleOutput;
	friend class CRawReg;
	DEFINE_INTERFACE;

	enum {
		TIMER_OSD = 1,
		TIMER_GPS = 2,
		TIMER_ACC = 3,
		TIMER_OBD = 4,
		TIMER_UPLOAD = 5,
	};

	enum {
		OUTPUT_OSD = 0,
		OUTPUT_GPS = 1,
		OUTPUT_ACCEL = 2,
		OUTPUT_OBD = 3,
		NUM_OUTPUT
	};

	// TODO - keep it same with iio_dev.cpp
#if defined(ARCH_A5S)
	typedef acc_raw_data_t iio_info_t;
#else
	typedef iio_raw_data_t iio_info_t;
#endif

public:
	static IFilter* Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);

private:
	CSubtitleFilter(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig);
	virtual ~CSubtitleFilter();
	static bool GetConfig(CConfigNode *pFilterConfig, const char *name);

// IFilter
public:
	virtual avf_status_t InitFilter();
	virtual avf_status_t RunFilter();
	virtual avf_status_t StopFilter();
	virtual avf_size_t GetNumOutput() { return NUM_OUTPUT; }
	virtual IPin *GetOutputPin(avf_uint_t index);

// ISourceFilter
public:
	virtual avf_status_t StartSourceFilter(bool *pbDone) {
		return E_OK;
	}
	virtual avf_status_t StopSourceFilter();

// ISubtitleInput
public:
	virtual avf_status_t SendFirstSubtitle();

// ITimerTarget
public:
	virtual avf_status_t TimerTriggered(avf_enum_t id);

private:
	void ThreadTriggered(int id);
	void OnRawData(uint32_t fcc, const avf_u8_t *data, int data_size, uint64_t ts);

	INLINE void SetTimer(avf_enum_t id, avf_int_t interval) {
		mpTimerManager->SetTimer(this, id, interval, true);
	}
	INLINE void KillTimer(avf_enum_t id) {
		mpTimerManager->KillTimer(this, id);
	}
	avf_u64_t GetTickCount();

	void SendGPS(int pin_index, avf_u64_t tick);

	void GenerateOSD();
	void CheckGPS(int b_sync);
	void CheckAcc();
	void CheckObd(int b_sync);

	void CheckUpload();
	void SaveDataForUpload(const char *name, int value, avf_u64_t tick,
		int data_type, const avf_u8_t *data, avf_size_t size);

private:
	CBuffer *GetBuffer(CMediaFormat *pFormat, avf_size_t size);
	int GetLocalTime(char *buffer, int buffer_size);
	avf_status_t SendSubtitle(const avf_u8_t *content,
		avf_size_t size, avf_uint_t duration_ms, avf_size_t pin_index, avf_u64_t ticks);

private:
	int GetOtherOsdInfo(char *buffer, int offset, int size, char *reg);
	int GetPowerInfo(char *buffer, int offset, int size);
	int GetAccelInfo(char *buffer, int offset, int size);
	int GetObdInfo(char *buffer, int offset, int size);
	int GetGPSInfo(char *buffer, int offset, int size);

private:
	CSubtitleOutput *mpOutput[NUM_OUTPUT];
	ITimerManager *mpTimerManager;
	bool mbTimerSet;

	avf_int_t mSubtitleInterval;

	iio_info_t mLastIioInfo;
	bool mbLastIioInfoValid;

	// ----------------------------------------
	CMutex mMutex;
	raw_data_t *mpLastObdData;
	char m_gps_latlng[128];
	float m_gps_speed;
	int m_gps_num_svs;
	int m_gps_rval;
	// ----------------------------------------

private:
	void SetObdData(raw_data_t *raw);
	INLINE void ClearObdData() {
		SetObdData(NULL);
	}

	void ClearGpsData();

//----------------------------------------------------------------------------

#ifdef INCLUDE_GPS
	bool mbEnableGps;
	avf_int_t mGpsInterval;

#ifdef SYNC_GPS
	CRawThreadHelper *mpGpsThread;
#endif

#endif

//----------------------------------------------------------------------------

#ifdef INCLUDE_OBD
	bool mbEnableObd;
	bool mbObdOpen;
	avf_int_t mObdInterval;
	avf_int_t mNumPids;	// if < 0 then read all
	avf_u8_t mPidList[128];

#ifdef SYNC_OBD
	CRawThreadHelper *mpObdThread;
#endif

#endif

//----------------------------------------------------------------------------

#ifdef INCLUDE_IIO
	bool mbEnableIio;
	avf_int_t mAccInterval;
#endif

//----------------------------------------------------------------------------

private:
	CRawReg *mpRawReg;

//----------------------------------------------------------------------------

private:
	CDynamicBufferPool *mpBufferPool;
	IRecordObserver *mpObserver;

	CRawDataCache *mpGpsRawCache;
	CRawDataCache *mpAccRawCache;

private:
	bool mb_uploading;
	CMemBuffer *mp_upload_mb;

	CMutex mUploadMutex;
	avf_u64_t m_upload_start_tick;
	avf_u32_t m_upload_counter;

private:
//	char m_gps_buffer[1024];
	avf_u64_t mStartTick;
	avf_u64_t mLastTick;
	avf_int_t mUploadInterval;
	bool mbVinSent;

private:
	void GetUploadingFilename(char *buffer, int len, avf_uint_t index);
	void SaveToFileLocked();
	void StartUploadingLocked();
	void EndUploadingLocked(bool bLast);
};

//-----------------------------------------------------------------------
//
//  CSubtitleOutput
//
//-----------------------------------------------------------------------
class CSubtitleOutput: public COutputPin
{
	typedef COutputPin inherited;
	friend class CSubtitleFilter;

private:
	CSubtitleOutput(CSubtitleFilter *pFilter);
	virtual ~CSubtitleOutput();

// IPin
protected:
	virtual CMediaFormat* QueryMediaFormat();
	virtual IBufferPool* QueryBufferPool();

private:
	CSubtitleFilter *mpSubtitleFilter;
};

#endif


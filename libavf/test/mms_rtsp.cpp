
#define LOG_TAG	"rtsp"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "mms_if.h"

//{459A46CA-AB5D-4FAE-84A8-02D8CE4DE4C1}
AVF_DEFINE_IID(IID_IMMRtspServer,
0x459A46CA, 0xAB5D, 0x4FAE, 0x84, 0xA8, 0x2, 0xD8, 0xCE, 0x4D, 0xE4, 0xC1);

class CMMInputSource;
class CMMMediaSubsession;
class CMMRtspServer;

//-----------------------------------------------------------------------
//
// CMMInputSource
//
//-----------------------------------------------------------------------
class CMMInputSource: public FramedSource
{
	typedef FramedSource inherited;

public:
	static CMMInputSource *createNew(UsageEnvironment& env, IMMSource *pSource);

private:
	CMMInputSource(UsageEnvironment& env, IMMSource *pSource);
	virtual ~CMMInputSource();

private:
	virtual void doGetNextFrame();
	virtual void doStopGettingFrames();

private:
	static void doReadHandler(CMMInputSource* self, int mask);
	void doRead();

private:
	IMMSource *mpSource;
	bool mbHaveStartedReading;
	bool mbStarted;
	uint64_t m_last_timestamp_ms;
};

//-----------------------------------------------------------------------
//
// CMMMediaSubsession
//
//-----------------------------------------------------------------------
class CMMMediaSubsession: public OnDemandServerMediaSubsession
{
	typedef OnDemandServerMediaSubsession inherited;

public:
	static CMMMediaSubsession *createNew(UsageEnvironment& env, IMMSource *p_mm_src);

private:
	CMMMediaSubsession(UsageEnvironment& env, IMMSource *p_mm_src);
	virtual ~CMMMediaSubsession();

private:
	virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
	virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource);

private:
	IMMSource *mp_mm_src;
};

//-----------------------------------------------------------------------
//
// CMMRtspServer
//
//-----------------------------------------------------------------------
class CMMRtspServer: public CObject, public IMMRtspServer
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

	enum {
		UNICAST,
		MULTICAST,
	};

public:
	static CMMRtspServer *Create(IMMSource *pSource);

private:
	CMMRtspServer(IMMSource *pSource);
	virtual ~CMMRtspServer();

// IMMRtspServer
public:
	virtual avf_status_t Start();
	virtual avf_status_t Stop();

private:
	static void ThreadEntry(void *p);

private:
	IMMSource *mpSource;
	CThread *mpThread;

private:
	int m_mode;
	TaskScheduler* mp_scheduler;
	UsageEnvironment* mp_env;
	UserAuthenticationDatabase* mp_authDB;
	RTSPServer* mp_rtspServer;
	char volatile mb_stop_flag;
};

//-----------------------------------------------------------------------
//
// CMMInputSource
//
//-----------------------------------------------------------------------
CMMInputSource* CMMInputSource::createNew(UsageEnvironment& env, IMMSource *pSource)
{
	return new CMMInputSource(env, pSource);
}

CMMInputSource::CMMInputSource(UsageEnvironment& env, IMMSource *pSource):
	inherited(env),
	mpSource(pSource),
	mbHaveStartedReading(false),
	mbStarted(false)
{
}

CMMInputSource::~CMMInputSource()
{
}

void CMMInputSource::doGetNextFrame()
{
	doRead();
}

void CMMInputSource::doStopGettingFrames()
{
	AVF_LOGD("doStopGettingFrames");
	if (mbHaveStartedReading) {
		envir().taskScheduler().turnOffBackgroundReadHandling(mpSource->GetFileNo());
		mbHaveStartedReading = false;
	}
//	mbStarted = false;
	inherited::doStopGettingFrames();
}

void CMMInputSource::doReadHandler(CMMInputSource* self, int mask)
{
	self->doRead();
}

void CMMInputSource::doRead()
{
	if (!isCurrentlyAwaitingData()) {
		return;
	}

	unsigned preferredSize = 7 * 188;
	if (fMaxSize > preferredSize) {
		fMaxSize = preferredSize;
	}

	IMMSource::FrameInfo frameInfo;
	int nread = mpSource->Read(fTo, fMaxSize, &frameInfo);

	if (nread < 0) {
		AVF_LOGD("Read failed");
		handleClosure();
		return;
	}

	if (nread == 0) {
		if (!mbHaveStartedReading) {
			envir().taskScheduler().turnOnBackgroundReadHandling(mpSource->GetFileNo(),
				(TaskScheduler::BackgroundHandlerProc*)doReadHandler, this);
			mbHaveStartedReading = true;
		}
		return;
	}

	fFrameSize = nread;
	fNumTruncatedBytes = 0;

	if (frameInfo.new_frame) {
		if (!mbStarted) {
			mbStarted = true;
			gettimeofday(&fPresentationTime, NULL);
		} else {
			unsigned u_seconds = fPresentationTime.tv_usec + 
				(unsigned)(frameInfo.timestamp_ms - m_last_timestamp_ms) * 1000;
			fPresentationTime.tv_sec += u_seconds / 1000000;
			fPresentationTime.tv_usec = u_seconds % 1000000;
		}
		m_last_timestamp_ms = frameInfo.timestamp_ms;
	}

	FramedSource::afterGetting(this);
}

//-----------------------------------------------------------------------
//
// CMMMediaSubsession
//
//-----------------------------------------------------------------------
CMMMediaSubsession *CMMMediaSubsession::createNew(UsageEnvironment& env, IMMSource *p_mm_src)
{
	return new CMMMediaSubsession(env, p_mm_src);
}

CMMMediaSubsession::CMMMediaSubsession(UsageEnvironment& env, IMMSource *p_mm_src):
	inherited(env, true),	// true - reuse the first source
	mp_mm_src(p_mm_src)
{
}

CMMMediaSubsession::~CMMMediaSubsession()
{
}

FramedSource* CMMMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate)
{
	return CMMInputSource::createNew(envir(), mp_mm_src);
}

RTPSink* CMMMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,
	unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource)
{
	// 33 - payload type MPEG2 TS
	return SimpleRTPSink::createNew(envir(), rtpGroupsock, 33, 90000, "video", "MP2T", 1, true, false);
}

//-----------------------------------------------------------------------
//
// CMMRtspServer
//
//-----------------------------------------------------------------------
void *CMMRtspServer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMMRtspServer)
		return static_cast<IMMRtspServer*>(this);
	return inherited::GetInterface(refiid);
}

CMMRtspServer* CMMRtspServer::Create(IMMSource *pSource)
{
	CMMRtspServer *result = avf_new CMMRtspServer(pSource);
	CHECK_STATUS(result);
	return result;
}

CMMRtspServer::CMMRtspServer(IMMSource *pSource):
	mpSource(pSource),
	mpThread(NULL),

	m_mode(UNICAST),
	mp_scheduler(NULL),
	mp_env(NULL),
	mp_authDB(NULL),
	mp_rtspServer(NULL),
	mb_stop_flag(0)
{
	CREATE_OBJ(mp_scheduler = BasicTaskScheduler::createNew());
	CREATE_OBJ(mp_env = BasicUsageEnvironment::createNew(*mp_scheduler));
	CREATE_OBJ(mp_rtspServer = RTSPServer::createNew(*mp_env, 554, mp_authDB));

	ServerMediaSession *sms = ServerMediaSession::createNew(*mp_env, "", NULL, "avf RTSP server",
		m_mode == MULTICAST);
	if (sms == NULL) {
		mStatus = E_NOMEM;
		return;
	}

	sms->addSubsession(CMMMediaSubsession::createNew(*mp_env, mpSource));

	mp_rtspServer->addServerMediaSession(sms);

	char *url = mp_rtspServer->rtspURL(sms);
	AVF_LOGI("rtsp URL: " C_CYAN "%s" C_NONE, url);
	delete[] url;
}

CMMRtspServer::~CMMRtspServer()
{
	if (mp_rtspServer) {
		Medium::close(mp_rtspServer);
	}
	if (mp_env) {
		mp_env->reclaim();
	}
	if (mp_scheduler) {
		delete mp_scheduler;
	}
}

avf_status_t CMMRtspServer::Start()
{
	if (mpThread == NULL) {
		mpThread = CThread::Create("rtsp-server", ThreadEntry, this);
		if (mpThread == NULL) {
			return E_NOMEM;
		}
	}

	return E_OK;
}

avf_status_t CMMRtspServer::Stop()
{
	if (mpThread) {
		mb_stop_flag = 1;
		mpThread->Release();
		mpThread = NULL;
	}

	return E_OK;
}

void CMMRtspServer::ThreadEntry(void *p)
{
	AVF_LOGD("RTSP server starts");
	CMMRtspServer *self = (CMMRtspServer*)p;
	self->mp_env->taskScheduler().doEventLoop(&self->mb_stop_flag);
	AVF_LOGD("RTSP server exits");
}

//-----------------------------------------------------------------------

extern "C" IMMRtspServer* mms_create_rtsp_server(IMMSource *pSource)
{
	return CMMRtspServer::Create(pSource);
}


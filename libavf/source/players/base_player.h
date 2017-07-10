
#ifndef __BASE_PLAYER_H__
#define __BASE_PLAYER_H__

class CConfig;
class CConfigNode;
class CBasePlayer;

//-----------------------------------------------------------------------
//
//  CBasePlayer
//
//-----------------------------------------------------------------------
class CBasePlayer:
	public CObject,
	public IMediaPlayer
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

public:
	static CBasePlayer* Create(int id, IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);

protected:
	CBasePlayer(int id, IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);
	virtual ~CBasePlayer();

// IMediaPlayer
public:
	virtual avf_status_t OpenMedia(bool *pbDone);
	virtual avf_status_t PrepareMedia(bool *pbDone) { return E_OK; }
	virtual avf_status_t RunMedia(bool *pbDone);
	virtual avf_status_t StopMedia(bool *pbDone);

	virtual void SetState(avf_enum_t state) { mState = state; }
	virtual void CancelActionAsync() {}

	virtual void ProcessFilterMsg(const avf_msg_t& msg);
	virtual avf_status_t DispatchTimer(IFilter *pFilter, avf_enum_t id);
	virtual avf_status_t Command(avf_enum_t cmdId, avf_intptr_t arg) {
		return E_ERROR;
	}

	INLINE int GetID() {
		return m_id;
	}
	virtual avf_status_t DumpState();

// for derived class
protected:
	virtual avf_status_t OnFilterCreated(IFilter *pFilter) {
		return E_OK;
	}
	INLINE avf_enum_t GetState() { return mState; }

	INLINE void PostPlayerMsg(avf_enum_t event, avf_intptr_t p0, avf_intptr_t p1) {
		avf_msg_t msg;
		msg.sid = m_id;
		msg.code = event;
		msg.p0 = p0;
		msg.p1 = p1;
		mpEngine->PostPlayerMsg(msg);
	}

	INLINE void PostPlayerErrorMsg(int status = E_ERROR) {
		PostPlayerMsg(IEngine::EV_PLAYER_ERROR, status, 0);
	}

protected:
	const int m_id;	// player id
	IEngine *mpEngine;
	ap<string_t> mMediaSrc;
	CConfig *mpConfig;
	avf_enum_t mState;
	const int mConf;
	CFilterGraph mFilterGraph;

private:
	const char *GetPinMediaTypeName(IPin *pPin);

protected:
	avf_status_t RenderAllFilters(bool bUseExisting);
	avf_status_t RenderFilter(CConfigNode *connections, bool bUseExisting, IFilter *pFilter);
	void ResumeAllMediaRenderers();
	IFilter *CreateDownStreamFilter(CConfigNode *connections, bool bUseExisting,
		IFilter *pFilter, IPin *pPin, avf_size_t outIndex, avf_size_t& inIndex, bool& bNew);
	void ErrorStop();

protected:
	avf_status_t LoadAllFilters();
	avf_status_t CreateAllConnections();
	avf_status_t CreateGraph();
	IFilter *CreateFilter(const char *pName, const char *pType, CConfigNode *pFilterConfig);
	avf_status_t CreateConnection(CConfigNode *pConnection);
};

#endif


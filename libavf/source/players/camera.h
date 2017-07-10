
#ifndef __CAMERA_H__
#define __CAMERA_H__

class CConfig;
class CCamera;
class IRecordObserver;
class IImageControl;
class IVideoControl;
class ISubtitleInput;

//-----------------------------------------------------------------------
//
//  CCamera
//
//-----------------------------------------------------------------------
class CCamera: public CBasePlayer
{
	typedef CBasePlayer inherited;

public:
	static CCamera* Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);

protected:
	CCamera(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);
	virtual ~CCamera();

// IMediaPlayer
public:
	virtual avf_status_t RunMedia(bool *pbDone);
	virtual avf_status_t StopMedia(bool *pbDone);
	virtual void ProcessFilterMsg(const avf_msg_t& msg);
	virtual avf_status_t Command(avf_enum_t cmdId, avf_intptr_t arg);

// derived
protected:
	virtual avf_status_t OnFilterCreated(IFilter *pFilter);

private:
	virtual avf_status_t RunAllFilters();

private:
	INLINE IRecordObserver *GetRecordObserver() {
		return (IRecordObserver*)mpEngine->FindComponent(IID_IRecordObserver);
	}
	avf_status_t StartRecordVdb();
	void StopRecordVdb();

private:
	bool mbStarting;
	bool mbStopping;
	bool mbSaving;
	bool mbVdbRecording;
	bool mbDiskMonitor;

private:
	IImageControl *mpImageControl;
	IVideoControl *mpVideoControl;
	ISubtitleInput *mpSubtitleInput;
	IFilter *mpSubtitleFilter;
	ISegmentSaver *mpSegmentSaver;
};


#endif


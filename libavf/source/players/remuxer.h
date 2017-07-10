
#ifndef __REMUXER_H__
#define __REMUXER_H__

#include "filter_if.h"

class CConfig;
class IMediaSource;

//-----------------------------------------------------------------------
//
//  CRemuxer
//
//-----------------------------------------------------------------------
class CRemuxer: public CBasePlayer
{
	typedef CBasePlayer inherited;

public:
	static CRemuxer* Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);

protected:
	CRemuxer(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);
	virtual ~CRemuxer();

// IMediaPlayer
public:
	virtual avf_status_t OpenMedia(bool *pbDone);
	virtual avf_status_t RunMedia(bool *pbDone);
	virtual void ProcessFilterMsg(const avf_msg_t& msg);
	virtual avf_status_t Command(avf_enum_t cmdId, avf_intptr_t arg);

private:
	avf_status_t LoadFilter(const char *type, const char *name, IFilter **ppFilter, ap<string_t>& format);
	avf_status_t Connect();
	avf_status_t Connect2();
	IFilter *CreateFilterByName(const char *name, CConfigNode *config);
	avf_status_t MuxNextFile(bool bFirst);
	avf_status_t GetNextFile(char filename[], int filename_size, int *start_time, int *length);
	IAVIO *OpenFile(const char *filename);
	IMediaFileReader *CreateFileReader(const char *filename, const ap<string_t>& format);

private:
	static avf_status_t GetInput(const char **ppStart, char buffer[], int buffer_size, int ch);

protected:
	IFilter *mpSourceFilter;
	IFilter *mpAudioSourceFilter;

	IReadProgress *mpReadProgress;
	IMediaSource *mpMediaSource;
	IMediaSource *mpAudioMediaSource;

	IFilter *mpSinkFilter;

	ap<string_t> mSourceList;
	const char *mpSource;

	ap<string_t> mSourceFormat;
	ap<string_t> mAudioSourceFormat;
	ap<string_t> mSinkFormat;
};

#endif


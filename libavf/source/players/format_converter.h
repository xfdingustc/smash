
#ifndef __FORMAT_CONVERTER_H__
#define __FORMAT_CONVERTER_H__

#include "filter_if.h"

class CConfig;
class IMediaSource;

//-----------------------------------------------------------------------
//
//  CFormatConverter
//
//-----------------------------------------------------------------------
class CFormatConverter: public CBasePlayer
{
	typedef CBasePlayer inherited;

public:
	static CFormatConverter* Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);

protected:
	CFormatConverter(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);
	virtual ~CFormatConverter();

// IMediaPlayer
public:
	virtual avf_status_t OpenMedia(bool *pbDone);
	virtual avf_status_t PrepareMedia(bool *pbDone);
	virtual avf_status_t RunMedia(bool *pbDone);
	virtual void ProcessFilterMsg(const avf_msg_t& msg);
	virtual avf_status_t Command(avf_enum_t cmdId, avf_intptr_t arg) {
		return E_INVAL;
	}

protected:
	IMediaSource *mpMediaSource;
};

#endif


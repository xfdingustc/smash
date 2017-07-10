
#ifndef __AVF_REMUXER_IF_H__
#define __AVF_REMUXER_IF_H__

class IMediaControl;

//-----------------------------------------------------------------------
//
//  Remuxer - please do not subclass it
//
//-----------------------------------------------------------------------
class Remuxer
{
public:
	Remuxer(IMediaControl *pMediaControl) {
		mpMediaControl = pMediaControl;
	}
	~Remuxer() {}

private:
	IMediaControl *mpMediaControl;

public:
	enum {
		CMD_GetRemuxProgress,
	};

public:
	avf_status_t GetRemuxProgress(IReadProgress::progress_t *progress) {
		return mpMediaControl->Command(CMP_ID_REMUXER, CMD_GetRemuxProgress, (avf_intptr_t)progress);
	}
};

#endif


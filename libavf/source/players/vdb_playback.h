
#ifndef __VDB_PLAYBACK_H__
#define __VDB_PLAYBACK_H__

class CVdbPlayback;
class CVdbIO;

//-----------------------------------------------------------------------
//
//  CVdbPlayback
//
//-----------------------------------------------------------------------
class CVdbPlayback: public CFilePlayback
{
	typedef CFilePlayback inherited;

public:
	static CVdbPlayback* Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);

protected:
	CVdbPlayback(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig);
	virtual ~CVdbPlayback();

// IMediaPlayer
public:
	virtual avf_status_t OpenMedia(bool *pbDone);

private:
	avf_status_t OpenClip();

private:
	IVdbInfo::range_s mRange;
	CVdbIO *mpIO;
};

#endif


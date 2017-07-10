
#ifndef __FILTER_IF_H__
#define __FILTER_IF_H__

extern const avf_iid_t IID_ISourceFilter;
extern const avf_iid_t IID_ISubtitleInput;
extern const avf_iid_t IID_IImageControl;
extern const avf_iid_t IID_IVideoControl;
extern const avf_iid_t IID_IMediaSource;

class IAVIO;
class IMediaFileReader;

//-----------------------------------------------------------------------
//
// ISourceFilter
//
//-----------------------------------------------------------------------
class ISourceFilter: public IInterface
{
public:
	DECLARE_IF(ISourceFilter, IID_ISourceFilter);

	// Before running a source filter, first ask them to OpenDevice().
	// When all source filters has done (by sending MSG_STARTED),
	// then call RunFilters() on them
	virtual avf_status_t StartSourceFilter(bool *pbDone) = 0;

	// Stop source filters so they will generate EOS to other filters
	// When all filters report EOS, the player is then stopped
	virtual avf_status_t StopSourceFilter() = 0;
};

//-----------------------------------------------------------------------
//
// ISubtitleInput
//
//-----------------------------------------------------------------------
class ISubtitleInput: public IInterface
{
public:
	DECLARE_IF(ISubtitleInput, IID_ISubtitleInput);

//	virtual avf_status_t SendSubtitle(const avf_u8_t *content,
//		avf_size_t size, avf_uint_t duration_ms) = 0;
	virtual avf_status_t SendFirstSubtitle() = 0;
};

//-----------------------------------------------------------------------
//
// IImageControl
//
//-----------------------------------------------------------------------
class IImageControl: public IInterface
{
public:
	DECLARE_IF(IImageControl, IID_IImageControl);

	virtual avf_status_t SetImageConfig(name_value_t *param) = 0;
	virtual avf_status_t EnableVideoPreview() = 0;
	virtual avf_status_t DisableVideoPreview() = 0;
};

//-----------------------------------------------------------------------
//
// IVideoControl
//
//-----------------------------------------------------------------------
class IVideoControl: public IInterface
{
public:
	struct stillcap_info_s {
		avf_u8_t b_running;
		avf_u8_t stillcap_state;
		avf_u8_t b_raw;
		avf_u8_t last_error;
		int num_pictures;
		int burst_ticks;
	};

public:
	DECLARE_IF(IVideoControl, IID_IVideoControl);

	virtual avf_status_t SaveNextPicture(const char *filename) = 0;
	virtual avf_status_t StillCapture(int action, int flags) = 0;
	virtual avf_status_t GetStillCaptureInfo(stillcap_info_s *info) = 0;
	virtual avf_status_t ChangeVoutVideo(const char *config) = 0;
};

//-----------------------------------------------------------------------
//
// IMediaSource
//	todo - relation with ISourceFilter?
//
//-----------------------------------------------------------------------
class IMediaSource: public IInterface
{
public:
	DECLARE_IF(IMediaSource, IID_IMediaSource);

	virtual avf_status_t OpenMediaSource(IAVIO *io) = 0;
	virtual avf_status_t OpenMediaSource(IMediaFileReader *pReader) = 0;

	virtual avf_status_t GetMediaInfo(avf_media_info_t *info) = 0;
	virtual avf_status_t GetVideoInfo(avf_uint_t index, avf_video_info_t *info) = 0;
	virtual avf_status_t GetAudioInfo(avf_uint_t index, avf_audio_info_t *info) = 0;
	virtual avf_status_t GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info) = 0;

	virtual avf_status_t SetRange(avf_u32_t start_time_ms, avf_s32_t length_ms, bool bLastFile) = 0;
	virtual avf_status_t SeekToTime(avf_u32_t ms) = 0;

	virtual bool CanFastPlay() = 0;
	virtual avf_status_t SetFastPlay(int mode, int speed) = 0;
};

#endif


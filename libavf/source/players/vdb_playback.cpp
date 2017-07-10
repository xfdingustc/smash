
#define LOG_TAG "vdb_playback"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "filter_if.h"
#include "avf_config.h"
#include "mem_stream.h"

#include "avio.h"
#include "filter_list.h"
#include "file_demuxer.h"

#include "base_player.h"
#include "file_playback.h"

#include "vdb_playback.h"
#include "vdb_playback_priv.h"

#include "vdb_cmd.h"

//-----------------------------------------------------------------------
//
//  CVdbPlayback
//
//-----------------------------------------------------------------------
CVdbPlayback* CVdbPlayback::Create(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig)
{
	CVdbPlayback *result = avf_new CVdbPlayback(pEngine, mediaSrc, conf, pConfig);
	CHECK_STATUS(result);
	return result;
}

CVdbPlayback::CVdbPlayback(IEngine *pEngine, const ap<string_t>& mediaSrc, int conf, CConfig *pConfig):
	inherited(pEngine, mediaSrc, conf, pConfig),
	mpIO(NULL)
{
	IVdbInfo *info = (IVdbInfo*)mpEngine->FindComponent(IID_IVdbInfo);
	if (info == NULL) {
		AVF_LOGE("No IVdbInfo");
		mStatus = E_BADCALL;
		return;
	}

	CREATE_OBJ(mpIO = CVdbIO::Create(info));
}

CVdbPlayback::~CVdbPlayback()
{
	avf_safe_release(mpIO);
}

// open the file and recognize it
avf_status_t CVdbPlayback::OpenMedia(bool *pbDone)
{
	avf_status_t status;
	IMediaFileReader *reader = NULL;
	IFilter *pFilter = NULL;

	AVF_ASSERT(mpMediaSource == NULL);

	if ((status = OpenClip()) != E_OK)
		return status;

	if ((reader = avf_create_media_file_reader_by_format(mpEngine, "ts", mpIO)) == NULL) {
		status = E_ERROR;
		goto Done;
	}

	if ((pFilter = CFileDemuxer::Create(mpEngine, "FileDemuxer", NULL)) == NULL) {
		status = E_ERROR;
		goto Done;
	}

	status = OpenMediaSource(pFilter, reader, mpIO);

	if (status != E_OK) {
		avf_safe_release(mpMediaSource);
	}

	mMediaInfo.length_ms = mRange.length_ms;
	mMediaInfo.num_videos = 1;	// todo
	mMediaInfo.num_audios = 1;	// todo
	mMediaInfo.num_subtitles = 0;	// todo

Done:
	avf_safe_release(pFilter);
	avf_safe_release(reader);
	return status;
}

avf_status_t CVdbPlayback::OpenClip()
{
	char dir[8];

	if (::sscanf(mMediaSrc->string(), "vdb:%7[a-z]/%x_" LLD "_%x",
			dir, &mRange.ref_clip_id, &mRange.clip_time_ms, &mRange.length_ms) != 4) {
		AVF_LOGE("bad url %s", mMediaSrc->string());
		return E_ERROR;
	}

#if 1
	mRange._stream = 0;	// todo
	mRange.b_playlist = 0;
	mRange.length_ms = 0;
	if (::strcmp(dir, "buffer") == 0)
		mRange.clip_type = 0;
	else
		mRange.clip_type = 1;
#else
	mRange.clip_type = 256;
	mRange._stream = 0;
	mRange.b_playlist = 1;
	mRange.ref_clip_id = 256;
	mRange.length_ms = 0;
	mRange.clip_time_ms = 0;
#endif

	avf_status_t status = mpIO->OpenClip(&mRange);
	if (status != E_OK)
		return status;

	mRange.length_ms = mpIO->GetLengthMs();
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CVdbIO
//
//-----------------------------------------------------------------------
CVdbIO *CVdbIO::Create(IVdbInfo *pVdbInfo)
{
	CVdbIO *result = avf_new CVdbIO(pVdbInfo);
	CHECK_STATUS(result);
	return result;
}

CVdbIO::CVdbIO(IVdbInfo *pVdbInfo):
	mpVdbInfo(pVdbInfo),
	mpClipReader(NULL)
{
}

CVdbIO::~CVdbIO()
{
	avf_safe_release(mpClipReader);
}

void *CVdbIO::GetInterface(avf_refiid_t refiid)
{
//	if (refiid == IID_ISeekToTime && mpClipReader)	// todo
//		return mpClipReader->GetInterface(refiid);
	return inherited::GetInterface(refiid);
}

avf_status_t CVdbIO::OpenClip(IVdbInfo::range_s *range)
{
	IClipReader *reader = mpVdbInfo->CreateClipReaderOfTime(range, false, false, 0, 0);
	if (reader == NULL) {
		AVF_LOGE("Cannot create IClipReader");
		return E_ERROR;
	}

	avf_safe_release(mpClipReader);
	mpClipReader = reader;

	return E_OK;
}

avf_u32_t CVdbIO::GetLengthMs()
{
	return mpClipReader ? mpClipReader->GetLengthMs() : 0;
}

avf_status_t CVdbIO::Close()
{
	avf_safe_release(mpClipReader);
	return E_OK;
}

int CVdbIO::Read(void *buffer, avf_size_t size)
{
	if (mpClipReader == NULL) {
		AVF_LOGE("CVdbIO::Reader: no clip reader");
		return -1;
	}
	return mpClipReader->ReadClip((avf_u8_t*)buffer, size);
}

avf_u64_t CVdbIO::GetSize()
{
	return mpClipReader ? mpClipReader->GetSize() : 0;
}

avf_u64_t CVdbIO::GetPos()
{
	return mpClipReader ? mpClipReader->GetPos() : 0;
}

avf_status_t CVdbIO::Seek(avf_s64_t pos, int whence)
{
	return mpClipReader ? mpClipReader->Seek(pos, whence) : E_ERROR;
}


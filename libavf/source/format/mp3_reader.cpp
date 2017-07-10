
#define LOG_TAG "mp3_reader"

#include <limits.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"

#include "avf_media_format.h"
#include "mpeg_utils.h"

#include "mp3_reader.h"

//-----------------------------------------------------------------------
//
//  CMp3Reader
//
//-----------------------------------------------------------------------
avf_int_t CMp3Reader::Recognize(IAVIO *io)
{
	mp3_header_info_t info;
	CReadBuffer buf(io);
	return get_header_info(buf, info) == E_OK ? 100 : 0;
}

avf_u32_t CMp3Reader::sync_size(avf_u32_t size)
{
	return (((size>>24) & 0x7F) << 21) +
		(((size>>16) & 0x7F) << 14) + 
		(((size>>8) & 0x7F) << 7) +
		(size & 0x7F) + 10;
}

avf_status_t CMp3Reader::get_header_info(CReadBuffer& buf, mp3_header_info_t& info)
{
	avf_u32_t tmp = buf.read_be_32();

	// skip ID
	if ((tmp >> 8) == (('I'<<16)|('D'<<8)|('3'))) {
		buf.read_be_16();
		avf_u32_t size = buf.read_be_32();
		size = sync_size(size);
		buf.Skip(size - 10);
		tmp = buf.read_be_32();
	}

	// todo - find sync header

	if (mp3_parse_header(tmp, &info) < 0) {
		return E_UNKNOWN;
	}

	return E_OK;
}

IMediaFileReader* CMp3Reader::Create(IEngine *pEngine, IAVIO *io)
{
	CMp3Reader *result = avf_new CMp3Reader(pEngine, io);
	CHECK_STATUS(result);
	return result;
}

CMp3Reader::CMp3Reader(IEngine *pEngine, IAVIO *io):
	inherited(pEngine),
	mBuffer(io, 4*KB)
{
	mpIO = io->acquire();
	CREATE_OBJ(mpTrack = CMp3Track::Create(this));
}

CMp3Reader::~CMp3Reader()
{
	avf_safe_release(mpTrack);
	avf_safe_release(mpIO);
}

avf_status_t CMp3Reader::OpenMediaSource()
{
	mpIO->Seek(0);
	mBuffer.SetIO(mpIO);
	mPercent = 0;

	avf_status_t status = get_header_info(mBuffer, mInfo);
	if (status != E_OK)
		return status;

	avf_u64_t start_pos = mBuffer.GetReadPos() - 4;
	avf_u64_t end_pos = mBuffer.GetFileSize();

	mBuffer.SeekTo(end_pos - 128);
	if ((mBuffer.read_be_32() >> 8) == (('T'<<16)|('A'<<8)|'G')) {
		AVF_LOGD("mp3 tag at end");
		end_pos -= 128;
	}

	AVF_LOGD("start_pos: %d, end_pos: %d", (avf_uint_t)start_pos, (avf_uint_t)end_pos);

	mBuffer.SeekTo(start_pos);

	mpTrack->mReadBytes = 0;
	mpTrack->mTotalBytes = end_pos;
	mpTrack->mRemainBytes = end_pos - start_pos;
	mpTrack->m_pts = 0;

	CMp3Format *pFormat = mpTrack->mpFormat;

	// CMediaFormat
	pFormat->mMediaType = MEDIA_TYPE_AUDIO;
	pFormat->mFormatType = MF_Mp3Format;
	pFormat->mFrameDuration = mInfo.samples_per_frame;
	pFormat->mTimeScale = mInfo.sampling_rate;

	// CAudioFormat
	pFormat->mNumChannels = mInfo.num_channels;
	pFormat->mSampleRate = mInfo.sampling_rate;
	pFormat->mBitsPerSample = 16;	// todo
	pFormat->mBitsFormat = 0;	// todo
	pFormat->mFramesPerChunk = 1024;	// todo

	return E_OK;
}

void CMp3Reader::ReportProgress()
{
	if (mbReportProgress && mpTrack) {
		//printf("prog: %d, %d\n", (int)mpTrack->mTotalBytes, (int)mpTrack->mRemainBytes);
		DoReportProgress(mpTrack->mTotalBytes, mpTrack->mRemainBytes);
	}
}

avf_uint_t CMp3Reader::GetNumTracks()
{
	return 1;
}

IMediaTrack* CMp3Reader::GetTrack(avf_uint_t index)
{
	return mpTrack;
}

avf_status_t CMp3Reader::GetMediaInfo(avf_media_info_t *info)
{
	info->length_ms = 0;
	info->num_videos = 0;
	info->num_audios = 1;
	info->num_subtitles = 0;

	return E_OK;
}

avf_status_t CMp3Reader::GetVideoInfo(avf_uint_t index, avf_video_info_t *info)
{
	return E_ERROR;
}

avf_status_t CMp3Reader::GetAudioInfo(avf_uint_t index, avf_audio_info_t *info)
{
	return E_OK;
}

avf_status_t CMp3Reader::GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info)
{
	return E_ERROR;
}

avf_status_t CMp3Reader::SeekToTime(avf_u32_t ms, avf_u32_t *out_ms)
{
	return ms == 0 ? E_OK : E_ERROR;
}

avf_status_t CMp3Reader::FetchSample(avf_size_t *pTrackIndex)
{
	*pTrackIndex = 0;
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CMp3Track
//
//-----------------------------------------------------------------------

CMp3Track *CMp3Track::Create(CMp3Reader *pReader)
{
	CMp3Track *result = avf_new CMp3Track(pReader);
	CHECK_STATUS(result);
	return result;
}

CMp3Track::CMp3Track(CMp3Reader *pReader):
	mpReader(pReader)
{
	CREATE_OBJ(mpFormat = avf_alloc_media_format(CMp3Format));
}

CMp3Track::~CMp3Track()
{
	avf_safe_release(mpFormat);
}

CMediaFormat* CMp3Track::QueryMediaFormat()
{
	return mpFormat->acquire();
}

avf_status_t CMp3Track::SeekToTime(avf_u32_t ms)
{
	return ms == 0 ? E_OK : E_ERROR;
}

avf_u32_t CMp3Track::FindSync(CReadBuffer& buf)
{
	if (mRemainBytes == 0) {
		return 0;
	}

	avf_uint_t header = buf.read_le_8();
	mRemainBytes--;

	while (mRemainBytes) {
		header <<= 8;
		header += buf.read_le_8();
		mRemainBytes--;

		if ((header & 0xFFE0) == 0xFFE0) {
			if (mRemainBytes <= 2)
				return 0;
			header <<= 16;
			header += buf.read_be_16();
			mRemainBytes -= 2;
			return header;
		}
	}

	return header;
}

avf_status_t CMp3Track::ReadSample(CBuffer *pBuffer, bool& bEOS)
{
	CReadBuffer& buf = mpReader->mBuffer;

	avf_u32_t header = FindSync(buf);
	if (header == 0) {
		bEOS = true;
		return E_OK;
	}

	mp3_header_info_t& info = mpReader->mInfo;
	avf_uint_t frame_size = 144 * info.bitrate / info.sampling_rate;
	if ((header >> 9) & 1)
		frame_size++;

	//printf("frame_size: %d\n", frame_size);

	if (frame_size > mRemainBytes + 4) {
		bEOS = true;
		return E_OK;
	}

	avf_status_t status = pBuffer->AllocMem(frame_size);
	if (status != E_OK)
		return status;

	avf_u8_t *p = pBuffer->GetMem();
	avf_write_be_32(p, header);

	if (!buf.read_mem(p + 4, frame_size - 4)) {
		AVF_LOGE("read_mem failed: %d", frame_size);
		return E_IO;
	}

	mRemainBytes -= frame_size - 4;
	mReadBytes += frame_size;
	if (mReadBytes >= 32*KB) {
		mReadBytes = 0;
		mpReader->ReportProgress();
	}

	//pBuffer->mpData = data;
	//pBuffer->mSize = mPesSize;
	pBuffer->mType = CBuffer::DATA;
	pBuffer->mFlags = CBuffer::F_KEYFRAME;
	pBuffer->frame_type = 0;
	pBuffer->m_offset = 0;
	pBuffer->m_data_size = frame_size;
	pBuffer->m_dts = m_pts;
	pBuffer->m_pts = m_pts;
	pBuffer->m_duration = info.samples_per_frame;
	m_pts += info.samples_per_frame;

	return E_OK;
}


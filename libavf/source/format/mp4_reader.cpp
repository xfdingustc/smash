
#define LOG_TAG "mp4_reader"

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

#include "mp4_reader.h"

//-----------------------------------------------------------------------
//
//  CMP4Reader
//
//-----------------------------------------------------------------------

avf_int_t CMP4Reader::Recognize(IAVIO *io)
{
	io->Seek(4);
	if (io->read_be_32() == FCC_VAL("ftyp"))
		return 100;
	return 0;
}

IMediaFileReader* CMP4Reader::Create(IEngine *pEngine, IAVIO *io)
{
	CMP4Reader *result = avf_new CMP4Reader(pEngine, io);
	CHECK_STATUS(result);
	return result;
}

CMP4Reader::CMP4Reader(IEngine *pEngine, IAVIO *io):
	inherited(pEngine)
{
	mpIO = io->acquire();

	SET_ARRAY_NULL(mpTracks);
	mpVideoTrack = NULL;
	mpAudioTrack = NULL;
	mpTextTrack = NULL;

	mpCurrentTrack = NULL;
	mpAmbaFormat = NULL;

	mNumTracks = 0;
	mNumVideos = 0;
	mNumAudios = 0;
	mNumTexts = 0;
}

CMP4Reader::~CMP4Reader()
{
	SAFE_RELEASE_ARRAY(mpTracks);
	avf_safe_release(mpIO);
	avf_safe_release(mpAmbaFormat);
}

avf_status_t CMP4Reader::OpenMediaSource()
{
	avf_status_t status;
	avf_u64_t next_box_pos;
	bool bMovieRead = false;

	mNumTracks = 0;
	mNumVideos = 0;
	mNumAudios = 0;

	m_video_width = 0;
	m_video_height = 0;
	m_duration = 0;

	mpIO->Seek(0);
	while (true) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0)
			break;

		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC('f','t','y','p'):
			break;

		case MKFCC('m','d','a','t'):
			break;

		case MKFCC('m','o','o','v'):
			if ((status = ReadMovie(size)) != E_OK)
				return status;
			bMovieRead = true;
			break;

		default:
			break;
		}

		if (bMovieRead)
			break;

		mpIO->Seek(next_box_pos);
	}

	if (mNumVideos == 0 && mNumAudios == 0) {
		AVF_LOGE("no video nor audio");
		return E_ERROR;
	}

//	if (m_duration == 0) {
//		AVF_LOGE("duration is 0");
//		return E_ERROR;
//	}

	return E_OK;
}

avf_uint_t CMP4Reader::GetNumTracks()
{
	return mNumTracks;
}

IMediaTrack* CMP4Reader::GetTrack(avf_uint_t index)
{
	IMediaTrack *track = index >= mNumTracks ? NULL : mpTracks[index];
	return track;
}

avf_status_t CMP4Reader::GetMediaInfo(avf_media_info_t *info)
{
	info->length_ms = m_duration;
	info->num_videos = mNumVideos;
	info->num_audios = mNumAudios;
	info->num_subtitles = 0;
	return E_OK;
}

avf_status_t CMP4Reader::GetVideoInfo(avf_uint_t index, avf_video_info_t *info)
{
	if (index == 0 && mNumVideos > 0) {
		info->width = m_video_width;
		info->height = m_video_height;
		return E_OK;
	}
	return E_INVAL;
}

avf_status_t CMP4Reader::GetAudioInfo(avf_uint_t index, avf_audio_info_t *info)
{
	return E_ERROR; // todo
}

avf_status_t CMP4Reader::GetSubtitleInfo(avf_uint_t index, avf_subtitle_info_t *info)
{
	return E_ERROR; // todo
}

avf_status_t CMP4Reader::ReadMovie(avf_size_t box_size)
{
	avf_size_t box_pos = 8;
	avf_u64_t next_box_pos;

	while (box_pos < box_size) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0) {
			AVF_LOGE("bad box in movie");
			return E_ERROR;
		}
		box_pos += size;
		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC('m','v','h','d'):
			if (ReadMovieHeader(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('t','r','a','k'):
			if (ReadTrack(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('u','d','t','a'):
			if (ReadUserData(size) != E_OK)
				return E_ERROR;
			break;

		default:
			break;
		}

		mpIO->Seek(next_box_pos);
	}

	return E_OK;
}

avf_status_t CMP4Reader::ReadMovieHeader(avf_size_t box_size)
{
	mpIO->Skip(20-8);

	avf_u32_t timescale = mpIO->read_be_32();
	avf_u32_t duration = mpIO->read_be_32();

	if (timescale == 0) {
		AVF_LOGE("bad timescale 0");
		return E_ERROR;
	}

	m_duration = (avf_u32_t)((avf_u64_t)duration * 1000 / timescale);

	return E_OK;
}


avf_status_t CMP4Reader::ReadTrack(avf_size_t box_size)
{
	avf_size_t box_pos = 8;
	avf_u64_t next_box_pos;

	m_track_timescale = 0;
	m_track_duration = 0;
	mpCurrentTrack = NULL;

	while (box_pos < box_size) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0) {
			AVF_LOGE("bad box in track");
			return E_ERROR;
		}
		box_pos += size;
		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC('t','k','h','d'):
			if (ReadTrackHeader(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('e','d','t','s'):
			break;

		case MKFCC('m','d','i','a'):
			if (ReadMedia(size) != E_OK)
				return E_ERROR;
			break;

		default:
			break;
		}

		mpIO->Seek(next_box_pos);
	}

	if (IsInVideoTrack()) {
		// we're in video track, fetch the resolution
		m_video_width = m_tmp_width;
		m_video_height = m_tmp_height;
	}

	return E_OK;
}


avf_status_t CMP4Reader::ReadUserData(avf_size_t box_size)
{
	avf_size_t box_pos = 8;
	avf_u64_t next_box_pos;

	while (box_pos < box_size) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0) {
			AVF_LOGE("bad box in media");
			return E_ERROR;
		}
		box_pos += size;
		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC((avf_u32_t)0xA9, 'e','n','c'):
			// version info
			break;

		case MKFCC('A','M','B','A'):
			if (size < 128) {
				AVF_LOGE("bad AMBA box");
				return E_ERROR;
			} else {
				avf_status_t status = ReadAmbaFormat(type);
				if (status != E_OK)
					return status;
			}
			break;

		default:
			break;
		}

		mpIO->Seek(next_box_pos);
	}

	return E_OK;
}

avf_status_t CMP4Reader::ReadAmbaFormat(avf_u32_t type)
{
	CAmbaAVCFormat *pAmbaFormat = avf_alloc_media_format(CAmbaAVCFormat);
	if (pAmbaFormat == NULL)
		return E_NOMEM;

	avf_safe_release(mpAmbaFormat);
	mpAmbaFormat = pAmbaFormat;

	pAmbaFormat->mFlags = 0;
	pAmbaFormat->mMediaType = MEDIA_TYPE_VIDEO;
	pAmbaFormat->mFormatType = MF_H264VideoFormat;
	pAmbaFormat->mFrameDuration = 0;
	pAmbaFormat->mTimeScale = _90kHZ;
	pAmbaFormat->mWidth = 0;
	pAmbaFormat->mHeight = 0;
	pAmbaFormat->mDataFourcc = type;

	pAmbaFormat->data.info.ar_x = mpIO->read_be_16();
	pAmbaFormat->data.info.ar_y = mpIO->read_be_16();
	pAmbaFormat->data.info.mode = mpIO->read_be_8();
	pAmbaFormat->data.info.M = mpIO->read_be_8();
	pAmbaFormat->data.info.N = mpIO->read_be_8();
	pAmbaFormat->data.info.gop_model = mpIO->read_be_8();
	pAmbaFormat->data.info.idr_interval = mpIO->read_be_32();
	pAmbaFormat->data.info.rate = mpIO->read_be_32();
	pAmbaFormat->data.info.scale = mpIO->read_be_32();
	pAmbaFormat->data.info.bitrate = mpIO->read_be_32();
	pAmbaFormat->data.info.bitrate_min = mpIO->read_be_32();
	pAmbaFormat->data.info.idr_size = mpIO->read_be_32();
	::memset(pAmbaFormat->data.reserved, 0, sizeof(pAmbaFormat->data.reserved));

	return E_OK;
}

avf_status_t CMP4Reader::ReadTrackHeader(avf_size_t box_size)
{
	mpIO->Skip(84-8);
	m_tmp_width = mpIO->read_be_32() >> 16;
	m_tmp_height = mpIO->read_be_32() >> 16;
	return E_OK;
}

avf_status_t CMP4Reader::ReadMedia(avf_size_t box_size)
{
	avf_size_t box_pos = 8;
	avf_u64_t next_box_pos;

	while (box_pos < box_size) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0) {
			AVF_LOGE("bad box in media");
			return E_ERROR;
		}
		box_pos += size;
		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC('m','d','h','d'):
			if (ReadMediaHeader(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('h','d','l','r'):
			if (ReadHandlerReference(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('m','i','n','f'):
			if (ReadMediaInformation(size) != E_OK)
				return E_ERROR;
			break;

		default:
			break;
		}

		mpIO->Seek(next_box_pos);
	}

	return E_OK;
}


avf_status_t CMP4Reader::ReadMediaHeader(avf_size_t box_size)
{
	mpIO->Skip(20-8);
	m_track_timescale = mpIO->read_be_32();
	m_track_duration = mpIO->read_be_32();
	return E_OK;
}

avf_status_t CMP4Reader::ReadHandlerReference(avf_size_t box_size)
{
	mpIO->Skip(16-8);

	avf_u32_t handler_type = mpIO->read_be_32();
	switch (handler_type) {
	case MKFCC('v','i','d','e'):
		if (mpVideoTrack == NULL) {
			if ((mpVideoTrack = AllocTrack()) == NULL)
				return E_NOMEM;

			mNumVideos++;
			mpVideoTrack->m_timescale = m_track_timescale;
			mpVideoTrack->m_duration = m_track_duration;

			mpCurrentTrack = mpVideoTrack;
		}
		break;

	case MKFCC('s','o','u','n'):
		if (mpAudioTrack == NULL) {
			if ((mpAudioTrack = AllocTrack()) == NULL)
				return E_NOMEM;

			mNumAudios++;
			mpAudioTrack->m_timescale = m_track_timescale;
			mpAudioTrack->m_duration = m_track_duration;

			mpCurrentTrack = mpAudioTrack;
		}
		break;

	case MKFCC('t','e','x','t'):
		if (mpTextTrack == NULL) {
			if ((mpTextTrack = AllocTrack()) == NULL)
				return E_NOMEM;

			// todo
			mNumTracks--;

			mNumTexts++;
			mpTextTrack->m_timescale = m_track_timescale;
			mpTextTrack->m_duration = m_track_duration;

			mpCurrentTrack = mpTextTrack;
		}
		break;

	default:
		break;
	}

	return E_OK;
}

avf_status_t CMP4Reader::ReadMediaInformation(avf_size_t box_size)
{
	avf_size_t box_pos = 8;
	avf_u64_t next_box_pos;

	if (mpCurrentTrack == NULL) {	// ignore this track
		return E_OK;
	}

	while (box_pos < box_size) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0) {
			AVF_LOGE("bad box in media information");
			return E_ERROR;
		}
		box_pos += size;
		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC('v','m','h','d'):
			if (ReadVideoMediaHeader(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('s','m','h','d'):
			if (ReadAudioMediaHeader(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('d','i','n','f'):
			break;

		case MKFCC('s','t','b','l'):
			if (ReadSampleTable(size) != E_OK)
				return E_ERROR;
			break;

		default:
			break;
		}

		mpIO->Seek(next_box_pos);
	}

	return E_OK;
}

avf_status_t CMP4Reader::ReadVideoMediaHeader(avf_size_t box_size)
{
	if (!IsInVideoTrack()) {
		AVF_LOGE("not in video track");
		return E_ERROR;
	}
	return E_OK;
}

avf_status_t CMP4Reader::ReadAudioMediaHeader(avf_size_t box_size)
{
	if (!IsInAudioTrack()) {
		AVF_LOGE("not in audio track");
		return E_ERROR;
	}
	return E_OK;
}

// stbl
avf_status_t CMP4Reader::ReadSampleTable(avf_size_t box_size)
{
	avf_size_t box_pos = 8;
	avf_u64_t next_box_pos;

	while (box_pos < box_size) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0) {
			AVF_LOGE("bad box in sample table");
			return E_ERROR;
		}
		box_pos += size;
		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC('s','t','s','d'):
			if (mpCurrentTrack->ReadSampleDescription(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('s','t','t','s'):
			if (mpCurrentTrack->ReadDecodingTimeToSample(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('c','t','t','s'):
			if (mpCurrentTrack->ReadCompositionTimeToSample(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('s','t','s','s'):
			if (mpCurrentTrack->ReadSyncSampleTable(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('s','t','s','c'):
			if (mpCurrentTrack->ReadSampleToChunk(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('s','t','s','z'):
			if (mpCurrentTrack->ReadSampleSize(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('s','t','c','o'):
			if (mpCurrentTrack->ReadChunkOffset(size) != E_OK)
				return E_ERROR;
			break;

		case MKFCC('c','o','6','4'):
			if (mpCurrentTrack->ReadChunkOffset64(size) != E_OK)
				return E_ERROR;
			break;

		default:
			break;
		}

		mpIO->Seek(next_box_pos);
	}

	return E_OK;
}

CMP4Track *CMP4Reader::AllocTrack()
{
	if (mNumTracks >= ARRAY_SIZE(mpTracks)) {
		AVF_LOGE("too many tracks %d", mNumTracks);
		return NULL;
	}

	CMP4Track *track = avf_new CMP4Track(this, mNumTracks, mpIO, true);	// bBufferAll
	CHECK_STATUS(track);

	if (track) {
		mpTracks[mNumTracks++] = track;
	}

	return track;
}

//-----------------------------------------------------------------------
//
//  CMP4Track
//
//-----------------------------------------------------------------------
CMP4Track::CMP4Track(CMP4Reader *pReader, avf_uint_t index, IAVIO *io, bool bBufferAll):
	mpReader(pReader),
	mIndex(index),
	mpIO(io),
	mbBufferAll(bBufferAll),
	mp_media_format(NULL)
{
	m_sample_count = 0;
	mb_chunk_valid = false;
	mb_dts_valid = false;
	mb_pts_valid = false;

	m_stts_entry_count = 0;
	m_ctts_entry_count = 0;
	m_stss_entry_count = 0;
	m_stsc_entry_count = 0;
	m_stsz_entry_count = 0;
	m_stco_entry_count = 0;

	m_stts_orig = NULL;
	m_ctts_orig = NULL;
	m_stss_orig = NULL;
	m_stsc_orig = NULL;
	m_stsz_orig = NULL;
	m_stco_orig = NULL;

	m_sps = NULL;
	m_pps = NULL;
}

CMP4Track::~CMP4Track()
{
	avf_safe_release(mp_media_format);

	avf_safe_free(m_stts_orig);
	avf_safe_free(m_ctts_orig);
	avf_safe_free(m_stss_orig);
	avf_safe_free(m_stsc_orig);
	avf_safe_free(m_stsz_orig);
	avf_safe_free(m_stco_orig);

	avf_safe_free(m_sps);
	avf_safe_free(m_pps);
}

CMediaFormat* CMP4Track::QueryMediaFormat()
{
	CMediaFormat *pFormat = mp_media_format;
	if (pFormat == NULL)
		return NULL;

	if (pFormat->mFormatType == MF_H264VideoFormat) {
		CAmbaAVCFormat *pAmbaFormat = mpReader->mpAmbaFormat;
		if (pAmbaFormat) {
			CH264VideoFormat *pAVCFormat = (CH264VideoFormat*)pFormat;

			pAmbaFormat->mFlags = pAVCFormat->mFlags;
			//pAmbaFormat->mMediaType = MEDIA_TYPE_VIDEO;
			//pAmbaFormat->mFormatType = MF_H264VideoFormat;
			pAmbaFormat->mFrameDuration = pAVCFormat->mFrameDuration;
			pAmbaFormat->mTimeScale = pAVCFormat->mTimeScale;
			pAmbaFormat->mWidth = pAVCFormat->mWidth;
			pAmbaFormat->mHeight = pAVCFormat->mHeight;

			pFormat = pAmbaFormat;
		}
	}

	return pFormat->acquire();
}

// stsd
avf_status_t CMP4Track::ReadSampleDescription(avf_size_t box_size)
{
	avf_size_t entry_count;

	mpIO->Skip(4);	// version & flag

	entry_count = mpIO->read_be_32();
	if (entry_count != 1) {
		AVF_LOGE("sample description entry count is %d, not supported", entry_count);
		return E_INVAL;
	}

	// mpIO->Skip(4);
	avf_size_t size = mpIO->read_be_32();

	if (mpReader->IsInVideoTrack()) {
		return ReadVideoSampleDescription(size);
	}

	if (mpReader->IsInAudioTrack()) {
		return ReadAudioSampleDescription(size);
	}

	if (mpReader->IsInTextTrack()) {
		return ReadTextSampleDescription(size);
	}

	return E_OK;
}

avf_status_t CMP4Track::ReadVideoSampleDescription(avf_size_t box_size)
{
	avf_u32_t entry_type;

	entry_type = mpIO->read_be_32();
	if (entry_type != MKFCC('a','v','c','1')) {
		AVF_LOGE("only avc1 is supported");
		return E_INVAL;
	}

	avf_size_t width;
	avf_size_t height;

	mpIO->Skip(32-8);
	width = mpIO->read_be_16();
	height = mpIO->read_be_16();

	AVF_ASSERT(mp_media_format == NULL);
	CH264VideoFormat *pFormat = avf_alloc_media_format(CH264VideoFormat);
	if (pFormat == NULL)
		return E_NOMEM;
	mp_media_format = pFormat;

	pFormat->mMediaType = MEDIA_TYPE_VIDEO;
	pFormat->mFormatType = MF_H264VideoFormat;
	pFormat->mFrameDuration = 0;	// todo
	pFormat->mTimeScale = mpReader->m_track_timescale;
	pFormat->mWidth = width;
	pFormat->mHeight = height;
	pFormat->mDataFourcc = 0;

	// avc configuration
	mpIO->Skip(86-36+4);
	avf_u32_t avf_config_type = mpIO->read_be_32();
	if (avf_config_type != MKFCC('a','v','c','C')) {
		AVF_LOGE("no avcC");
		return E_INVAL;
	}

	// SPS
	mpIO->Skip(4);
	avf_u32_t value = mpIO->read_be_16();
	if ((value & 0x1F) != 1) {	// numOfSPS
		AVF_LOGE("numOfSPS: %d", value & 0x1F);
		return E_INVAL;
	}

	AVF_ASSERT(m_sps == NULL);
	m_sps_size = mpIO->read_be_16();
	if ((m_sps = avf_malloc_bytes(m_sps_size)) == NULL)
		return E_NOMEM;

	if (mpIO->Read(m_sps, m_sps_size) < 0)
		return E_IO;

	// PPS
	value = mpIO->read_be_8();
	if (value != 1) {
		AVF_LOGE("numOfPPS: %d", value);
		return E_INVAL;
	}

	AVF_ASSERT(m_pps == NULL);
	m_pps_size = mpIO->read_be_16();
	if ((m_pps = avf_malloc_bytes(m_pps_size)) == NULL)
		return E_NOMEM;

	if (mpIO->Read(m_pps, m_pps_size) < 0)
		return E_IO;

	AVF_LOGD("sps: %d, pps: %d", m_sps_size, m_pps_size);
	return E_OK;
}

avf_status_t CMP4Track::ReadAudioSampleDescription(avf_size_t box_size)
{
	avf_u32_t entry_type;

	entry_type = mpIO->read_be_32();
	if(entry_type != MKFCC('m','p','4','a')) {
		AVF_LOGE("only mp4a is supported");
		return E_INVAL;
	}

	mpIO->Skip(8);	// reserved 6, data_reference_index 2

	avf_uint_t version = mpIO->read_be_16();
	avf_uint_t revision = mpIO->read_be_16();
	if (version != 0 && revision != 0) {
		AVF_LOGE("unknown version %d revision %d", version, revision);
		return E_INVAL;
	}

	mpIO->Skip(4);	// vender

	avf_size_t channels = mpIO->read_be_16();
	avf_size_t sample_size = mpIO->read_be_16();

	mpIO->Skip(4);	// predefined 2, packet_size 2

	avf_u32_t sample_rate = mpIO->read_be_16();

	mpIO->Skip(2);	// reserved

	if (channels != 1 && channels != 2) {
		AVF_LOGE("unknown channels %d, must be 1 or 2", channels);
		return E_INVAL;
	}

	if (sample_size != 16) {
		AVF_LOGE("unknown sample_size %d", sample_size);
		return E_INVAL;
	}

	if ((m_aac_sample_rate_index = aac_get_sample_rate_index(sample_rate)) < 0) {
		AVF_LOGE("bad sample rate: %d", sample_rate);
		return E_INVAL;
	}

	m_aac_layer = 0;	// TODO:
	m_aac_profile = 1;	// TODO:

	AVF_ASSERT(mp_media_format == NULL);
	CAacFormat *pFormat = avf_alloc_media_format(CAacFormat);
	if (pFormat == NULL)
		return E_NOMEM;
	mp_media_format = pFormat;

	// CMediaFormat
	pFormat->mMediaType = MEDIA_TYPE_AUDIO;
	pFormat->mFormatType = MF_AACFormat;
	pFormat->mFrameDuration = 1024; // todo
	pFormat->mTimeScale = sample_rate;

	// CAudioFormat
	pFormat->mNumChannels = channels;
	pFormat->mSampleRate = sample_rate;
	pFormat->mBitsPerSample = sample_size;
	pFormat->mBitsFormat = 0;
	pFormat->mFramesPerChunk = 1024;	// todo

	pFormat->mDecoderDataSize = 0;
	::memset(&pFormat->mDecoderData, 0, sizeof(pFormat->mDecoderData));

	AVF_LOGD("channels: %d, sample_rate: %d", channels, sample_rate);

	// ------------------------------
	// ESDescriptor
	// ------------------------------

	avf_size_t box_pos = 36;
	avf_u64_t next_box_pos;

	while (box_pos < box_size) {
		avf_u32_t size = mpIO->read_be_32();
		avf_u32_t type = mpIO->read_be_32();

		if (size == 0) {
			AVF_LOGE("bad box in sample table");
			return E_ERROR;
		}
		box_pos += size;
		next_box_pos = mpIO->GetPos() + (size - 8);

		switch (type) {
		case MKFCC('e','s','d','s'):
			if (mpIO->read_be_8() == 0) {	// version
				mpIO->Skip(3);	// flags
				if (mpIO->read_be_8() == 3) {	// ES_DescrTag
					avf_status_t status = ReadAudioESDescTag();
					if (status != E_OK) {
						return status;
					}
				}
			}
			break;
		default:
			break;
		}

		mpIO->Seek(next_box_pos);
	}

	return E_OK;
}

// ES_DescrTag(3)
// save DecSpecificInfo(5) to mDecoderData
avf_status_t CMP4Track::ReadAudioESDescTag()
{
	avf_size_t nbytes = 0;
	avf_size_t offset = 0;

	avf_size_t desc_size = ReadVarSize(&nbytes);
	mpIO->Skip(2);	// ES_ID
	offset += 2;

	avf_uint_t flags = mpIO->read_be_8();
	offset++;

	if (flags & 0x80) {
		// streamDependenceFlag
		mpIO->Skip(2);	// dependsOn_ES_ID
		offset += 2;
	}
	if (flags & 0x40) {
		// URL_Flag
		avf_size_t urllen = mpIO->read_be_8();
		if (urllen > 0) {
			mpIO->Skip(urllen);	// URLString
		}
		offset += (urllen + 1);
	}
	if (flags & 0x20) {
		// OCRstreamFlag
		mpIO->Skip(2);	// OCR_ES_ID
		offset += 2;
	}

	AVF_LOGD("desc_size %d, offset %d", desc_size, offset);

	// descriptors: DecoderConfiguration, SLConfig
	while (desc_size > offset) {
		int tag = mpIO->read_be_8();
		avf_size_t tag_remain_size = ReadVarSize(&nbytes);
		avf_size_t tag_size = 1 + nbytes + tag_remain_size;
		offset += tag_size;

		avf_u64_t next_tag_pos = mpIO->GetPos() + tag_remain_size;

		if (tag == 4) {
			// DecoderConfigDescrTag (4)
			avf_status_t status = ReadAudioDecoderConfiguration(tag_remain_size);
			if (status != E_OK) {
				return status;
			}
		}

		mpIO->Seek(next_tag_pos);
	}

	return E_OK;
}

avf_status_t CMP4Track::ReadAudioDecoderConfiguration(avf_size_t tag_remain_size)
{
	avf_size_t offset = 1 + 1 + 3 + 8;	// objectTypeIndication 1, streamType etc 1, bufferSizeDB 3,
		// maxBitrate 4, avgBitrate 4
	mpIO->Skip(offset);

	while (tag_remain_size > offset) {
		int tag = mpIO->read_be_8();
		avf_size_t nbytes = 0;
		avf_size_t tag_remain_size = ReadVarSize(&nbytes);
		avf_size_t tag_size = 1 + nbytes + tag_remain_size;
		offset += tag_size;

		avf_u64_t next_tag_pos = mpIO->GetPos() + tag_remain_size;

		if (tag == 5) {	// DecSpecificInfoTag
			CAudioFormat *pFormat = (CAudioFormat*)mp_media_format;
			if (pFormat->mFlags & CMediaFormat::F_HAS_EXTRA_DATA) {
				AVF_LOGW("media format already has extra data");
			} else if (tag_remain_size > sizeof(pFormat->mDecoderData)) {
				AVF_LOGW("DecSpecificInfoTag too large %d", tag_remain_size);
			} else {
				if (mpIO->Read(pFormat->mDecoderData, tag_remain_size) != (int)tag_remain_size) {
					return E_IO;
				}
				pFormat->mDecoderDataSize = tag_remain_size;
				pFormat->mFlags |= CMediaFormat::F_HAS_EXTRA_DATA;
				AVF_LOGD("DecSpecificInfoTag size: %d bytes", tag_remain_size);
			}
		}

		mpIO->Seek(next_tag_pos);
	}

	return E_OK;
}

// 0xxxxxxx
// 1xxxxxxx 0xxxxxxx
// 1xxxxxxx 1xxxxxxx 0xxxxxxx
// 1xxxxxxx 1xxxxxxx 1xxxxxxx 0xxxxxxx
avf_size_t CMP4Track::ReadVarSize(avf_size_t *nbytes)
{
	avf_size_t result = 0;
	*nbytes = 0;

	while (true) {
		avf_uint_t v = mpIO->read_be_8();
		(*nbytes)++;

		result = (result << 7) | (v & 0x7F);
		if ((v & 0x80) == 0)
			break;
	}

	return result;
}

avf_status_t CMP4Track::ReadTextSampleDescription(avf_size_t box_size)
{
	// todo
	return E_OK;
}

avf_u8_t *CMP4Track::ReadBlock(avf_size_t size, const char *name)
{
	avf_u8_t *ptr = avf_malloc_bytes(size);
	if (ptr && mpIO->Read(ptr, size) < 0) {
		AVF_LOGD("read %s failed", name);
		avf_free(ptr);
		ptr = NULL;
	}
	return ptr;
}

avf_status_t CMP4Track::ReadDecodingTimeToSample(avf_size_t box_size)
{
	mpIO->Skip(4);
	m_stts_entry_count = mpIO->read_be_32();
	m_stts_offset = mpIO->GetPos();

	if (mbBufferAll) {
		if ((m_stts_orig = ReadBlock(m_stts_entry_count * 8, "stts")) == NULL) {
			return E_DEVICE;
		}
	}

	return E_OK;
}

avf_status_t CMP4Track::ReadCompositionTimeToSample(avf_size_t box_size)
{
	mpIO->Skip(4);
	m_ctts_entry_count = mpIO->read_be_32();
	m_ctts_offset = mpIO->GetPos();

	if (mbBufferAll) {
		if ((m_ctts_orig = ReadBlock(m_ctts_entry_count * 8, "ctts")) == NULL) {
			return E_DEVICE;
		}
	}

	return E_OK;
}

avf_status_t CMP4Track::ReadSyncSampleTable(avf_size_t box_size)
{
	mpIO->Skip(4);
	m_stss_entry_count = mpIO->read_be_32();
	m_stss_offset = mpIO->GetPos();

	if (mbBufferAll) {
		if ((m_stss_orig = ReadBlock(m_stss_entry_count * 4, "stss")) == NULL) {
			return E_DEVICE;
		}
	}

	return E_OK;
}

avf_status_t CMP4Track::ReadSampleToChunk(avf_size_t box_size)
{
	mpIO->Skip(4);
	m_stsc_entry_count = mpIO->read_be_32();
	m_stsc_offset = mpIO->GetPos();

	if (mbBufferAll) {
		if ((m_stsc_orig = ReadBlock(m_stsc_entry_count * 12, "stsc")) == NULL)
			return E_DEVICE;
	}

	return E_OK;
}

avf_status_t CMP4Track::ReadSampleSize(avf_size_t box_size)
{
	mpIO->Skip(4);
	m_sample_size = mpIO->read_be_32();
	m_stsz_entry_count = mpIO->read_be_32();
	m_stsz_offset = mpIO->GetPos();

	if (mbBufferAll) {
		if ((m_stsz_orig = ReadBlock(m_stsz_entry_count * 4, "stsz")) == NULL)
			return E_DEVICE;
	}

	// here init the sample pointer
	m_sample_count = m_stsz_entry_count;

	return E_OK;
}

avf_status_t CMP4Track::ReadChunkOffset(avf_size_t box_size)
{
	mpIO->Skip(4);
	m_stco_entry_count = mpIO->read_be_32();
	m_stco_offset = mpIO->GetPos();

	if (mbBufferAll) {
		if ((m_stco_orig = ReadBlock(m_stco_entry_count * 4, "stco")) == NULL)
			return E_DEVICE;
	}

	return E_OK;
}

avf_status_t CMP4Track::ReadChunkOffset64(avf_size_t box_size)
{
	// todo
	return E_ERROR;
}

avf_status_t CMP4Track::SeekToTime(avf_u32_t ms)
{
	avf_u64_t dts;
	avf_u64_t start_dts;
	avf_u32_t sample_index;
	avf_u32_t sync_sample_index;
	avf_status_t status;

	// reset cache
	m_started = false;
	m_cache_sample_remain = 0;
	m_cache_dts_remain = 0;
	m_cache_pts_remain = 0;
	m_sample_index = 0;

	dts = mp_media_format->ms_to_pts(ms);
	status = GetSampleOfDts(dts, &sample_index, &start_dts);
	if (status != E_OK)
		return status;

	if (m_stss_entry_count > 0) {
		status = GetSyncSample(sample_index, &sync_sample_index);
		if (status != E_OK)
			return status;
		status = GetDtsOfSample(sample_index);
		if (status != E_OK)
			return status;
		start_dts = m_curr_dts.DtsOfSample(sample_index);
	} else {
		sync_sample_index = sample_index;
	}

	m_sample_index = sync_sample_index;

	m_start_dts = start_dts;
	m_last_dts = start_dts;

	return E_OK;
}

avf_status_t CMP4Track::ReadSample(CBuffer *pBuffer, bool& bEOS)
{
	avf_status_t status;
	avf_u64_t offset;
	avf_u32_t size;
	avf_u64_t dts;
	avf_u64_t pts;
	avf_u32_t duration;

	// check EOS
	if (m_sample_index >= m_sample_count) {
		// pBuffer->SetEOS();
		bEOS = true;
		return E_OK;
	}

	// get sample offset and size
	while (true) {

		// look in cache
		if (m_cache_sample_remain) {
			offset = m_cache_sample_offset;
			if ((status = GetSampleSize(m_sample_index, &size)) != E_OK)
				return status;

			m_cache_sample_offset += size;
			m_cache_sample_remain--;

			break;
		}

		// find the chunk
		if ((status = GetChunkOfSample(m_sample_index)) != E_OK)
			return status;

		if ((status = StartChunk()) != E_OK)
			return status;
	}

	// get sample dts
	while (true) {

		// look in cache
		if (m_cache_dts_remain) {
			dts = m_cache_sample_dts;
			duration = m_cache_dts_delta;
			m_last_dts = dts + duration;

			m_cache_sample_dts += m_cache_dts_delta;
			m_cache_dts_remain--;
			break;
		}

		// find the dts block
		if ((status = GetDtsOfSample(m_sample_index)) != E_OK)
			return status;

		StartDts(m_sample_index);
	}

	dts += m_dts_offset;
	pts = dts;

	// get sample pts
	if (m_ctts_entry_count) {
		while (true) {

			// look in cache
			if (m_cache_pts_remain) {
				pts += (avf_s32_t)m_cache_pts_offset;
				m_cache_pts_remain--;
				break;
			}

			// find the pts block
			if ((status = GetPtsOfSample(m_sample_index)) != E_OK)
				return status;

			StartPts(m_sample_index);
		}
	}

	pBuffer->m_dts = dts;
	pBuffer->m_pts = pts;
	pBuffer->m_duration = duration;

	mpIO->Seek(offset);

	switch (mp_media_format->mMediaType) {
	case MEDIA_TYPE_AUDIO:
		if (mp_media_format->mFormatType != MF_AACFormat)
			return E_INVAL;
		status = ReadAACSample(pBuffer, offset, size);
		break;

	case MEDIA_TYPE_VIDEO:
		if (mp_media_format->mFormatType != MF_H264VideoFormat)
			return E_INVAL;
		status = ReadAVCSample(pBuffer, offset, size);
		break;

	case MEDIA_TYPE_SUBTITLE:
		if (mp_media_format->mFormatType != MF_SubtitleFormat)
			return E_INVAL;
		status = ReadTextSample(pBuffer, offset, size);
		break;

	default:
		return E_INVAL;
	}

	if (status != E_OK)
		return status;

	m_sample_index++;

	bEOS = false;
	return E_OK;
}

avf_status_t CMP4Track::ReadAVCSample(CBuffer *pBuffer, avf_u64_t offset, avf_size_t size)
{
	AVF_ASSERT(pBuffer->mpData == NULL);

	avf_u32_t flags = 0;
	avf_size_t total_size = size;

	if (!m_started) {
		if (m_sps)
			total_size += 4 + m_sps_size;
		if (m_pps)
			total_size += 4 + m_pps_size;
	}

	avf_status_t status = pBuffer->AllocMem(total_size);
	if (status != E_OK)
		return status;

	avf_u8_t *ptr = pBuffer->mpData;
	if (!m_started) {
		if (m_sps) {
			avf_write_be_32(ptr, 1);
			ptr += 4;
			::memcpy(ptr, m_sps, m_sps_size);
			ptr += m_sps_size;
		}
		if (m_pps) {
			avf_write_be_32(ptr, 1);
			ptr += 4;
			::memcpy(ptr, m_pps, m_pps_size);
			ptr += m_pps_size;
		}
		m_started = true;
	}

	if (mpIO->Read(ptr, size) < 0) {
		AVF_LOGD("read AVC sample %d failed at addr " LLD ", size %d", m_sample_index, offset, size);
		return E_IO;
	}

	int frame_type = CH264VideoFormat::UNK;
	avf_size_t index = ptr - pBuffer->mpData;
	while (index < total_size) {
		avf_u32_t length = avf_read_be_32(ptr);
		if (length > 1) {
			int nal_type = ptr[4] & 0x1F;
			switch (nal_type) {
			case 5:		// IDR picture
				flags |= CBuffer::F_AVC_IDR | CBuffer::F_KEYFRAME;
				frame_type = CH264VideoFormat::I;
				break;

			case 1:	 {	// non-IDR picture
					int slice_type = ptr[5] >> 2;	// first_mb(1), slice_type(5)
					if (slice_type == 0x26)
						frame_type = CH264VideoFormat::P;
					else if (slice_type == 0x27)
						frame_type = CH264VideoFormat::B;
				}
				break;

			default:
				break;
			}
		}
		ptr[0] = 0; ptr[1] = 0; ptr[2] = 0; ptr[3] = 1;
		ptr += 4 + length;
		index += 4 + length;
	}

	pBuffer->mType = CBuffer::DATA;
	pBuffer->mFlags = flags;
	pBuffer->frame_type = frame_type;
	//pBuffer->mSize = total_size;
	pBuffer->m_offset = 0;
	pBuffer->m_data_size = total_size;
	//pBuffer->m_dts = 0;
	//pBuffer->m_pts = 0;
	//pBuffer->m_duration = 0;

	return E_OK;
}

avf_status_t CMP4Track::ReadAACSample(CBuffer *pBuffer, avf_u64_t offset, avf_size_t size)
{
	// currently we can only handle AAC format
	CAacFormat *pFormat = (CAacFormat*)mp_media_format;

	size += 7;	// todo

	avf_status_t status = pBuffer->AllocMem(size);
	if (status != E_OK)
		return status;

	avf_uint_t ID = 1;	// MPEG-2
	avf_uint_t layer = m_aac_layer;
	avf_uint_t profile = m_aac_profile;
	avf_uint_t sample_rate_index = m_aac_sample_rate_index;
	avf_uint_t channels = pFormat->mNumChannels;
	avf_uint_t buffer_fullness = 0;

	avf_u8_t *ptr = pBuffer->mpData;
	*ptr++ = 0xFF;
	*ptr++ = 0xF0 | (ID << 3) | (layer << 2) | (1 << 0);
	*ptr++ = (profile << 6) | (sample_rate_index << 2) | (channels >> 2);
	*ptr++ = (channels << 6) | (size >> 11);
	*ptr++ = (size >> 3);
	*ptr++ = (size << 5) | (buffer_fullness >> 6);
	*ptr++ = (buffer_fullness << 2);

	if (mpIO->Read(ptr, size - 7) < 0) {
		AVF_LOGD("read AAC sample %d failed at addr " LLD ", size %d", m_sample_index, offset, size - 7);
		return E_IO;
	}

	pBuffer->mType = CBuffer::DATA;
	pBuffer->mFlags = 0;
	//pBuffer->mSize = size;
	pBuffer->m_offset = 0;
	pBuffer->m_data_size = size;
	//pBuffer->m_dts = 0;
	//pBuffer->m_pts = 0;
	//pBuffer->m_duration = 1024;

	return E_OK;
}

avf_status_t CMP4Track::ReadTextSample(CBuffer *pBuffer, avf_u64_t offset, avf_size_t size)
{
	// todo
	return E_ERROR;
}

avf_status_t CMP4Track::GetChunkOfSample(avf_u32_t sample_index)
{
	avf_status_t status;

	if (!mb_chunk_valid) {
		if ((status = GetFirstChunkInfo()) != E_OK)
			return status;
		mb_chunk_valid = true;
	}

	if (sample_index < m_curr_chunk.sample_start_index) {
		// look backward from the first chunk
		if ((status = GetFirstChunkInfo()) != E_OK)
			return status;
	}

	while (true) {
		if (IsSampleInCurrChunk(sample_index))
			return E_OK;
		if (++m_curr_chunk.chunk_index < m_next_chunk_index) {
			FillNextChunk();
		} else {
			if ((status = GetNextChunkInfo()) != E_OK)
				return status;
		}
	}
}

// this chunk entry is same with the previous
void CMP4Track::FillNextChunk()
{
	m_curr_chunk.sample_start_index = m_curr_chunk.sample_end_index;
	m_curr_chunk.sample_end_index += m_curr_chunk.num_samples;

	if (m_curr_chunk.sample_end_index > m_sample_count) {
		// exceeds total samples
		avf_size_t delta = m_curr_chunk.sample_end_index - m_sample_count;
		if (delta < m_curr_chunk.num_samples) {
			m_curr_chunk.num_samples -= delta;
			m_curr_chunk.sample_end_index -= delta;
		} else {
			// no samples in this chunk!
			m_curr_chunk.num_samples = 0;
			m_curr_chunk.sample_end_index = m_curr_chunk.sample_start_index;
		}
	}
}

avf_status_t CMP4Track::GetFirstChunkInfo()
{
	if (GetSampleToChunk(0, 0,
			&m_curr_chunk.num_samples, &m_next_chunk_index) != E_OK)
		return E_ERROR;

	m_curr_chunk.chunk_index = 0;
	m_curr_chunk.sample_start_index = 0;
	m_curr_chunk.sample_end_index = m_curr_chunk.num_samples;

	m_chunk_read_index = 1;
	return E_OK;
}

avf_status_t CMP4Track::GetNextChunkInfo()
{
	if (GetSampleToChunk(m_chunk_read_index, m_curr_chunk.chunk_index,
			&m_curr_chunk.num_samples, &m_next_chunk_index) != E_OK)
		return E_ERROR;

	m_curr_chunk.sample_start_index = m_curr_chunk.sample_end_index;
	m_curr_chunk.sample_end_index += m_curr_chunk.num_samples;

	m_chunk_read_index++;
	return E_OK;
}

avf_status_t CMP4Track::StartChunk()
{
	avf_status_t status = GetChunkOffset(m_curr_chunk.chunk_index, &m_cache_sample_offset);
	if (status != E_OK)
		return status;
	m_cache_sample_remain = m_curr_chunk.num_samples;
	return E_OK;
}

avf_status_t CMP4Track::GetSampleOfDts(avf_u64_t dts, avf_u32_t *sample_index, avf_u64_t *start_dts)
{
	avf_status_t status;

	if (!mb_dts_valid) {
		if ((status = GetFirstDtsInfo()) != E_OK)
			return status;
		mb_dts_valid = true;
	}

	if (dts < m_curr_dts.start_dts) {
		// goto first block
		if ((status = GetFirstDtsInfo()) != E_OK)
			return status;
	}

	while (true) {
		if (dts < m_curr_dts.EndDts()) {
			if (m_curr_dts.sample_delta == 0) {
				AVF_LOGE("bad stts entry %d pin %d", m_dts_read_index, mIndex);
				return E_ERROR;
			}
			avf_u32_t index = (dts - m_curr_dts.start_dts) / m_curr_dts.sample_delta;
			*start_dts = m_curr_dts.DtsOfSample(index);
			index += m_curr_dts.sample_start_index;
			*sample_index = index;
			return E_OK;
		}
		if ((status = GetNextDtsInfo()) != E_OK)
			return status;
	}
}

avf_status_t CMP4Track::GetDtsOfSample(avf_u32_t sample_index)
{
	avf_status_t status;

	if (!mb_dts_valid) {
		if ((status = GetFirstDtsInfo()) != E_OK)
			return status;
		mb_dts_valid = true;
	}

	if (sample_index < m_curr_dts.sample_start_index) {
		// goto first block
		if ((status = GetFirstDtsInfo()) != E_OK)
			return status;
	}

	while (true) {
		if (IsSampleInCurrDts(sample_index))
			return E_OK;
		if ((status = GetNextDtsInfo()) != E_OK)
			return status;
	}
}

avf_status_t CMP4Track::GetFirstDtsInfo()
{
	avf_u32_t sample_count;
	avf_u32_t sample_delta;

	if (GetDecodingTimeToSample(0, &sample_count, &sample_delta) != E_OK)
		return E_ERROR;

	m_curr_dts.sample_start_index = 0;
	m_curr_dts.sample_end_index = sample_count;
	m_curr_dts.sample_delta = sample_delta;
	m_curr_dts.start_dts = 0;

	m_dts_read_index = 1;
	return E_OK;
}

avf_status_t CMP4Track::GetNextDtsInfo()
{
	avf_u32_t sample_count;
	avf_u32_t sample_delta;

	if (GetDecodingTimeToSample(m_dts_read_index, &sample_count, &sample_delta) != E_OK)
		return E_ERROR;

	m_curr_dts.start_dts += m_curr_dts.sample_delta * 
		(m_curr_dts.sample_end_index - m_curr_dts.sample_start_index);

	m_curr_dts.sample_start_index = m_curr_dts.sample_end_index;
	m_curr_dts.sample_end_index += sample_count;
	m_curr_dts.sample_delta = sample_delta;

	m_dts_read_index++;
	return E_OK;
}

void CMP4Track::StartDts(avf_u32_t sample_index)
{
	m_cache_sample_dts = m_curr_dts.start_dts +
		(sample_index - m_curr_dts.sample_start_index) * m_curr_dts.sample_delta;
	m_cache_dts_delta = m_curr_dts.sample_delta;
	m_cache_dts_remain = m_curr_dts.sample_end_index - sample_index;
}

avf_status_t CMP4Track::GetPtsOfSample(avf_u32_t sample_index)
{
	avf_status_t status;

	if (!mb_pts_valid) {
		if ((status = GetFirstPtsInfo()) != E_OK)
			return status;
		mb_pts_valid = true;
	}

	if (sample_index < m_curr_pts.sample_start_index) {
		// goto first block
		if ((status = GetFirstPtsInfo()) != E_OK)
			return status;
	}

	while (true) {
		if (IsSampleInCurrPts(sample_index))
			return E_OK;
		if ((status = GetNextPtsInfo()) != E_OK)
			return status;
	}
}

avf_status_t CMP4Track::GetFirstPtsInfo()
{
	avf_u32_t sample_count;
	avf_u32_t sample_offset;

	if (GetCompositionTimeToSample(0, &sample_count, &sample_offset) != E_OK) {
		return E_ERROR;
	}

	m_curr_pts.sample_start_index = 0;
	m_curr_pts.sample_end_index = sample_count;
	m_curr_pts.sample_offset = sample_offset;

	m_pts_read_index = 1;
	return E_OK;
}

avf_status_t CMP4Track::GetNextPtsInfo()
{
	avf_u32_t sample_count;
	avf_u32_t sample_offset;

	if (GetCompositionTimeToSample(m_pts_read_index, &sample_count, &sample_offset) != E_OK) {
		return E_ERROR;
	}

	m_curr_pts.sample_start_index = m_curr_pts.sample_end_index;
	m_curr_pts.sample_end_index += sample_count;
	m_curr_pts.sample_offset = sample_offset;

	m_pts_read_index++;
	return E_OK;
}

void CMP4Track::StartPts(avf_u32_t sample_index)
{
	m_cache_pts_offset = m_curr_pts.sample_offset;
	m_cache_pts_remain = m_curr_pts.sample_end_index - sample_index;
}

// stts
avf_status_t CMP4Track::GetDecodingTimeToSample(avf_u32_t read_index,
	avf_u32_t *sample_count, avf_u32_t *sample_delta)
{
	if (read_index >= m_stts_entry_count) {
		AVF_LOGE("no such stts entry %d pin %d", read_index, mIndex);
		return E_ERROR;
	}

	if (!mbBufferAll) {
		return E_ERROR;
	} else {
		*sample_count = avf_read_be_32(m_stts_orig + read_index * 8);
		*sample_delta = avf_read_be_32(m_stts_orig + read_index * 8 + 4);
		if (*sample_count == 0) {
			AVF_LOGE("bad stts entry %d", read_index);
			return E_ERROR;
		}
		return E_OK;
	}
}

// ctts
avf_status_t CMP4Track::GetCompositionTimeToSample(avf_u32_t read_index,
	avf_u32_t *sample_count, avf_u32_t *sample_offset)
{
	if (read_index >= m_ctts_entry_count) {
		AVF_LOGE("no such ctts entry %d", read_index);
		return E_ERROR;
	}

	if (!mbBufferAll) {
		return E_ERROR;
	} else {
		*sample_count = avf_read_be_32(m_ctts_orig + read_index * 8);
		*sample_offset = avf_read_be_32(m_ctts_orig + read_index * 8 + 4);
		if (*sample_count == 0) {
			AVF_LOGE("bad ctts entry %d", read_index);
			return E_ERROR;
		}
		return E_OK;
	}
}

avf_status_t CMP4Track::GetSyncSample(avf_u32_t sample_index, avf_u32_t *sync_sample_index)
{
	avf_u8_t *ptr = m_stss_orig;
	avf_u32_t prev_sync_index = 0;
	for (avf_u32_t i = 0; i < m_stss_entry_count; i++, ptr += 4) {
		avf_u32_t sync_index = avf_read_be_32(ptr) - 1;
		if (sample_index < sync_index)
			break;
		prev_sync_index = sync_index;
	}
	*sync_sample_index = prev_sync_index;
	return E_OK;
}

// stsc
avf_status_t CMP4Track::GetSampleToChunk(avf_u32_t read_index, 
		avf_u32_t curr_chunk_index, avf_u32_t *num_samples, avf_u32_t *next_chunk_index)
{
	if (read_index >= m_stsc_entry_count) {
		AVF_LOGE("no such stsc entry %d", read_index);
		return E_INVAL;
	}

	if (!mbBufferAll) {
		return E_ERROR;
	} else {
		*num_samples = avf_read_be_32(m_stsc_orig + (read_index*12 + 4));

		read_index++;
		if (read_index < m_stsc_entry_count) {
			*next_chunk_index = avf_read_be_32(m_stsc_orig + read_index*12) - 1;
		} else {
			*next_chunk_index = UINT_MAX;
		}

		if (*num_samples == 0 || *next_chunk_index <= curr_chunk_index) {
			AVF_LOGE("bad stsc entry %d", read_index);
			return E_ERROR;
		}

		return E_OK;
	}
}

// stco
avf_status_t CMP4Track::GetChunkOffset(avf_u32_t chunk_index, avf_u64_t *offset)
{
	if (chunk_index >= m_stco_entry_count) {
		AVF_LOGE("no such stco entry %d", chunk_index);
		return E_INVAL;
	}

	if (!mbBufferAll) {
		return E_ERROR;
	} else {
		*offset = avf_read_be_32(m_stco_orig + chunk_index * 4);
		return E_OK;
	}
}

// stsz
avf_status_t CMP4Track::GetSampleSize(avf_u32_t sample_index, avf_u32_t *size)
{
	if (sample_index >= m_stsz_entry_count) {
		AVF_LOGE("no stsz entry %d", sample_index);
		return E_INVAL;
	}

	if (!mbBufferAll) {
		return E_ERROR;
	} else {
		*size = avf_read_be_32(m_stsz_orig + sample_index * 4);
		return E_OK;
	}
}


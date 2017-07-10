
#define LOG_TAG "mp4_builder"

#include <limits.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "mpeg_utils.h"

#include "avio.h"
#include "sys_io.h"

#include "avf_media_format.h"
#include "mp4_writer.h"
#include "mp4_builder.h"

//-----------------------------------------------------------------------
//
//  CMP4Builder
//
//-----------------------------------------------------------------------

CMP4Builder* CMP4Builder::Create(bool bForceBigFile)
{
	CMP4Builder *result = avf_new CMP4Builder(bForceBigFile);
	CHECK_STATUS(result);
	return result;
}

CMP4Builder::CMP4Builder(bool bForceBigFile):
	mbForceBigFile(bForceBigFile),
	mState(STATE_IDLE),
	mpIO(NULL),
	m_filesize(0),
	m_filepos(0),
	m_buf(NULL),
	m_buf_size(0),
	m_aux_buf(NULL),
	m_aux_buf_size(0),
	mp_v_desc(NULL),
	mp_a_desc(NULL),
	mp_udata(NULL)
{
	m_v_tracks._Init();
	m_a_tracks._Init();
	m_v_stts._Init();
	m_a_stts._Init();
	m_filelist._Init();
	CREATE_OBJ(mpIO = CSysIO::Create());
}

CMP4Builder::~CMP4Builder()
{
	Close();
	avf_safe_release(mpIO);
}

#define START_BOX() \
	avf_size_t box_pos = 0; \
	while (box_pos < box_remain) { \
		uint32_t box_size = rbuf.read_be_32(); \
		uint32_t box_type = rbuf.read_be_32(); \
		uint32_t next_box_pos; \
		if (box_size < 8) { \
			if (box_size == 0) \
				break; \
			AVF_LOGE("bad box size: %d", box_size); \
			return E_ERROR; \
		} \
		box_pos += box_size; \
		next_box_pos = rbuf.GetReadPos() + (box_size - 8)

#define END_BOX() \
		if ((status = rbuf.SeekTo(next_box_pos)) != E_OK) \
			return status; \
	}

// requirements:
//	1. file size < 4GB
//	2. one video track, one audio track
//	3. duration in MovieHeader is accurate
//	4. samples per chunk is 1
//	5. FileType: avc1, M4V, M4A, isom
//	6. stts must be first
avf_status_t CMP4Builder::ParseFile(CReadBuffer& rbuf, avf_size_t box_remain)
{
	avf_status_t status;

	START_BOX();

	switch (box_type) {
	case MKFCC('f','t','y','p'):
		break;

	case MKFCC('m','d','a','t'):
		break;

	case MKFCC('m','o','o','v'):
		if ((status = ParseMovie(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	default:
		break;
	}

	END_BOX();

	return E_OK;
}

avf_status_t CMP4Builder::ParseMovie(CReadBuffer& rbuf, avf_size_t box_remain)
{
	avf_status_t status;

	mp_track = NULL;
	mp_v_track = NULL;
	mp_a_track = NULL;

	START_BOX();

	switch (box_type) {
	case MKFCC('m','v','h','d'):
		if ((status = ParseMovieHeader(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('t','r','a','k'):
		if ((status = ParseTrack(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('u','d','t','a'):
		mp_track = NULL;
		if ((status = ParseUserData(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	default:
		break;
	}

	END_BOX();

	return E_OK;
}

avf_status_t CMP4Builder::ParseMovieHeader(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (m_file_index > 0)
		return E_OK;

	rbuf.Skip(4);

	m_info.creation_time = rbuf.read_be_32();
	m_info.modification_time = rbuf.read_be_32();
	m_info.timescale = rbuf.read_be_32();

	if (m_info.timescale == 0) {
		AVF_LOGE("timescale is 0");
		return E_ERROR;
	}

	return rbuf.IOStatus();
}

avf_status_t CMP4Builder::ParseTrack(CReadBuffer& rbuf, avf_size_t box_remain)
{
	avf_status_t status;

	START_BOX();

	switch (box_type) {
	case MKFCC('t','k','h','d'):
		if ((status = ParseTrackHeader(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('m','d','i','a'):
		if ((status = ParseMedia(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	default:
		break;
	}

	END_BOX();

	return E_OK;
}

avf_status_t CMP4Builder::ParseUserData(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (m_file_index > 0)
		return E_OK;

	avf_safe_release(mp_udata);
	mp_udata = avf_alloc_raw_data(box_remain, 0);
	if (mp_udata == NULL) {
		return E_NOMEM;
	}

	rbuf.read_mem(mp_udata->GetData(), box_remain);

	return rbuf.IOStatus();
}

avf_status_t CMP4Builder::ParseTrackHeader(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (m_file_index > 0)
		return E_OK;

	rbuf.Skip(76);
	m_width = rbuf.read_be_16();
	rbuf.Skip(2);
	m_height = rbuf.read_be_16();

	return rbuf.IOStatus();
}

avf_status_t CMP4Builder::ParseMedia(CReadBuffer& rbuf, avf_size_t box_remain)
{
	avf_status_t status;

	START_BOX();

	switch (box_type) {
	case MKFCC('m','d','h','d'):
		if ((status = ParseMediaHeader(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('m','i','n','f'):
		if ((status = ParseMediaInformation(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	default:
		break;
	}

	END_BOX();

	return E_OK;
}

avf_status_t CMP4Builder::ParseMediaHeader(CReadBuffer& rbuf, avf_size_t box_remain)
{
	rbuf.Skip(12);
	m_timescale = rbuf.read_be_32();
	return rbuf.IOStatus();
}

avf_status_t CMP4Builder::ParseMediaInformation(CReadBuffer& rbuf, avf_size_t box_remain)
{
	avf_status_t status;

	START_BOX();

	switch (box_type) {
	case MKFCC('v','m','h','d'):
		if ((status = ParseVideoMediaHeader(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('s','m','h','d'):
		if ((status = ParseSoundMediaHeader(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('s','t','b','l'):
		if (mp_track == NULL) {
			AVF_LOGE("no track");
			return E_ERROR;
		}
		if ((status = ParseSampleTable(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	default:
		break;
	}

	END_BOX();

	return E_OK;
}

avf_status_t CMP4Builder::ParseVideoMediaHeader(CReadBuffer& rbuf, avf_size_t box_remain)
{
	m_media_type = VIDEO;
	mp_track = mp_v_track = m_v_tracks._Append(NULL);
	::memset(mp_track, 0, sizeof(*mp_track));

	mp_track->width = m_width;
	mp_track->height = m_height;
	mp_track->timescale = m_timescale;
	mp_track->sample_start = m_info.total_v_samples;

	return E_OK;
}

avf_status_t CMP4Builder::ParseSoundMediaHeader(CReadBuffer& rbuf, avf_size_t box_remain)
{
	m_media_type = AUDIO;
	mp_track = mp_a_track = m_a_tracks._Append(NULL);
	::memset(mp_track, 0, sizeof(*mp_track));

	mp_track->timescale = m_timescale;
	mp_track->sample_start = m_info.total_a_samples;

	return E_OK;
}

avf_status_t CMP4Builder::ParseSampleTable(CReadBuffer& rbuf, avf_size_t box_remain)
{
	avf_status_t status;

	START_BOX();

	switch (box_type) {
	case MKFCC('s','t','s','d'):
		if ((status = ParseSampleDescription(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('s','t','t','s'):
		if ((status = ParseTimeToSample(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('s','t','s','s'):
		if ((status = ParseSyncSample(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('s','t','s','c'):
		if ((status = ParseSampleToChunk(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('s','t','s','z'):
		if ((status = ParseSampleSize(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	case MKFCC('s','t','c','o'):
		if ((status = ParseChunkOffset(rbuf, box_size - 8)) != E_OK)
			return status;
		break;

	default:
		break;
	}

	END_BOX();

	return E_OK;
}

avf_status_t CMP4Builder::ParseSampleDescription(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (m_file_index > 0)
		return E_OK;

	raw_data_t **p_desc = get_media_desc(m_media_type);
	avf_safe_release(*p_desc);

	*p_desc = avf_alloc_raw_data(box_remain, 0);
	if (*p_desc == NULL) {
		return E_NOMEM;
	}

	rbuf.read_mem((*p_desc)->GetData(), box_remain);

	return rbuf.IOStatus();
}

void CMP4Builder::add_stts(uint32_t n, uint32_t delta)
{
	TDynamicArray<stts_entry_s>& stts = get_stts(m_media_type);

	if (stts.count > 0) {
		stts_entry_s *entry = stts.Item(stts.count - 1);
		if (entry->sample_delta == delta) {
			entry->sample_count += n;
			return;
		}
	}

	stts_entry_s *entry = stts._Append(NULL);
	entry->sample_count = n;
	entry->sample_delta = delta;
}

avf_status_t CMP4Builder::ParseTimeToSample(CReadBuffer& rbuf, avf_size_t box_remain)
{
	rbuf.Skip(4);	// version, flags

	uint32_t entry_count = rbuf.read_be_32();

	uint32_t start_index = -1;
	uint32_t end_index = -1;
	uint32_t duration_pts = 0;

	uint32_t b_index = 0;
	int32_t b_start_pts = 0;
	int32_t timescale = mp_track->timescale;

	uint32_t track_start_pts = 0;
	uint32_t track_end_pts = 0;

	for (avf_size_t i = 0; i < entry_count; i++) {
		uint32_t sample_count = rbuf.read_be_32();
		uint32_t sample_delta = rbuf.read_be_32();	// **

		if (rbuf.IOError()) {
			return rbuf.IOStatus();
		}
		if (sample_delta == 0) {
			continue;
		}

		uint32_t b_length_pts = sample_count * sample_delta;
		uint32_t b_end_pts = b_start_pts + b_length_pts;
		uint32_t b_end_ms = (uint64_t)b_end_pts * 1000 / timescale;

		if ((start_index + 1) == 0) {
			if (m_range.start_time_ms < (int)b_end_ms) {
				int n = ((uint64_t)m_range.start_time_ms * timescale - (uint64_t)b_start_pts * 1000) / (sample_delta * 1000);
				track_start_pts = b_start_pts + n * sample_delta;
				start_index = b_index + n;
			}
		}

		int end_time_ms = (int)m_range.end_time_ms - 1;
		if (end_time_ms < (int)b_end_ms) {
			int n = ((uint64_t)end_time_ms * timescale - (uint64_t)b_start_pts * 1000) / (sample_delta * 1000) + 1;
			track_end_pts = b_start_pts + n * sample_delta;
			end_index = b_index + n;
			add_stts(end_index - start_index, sample_delta);
			break;
		}

		b_index += sample_count;
		b_start_pts = b_end_pts;

		if ((start_index + 1) != 0) {
			add_stts(b_index - start_index, sample_delta);
		}
	}

	if ((start_index + 1) == 0) {
		start_index = 0;
		end_index = 0;
		duration_pts = 0;
	} else {
		if ((end_index + 1) == 0) {
			end_index = b_index;
			track_end_pts = b_start_pts;
		}
		duration_pts = track_end_pts - track_start_pts;
	}

	if (m_media_type == VIDEO) {
		m_info.total_v_samples += end_index - start_index;
	} else {
		m_info.total_a_samples += end_index - start_index;
	}

	mp_track->sample_start_index = start_index;
	mp_track->sample_end_index = end_index;
	mp_track->sample_count = end_index - start_index;

	if (m_media_type == VIDEO && m_range.b_set_duration) {
		mp_track->duration_pts = (uint64_t)(m_range.end_time_ms - m_range.start_time_ms) * timescale / 1000;
	} else {
		mp_track->duration_pts = duration_pts;
	}

	return E_OK;
}

avf_status_t CMP4Builder::ParseSyncSample(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (mp_track->sample_count == 0) {
		AVF_LOGE("no samples");
		return E_OK;
	}

	rbuf.Skip(4);

	uint32_t entry_count = rbuf.read_be_32();
	uint32_t start_index = mp_track->sample_start_index + 1;

	for (avf_size_t i = 0; i < entry_count; i++) {
		uint32_t sample_number = rbuf.read_be_32();
		if (sample_number == 0) {
			return E_ERROR;
		}

		if (sample_number == start_index) {
			mp_track->stss_fpos = rbuf.GetReadPos() - 4;

			uint32_t end_index = mp_track->sample_end_index + 1;
			for (i++; i < entry_count; i++) {
				sample_number = rbuf.read_be_32();
				if (sample_number == 0) {
					return E_ERROR;
				}
				if (sample_number >= end_index) {
					mp_track->stss_count = ((rbuf.GetReadPos() - 4) - mp_track->stss_fpos) / 4;
					return E_OK;
				}
			}

			mp_track->stss_count = (rbuf.GetReadPos() - mp_track->stss_fpos) / 4;
			return E_OK;
		}

		if (sample_number > start_index) {
			AVF_LOGE("not at sync sample, start: %d, number: %d",
				start_index, sample_number);
			return E_ERROR;
		}
	}

	AVF_LOGE("no sync samples");
	return E_ERROR;
}

avf_status_t CMP4Builder::ParseSampleToChunk(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (mp_track->sample_count == 0)
		return E_OK;

	rbuf.Skip(4);

	uint32_t entry_count = rbuf.read_be_32();
	if (entry_count != 1) {
		return E_ERROR;
	}

	return E_OK;
}

avf_status_t CMP4Builder::ParseSampleSize(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (mp_track->sample_count == 0)
		return E_OK;

	rbuf.Skip(8);

	uint32_t sample_count = rbuf.read_be_32();
	if (sample_count < mp_track->sample_end_index) {
		return E_ERROR;
	}

	mp_track->stsz_fpos = rbuf.GetReadPos() + mp_track->sample_start_index * 4;

	rbuf.Skip(4 * (mp_track->sample_end_index - 1));
	mp_track->last_sample_size = rbuf.read_be_32();

	if (rbuf.IOError()) {
		return rbuf.IOStatus();
	}

	return E_OK;
}

avf_status_t CMP4Builder::ParseChunkOffset(CReadBuffer& rbuf, avf_size_t box_remain)
{
	if (mp_track->sample_count == 0)
		return E_OK;

	rbuf.Skip(4);

	uint32_t entry_count = rbuf.read_be_32();
	if (entry_count < mp_track->sample_end_index) {
		return E_ERROR;
	}

	mp_track->stco_fpos = rbuf.GetReadPos() + mp_track->sample_start_index * 4;

	rbuf.Skip(4 * mp_track->sample_start_index);
	mp_track->first_sample_fpos = rbuf.read_be_32();

	if (mp_track->sample_count == 1) {
		mp_track->last_sample_fpos = mp_track->first_sample_fpos;
	} else {
		rbuf.Skip(4 * (mp_track->sample_count - 2));
		mp_track->last_sample_fpos = rbuf.read_be_32();
	}

	if (rbuf.IOError()) {
		return rbuf.IOStatus();
	}

	return E_OK;
}

avf_status_t CMP4Builder::AddFile(const char *pFileName, const range_s *range)
{
	m_range = *range;
	if (!m_range.b_set_duration) {
		m_range.end_time_ms = INT_MAX;
	}
	if (m_range.end_time_ms <= m_range.start_time_ms) {
		AVF_LOGE("AddFile: bad range");
		return E_INVAL;
	}

	switch (mState) {
	default:
	case STATE_IDLE:
		InitParser();
		break;

	case STATE_PARSING:
		break;

	case STATE_READY:
	case STATE_ERROR:
		AVF_LOGE("cannot add file in state %d", mState);
		break;
	}

	if (!avf_file_exists(pFileName)) {
		AVF_LOGE("file %s not exists", pFileName);
		return E_INVAL;
	}

	file_info_s *fileinfo = m_filelist._Append(NULL);
	fileinfo->filename = avf_strdup(pFileName);
	fileinfo->range = m_range;

	mState = STATE_PARSING;
	return E_OK;
}

void CMP4Builder::SetDate(uint32_t creation_time, uint32_t modification_time)
{
	m_info.creation_time = creation_time + 2082844800;
	m_info.modification_time = modification_time + 2082844800;
	AVF_LOGD("Date: %x, %x", m_info.creation_time, m_info.modification_time);
}

// from idle to parsing
void CMP4Builder::InitParser()
{
	m_file_index = 0;

	::memset(&m_info, 0, sizeof(m_info));
	m_width = 0;
	m_height = 0;
	m_timescale = 0;
	m_info.v.media_type = VIDEO;
	m_info.a.media_type = AUDIO;
}

avf_status_t CMP4Builder::FinishAddFile()
{
	if (mState != STATE_PARSING) {
		AVF_LOGE("FinishAddFile: bad state %d", mState);
		return E_STATE;
	}

	if (m_filelist.count == 0) {
		AVF_LOGE("no valid files");
		return E_ERROR;
	}

	CReadBuffer rbuf(NULL, 2*KB);

	for (avf_size_t i = 0; i < m_filelist.count; i++) {
		file_info_s *fileinfo = m_filelist.Item(i);

		m_file_index = i;
		m_range = fileinfo->range;

		AVF_LOGD("parse file %s, %d-%d", fileinfo->filename,
			fileinfo->range.start_time_ms, fileinfo->range.end_time_ms);

		avf_status_t status;

		if ((status = mpIO->OpenRead(fileinfo->filename)) != E_OK) {
			AVF_LOGE("cannot open %s", fileinfo->filename);
			return E_ERROR;
		}

		rbuf.SetIO(mpIO);
		status = ParseFile(rbuf, rbuf.GetFileSize());

		mpIO->Close();

		if (status != E_OK) {
			mState = STATE_ERROR;
			return status;
		}

		if (mp_v_track && mp_a_track) {
			uint32_t start_pos = MIN(mp_v_track->first_sample_fpos, mp_a_track->last_sample_fpos);
			uint32_t last_pos_v = mp_v_track->last_sample_fpos + mp_v_track->last_sample_size;
			uint32_t last_pos_a = mp_a_track->last_sample_fpos + mp_a_track->last_sample_size;
			uint32_t end_pos = MAX(last_pos_v, last_pos_a);

			fileinfo->mdata_size = end_pos - start_pos;
			fileinfo->mdata_orig_start = start_pos;
			fileinfo->mdata_new_start = m_info.total_mdata_size;

			m_info.total_mdata_size += fileinfo->mdata_size;
		} else {
			AVF_LOGW("v: %p, a: %p", mp_v_track, mp_a_track);
		}
	}

	if (m_v_tracks.count != m_filelist.count || m_a_tracks.count != m_filelist.count) {
		AVF_LOGE("tracks: %d,%d, files: %d", m_v_tracks.count, m_a_tracks.count, m_filelist.count);
		return E_ERROR;
	}

	mp4_track_info_s *v_track = m_v_tracks.Item(0);
	m_info.v_timescale = v_track->timescale;
	m_info.video_width = v_track->width;
	m_info.video_height = v_track->height;

	mp4_track_info_s *a_track = m_a_tracks.Item(0);
	m_info.a_timescale = a_track->timescale;

	m_info.v_duration_pts = 0;
	for (avf_size_t i = 0; i < m_v_tracks.count; i++) {
		m_info.v_duration_pts += m_v_tracks.Item(i)->duration_pts;
	}

	m_info.a_duration_pts = 0;
	for (avf_size_t i = 0; i < m_a_tracks.count; i++) {
		m_info.a_duration_pts += m_a_tracks.Item(i)->duration_pts;
	}

	m_big_file = 0;

	for (int i = 0; i < 2; i++) {
		WriteFileType(NULL);
		WriteMovie(NULL);
		WriteMediaData(NULL);

		m_info.mdat_fpos = m_info.ftyp_size + m_info.moov_total_size + m_info.mdat_size;
		m_filesize = m_info.ftyp_size + m_info.moov_total_size + m_info.mdat_total_size;

		if (mbForceBigFile) {
			m_big_file = 1;
		} else {
			m_big_file = m_filesize >> 32;
			if (!m_big_file)
				break;
		}
	}

	m_file_state = S_ftyp;
	m_state_pos = 0;
	m_buf_remain = 0;

	mpIO->Close();
	m_io_index = -1;

	mState = STATE_READY;
	return E_OK;
}

avf_status_t CMP4Builder::Close()
{
	mpIO->Close();

	avf_safe_free(m_buf);
	m_buf_size = 0;

	avf_safe_free(m_aux_buf);
	m_aux_buf_size = 0;

	avf_safe_release(mp_v_desc);
	avf_safe_release(mp_a_desc);
	avf_safe_release(mp_udata);

	m_v_tracks._Release();
	m_a_tracks._Release();
	m_v_stts._Release();
	m_a_stts._Release();

	for (avf_size_t i = 0; i < m_filelist.count; i++) {
		file_info_s *fileinfo = m_filelist.Item(i);
		avf_safe_free(fileinfo->filename);
	}
	m_filelist._Release();

	mState = STATE_IDLE;
	return E_OK;
}

#define SAVE_BOX(_p, _size, _fcc) \
	do { \
		SAVE_BE_32(_p, _size); \
		SAVE_FCC(_p, _fcc); \
	} while (0)

void CMP4Builder::WriteFileType(uint8_t *p)
{
	if (!p) {
		m_info.ftyp_size = 28;
	} else {
		SAVE_BOX(p, m_info.ftyp_size, "ftyp");
		SAVE_FCC(p, "avc1");
		SAVE_BE_32(p, 0);
		SAVE_FCC(p, "M4V ");
		SAVE_FCC(p, "M4A ");
		SAVE_FCC(p, "isom");
	}
}

void CMP4Builder::WriteMovie(uint8_t *p)
{
	if (!p) {
		WriteMovieHeader(NULL);

		mp_write_track = &m_info.v;
		WriteTrack(NULL);

		mp_write_track = &m_info.a;
		WriteTrack(NULL);

		WriteUserData(NULL);

		m_info.moov_size = 8;
		m_info.moov_total_size = 
			m_info.moov_size + m_info.mvhd_size +
			m_info.v.trak_total_size + 
			m_info.a.trak_total_size +
			m_info.udta_size;
	} else {
		SAVE_BOX(p, m_info.moov_total_size, "moov");
	}
}

void CMP4Builder::WriteMediaData(uint8_t *p)
{
	if (!p) {
		m_info.mdat_size = 16;
		m_info.mdat_total_size = m_info.mdat_size;

		for (avf_size_t i = 0; i < m_filelist.count; i++) {
			file_info_s *info = m_filelist.Item(i);
			m_info.mdat_total_size += info->mdata_size;
		}

	} else {
		if (m_big_file) {
			SAVE_BE_32(p, 1);
			SAVE_FCC(p, "mdat");
			uint32_t size_hi = m_info.mdat_total_size >> 32;
			uint32_t size_lo = m_info.mdat_total_size;
			SAVE_BE_32(p, size_hi);
			SAVE_BE_32(p, size_lo);
		} else {
			SAVE_BOX(p, 8, "wide");
			uint32_t size = m_info.mdat_total_size - 8;
			SAVE_BOX(p, size, "mdat");
		}
	}
}

void CMP4Builder::WriteMovieHeader(uint8_t *p)
{
	if (!p) {
		m_info.mvhd_size = 108;
	} else {
		SAVE_BOX(p, m_info.mvhd_size, "mvhd");
		SAVE_BE_32(p, 0);	// version, flags

		SAVE_BE_32(p, m_info.creation_time);
		SAVE_BE_32(p, m_info.modification_time);

		SAVE_BE_32(p, m_info.timescale);
		SAVE_BE_32(p, m_info.v_duration_pts * m_info.timescale / m_info.v_timescale);

		SAVE_BE_32(p, 0x10000);	// rate
		SAVE_BE_16(p, 0x100);	// volumn

		::memset(p, 0, 10);
		p += 10;

		for (avf_size_t i = 0; i < 9; i++) {
			SAVE_BE_32(p, g_mp4_matrix[i]);
		}

		::memset(p, 0, 24);
		p += 24;

		SAVE_BE_32(p, 3);
	}
}

void CMP4Builder::WriteTrack(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		WriteTrackHeader(NULL);
		WriteEditList(NULL);
		WriteMedia(NULL);

		ti->trak_size = 8;
		ti->trak_total_size = ti->trak_size +
			ti->tkhd_size + ti->edts_size + ti->mdia_total_size;
	} else {
		SAVE_BOX(p, ti->trak_total_size, "trak");
	}
}

void CMP4Builder::WriteTrackHeader(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		ti->tkhd_size = 92;
	} else {
		SAVE_BOX(p, ti->tkhd_size, "tkhd");
		SAVE_BE_32(p, 0x0F);	// version, flags
		SAVE_BE_32(p, m_info.creation_time);
		SAVE_BE_32(p, m_info.modification_time);
		if (ti->media_type == VIDEO) {
			SAVE_BE_32(p, 1);
			SAVE_BE_32(p, 0);
			SAVE_BE_32(p, m_info.v_duration_pts * m_info.timescale / m_info.v_timescale);
		} else {
			SAVE_BE_32(p, 2);
			SAVE_BE_32(p, 0);
			SAVE_BE_32(p, m_info.a_duration_pts * m_info.timescale / m_info.a_timescale);
		}
		SAVE_BE_32(p, 0);
		SAVE_BE_32(p, 0);
		SAVE_BE_32(p, 0);	// layer, alternate_group
		SAVE_BE_16(p, 0x100);
		SAVE_BE_16(p, 0);
		for (avf_size_t i = 0; i < 9; i++) {
			SAVE_BE_32(p, g_mp4_matrix[i]);
		}
		if (ti->media_type == VIDEO) {
			SAVE_BE_16(p, m_info.video_width);
			SAVE_BE_16(p, 0);
			SAVE_BE_16(p, m_info.video_height);
			SAVE_BE_16(p, 0);
		} else {
			SAVE_BE_32(p, 0);
			SAVE_BE_32(p, 0);
		}
	}
}

void CMP4Builder::WriteEditList(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		ti->edts_size = 36;
	} else {
		SAVE_BOX(p, ti->edts_size, "edts");
		SAVE_BOX(p, 28, "elst");
		SAVE_BE_32(p, 0);	// version, flags
		SAVE_BE_32(p, 1);
		if (ti->media_type == VIDEO) {
			SAVE_BE_32(p, m_info.v_duration_pts * m_info.timescale / m_info.v_timescale);
		} else {
			SAVE_BE_32(p, m_info.a_duration_pts * m_info.timescale / m_info.a_timescale);
		}
		SAVE_BE_32(p, 0);
		SAVE_BE_16(p, 1);
		SAVE_BE_16(p, 0);
	}
}

void CMP4Builder::WriteMedia(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		WriteMediaHeader(NULL);
		WriteHandlerReference(NULL);
		WriteMediaInformation(NULL);
		ti->mdia_size = 8;
		ti->mdia_total_size = ti->mdia_size + 
			ti->mdhd_size + ti->hdlr_size + ti->minf_total_size;
	} else {
		SAVE_BOX(p, ti->mdia_total_size, "mdia");
	}
}

void CMP4Builder::WriteMediaHeader(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		ti->mdhd_size = 32;
	} else {
		SAVE_BOX(p, ti->mdhd_size, "mdhd");

		SAVE_BE_32(p, 0);	// version, flags
		SAVE_BE_32(p, m_info.creation_time);
		SAVE_BE_32(p, m_info.modification_time);
		if (ti->media_type == VIDEO) {
			SAVE_BE_32(p, m_info.v_timescale);
		} else {
			SAVE_BE_32(p, m_info.a_timescale);
		}
		if (ti->media_type == VIDEO) {
			SAVE_BE_32(p, m_info.v_duration_pts);
		} else {
			SAVE_BE_32(p, m_info.a_duration_pts);
		}
		SAVE_BE_16(p, 0x55C4);
		SAVE_BE_16(p, 0);
	}
}

void CMP4Builder::WriteHandlerReference(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		if (ti->media_type == VIDEO) {
			ti->hdlr_size = 32 + 1 + g_video_handler_len;
		} else {
			ti->hdlr_size = 32 + 1 + g_audio_handler_len;
		}
	} else {
		SAVE_BOX(p, ti->hdlr_size, "hdlr");
		SAVE_BE_32(p, 0);	// version, flags
		SAVE_BE_32(p, 0);	// pre_defined
		if (ti->media_type == VIDEO) {
			SAVE_FCC(p, "vide");
		} else {
			SAVE_FCC(p, "soun");
		}
		::memset(p, 0, 12);
		p += 12;
		if (ti->media_type == VIDEO) {
			*p++ = g_video_handler_len;
			::memcpy(p, g_video_handler, g_video_handler_len);
		} else {
			*p++ = g_audio_handler_len;
			::memcpy(p, g_audio_handler, g_audio_handler_len);
		}
	}
}

void CMP4Builder::WriteMediaInformation(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		WriteAVMediaHeader(NULL);
		WriteDataInformation(NULL);
		WriteSampleTable(NULL);
		ti->minf_size = 8;
		ti->minf_total_size = ti->minf_size + 
			ti->xmhd_size + ti->dinf_size + ti->stbl_total_size;
	} else {
		SAVE_BOX(p, ti->minf_total_size, "minf");
	}
}

void CMP4Builder::WriteAVMediaHeader(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (ti->media_type == VIDEO) {
		if (!p) {
			ti->xmhd_size = 20;
		} else {
			SAVE_BOX(p, ti->xmhd_size, "vmhd");
			SAVE_BE_32(p, 1);	// version, flags
			SAVE_BE_32(p, 0);
			SAVE_BE_32(p, 0);
		}
	} else {
		if (!p) {
			ti->xmhd_size = 16;
		} else {
			SAVE_BOX(p, ti->xmhd_size, "smhd");
			SAVE_BE_32(p, 0);	// version, flags
			SAVE_BE_32(p, 0);	// balance, reserved
		}
	}
}

void CMP4Builder::WriteDataInformation(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		ti->dinf_size = 36;
	} else {
		SAVE_BOX(p, ti->dinf_size, "dinf");
		SAVE_BOX(p, 28, "dref");
		SAVE_BE_32(p, 0);	// version, flags
		SAVE_BE_32(p, 1);	// entry_count
		SAVE_BOX(p, 12, "url ");
		SAVE_BE_32(p, 1);	// version, flags
	}
}

void CMP4Builder::WriteSampleTable(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		WriteSampleDescription(NULL);
		WriteTimeToSample(NULL);
		WriteSyncSample(NULL);
		WriteSampleToChunk(NULL);
		WriteSampleSize(NULL);
		WriteChunkOffset(NULL);
		ti->stbl_size = 8;
		ti->stbl_total_size = ti->stbl_size +
			ti->stsd_size + ti->stts_total_size +
			ti->stss_total_size + ti->stsc_size +
			ti->stsz_total_size + ti->stco_total_size;
	} else {
		SAVE_BOX(p, ti->stbl_total_size, "stbl");
	}
}

void CMP4Builder::WriteSampleDescription(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	raw_data_t *desc = *get_media_desc(ti->media_type);

	if (!p) {
		ti->stsd_size = 8 + (desc ? desc->GetSize() : 0);
	} else {
		SAVE_BOX(p, ti->stsd_size, "stsd");
		if (desc) {
			::memcpy(p, desc->GetData(), desc->GetSize());
		}
	}
}

void CMP4Builder::WriteTimeToSample(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	TDynamicArray<stts_entry_s>& stts = get_stts(ti->media_type);

	if (!p) {
		ti->stts_size = 16;
		ti->stts_total_size = ti->stts_size + stts.count * 8;
	} else {
		SAVE_BOX(p, ti->stts_total_size, "stts");
		SAVE_BE_32(p, 0);
		SAVE_BE_32(p, stts.count);
	}
}

void CMP4Builder::write_stts(uint8_t *p, avf_size_t nitems)
{
	TDynamicArray<stts_entry_s>& stts = get_stts(mp_write_track->media_type);
	stts_entry_s *entry = stts.Item(m_array_index);
	for (avf_size_t i = nitems; i > 0; i--, entry++) {
		SAVE_BE_32(p, entry->sample_count);
		SAVE_BE_32(p, entry->sample_delta);
	}
	m_array_index += nitems;
	m_array_remain -= nitems;
}

void CMP4Builder::WriteSyncSample(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (ti->media_type == VIDEO) {
		if (!p) {
			TDynamicArray<mp4_track_info_s>& tracks = m_v_tracks;

			ti->stss_size = 16;
			ti->stss_total_size = ti->stss_size;

			for (avf_size_t i = 0; i < tracks.count; i++) {
				mp4_track_info_s *info = tracks.Item(i);
				ti->stss_total_size += info->stss_count * 4;
			}

		} else {
			SAVE_BOX(p, ti->stss_total_size, "stss");
			SAVE_BE_32(p, 0);
			SAVE_BE_32(p, (ti->stss_total_size - ti->stss_size) / 4);
		}
	} else {
		if (!p) {
			ti->stss_size = 0;
		}
	}
}

bool CMP4Builder::DoOpenSegFile()
{
	file_info_s *info = m_filelist.Item(m_file_index);
	avf_status_t status = mpIO->OpenRead(info->filename);
	if (status != E_OK) {
		AVF_LOGE("cannot open %s", info->filename);
		m_file_state = STATE_ERROR;
		return false;
	}
	m_io_index = m_file_index;
	return true;
}

void CMP4Builder::write_stss(uint8_t *p, avf_size_t nitems)
{
	if (!OpenSegFile())
		return;

	out_track_info_s *ti = mp_write_track;
	TDynamicArray<mp4_track_info_s>& tracks = get_tracks(ti->media_type);
	mp4_track_info_s *track = tracks.Item(m_file_index);

	avf_status_t status = mpIO->Seek(track->stss_fpos + m_array_index * 4);
	if (status != E_OK) {
		m_file_state = STATE_ERROR;
		return;
	}

	avf_size_t size = nitems * 4;
	int nread = mpIO->Read(p, size);
	if (nread != (int)size) {
		AVF_LOGE("read failed");
		m_file_state = STATE_ERROR;
		return;
	}

	uint8_t *ptr = p;
	for (avf_size_t i = 0; i < nitems; i++) {
		uint32_t sample_number = load_be_32(ptr);
		sample_number -= track->sample_start_index;
		sample_number += track->sample_start;
		SAVE_BE_32(ptr, sample_number);
	}

	m_array_index += nitems;
	m_array_remain -= nitems;

	if (m_array_remain == 0) {
		m_file_index++;
		if (m_file_index < m_filelist.count) {
			mp4_track_info_s *track = tracks.Item(m_file_index);
			m_array_index = 0;
			m_array_remain = track->stss_count;
		}
	}
}

void CMP4Builder::WriteSampleToChunk(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		ti->stsc_size = 28;
	} else {
		SAVE_BOX(p, ti->stsc_size, "stsc");
		SAVE_BE_32(p, 0);
		SAVE_BE_32(p, 1);
		SAVE_BE_32(p, 1);
		SAVE_BE_32(p, 1);
		SAVE_BE_32(p, 1);
	}
}

void CMP4Builder::WriteSampleSize(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		TDynamicArray<mp4_track_info_s>& tracks = get_tracks(ti->media_type);

		ti->stsz_size = 20;
		ti->stsz_total_size = ti->stsz_size;

		for (avf_size_t i = 0; i < tracks.count; i++) {
			mp4_track_info_s *info = tracks.Item(i);
			ti->stsz_total_size += info->sample_count * 4;
		}

	} else {
		SAVE_BOX(p, ti->stsz_total_size, "stsz");
		SAVE_BE_32(p, 0);
		SAVE_BE_32(p, 0);
		SAVE_BE_32(p, (ti->stsz_total_size - ti->stsz_size) / 4);
	}
}

void CMP4Builder::write_stsz(uint8_t *p, avf_size_t nitems)
{
	if (!OpenSegFile())
		return;

	out_track_info_s *ti = mp_write_track;
	TDynamicArray<mp4_track_info_s>& tracks = get_tracks(ti->media_type);
	mp4_track_info_s *track = tracks.Item(m_file_index);

	avf_status_t status = mpIO->Seek(track->stsz_fpos + m_array_index * 4);
	if (status != E_OK) {
		m_file_state = STATE_ERROR;
		return;
	}

	avf_size_t size = nitems * 4;
	int nread = mpIO->Read(p, size);
	if (nread != (int)size) {
		AVF_LOGE("read failed");
		m_file_state = STATE_ERROR;
		return;
	}

	m_array_index += nitems;
	m_array_remain -= nitems;

	if (m_array_remain == 0) {
		m_file_index++;
		if (m_file_index < m_filelist.count) {
			mp4_track_info_s *track = tracks.Item(m_file_index);
			m_array_index = 0;
			m_array_remain = track->sample_count;
		}
	}
}

void CMP4Builder::WriteChunkOffset(uint8_t *p)
{
	out_track_info_s *ti = mp_write_track;
	if (!p) {
		TDynamicArray<mp4_track_info_s>& tracks = get_tracks(ti->media_type);

		ti->stco_size = 16;
		ti->stco_total_size = ti->stco_size;

		int item_size_2 = m_big_file ? 3 : 2;

		for (avf_size_t i = 0; i < tracks.count; i++) {
			mp4_track_info_s *info = tracks.Item(i);
			ti->stco_total_size += info->sample_count << item_size_2;
		}

	} else {
		SAVE_BE_32(p, ti->stco_total_size);
		if (m_big_file) {
			SAVE_FCC(p, "co64");
			SAVE_BE_32(p, 0);
			SAVE_BE_32(p, (ti->stco_total_size - ti->stco_size) / 8);
		} else {
			SAVE_FCC(p, "stco");
			SAVE_BE_32(p, 0);
			SAVE_BE_32(p, (ti->stco_total_size - ti->stco_size) / 4);
		}
	}
}

void CMP4Builder::write_stco(uint8_t *p, avf_size_t nitems)
{
	if (!OpenSegFile())
		return;

	out_track_info_s *ti = mp_write_track;
	TDynamicArray<mp4_track_info_s>& tracks = get_tracks(ti->media_type);
	mp4_track_info_s *track = tracks.Item(m_file_index);

	avf_status_t status = mpIO->Seek(track->stco_fpos + m_array_index * 4);
	if (status != E_OK) {
		m_file_state = STATE_ERROR;
		return;
	}

	avf_size_t size = nitems * 4;
	int nread;

	if (m_big_file) {
		nread = mpIO->Read(GetAuxBuffer(size), size);
	} else {
		nread = mpIO->Read(p, size);
	}

	if (nread != (int)size) {
		AVF_LOGE("read failed");
		m_file_state = STATE_ERROR;
		return;
	}

	file_info_s *info = m_filelist.Item(m_file_index);

	if (m_big_file) {
		uint64_t delta = info->mdata_new_start - info->mdata_orig_start + m_info.mdat_fpos;
		uint8_t *from = m_aux_buf;
		uint8_t *to = p;
		for (avf_size_t i = nitems; i; i--) {
			uint32_t chunk_offset = load_be_32(from);
			uint64_t new_offset = chunk_offset + delta;
			uint32_t offset_lo = new_offset;
			uint32_t offset_hi = new_offset >> 32;
			SAVE_BE_32(to, offset_hi);
			SAVE_BE_32(to, offset_lo);
			from += 4;
		}
	} else {
		uint32_t delta = info->mdata_new_start - info->mdata_orig_start + m_info.mdat_fpos;
		uint8_t *ptr = p;
		for (avf_size_t i = nitems; i; i--) {
			uint32_t chunk_offset = load_be_32(ptr);
			chunk_offset += delta;
			SAVE_BE_32(ptr, chunk_offset);
		}
	}

	m_array_index += nitems;
	m_array_remain -= nitems;

	if (m_array_remain == 0) {
		m_file_index++;
		if (m_file_index < m_filelist.count) {
			mp4_track_info_s *track = tracks.Item(m_file_index);
			m_array_index = 0;
			m_array_remain = track->sample_count;
		}
	}
}

void CMP4Builder::WriteUserData(uint8_t *p)
{
	if (!p) {
		m_info.udta_size = 8 + (mp_udata ? mp_udata->GetSize() : 0);
	} else {
		SAVE_BOX(p, m_info.udta_size, "udta");
		if (mp_udata) {
			::memcpy(p, mp_udata->GetData(), mp_udata->GetSize());
		}
	}
}

uint8_t *CMP4Builder::DoGetBuffer(avf_size_t size)
{
	avf_safe_free(m_buf);
	m_buf_size = 0;

	m_buf = avf_malloc_bytes(size);
	m_buf_size = size;

	return m_buf;
}

uint8_t *CMP4Builder::DoGetAuxBuffer(avf_size_t size)
{
	avf_safe_free(m_aux_buf);
	m_aux_buf_size = 0;

	m_aux_buf = avf_malloc_bytes(size);
	m_aux_buf_size = size;

	return m_aux_buf;
}

bool CMP4Builder::ReadData(void *buffer, avf_size_t buffer_size,
	WriteBoxData_proc write_proc, avf_size_t data_size, avf_size_t *nread)
{
	if (m_state_pos == 0 && buffer_size >= data_size) {
		(this->*write_proc)((uint8_t*)buffer);
		m_buf_remain = 0;
		*nread = data_size;
		return true;
	}

	if (m_buf_remain == 0) {
		(this->*write_proc)(GetBuffer(data_size));
		m_buf_ptr = m_buf + m_state_pos;
		m_buf_remain = data_size - m_state_pos;
	}

	avf_size_t tocopy = MIN(buffer_size, m_buf_remain);
	::memcpy(buffer, m_buf_ptr, tocopy);

	m_state_pos += tocopy;
	m_buf_ptr += tocopy;
	m_buf_remain -= tocopy;
	*nread = tocopy;

	if (m_buf_remain == 0) {
		m_state_pos = 0;
		return true;
	}

	return false;
}

bool CMP4Builder::ReadArray(void *buffer, avf_size_t buffer_size,
	WriteArray_proc write_proc, avf_size_t *nread)
{
	avf_size_t nitems = MIN(256, m_array_remain);
	avf_size_t data_size = nitems * m_array_item_size;

	if (m_state_pos + m_buf_remain == 0 && buffer_size >= data_size) {
		(this->*write_proc)((uint8_t*)buffer, nitems);
		*nread = data_size;

		if (m_array_remain == 0) {
			m_buf_remain = 0;
			return true;
		}

		return false;
	}

	if (m_buf_remain == 0) {
		(this->*write_proc)(GetBuffer(data_size), nitems);
		m_buf_ptr = m_buf + m_state_pos;
		m_buf_remain = data_size - m_state_pos;
	}

	avf_size_t tocopy = MIN(buffer_size, m_buf_remain);
	::memcpy(buffer, m_buf_ptr, tocopy);

	m_state_pos += tocopy;
	m_buf_ptr += tocopy;
	m_buf_remain -= tocopy;
	*nread = tocopy;

	if (m_buf_remain == 0) {
		m_state_pos = 0;
		return m_array_remain == 0;
	}

	return false;
}

void CMP4Builder::ReadMediaData(void *buffer, avf_size_t buffer_size, avf_size_t *nread)
{
	if (!OpenSegFile())
		return;

	out_track_info_s *ti = mp_write_track;
	TDynamicArray<mp4_track_info_s>& tracks = get_tracks(ti->media_type);
	file_info_s *info = m_filelist.Item(m_file_index);

	avf_status_t status = mpIO->Seek(m_mdat_offset + info->mdata_orig_start);
	if (status != E_OK) {
		m_file_state = STATE_ERROR;
		return;
	}

	avf_size_t toread = MIN(buffer_size, m_mdat_remain);
	if ((*nread = mpIO->Read(buffer, toread)) != toread) {
		AVF_LOGE("read failed");
		m_file_state = STATE_ERROR;
		return;
	}

	m_mdat_offset += toread;
	m_mdat_remain -= toread;

	if (m_mdat_remain == 0) {
		m_file_index++;
		if (m_file_index < m_filelist.count) {
			mp4_track_info_s *track = tracks.Item(m_file_index);
			file_info_s *info = m_filelist.Item(m_file_index);
			m_mdat_offset = 0;
			m_mdat_remain = info->mdata_size;
			m_array_index = 0;
			m_array_remain = track->stss_count;
		}
	}
}

int CMP4Builder::Read(void *buffer, avf_size_t size)
{
	if (mState != STATE_READY) {
		AVF_LOGE("Read: bad state %d", mState);
		return E_STATE;
	}

	avf_size_t total_read = 0;

	while (size > 0) {
		avf_size_t nread = 0;

		switch (m_file_state) {
		case S_ftyp:
			if (ReadData(buffer, size, &CMP4Builder::WriteFileType, m_info.ftyp_size, &nread)) {
				m_file_state = S_moov;
			}
			break;

		case S_moov:
			if (ReadData(buffer, size, &CMP4Builder::WriteMovie, m_info.moov_size, &nread)) {
				m_file_state = S_mvhd;
			}
			break;

		case S_mvhd:
			if (ReadData(buffer, size, &CMP4Builder::WriteMovieHeader, m_info.mvhd_size, &nread)) {
				m_file_state = S_trak;
				mp_write_track = &m_info.v;
			}
			break;

		case S_trak:
			if (ReadData(buffer, size, &CMP4Builder::WriteTrack, mp_write_track->trak_size, &nread)) {
				m_file_state = S_tkhd;
			}
			break;

		case S_tkhd:
			if (ReadData(buffer, size, &CMP4Builder::WriteTrackHeader, mp_write_track->tkhd_size, &nread)) {
				m_file_state = S_edts;
			}
			break;

		case S_edts:
			if (ReadData(buffer, size, &CMP4Builder::WriteEditList, mp_write_track->edts_size, &nread)) {
				m_file_state = S_mdia;
			}
			break;

		case S_mdia:
			if (ReadData(buffer, size, &CMP4Builder::WriteMedia, mp_write_track->mdia_size, &nread)) {
				m_file_state = S_mdhd;
			}
			break;

		case S_mdhd:
			if (ReadData(buffer, size, &CMP4Builder::WriteMediaHeader, mp_write_track->mdhd_size, &nread)) {

				m_file_state = S_hdlr;
			}
			break;

		case S_hdlr:
			if (ReadData(buffer, size, &CMP4Builder::WriteHandlerReference, mp_write_track->hdlr_size, &nread)) {
				m_file_state = S_minf;
			}
			break;

		case S_minf:
			if (ReadData(buffer, size, &CMP4Builder::WriteMediaInformation, mp_write_track->minf_size, &nread)) {
				m_file_state = S_xmhd;
			}
			break;

		case S_xmhd:
			if (ReadData(buffer, size, &CMP4Builder::WriteAVMediaHeader, mp_write_track->xmhd_size, &nread)) {
				m_file_state = S_dinf;
			}
			break;

		case S_dinf:
			if (ReadData(buffer, size, &CMP4Builder::WriteDataInformation, mp_write_track->dinf_size, &nread)) {
				m_file_state = S_stbl;
			}
			break;

		case S_stbl:
			if (ReadData(buffer, size, &CMP4Builder::WriteSampleTable, mp_write_track->stbl_size, &nread)) {
				m_file_state = S_stsd;
			}
			break;

		case S_stsd:
			if (ReadData(buffer, size, &CMP4Builder::WriteSampleDescription, mp_write_track->stsd_size, &nread)) {
				m_file_state = S_stts;
			}
			break;

		case S_stts:
			if (ReadData(buffer, size, &CMP4Builder::WriteTimeToSample, mp_write_track->stts_size, &nread)) {
				TDynamicArray<stts_entry_s>& stts = get_stts(mp_write_track->media_type);

				m_file_index = 0;
				m_array_index = 0;
				m_array_remain = stts.count;
				m_array_item_size = 8;

				m_file_state = S_stts_data;
			}
			break;

		case S_stts_data:
			if (ReadArray(buffer, size, &CMP4Builder::write_stts, &nread)) {
				m_file_state = S_stss;
			}
			break;

		case S_stss:
			if (ReadData(buffer, size, &CMP4Builder::WriteSyncSample, mp_write_track->stss_size, &nread)) {
				TDynamicArray<mp4_track_info_s>& tracks = get_tracks(mp_write_track->media_type);

				m_file_index = 0;
				m_array_index = 0;
				m_array_remain = tracks.Item(0)->stss_count;
				m_array_item_size = 4;

				m_file_state = S_stss_data;
			}
			break;

		case S_stss_data:
			if (ReadArray(buffer, size, &CMP4Builder::write_stss, &nread)) {
				m_file_state = S_stsc;
			}
			break;

		case S_stsc:
			if (ReadData(buffer, size, &CMP4Builder::WriteSampleToChunk, mp_write_track->stsc_size, &nread)) {
				m_file_state = S_stsz;
			}
			break;

		case S_stsz:
			if (ReadData(buffer, size, &CMP4Builder::WriteSampleSize, mp_write_track->stsz_size, &nread)) {
				TDynamicArray<mp4_track_info_s>& tracks = get_tracks(mp_write_track->media_type);

				m_file_index = 0;
				m_array_index = 0;
				m_array_remain = tracks.Item(0)->sample_count;
				m_array_item_size = 4;

				m_file_state = S_stsz_data;
			}
			break;

		case S_stsz_data:
			if (ReadArray(buffer, size, &CMP4Builder::write_stsz, &nread)) {
				m_file_state = S_stco;
			}
			break;

		case S_stco:
			if (ReadData(buffer, size, &CMP4Builder::WriteChunkOffset, mp_write_track->stco_size, &nread)) {
				TDynamicArray<mp4_track_info_s>& tracks = get_tracks(mp_write_track->media_type);

				m_file_index = 0;
				m_array_index = 0;
				m_array_remain = tracks.Item(0)->sample_count;
				m_array_item_size = m_big_file ? 8 : 4;

				m_file_state = S_stco_data;
			}
			break;

		case S_stco_data:
			if (ReadArray(buffer, size, &CMP4Builder::write_stco, &nread)) {
				if (mp_write_track->media_type == VIDEO) {
					mp_write_track = &m_info.a;
					m_file_state = S_trak;
				} else {
					m_file_state = S_udta;
				}
			}
			break;

		case S_udta:
			if (ReadData(buffer, size, &CMP4Builder::WriteUserData, m_info.udta_size, &nread)) {
				m_file_state = S_mdat;
			}
			break;

		case S_mdat:
			if (ReadData(buffer, size, &CMP4Builder::WriteMediaData, m_info.mdat_size, &nread)) {
				file_info_s *info = m_filelist.Item(0);

				m_file_index = 0;
				m_mdat_offset = 0;
				m_mdat_remain = info->mdata_size;

				m_file_state = S_mdat_data;
			}
			break;

		case S_mdat_data:
			ReadMediaData(buffer, size, &nread);
			if (m_mdat_remain == 0) {
				m_file_state = S_eof;
			}
			break;

		case S_eof:
			return total_read;

		default:
			AVF_LOGE("bad file state %d", m_file_state);
			return E_ERROR;
		}

		buffer = (uint8_t*)buffer + nread;
		size -= nread;
		total_read += nread;

		if ((m_filepos += nread) >= m_filesize) {
			m_file_state = S_eof;
			m_buf_remain = 0;
		}
	}

	return total_read;
}

int CMP4Builder::ReadAt(void *buffer, avf_size_t size, avf_u64_t pos)
{
	return -1;
}

avf_status_t CMP4Builder::Seek(avf_s64_t pos, int whence)
{
	if (mState != STATE_READY) {
		AVF_LOGE("Seek: bad state %d", mState);
		return E_STATE;
	}

	avf_u64_t new_file_pos;

	switch (whence) {
	case kSeekSet:
		new_file_pos = pos;
		break;

	case kSeekCur:
		new_file_pos = m_filepos + pos;
		break;

	case kSeekEnd:
		new_file_pos = m_filesize + pos;
		break;

	default:
		AVF_LOGE("Bad seek %d", whence);
		return E_INVAL;
	}

	if (new_file_pos == m_filepos)
		return E_OK;

	m_filepos = new_file_pos;

	if (new_file_pos < m_info.ftyp_size) {
		return SetFileState(S_ftyp, new_file_pos);
	}
	new_file_pos -= m_info.ftyp_size;

	if (new_file_pos < m_info.moov_total_size) {
		uint32_t offset = (uint32_t)new_file_pos;

		if (offset < m_info.moov_size) {
			return SetFileState(S_moov, offset);
		}
		offset -= m_info.moov_size;

		if (offset < m_info.mvhd_size) {
			return SetFileState(S_mvhd, offset);
		}
		offset -= m_info.mvhd_size;

		out_track_info_s *track = &m_info.v;
		if (offset < track->trak_total_size) {
			avf_status_t status = SeekInTrack(track, offset);
			if (status == E_OK) {
				mp_write_track = track;
			}
			return status;
		}
		offset -= track->trak_total_size;

		track = &m_info.a;
		if (offset < track->trak_total_size) {
			avf_status_t status = SeekInTrack(track, offset);
			if (status == E_OK) {
				mp_write_track = track;
			}
			return status;
		}
		offset -= track->trak_total_size;

		if (offset < m_info.udta_size) {
			return SetFileState(S_udta, offset);
		}

		AVF_LOGE("internal error");
		return E_ERROR;
	}
	new_file_pos -= m_info.moov_total_size;

	if (new_file_pos < m_info.mdat_total_size) {
		if (new_file_pos < m_info.mdat_size) {
			return SetFileState(S_mdat, new_file_pos);
		}
		new_file_pos -= m_info.mdat_size;

		for (avf_size_t i = 0; i < m_filelist.count; i++) {
			file_info_s *info = m_filelist.Item(i);
			if (new_file_pos < info->mdata_size) {
				m_file_index = i;
				m_mdat_offset = new_file_pos;
				m_mdat_remain = info->mdata_size - new_file_pos;
				m_file_state = S_mdat_data;
				return E_OK;
			}
			new_file_pos -= info->mdata_size;
		}
	}

	if (m_filepos >= m_filesize) {
		m_filepos = m_filesize;
		m_file_state = S_eof;
		return E_OK;
	}

	AVF_LOGE("internal error");
	return E_ERROR;
}

avf_status_t CMP4Builder::SeekInTrack(out_track_info_s *track, uint32_t offset)
{
	if (offset < track->trak_size) {
		return SetFileState(S_trak, offset);
	}
	offset -= track->trak_size;

	if (offset < track->tkhd_size) {
		return SetFileState(S_tkhd, offset);
	}
	offset -= track->tkhd_size;

	if (offset < track->edts_size) {
		return SetFileState(S_edts, offset);
	}
	offset -= track->edts_size;

	if (offset < track->mdia_total_size) {
		if (offset < track->mdia_size) {
			return SetFileState(S_mdia, offset);
		}
		offset -= track->mdia_size;

		if (offset < track->mdhd_size) {
			return SetFileState(S_mdhd, offset);
		}
		offset -= track->mdhd_size;

		if (offset < track->hdlr_size) {
			return SetFileState(S_hdlr, offset);
		}
		offset -= track->hdlr_size;

		if (offset < track->minf_total_size) {
			if (offset < track->minf_size) {
				return SetFileState(S_minf, offset);
			}
			offset -= track->minf_size;

			if (offset < track->xmhd_size) {
				return SetFileState(S_xmhd, offset);
			}
			offset -= track->xmhd_size;

			if (offset < track->dinf_size) {
				return SetFileState(S_dinf, offset);
			}
			offset -= track->dinf_size;

			if (offset < track->stbl_total_size) {
				if (offset < track->stbl_size) {
					return SetFileState(S_stbl, offset);
				}
				offset -= track->stbl_size;

				if (offset < track->stsd_size) {
					return SetFileState(S_stsd, offset);
				}
				offset -= track->stsd_size;

				// time to sample
				if (offset < track->stts_total_size) {
					if (offset < track->stts_size) {
						return SetFileState(S_stts, offset);
					}
					offset -= track->stts_size;

					TDynamicArray<stts_entry_s>& stts = get_stts(track->media_type);
					if (offset < stts.count * 8) {
						m_array_index = offset / 8;
						m_array_remain = stts.count - m_array_index;
						m_array_item_size = 8;
						return SetFileState(S_stts_data, offset & 7);
					}

					AVF_LOGE("internal error");
					return E_ERROR;
				}
				offset -= track->stts_total_size;

				// sync samples
				if (offset < track->stss_total_size) {
					if (offset < track->stss_size) {
						return SetFileState(S_stss, offset);
					}
					offset -= track->stss_size;

					TDynamicArray<mp4_track_info_s>& tracks = get_tracks(track->media_type);
					mp4_track_info_s *t = tracks.Item(0);
					for (avf_size_t i = 0; i < tracks.count; i++, t++) {
						uint32_t tmp = t->stss_count * 4;
						if (offset < tmp) {
							m_file_index = i;
							m_array_index = offset / 4;
							m_array_remain = t->stss_count - m_array_index;
							m_array_item_size = 4;
							return SetFileState(S_stss_data, offset & 3);
						}
						offset -= tmp;
					}

					AVF_LOGE("internal error");
					return E_ERROR;
				}
				offset -= track->stss_total_size;

				// sample to chunk
				if (offset < track->stsc_size) {
					return SetFileState(S_stsc, offset);
				}
				offset -= track->stsc_size;

				// sample size
				if (offset < track->stsz_total_size) {
					if (offset < track->stsz_size) {
						return SetFileState(S_stsz, offset);
					}
					offset -= track->stsz_size;

					TDynamicArray<mp4_track_info_s>& tracks = get_tracks(track->media_type);
					mp4_track_info_s *t = tracks.Item(0);
					for (avf_size_t i = 0; i < tracks.count; i++, t++) {
						uint32_t tmp = t->sample_count * 4;
						if (offset < tmp) {
							m_file_index = i;
							m_array_index = offset / 4;
							m_array_remain = t->sample_count - m_array_index;
							m_array_item_size = 4;
							return SetFileState(S_stsz_data, offset & 3);
						}
						offset -= tmp;
					}

					AVF_LOGE("internal error");
					return E_ERROR;
				}
				offset -= track->stsz_total_size;

				// chunk offset
				if (offset < track->stco_total_size) {
					if (offset < track->stco_size) {
						return SetFileState(S_stco, offset);
					}
					offset -= track->stco_size;

					TDynamicArray<mp4_track_info_s>& tracks = get_tracks(track->media_type);
					mp4_track_info_s *t = tracks.Item(0);
					int item_size = m_big_file ? 8 : 4;

					for (avf_size_t i = 0; i < tracks.count; i++, t++) {
						uint32_t tmp = t->sample_count * item_size;
						if (offset < tmp) {
							m_file_index = i;
							m_array_index = offset / item_size;
							m_array_remain = t->sample_count - m_array_index;
							m_array_item_size = item_size;
							return SetFileState(S_stco_data, offset & (item_size - 1));
						}
						offset -= tmp;
					}

					AVF_LOGE("internal error");
					return E_ERROR;
				}
			}

			AVF_LOGE("internal error");
			return E_ERROR;
		}

		AVF_LOGE("internal error");
		return E_ERROR;
	}

	AVF_LOGE("internal error");
	return E_ERROR;
}

avf_u64_t CMP4Builder::GetSize()
{
	return mState == STATE_READY ? m_filesize : 0;
}

avf_u64_t CMP4Builder::GetPos()
{
	return mState == STATE_READY ? m_filepos : 0;
}


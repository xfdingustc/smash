
#define LOG_TAG "ts_writer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "mem_stream.h"
#include "avio.h"
#include "buffer_io.h"
#include "sys_io.h"

#include "avf_config.h"
#include "avf_media_format.h"
#include "mpeg_utils.h"

#include "sample_queue.h"
#include "ts_writer.h"

//-----------------------------------------------------------------------
//
//  CTSWriter
//
//-----------------------------------------------------------------------
IMediaFileWriter* CTSWriter::Create(IEngine *pEngine, CConfigNode *node,
		int index, const char *avio)
{
	CTSWriter *result = avf_new CTSWriter(pEngine, node, index, avio);
	CHECK_STATUS(result);
	return result;
}

CTSWriter::CTSWriter(IEngine *pEngine, CConfigNode *node, int index, const char *avio):
	inherited(pEngine),
	mIndex(index),
	mGenTime(AVF_INVALID_TIME),
	mpIO(NULL),
	mpObserver(NULL),
	mb_uploading(false),
	mp_upload_io(NULL),
	m_upload_counter(0)
{
	SET_ARRAY_NULL(mpFormat);

	if (node) {
		CConfigNode *pAttr = node->FindChild("vdb");
		if (pAttr && ::atoi(pAttr->GetValue())) {
			mpObserver = GetRecordObserver();
		}
	}

	int type = CBufferIO::TYPE_DIRECT_IO;
	if (avio && !::strcmp(avio, "mmap")) {
		type = CBufferIO::TYPE_FILEMAP_IO;
	}

	CREATE_OBJ(mpIO = CBufferIO::Create(type));
	mpBlockIO = IBlockIO::GetInterfaceFrom(mpIO);

	m_video_upload_num = mpEngine->ReadRegInt32(NAME_UPLOAD_VIDEO_NUM, mIndex, 5);
}

CTSWriter::~CTSWriter()
{
	avf_safe_release(mp_upload_io);
	avf_safe_release(mpIO);
	SAFE_RELEASE_ARRAY(mpFormat);
}

avf_status_t CTSWriter::EndWriter()
{
	inherited::EndWriter();
	EndUploading(true);
	return E_OK;
}

IAVIO *CTSWriter::GetIO()
{
	return mpIO;
}

avf_status_t CTSWriter::StartFile(string_t *filename, avf_time_t video_time,
	string_t *picture_name, avf_time_t picture_time, string_t *basename)
{
	mb_file_too_large = false;
	if (mIndex == 0) {
		SetLargeFileFlag(0);
	}

	pmt_written = 0;
	add_video_started = 0;

	if (mbEnableRecord) {
		// adjust file time
		mGenTime = video_time;

		// current filename
		const char *ext = avf_get_file_ext(filename->string());
		if (ext == NULL) {
			mFileName = ap<string_t>(filename, ".ts");
		} else {
			mFileName = filename;
		}

		avf_status_t status = mpIO->CreateFile(mFileName->string());
		if (status != E_OK) {
			return status;
		}
		AVF_LOGD("writing %s, writer[%d]", mFileName->string(), mIndex);

		if (mpObserver == NULL) {
			mpEngine->WriteRegString(NAME_FILE_CURRENT, mIndex, mFileName->string());
			PostFilterMsg(IEngine::MSG_FILE_START);
		}
	}

	return E_OK;
}

avf_status_t CTSWriter::EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms)
{
	if (mbEnableRecord) {
		if (fsize) {
			*fsize = (avf_u32_t)mpIO->GetSize();
		}

		mpIO->Close();
		if (mGenTime != AVF_INVALID_TIME) {
			ModifyFileTime(mFileName->string(), mGenTime);
		}
		AVF_LOGI("%s completed, writer[%d]", mFileName->string(), mIndex);

		if (mpObserver == NULL) {
			mpEngine->WriteRegString(NAME_FILE_PREVIOUS, mIndex, mFileName->string());
			PostFilterMsg(IEngine::MSG_FILE_END);
		}
	}

	return E_OK;
}

const char *CTSWriter::GetCurrentFile()
{
	return mFileName->string();
}

avf_status_t CTSWriter::SetRawData(avf_size_t stream, raw_data_t *raw)
{
	// todo
	return E_OK;
}

avf_status_t CTSWriter::ClearMediaFormats()
{
	for (avf_size_t stream = 0; stream < MAX_STREAMS; stream++) {
		avf_safe_release(mpFormat[stream]);
	}
	return E_OK;
}

avf_status_t CTSWriter::CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	if (pMediaFormat->mFormatType != MF_H264VideoFormat &&
		pMediaFormat->mFormatType != MF_AACFormat &&
		pMediaFormat->mFormatType != MF_Mp3Format &&
		pMediaFormat->mFormatType != MF_SubtitleFormat &&
		pMediaFormat->mFormatType != MF_NullFormat &&
		pMediaFormat->mFormatType != MF_MJPEGVideoFormat) {
		AVF_LOGE("cannot accept this media '%c%c%c%c'", FCC_CHARS(pMediaFormat->mFormatType));
		return E_INVAL;
	}
	return E_OK;
}

avf_status_t CTSWriter::SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	if (stream >= MAX_STREAMS) {
		AVF_LOGE("too many streams");
		return E_NOENT;
	}

	if (pMediaFormat) {
		if (mpFormat[stream] != NULL) {
			AVF_LOGE("stream %d already set", stream);
			return E_INVAL;
		}

		mpFormat[stream] = pMediaFormat->acquire();
	} else {
		avf_safe_release(mpFormat[stream]);
	}

	return E_OK;
}

avf_status_t CTSWriter::CheckStreams()
{
	//AVF_LOGD("CheckStreams[%d]", mIndex);

	m_video_index = -1;
	m_num_video = 0;

	m_audio_index = -1;
	m_num_audio = 0;

	m_subt_index = -1;
	m_num_subt = 0;

	m_pcr_pid = 0;
	for (avf_size_t i = 0; i < MAX_STREAMS; i++) {
		CMediaFormat *format = mpFormat[i];
		if (format) {
			mPIDs[i] = START_PID + i;
			mCounter[i] = 0;
			//mStartPTS[i] = 0;
			//mStartDTS[i] = 0;
			mbStarted[i] = false;

			switch (format->mFormatType) {
			case MF_H264VideoFormat:
			case MF_NullFormat:
				mStreamId[i] = 0xE0 + m_num_video++;
				if (m_video_index < 0) {
					m_video_index = i;
				}
				break;

			case MF_AACFormat:
			case MF_Mp3Format:
				mStreamId[i] = 0xC0 + m_num_audio++;
				if (m_audio_index < 0) {
					m_audio_index = i;
				}
				break;

			case MF_SubtitleFormat:
				mStreamId[i] = 0xFA + m_num_subt++;
				if (m_subt_index < 0) {
					m_subt_index = i;
				}
				break;

			default:
				break;
			}
		}
	}

	if (m_video_index >= 0) {
		m_pcr_pid = mPIDs[m_video_index];
	} else if (m_audio_index >= 0) {
		AVF_LOGW("no video");
		m_pcr_pid = mPIDs[m_audio_index];
	} else {
		AVF_LOGE("no video, no audio");
		return E_ERROR;
	}

	pat_counter = 0;
	pmt_counter = 0;

	mb_stream_started = false;
	mb_pat_valid = false;
	mb_pmt_valid = false;

	return E_OK;
}

avf_status_t CTSWriter::SetStreamAttr(avf_stream_attr_s& stream_attr)
{
	m_stream_attr = stream_attr;
	return E_OK;
}

void CTSWriter::GetUploadingFilename(char *buffer, int len, avf_uint_t index)
{
	char path[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_UPLOAD_PATH_VIDEO, mIndex, path, VALUE_UPLOAD_PATH_VIDEO);
	avf_snprintf(buffer, len, "%s%d.ts", path, index);
	buffer[len-1] = 0;
}

// uploading sequence:
//	Start, End(false),Start, End(false),Start, ..., Start,End(true)
void CTSWriter::StartUploading(CBuffer *pBuffer)
{
	if (mp_upload_io == NULL) {
		mp_upload_io = CSysIO::Create();
	}

	// filename
	char filename[256];
	GetUploadingFilename(with_size(filename), m_upload_counter);
	m_upload_filename = filename;

	if (mp_upload_io->CreateFile(filename) != E_OK) {
		AVF_LOGE("Create file failed: %s", filename);
		avf_safe_release(mp_upload_io);
		return;
	}

	// upload header
	m_upload_header.u_data_type = VDB_DATA_VIDEO;
	m_upload_header.u_data_size = 0;	// fixed later
	m_upload_header.u_timestamp = (pBuffer->m_pts - m_upload_start_pts) * 1000 / pBuffer->GetTimeScale();
	m_upload_header.u_stream = 0;
	m_upload_header.u_duration = 0;
	m_upload_header.u_version = UPLOAD_VERSION_v2;
	m_upload_header.u_flags = 0;
	m_upload_header.u_param1 = 0;
	m_upload_header.u_param2 = 0;

	mp_upload_io->Write(&m_upload_header, sizeof(m_upload_header));

	// stream attr
	mp_upload_io->Write(&m_stream_attr, sizeof(m_stream_attr));
	m_upload_header.u_data_size += sizeof(m_stream_attr);

	mb_uploading = true;
}

// bLast: end recording
void CTSWriter::EndUploading(bool bLast)
{
	if (mp_upload_io) {
		// fix data_size
		mp_upload_io->Seek(0);
		mp_upload_io->Write(&m_upload_header, sizeof(m_upload_header));

		mp_upload_io->Close();

		// notify app
		mpEngine->WriteRegString(NAME_UPLOAD_VIDEO_PREV, mIndex, m_upload_filename->string());
		mpEngine->WriteRegInt32(NAME_UPLOAD_VIDEO_INDEX, mIndex, m_upload_counter);
		PostFilterMsg(IEngine::MSG_UPLOAD_VIDEO);

		m_upload_counter++;

		// remove oldest one
		if (m_upload_counter > m_video_upload_num) {
			char filename[256];
			GetUploadingFilename(with_size(filename), m_upload_counter - m_video_upload_num);
			avf_remove_file(filename, false);
		}

		if (bLast) {
			avf_safe_release(mp_upload_io);
			mb_uploading = false;
			/// todo - write end flag
		}
	}
}

int CTSWriter::GetStream(CMediaFormat *format)
{
	for (int i = 0; i < MAX_STREAMS; i++) {
		if (format == mpFormat[i])
			return i;
	}
	return -1;
}

avf_status_t CTSWriter::WriteSample(int stream, CBuffer* pBuffer)
{
	if (stream < 0) {
		if ((stream = GetStream(pBuffer->mpFormat)) < 0) {
			AVF_LOGE("stream not found");
			return E_INVAL;
		}
	} else if (stream >= MAX_STREAMS) {
		AVF_LOGE("no such stream %d", stream);
		return E_INVAL;
	}

	CMediaFormat *format = mpFormat[stream];
	if (format == NULL) {
		AVF_LOGE("no such stream %d", stream);
		return E_INVAL;
	}
/*
	if (!pmt_written && mbEnableRecord && !mb_uploading) {
		WritePAT_PMT(1, 0);
		pmt_written = 1;
	}
*/
	// check for uploading at IDR
	if (format->mFormatType == MF_H264VideoFormat && (pBuffer->mFlags & CBuffer::F_AVC_IDR)) {
		if (mpEngine->ReadRegBool(NAME_UPLOAD_VIDEO, mIndex, VALUE_UPLOAD_VIDEO)) {
			if (mb_uploading) {
				// continue uploading: start a new file
				EndUploading(false);
				StartUploading(pBuffer);
			} else {
				// start uploading
				AVF_LOGD("-- start uploading --");
				m_upload_counter = 1;
				m_upload_start_pts = pBuffer->m_pts;
				StartUploading(pBuffer);
			}

		} else {

			if (mb_uploading) {
				// stop uploading
				AVF_LOGD("-- stop uploading --");
				EndUploading(true);
			} else {
				// do nothing
			}
		}

		if (mpObserver && mbEnableRecord) {
			// from prew IDR or file start to this IDR (not including)
			avf_u32_t fpos = add_video_started ? (avf_u32_t)mpIO->GetPos() : 0;
			add_video_started = 1;
			mpObserver->AddVideo(pBuffer->m_pts, pBuffer->GetTimeScale(), fpos, mIndex);

			// notify video source to start new segment: 1.5 GB (1500 MB)
			if (mIndex == 0 && !mb_file_too_large) {
				avf_u32_t size_mb = (avf_u32_t)(mpIO->GetSize() >> 20);
				if (size_mb >= 1500) {
					mb_file_too_large = true;
					AVF_LOGW("file is too large: %d MB", size_mb);
					SetLargeFileFlag(1);
				}
			}
		}

		if (mbEnableRecord || mb_uploading) {
			// PAT/PMT before IDR
			WritePAT_PMT(mbEnableRecord, mb_uploading);
			pmt_written = 1;
		}
	}

	if (mbEnableRecord || mb_uploading) {
		switch (format->mFormatType) {
		case MF_H264VideoFormat:
			WritePES(format, stream, pBuffer, 0, 1);
			m_upload_header.u_duration += pBuffer->m_duration;
			break;

		case MF_AACFormat:
		case MF_Mp3Format:
			WritePES(format, stream, pBuffer, 0, 0);
			break;

		case MF_SubtitleFormat:
			WritePES(format, stream, pBuffer, 2, 0);
			break;
		}
	}

	return E_OK;
}

void CTSWriter::WritePAT_PMT(int b_io, int b_uploading)
{
	if (!mb_pat_valid) {
		mb_pat_valid = true;
		CreatePAT();
	}

	if (!mb_pmt_valid) {
		mb_pmt_valid = true;
		CreatePMT();
	}

	WriteSection(PAT_PID, PA_TID, &pat_counter, m_pat_data, m_pat_size, b_io, b_uploading);
	WriteSection(PMT_PID, PM_TID, &pmt_counter, m_pmt_data, m_pmt_size, b_io, b_uploading);
}

// pcr: 1 or 0
avf_u64_t CTSWriter::WritePES(CMediaFormat *pFormat, avf_uint_t stream, 
	CBuffer* pBuffer, avf_size_t subt, int pcr)
{
	avf_u8_t *p;

	avf_u64_t pts;
	avf_u64_t dts;

	avf_uint_t pid = mPIDs[stream];
	avf_uint_t counter = mCounter[stream];

	avf_u8_t *pes_data;
	avf_size_t pes_remain;
	avf_uint_t pusi;

	avf_size_t pcr_size;	// 0, 6
	avf_size_t stuff_size;	//
	avf_size_t adap_size;	// 2 + pcr_size + stuff_size

	avf_uint_t tmp;
	avf_u32_t t32;

	AVF_ASSERT(pBuffer->mpNext == NULL);

	pes_data = pBuffer->GetData();
	pes_remain  = 14 + pBuffer->GetDataSize() + subt;	// 14: pes header size
	if (pcr) {
		// dts
		pes_remain += 5;
	}

	if (!mbStarted[stream]) {
		mbStarted[stream] = true;
		mStartPTS[stream] = pBuffer->m_pts;
		mStartDTS[stream] = pBuffer->m_dts;
		pts = 0;
		dts = 0;
	} else {
		pts = pBuffer->m_pts - mStartPTS[stream];
		dts = pBuffer->m_dts - mStartDTS[stream];
	}

	if (pFormat->mTimeScale != _90kHZ) {
		pts = pts * _90kHZ / pFormat->mTimeScale;
		dts = dts * _90kHZ / pFormat->mTimeScale;
	}

	pusi = 2<<13;	// error, PUSI, priority
	pcr_size = (pid == m_pcr_pid) ? 6 : 0;

	CTSBuffer tsb(mpIO, mpBlockIO);

	while (true) {
		if (pcr_size) {
			tmp = PACKET_SIZE - 4 - 8;	// 8: adap_length+flags+PCR
			if (pes_remain < tmp)
				stuff_size = tmp - pes_remain;
			else
				stuff_size = 0;
			adap_size = 8 + stuff_size;
		} else {
			tmp = PACKET_SIZE - 4;
			if (pes_remain < tmp) {
				adap_size = tmp - pes_remain;
				if (adap_size > 2)
					stuff_size = adap_size - 2;
				else
					stuff_size = 0;
			} else {
				stuff_size = 0;
				adap_size = 0;
			}
		}

		avf_u8_t *pkt = tsb.GetPacket();
		p = pkt;

		*p++ = SYNC_BYTE;

		// error, PUSI, priority, PID
		tmp = pusi | pid;
		SAVE_BE_16(p, tmp);

		if (adap_size == 0) {
			*p++ = (1<<4) | (counter++ & 0x0F);
		} else {
			*p++ = (3<<4) | (counter++ & 0x0F);
			*p++ = adap_size - 1;	// adaptation_field_length
			if (adap_size > 1) {
				if (pcr_size == 0) {
					*p++ = 0;	// flags
				} else {
					*p++ = (pBuffer->mFlags & CBuffer::F_KEYFRAME) ? 0x50 : 0x10;
					// pcr
					t32 = (avf_u32_t)(dts >> 1);
					SAVE_BE_32(p, t32);
					tmp = (((avf_uint_t)dts & 1) << 15) | (0x3F<<9) | 0;
					SAVE_BE_16(p, tmp);
				}
				if (stuff_size > 0) {
					::memset(p, 0xFF, stuff_size);
					p += stuff_size;
				}
			}
		}

		// PES
		if (pusi) {
			t32 = 0x00000100 | mStreamId[stream];
			SAVE_BE_32(p, t32);

			tmp = pcr ? 0 : pes_remain - 6;
			SAVE_BE_16(p, tmp);

			*p++ = 0x84;	// '10' 00 0 1 0 0 : data_alignment_indicator
			*p++ = pcr ? 0xC0 : 0x80;
			*p++ = pcr ? 10 : 5;		// PES_header_data_length

			// PTS[32..30], marker_bit
			tmp = (avf_u32_t)(pts >> 29) & (0x07<<1);
			if (pcr)
				*p++ = (3<<4) | tmp | 1;	// '0011'
			else
				*p++ = (2<<4) | tmp | 1;	// '0010'

			// PTS[29..15], marker_bit
			tmp = ((avf_u32_t)pts >> 14) & (0x7FFF<<1);
			tmp |= 1;
			SAVE_BE_16(p, tmp);

			// PTS[14..0], marker_bit
			tmp = ((avf_u32_t)pts << 1) & (0x7FFF<<1);
			tmp |= 1;
			SAVE_BE_16(p, tmp);

			pes_remain -= 14;	// header was written

			if (pcr) {

				// DTS[32..30], marker_bit
				tmp = (avf_u32_t)(dts >> 29) & (0x07<<1);
				*p++ = (1<<4) | tmp | 1;	// '0001'

				// DTS[29..15], marker_bit
				tmp = ((avf_u32_t)dts >> 14) & (0x7FFF<<1);
				tmp |= 1;
				SAVE_BE_16(p, tmp);

				// DTS[14..0], marker_bit
				tmp = ((avf_u32_t)dts << 1) & (0x7FFF<<1);
				tmp |= 1;
				SAVE_BE_16(p, tmp);

				pes_remain -= 5;
			}
		}

		// write data
		tmp = PACKET_SIZE - (p - pkt);
		AVF_ASSERT(pes_remain >= tmp);	// already has stuffing

		if (subt) {
			subt = 0;
			SAVE_BE_16(p, pBuffer->GetDataSize());
			pes_remain -= 2;
			tmp -= 2;
		}

		::memcpy(p, pes_data, tmp);
		pes_data += tmp;
		pes_remain -= tmp;

		WriteUpload(pkt, PACKET_SIZE);
		tsb.WritePacket();

		// other packets
		avf_size_t npackets = pes_remain / (PACKET_SIZE - 4);
		pes_remain -= npackets * (PACKET_SIZE - 4);
		while (npackets--) {
			avf_u8_t *tspacket = tsb.GetPacket();
			p = tspacket;

			*p++ = SYNC_BYTE;
			SAVE_BE_16(p, pid);
			*p++ = (1<<4) | (counter++ & 0x0F);

			::memcpy(p, pes_data, PACKET_SIZE - 4);
			pes_data += PACKET_SIZE - 4;

			WriteUpload(pkt, PACKET_SIZE);
			tsb.WritePacket();
		}

		if (pes_remain == 0)
			break;

		pusi = 0<<13;
		pcr_size = 0;
	}

	tsb.Update();

	mCounter[stream] = counter;

	return pts;
}

void CTSWriter::CreatePAT()
{
	avf_u8_t *p = m_pat_data;
	avf_u8_t *plength;
	avf_uint_t tmp;

	*p++ = PA_TID;

	plength = p;
	SAVE_BE_16(p, 0);	// section length, patch later

	SAVE_BE_16(p, TS_ID);

	*p++ = (3<<6) | (0<<1) | 1;	// current_next=1
	SAVE_BE_16(p, 0);	// section number, last section number

	// item
	SAVE_BE_16(p, PG_NUMBER);
	tmp = (7<<13) | PMT_PID;
	SAVE_BE_16(p, tmp);

	// fix section length
	int length = p - plength + (4 - 2);
	tmp = (1<<15) | (0<<14) | (3<<12) | length;
	SAVE_BE_16(plength, tmp);

	avf_u32_t crc = avf_crc_ieee(-1, m_pat_data, p - m_pat_data);
	SAVE_LE_32(p, crc);

	m_pat_size = p - m_pat_data;
}

void CTSWriter::CreatePMT()
{
	avf_u8_t *p = m_pmt_data;
	avf_u8_t *plength;
	avf_u8_t *plength_info;
	avf_uint_t tmp;

	*p++ = PM_TID;

	plength = p;
	SAVE_BE_16(p, 0);	// section length, patch later

	SAVE_BE_16(p, PG_NUMBER);

	*p++ = (3<<6) | (0<<1) | 1;	// current_next=1
	SAVE_BE_16(p, 0);	// section number, next section number

	tmp = (7<<13) | m_pcr_pid;
	SAVE_BE_16(p, tmp);

	plength_info = p;
	SAVE_BE_16(p, 0);	// program_info_length, patch later

	// registration descriptor
	*p++ = 5;	// descriptor tag
	*p++ = 4;	// descriptor length
	*p++ = 'H';
	*p++ = 'D';
	*p++ = 'M';
	*p++ = 'V';

	// IOD
	if (m_subt_index >= 0) {
		p += CreateIOD(p);
	}

	// fix program_info_length
	tmp = (15<<12) | (p - plength_info - 2);
	SAVE_BE_16(plength_info, tmp);

	// stream info
	for (avf_size_t i = 0; i < MAX_STREAMS; i++) {
		CMediaFormat *format = mpFormat[i];
		if (format) {
			switch (format->mFormatType) {
			case MF_H264VideoFormat: {
					CH264VideoFormat *pAVCFormat = (CH264VideoFormat*)format;
					avf_size_t descriptor_size = 0;
					amba_h264_t::info_t info;
					if (pAVCFormat->mDataFourcc == FCC_AMBA) {
						CAmbaAVCFormat *pAmbaFormat = (CAmbaAVCFormat*)format;
						info = pAmbaFormat->data.info;
						descriptor_size = sizeof(info);
					}

					*p++ = 0x1B;	// H.264
					tmp = (7<<13) | mPIDs[i];
					SAVE_BE_16(p, tmp);
					tmp = (15<<12) | (descriptor_size + 2);
					SAVE_BE_16(p, tmp);

					if (descriptor_size) {
						info.idr_interval = (pAVCFormat->mWidth << 16) | (info.idr_interval & 0xFFFF);
						info.idr_size = (pAVCFormat->mHeight << 16) | (info.idr_size & 0xFFFF);
						*p++ = AMBA_DESCRIPTOR_TAG;
						*p++ = descriptor_size;
						::memcpy(p, &info, descriptor_size);
						p += descriptor_size;
					}
				}
				break;

			case MF_AACFormat:
			case MF_Mp3Format:
				*p++ = format->mFormatType == MF_AACFormat ? 0x0F : 0x03;	// AAC, MPEG-1
				tmp = (7<<13) | mPIDs[i];
				SAVE_BE_16(p, tmp);
				tmp = (15<<12) | 0;	// ES_info_length==0
				SAVE_BE_16(p, tmp);
				break;

			case MF_SubtitleFormat:
				*p++ = 0x12;	//
				tmp = (7<<13) | mPIDs[i];
				SAVE_BE_16(p, tmp);
				tmp = (15<<12) | 4;	// FMC descriptor
				SAVE_BE_16(p, tmp);
				*p++ = 0x1F;	// FMC
				*p++ = 2;
				SAVE_BE_16(p, mPIDs[i]);
				break;
			}
		}
	}

	// fix section length
	int length = p - plength + (4-2);
	tmp = (1<<15) | (0<<14) | (3<<12) | length;
	SAVE_BE_16(plength, tmp);

	avf_u32_t crc = avf_crc_ieee(-1, m_pmt_data, p - m_pmt_data);
	SAVE_LE_32(p, crc);

	m_pmt_size = p - m_pmt_data;
}

avf_size_t CTSWriter::CreateIOD(avf_u8_t *pstart)
{
	pstart[0] = 0x1d;	// IOD_descriptor
	pstart[1] = 0;		// descriptor length, patch later
	pstart[2] = 0x11;	// IOD_label_scope
	pstart[3] = 1;		// IOD_label

	avf_size_t size = CreateInitialObjectDescriptor(pstart + 4);
	pstart[1] = 2 + size;

	return 4 + size;
}

avf_size_t CTSWriter::CreateInitialObjectDescriptor(avf_u8_t *pstart)
{
	avf_u8_t *p = pstart;
	avf_uint_t tmp;

	p[0] = 0x02;	// InitialObjectDescrTag
	//p[1] = 0;		// size, patch later
	//p[2] = 0;		// size, patch later
	//p[3] = 0;		// size, patch later
	p += 4;

	tmp = (1<<6) | (0<<5) | (0<<5) | 0x0F;
	SAVE_BE_16(p, tmp);

	p[0] = 0xFF;	// ODProfile
	p[1] = 0xFF;	// sceneProfile
	p[2] = 0xFE;	// audioProfile
	p[3] = 0xFE;	// visualProfile
	p[4] = 0xFF;	// graphicProfile
	p += 5;

	tmp = CreateESDescriptor(p);
	SetSize3(pstart + 1, tmp + 7);

	return tmp + 11;
}

avf_size_t CTSWriter::CreateESDescriptor(avf_u8_t *pstart)
{
	avf_u8_t *p = pstart;
	avf_uint_t tmp;

	p[0] = 0x03;	// ES_DescrTag
	//p[1] = 0;		// size, patch later
	//p[2] = 0;		// size, patch later
	//p[3] = 0;		// size, patch later
	p += 4;

	tmp = mPIDs[m_subt_index];
	SAVE_BE_16(p, tmp);
	*p++ = 0x1F;	// 

	tmp = CreateDecoderConfiguration(p);
	p += tmp;

	tmp += CreateSLConfig(p);
	SetSize3(pstart + 1, tmp + 3);

	return tmp + 7;
}

avf_size_t CTSWriter::CreateDecoderConfiguration(avf_u8_t *pstart)
{
	avf_u8_t *p = pstart;
	avf_uint_t tmp;

	p[0] = 0x04;	// DecoderConfigDescrTag
	//p[1] = 0;		// size, patch later
	//p[2] = 0;		// size, patch later
	//p[3] = 0;		// size, patch later
	p += 4;

	*p++ = 0x0B;	// test stream
	*p++ = (4<<2) | (0<<1) | 1;	// visual stream
	SAVE_BE_24(p, 0x100000);	// bufferSizeDB
	SAVE_BE_32(p, 0x7fffffff);	// maxBitrate
	SAVE_BE_32(p, 0);			// avgBitrate

	tmp = CreateDecSpecificInfo(p);
	SetSize3(pstart + 1, tmp + 13);

	return tmp + 17;
}

avf_size_t CTSWriter::CreateDecSpecificInfo(avf_u8_t *pstart)
{
	avf_u8_t *p = pstart;
	avf_u8_t *psize;
	avf_uint_t tmp;

	p[0] = 0x05;	// DecSpecificInfoTag
	//p[1] = 0;		// size, patch later
	//p[2] = 0;		// size, patch later
	//p[3] = 0;		// size, patch later
	p += 4;

	p[0] = 0x10;	// textFormat, 0x10 for 3GPP TS 26.245
	p[1] = 0;		// flags
	p += 2;

	psize = p;
	*p++ = 0;		// size, patch later

	SAVE_BE_32(p, 0);	// display flags
	*p++ = 0;		// horizontal justification (-1: left, 0 center, 1 right)
	*p++ = 1;		// vertical   justification (-1: top, 0 center, 1 bottom)

	SAVE_BE_24(p, 0);	// background rgb
	*p++ = 0xff;	// background a

	SAVE_BE_16(p, 0);	// text box top
	SAVE_BE_16(p, 0);	// text box left
	SAVE_BE_16(p, 0);	// text box bottom
	SAVE_BE_16(p, 0);	// text box right

	SAVE_BE_16(p, 0);	// start char
	SAVE_BE_16(p, 0);	// end char
	SAVE_BE_16(p, 0);	// default font id

	*p++ = 0;		// font style flags
	*p++ = 12;		// font size

	SAVE_BE_24(p, 0);	// foreground rgb
	*p++ = 0x00;	// foreground a

	SAVE_BE_32(p, 18);	// size
	SAVE_FCC(p, "ftab");
	SAVE_BE_16(p, 1);	// entry count
	SAVE_BE_16(p, 1);	// font id
	*p++ = 5;		// font name length
	SAVE_FCC(p, "Serif");
	*p++ = 'f';

	tmp = p - psize - 1;
	*psize = tmp;
	SetSize3(pstart + 1, tmp + 3);

	return tmp + 7;
}

avf_size_t CTSWriter::CreateSLConfig(avf_u8_t *pstart)
{
	avf_u8_t *p = pstart;

	*p++ = 0x06;	// SLConfgDescrTag
	SAVE_BE_24(p, 0x808008);	// 8 bytes

	*p++ = 1;		// predefined
	SAVE_BE_24(p, 0);	//
	SAVE_BE_32(p, 0);

	return 12;
}

void CTSWriter::SetSize3(avf_u8_t *p, avf_size_t size)
{
	AVF_ASSERT(size < (1<<21));
	p[0] = 0x80 | (size >> 14);
	size &= 0x3FFF;
	p[1] = 0x80 | (size >> 7);
	size &= 0x7F;
	p[2] = size;
}

void CTSWriter::WriteSection(int pid, int table_id, avf_u8_t *counter, 
		avf_u8_t *data, avf_size_t size, int b_io, int b_uploading)
{
	avf_u8_t packet[PACKET_SIZE];
	avf_u8_t *p;
	avf_size_t tmp;
	avf_size_t towrite;
	avf_uint_t pusi = 1;

	while (size > 0) {
		p = packet;
		*p++ = SYNC_BYTE;

		// error, PUSI, priority, PID
		tmp = (pusi << 14) | pid;
		SAVE_BE_16(p, tmp);

		*p++ = (1<<4) | (*counter & 0x0F);	// only payload
		(*counter)++;

		if (pusi) {
			*p++ = 0;	// pointer_field
			pusi = 0;
		}

		tmp = PACKET_SIZE - (p - packet);
		towrite = (tmp > size) ? size : tmp;

		::memcpy(p, data, towrite);

		if (tmp > size) {
			::memset(p + towrite, 0xFF, tmp - size);
			if (b_io) {
				WriteIO(packet, PACKET_SIZE);
			}
			if (b_uploading) {
				WriteUpload(packet, PACKET_SIZE);
			}
			return;
		}

		if (b_io) {
			WriteIO(packet, PACKET_SIZE);
		}
		if (b_uploading) {
			WriteUpload(packet, PACKET_SIZE);
		}

		data += towrite;
		size -= towrite;
	}
}


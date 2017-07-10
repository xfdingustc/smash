
#define LOG_TAG "mp4_writer"

#include <limits.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "avio.h"
#include "buffer_io.h"

#include "avf_media_format.h"
#include "mpeg_utils.h"
#include "avf_config.h"

#include "tsmap.h"

#include "sample_queue.h"
#include "mp4_writer.h"

const avf_u32_t g_mp4_matrix[] = {
	0x10000, 0, 0,
	0, 0x10000, 0,
	0, 0, 0x40000000
};

const char g_muxer_name[] = "AVF MP4 MUXER v2.1";
int g_muxer_name_len = sizeof(g_muxer_name);

const char g_video_handler[] = "AVF Video";
int g_video_handler_len = (sizeof(g_video_handler) - 1);

const char g_audio_handler[] = "AVF Audio";
int g_audio_handler_len = (sizeof(g_audio_handler) - 1);

const char g_subtitle_handler[] = "AVF Subtitle";
int g_subtitle_handler_len = (sizeof(g_subtitle_handler) - 1);

static const char g_subtitle_font_tx3g[] = "Serif";
static const char g_subtitle_font_text[] = "Helvetica";
static const bool g_use_text_box = false;

// when getting rid of the leading 00 00 01,
// if this macro is defined, writer will ignore the remaining bytes
// when IDR or non-IDR picture header is found
#define FAST_NAL

//-----------------------------------------------------------------------
//
//  CMP4Writer
//
//-----------------------------------------------------------------------
IMediaFileWriter* CMP4Writer::Create(IEngine *pEngine, CConfigNode *node, int index, const char *avio)
{
	CMP4Writer *result = avf_new CMP4Writer(pEngine, node, index, avio);
	CHECK_STATUS(result);
	return result;
}

CMP4Writer::CMP4Writer(IEngine *pEngine, CConfigNode *node, int index, const char *avio):
	inherited(pEngine),
	mpObserver(NULL),

	mpMap(NULL),
	mpIO(NULL),

	mIndex(index),
	mTimeScale(1000),
	mFileName(),
//	m_wbuf(4096),
	m_data_start(0),
	m_data_pos(0),

	//m_info_size(512*1024),
	m_info_size(0),

	//mb_flat_chunks(false)
	mb_flat_chunks(true)
{
	SET_ARRAY_NULL(mpStreams);

	// get Movie box size
	if (m_info_size == 0) {
		m_info_size = mpEngine->ReadRegInt32(NAME_MP4_MOOV_SIZE, 0, VALUE_MP4_MOOV_SIZE);
		if (m_info_size) {
			AVF_LOGI("Set moov size to %d", m_info_size);
		}
	}

	if (node) {
		CConfigNode *pAttr = node->FindChild("vdb");
		if (pAttr && ::atoi(pAttr->GetValue())) {
			mpObserver = GetRecordObserver();
		}
	}

	for (avf_size_t i = 0; i < MAX_STREAMS; i++) {
		mpStreams[i] = avf_new CMP4Stream(this, i);
		CHECK_STATUS(mpStreams[i]);
		CHECK_OBJ(mpStreams[i]);
	}
}

CMP4Writer::~CMP4Writer()
{
	avf_safe_release(mpIO);
	avf_safe_release(mpMap);
	SAFE_RELEASE_ARRAY(mpStreams);
}

void CMP4Writer::InitFile()
{
	if (m_info_size == 0) {
		// 'moov' at file end
		m_data_start = WriteFileType(0) + 16; // file-type + wide + mdat
	} else {
		// 'moov' at file start, before MediaData
		m_data_start = m_info_size;
	}

	m_data_pos = m_data_start;

	WRITE_BOX(WriteFileType);
}

// mTimeStamp, mDuration, mTotalTracks
void CMP4Writer::CalcGlobalInfo(avf_u64_t nextseg_ms)
{
//	int gmt_off = 0;
//	mModificationTimeStamp = avf_get_sys_time(&gmt_off) + 2082844800;
//	mModificationTimeStamp -= gmt_off;

	mDuration = nextseg_ms - m_seg_start_ms;

	mCreationTimeStamp = avf_time_to_sys_time(mGenTime) + 2082844800;
	mModificationTimeStamp = mCreationTimeStamp + (mDuration / 1000);

	mTotalTracks = 0;
	for (avf_size_t i = 0; i < ARRAY_SIZE(mpStreams); i++) {
		CMP4Stream *pStream = mpStreams[i];
		if (pStream && pStream->IsValid()) {
			mTotalTracks++;
			if (!mb_flat_chunks) {
				pStream->CalcChunkOffsetEntries();
			}
		}
	}

	m_seg_start_ms = nextseg_ms;
}

void CMP4Writer::CompleteFile()
{
	if (m_info_size) {
		StartStreamer();
		WriteFileTail();
		FlushStreamer();
		mpIO->Seek(WriteFileType(0));
	}

	// movie header
	StartStreamer();
	WRITE_BOX(WriteMovie);
	if (m_info_size) {
		avf_u64_t mdat_start = m_data_start - 16;
		avf_u64_t pos = m_wbuf.GetWritePos();
		avf_u32_t empty_size = (avf_u32_t)(mdat_start - pos);

		AVF_ASSERT(pos <= mdat_start);
		AVF_ASSERT(empty_size >= 8);

		write_box(empty_size, "free");
	} else {
		FlushStreamer();
		WriteFileTail();
	}
	FlushStreamer();

	// mdat box
	mpIO->Seek(m_data_start - 16);
	StartStreamer();
	WriteMediaDataBox();
	FlushStreamer();

	mpIO->Flush();

	IBlockIO *block_io = IBlockIO::GetInterfaceFrom(mpIO);
	if (block_io) {
		block_io->_EndWrite();
	}
}

void CMP4Writer::ResetStreams()
{
	for (avf_uint_t i = 0; i < ARRAY_SIZE(mpStreams); i++) {
		CMP4Stream *pStream = mpStreams[i];
		if (pStream && pStream->IsValid()) {
			pStream->Reset();
		}
	}
}

IAVIO *CMP4Writer::GetIO()
{
	return mpIO;
}

avf_status_t CMP4Writer::StartFile(string_t *filename, avf_time_t video_time,
	string_t *picture_name, avf_time_t picture_time, string_t *basename)
{
	if (!mbEnableRecord)
		return E_OK;

	mb_file_too_large = false;
	if (mIndex == 0) {
		SetLargeFileFlag(0);
	}

	if (mpObserver) {
		avf_safe_release(mpMap);
		if ((mpMap = CTSMap::Create(mCounter)) == NULL)
			return E_NOMEM;
	}

	avf_safe_release(mpIO);
	if ((mpIO = CBufferIO::Create()) == NULL)
		return E_NOMEM;

	if (mpObserver) {
		mpObserver->PostIO(mpIO, mpMap->GetMapInfo(), mIndex);
	}

	mGenTime = video_time;
	add_video_started = 0;

	avf_size_t len = filename->size();
	const char *p = filename->string() + len;
	if (len > 4 && p[-4] == '.' && p[-3] == 'm' && p[-2] == 'p' && p[-1] == '4') {
		mFileName = filename;
	} else if (len > 3 && p[-3] == '.' && p[-2] == 't' && p[-1] == 's') {
		string_t *name = string_t::Add(filename->string(), len - 2, "mp4", 3, NULL, 0, NULL, 0);
		mFileName = name;
		name->Release();
	} else {
		mFileName = ap<string_t>(filename, ".mp4");
	}

	avf_status_t status = mpIO->CreateFile(mFileName->string());
	if (status != E_OK) {
		return status;
	}
	AVF_LOGI("Writing file to %s", mFileName->string());

	if (mpObserver == NULL) {
		mpEngine->WriteRegString(NAME_FILE_CURRENT, mIndex, mFileName->string());
		PostFilterMsg(IEngine::MSG_FILE_START);
	}

	// init the file
	StartStreamer();
	InitFile();
	FlushStreamer();

	mpIO->Seek(m_data_pos);

	if (mpMap) {
		for (avf_uint_t i = 0; i < MAX_STREAMS; i++) {
			CMP4Stream *pStream = mpStreams[i];
			if (pStream) {
				CMediaFormat *format = pStream->mMediaFormat.get();
				if (format) {
					switch (format->mFormatType) {
					case MF_H264VideoFormat: {
							CH264VideoFormat *pAVCFormat = (CH264VideoFormat*)format;

							uint32_t tag = 0;
							amba_h264_t::info_t info;
							uint32_t info_size = 0;

							if (pAVCFormat->mDataFourcc == FCC_AMBA) {
								CAmbaAVCFormat *pAmbaFormat = (CAmbaAVCFormat*)format;
								info = pAmbaFormat->data.info;
								info.idr_interval = (pAVCFormat->mWidth << 16) | (info.idr_interval & 0xFFFF);
								info.idr_size = (pAVCFormat->mHeight << 16) | (info.idr_size & 0xFFFF);
								info_size = 2 + 1 + sizeof(info);	// length + tag + data
								tag = 0xFE;
							}

							raw_data_t *raw = avf_alloc_raw_data(1 + info_size + 2, 0);
							if (raw) {
								uint8_t *p = raw->GetData();
								*p++ = 0x1B;	// AVC
								if (info_size) {
									SAVE_BE_16(p, info_size);
									*p++ = tag;
									::memcpy(p, &info, sizeof(info));
									p += sizeof(info);
								}
								SAVE_BE_16(p, 0);

								mpMap->SetVideoDesc(raw);
								raw->Release();
							}
						}
						break;

					case MF_AACFormat:
					case MF_Mp3Format: {
							raw_data_t *raw = avf_alloc_raw_data(1 + 2, 0);
							if (raw) {
								uint8_t *p = raw->GetData();
								*p++ = format->mFormatType == MF_AACFormat ? 0x0F : 0x03;	// AAC, MPEG-1
								SAVE_BE_16(p, 0);

								mpMap->SetAudioDesc(raw);
								raw->Release();
							}
						}
						break;

					default:
						break;
					}
				}
			}
		}
	}

	return E_OK;
}

avf_status_t CMP4Writer::EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms)
{
	if (!mbEnableRecord)
		return E_OK;

	CalcGlobalInfo(nextseg_ms);

	if (mpMap) {
		int inc = mpMap->PreFinishFile(mpIO);
		if (inc > 0) {
			m_data_pos += inc;
		}
		if (fsize) {
			*fsize = mpMap->GetFileSize();
		}
	}

	CompleteFile();

	avf_safe_release(mpMap);
	avf_safe_release(mpIO);

//	ModifyFileTime(mFileName->string(), mGenTime);
	AVF_LOGI("%s completed", mFileName->string());

	ResetStreams();

	if (mpObserver == NULL) {
		mpEngine->WriteRegString(NAME_FILE_PREVIOUS, mIndex, mFileName->string());
		PostFilterMsg(IEngine::MSG_FILE_END);
	}

	return E_OK;
}

void CMP4Writer::WriteFileTail()
{
	IBlockIO *block_io = IBlockIO::GetInterfaceFrom(mpIO);
	if (block_io == NULL)
		return;

	IBlockIO::block_desc_s desc;
	if (block_io->_GetBlockDesc(desc) != E_OK)
		return;

	if (mpMap) {
		avf_size_t size_needed = mpMap->GetSizeNeeded();
		if (size_needed > 0) {
			if (desc.align_size > 0) {
				if (size_needed > desc.file_remain) {
					avf_size_t tmp = desc.file_remain + desc.align_size;
					while (tmp < size_needed)
						tmp += desc.align_size;
					size_needed = tmp;
					AVF_LOGD("file_remain: %d, size_needed: %d", desc.file_remain, size_needed);
				} else {
					size_needed = desc.file_remain;
				}
			}

			mpMap->FinishFile(m_wbuf, size_needed, mDuration, &mCounter);
			return;
		}
	}

	if (desc.align_size == 0)
		return;

	avf_size_t align_remain = desc.file_remain % desc.align_size;
	if (align_remain == 0)
		return;

	if (align_remain < 8)
		align_remain += desc.align_size;

	m_wbuf.write_be_32(align_remain);
	m_wbuf.write_fcc("free");

	FlushStreamer();
	mpIO->Write(NULL, align_remain - 8);
}

const char *CMP4Writer::GetCurrentFile()
{
	return mFileName->string();
}

avf_status_t CMP4Writer::StartWriter(bool bEnableRecord)
{
	avf_status_t status = inherited::StartWriter(bEnableRecord);
	if (status != E_OK)
		return status;

	m_seg_start_ms = 0;
	mCounter.Init();

	return E_OK;
}

avf_status_t CMP4Writer::EndWriter()
{
	inherited::EndWriter();

	for (avf_uint_t i = 0; i < ARRAY_SIZE(mpStreams); i++) {
		CMP4Stream *pStream = mpStreams[i];
		if (pStream && pStream->IsValid()) {
			pStream->StopStream();
		}
	}

	return E_OK;
}

avf_status_t CMP4Writer::SetRawData(avf_size_t stream, raw_data_t *raw)
{
	if (stream >= MAX_STREAMS)
		return E_NOENT;
	CMP4Stream *pStream = mpStreams[stream];
	return pStream->SetRawData(raw);
}

avf_status_t CMP4Writer::CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	if (stream >= MAX_STREAMS)
		return E_NOENT;
	CMP4Stream *pStream = mpStreams[stream];
	return pStream->CheckMediaFormat(pMediaFormat);
}

avf_status_t CMP4Writer::SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	if (stream >= MAX_STREAMS)
		return E_NOENT;
	CMP4Stream *pStream = mpStreams[stream];
	return pStream->SetMediaFormat(pMediaFormat);
}

avf_status_t CMP4Writer::CheckStreams()
{
	// todo - check if streams are valid
	return E_OK;
}

avf_status_t CMP4Writer::WriteSample(int stream, CBuffer* pBuffer)
{
	if (!mbEnableRecord)
		return E_OK;

	if (stream >= MAX_STREAMS) {
		AVF_LOGE("Cannot write stream %d", stream);
		return E_NOENT;
	}
	CMP4Stream *pStream = mpStreams[stream];

	if (pBuffer->mFlags & CBuffer::F_RAW) {
		pStream->WriteSampleData(pBuffer);
		return E_OK;
	}

	if (pStream->IsSubtitle()) {
		pStream->PreWriteSubtitleSample(pBuffer);
	} else {
		bool is_idr = pStream->IsAVCFormat() && (pBuffer->mFlags & CBuffer::F_AVC_IDR);

		if (is_idr && mpObserver && mpMap) {
			// from prev IDR or file start to this IDR (not including)
			avf_u32_t fpos = add_video_started ? (avf_u32_t)mpMap->GetFileSize() : 0;
			add_video_started = 1;
			mpObserver->AddVideo(pBuffer->m_pts, pBuffer->GetTimeScale(), fpos, mIndex);
		}

		pStream->WriteSampleData(pBuffer);

		if (is_idr && mpObserver) {
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
	}

	return E_OK;
}

bool CMP4Writer::ProvideSamples()
{
	return false;
}

ISampleProvider *CMP4Writer::CreateSampleProvider(avf_size_t before, avf_size_t after)
{
	return NULL;
}

void CMP4Writer::write_box(avf_u32_t size, const char *fcc)
{
	write_be_32(size);
	write_fcc(fcc);
}

void CMP4Writer::write_large_box(avf_u64_t size, const char *fcc)
{
	write_be_32(1);
	write_fcc(fcc);
	write_be_64(size);
}

void CMP4Writer::write_full_box(avf_u32_t size, const char *fcc, int version, avf_u32_t flags)
{
	write_be_32(size);
	write_fcc(fcc);
	write_be_8(version);
	write_be_24(flags);
}

void CMP4Writer::write_matrix()
{
	for (avf_uint_t i = 0; i < 9; i++)
		write_be_32(g_mp4_matrix[i]);
}

void CMP4Writer::write_time()
{
	write_be_32((avf_u32_t)mCreationTimeStamp);
	write_be_32((avf_u32_t)mModificationTimeStamp);
}

void CMP4Writer::write_string(const char *str)
{
	int len = ::strlen(str);
	write_mem((const avf_u8_t*)str, len + 1);
}

avf_u32_t CMP4Writer::get_string_size(const char *str)
{
	return ::strlen(str) + 1;
}

// ftyp
avf_u32_t CMP4Writer::WriteFileType(avf_u32_t size)
{
	if (size == 0) {
		return 28;
	}

	write_box(size, "ftyp");
	write_fcc("avc1");
	write_be_32(0);	// minor_version
	write_fcc("M4V ");
	write_fcc("M4A ");
	write_fcc("isom");

	return size;
}

// moov
avf_u32_t CMP4Writer::WriteMovie(avf_u32_t size)
{
	if (size == 0) {
		return 8 + 
			WriteMovieHeader(0) + 
			WriteTracks(0) + 
			WriteUserData(0);
	}

	write_box(size, "moov");
	WRITE_BOX(WriteMovieHeader);
	WRITE_BOX(WriteTracks);
	WRITE_BOX(WriteUserData);

	return size;
}

// moov.mvhd
avf_u32_t CMP4Writer::WriteMovieHeader(avf_u32_t size)
{
	if (size == 0) {
		return 108;
	}

	write_full_box(size, "mvhd", 0, 0);
	write_time();
	write_be_32(mTimeScale);
	write_be_32(mDuration);
	write_be_32(0x10000);	// rate
	write_be_16(0x100);		// volumn
	write_zero(10);
	write_matrix();
	write_zero(24);			// pre_defined
	write_be_32(mTotalTracks + 1);

	return size;
}

avf_u32_t CMP4Writer::WriteTracks(avf_u32_t size)
{
	avf_u32_t total_size = 0;

	mTrackID = 0;
	for (avf_uint_t i = 0; i < MAX_STREAMS; i++) {
		CMP4Stream *pStream = mpStreams[i];
		if (pStream && pStream->IsValid()) {
			++mTrackID;
			mpWriteStream = pStream;
			if (size == 0) {
				pStream->m_trak_size = WriteTrack(0);
			} else {
				WriteTrack(pStream->m_trak_size);
			}
			total_size += pStream->m_trak_size;
		}
	}

	return total_size;
}

// moov.udta
avf_u32_t CMP4Writer::WriteUserData(avf_u32_t size)
{
	if (size == 0) {
		return 8 + WriteUserDataInf(0) + WriteUserDataVideoBox(0);
	}

	write_box(size, "udta");
	WRITE_BOX(WriteUserDataInf);
	WRITE_BOX(WriteUserDataVideoBox);

	return size;
}

// moov.udta.\251enc
avf_u32_t CMP4Writer::WriteUserDataInf(avf_u32_t size)
{
	if (size == 0) {
		return 8 + 4 + g_muxer_name_len;
	}

	write_box(size, "\251enc");
	write_be_16(g_muxer_name_len);
	write_be_16(0);
	write_mem((const avf_u8_t*)g_muxer_name, g_muxer_name_len);

	return size;
}

// moov.udta.AMBA
avf_u32_t CMP4Writer::WriteUserDataVideoBox(avf_u32_t size)
{
	CMediaFormat *pMediaFormat = NULL;
	avf_size_t i;

	for (i = 0; i < ARRAY_SIZE(mpStreams); i++) {
		CMP4Stream *pStream = mpStreams[i];
		if (pStream && pStream->mMediaFormat.get()) {
			pMediaFormat = pStream->mMediaFormat.get();
			if (pMediaFormat->mFormatType == MF_H264VideoFormat)
				break;
		}
	}

	if (i >= ARRAY_SIZE(mpStreams))
		return 0;

	CH264VideoFormat *pFormat = (CH264VideoFormat*)pMediaFormat;
	if (pFormat->mDataFourcc != MKFCC('A','M','B','A'))
		return 0;

	if (size == 0) {
		return 8 + sizeof(amba_h264_t);
	}

	write_be_32(size);
	write_be_32(pFormat->mDataFourcc);

	CAmbaAVCFormat *pAmbaFormat = (CAmbaAVCFormat*)pFormat;
	write_be_16(pAmbaFormat->data.info.ar_x);
	write_be_16(pAmbaFormat->data.info.ar_y);
	write_be_8(pAmbaFormat->data.info.mode);
	write_be_8(pAmbaFormat->data.info.M);
	write_be_8(pAmbaFormat->data.info.N);
	write_be_8(pAmbaFormat->data.info.gop_model);
	write_be_32(pAmbaFormat->data.info.idr_interval);
	write_be_32(pAmbaFormat->data.info.rate);
	write_be_32(pAmbaFormat->data.info.scale);
	write_be_32(pAmbaFormat->data.info.bitrate);
	write_be_32(pAmbaFormat->data.info.bitrate_min);
	write_be_32(pAmbaFormat->data.info.idr_size);
	write_zero(sizeof(pAmbaFormat->data.reserved));

	return 0;
}

// moov.trak
avf_u32_t CMP4Writer::WriteTrack(avf_u32_t size)
{
	if (size == 0) {
		size = 8;
		size += mpWriteStream->m_tkhd_size = WriteTrackHeader(0);
		size += mpWriteStream->m_edts_size = WriteEditListContainer(0);
		size += mpWriteStream->m_mdia_size = WriteMedia(0);
		return size;
	}

	write_box(size, "trak");
	WriteTrackHeader(mpWriteStream->m_tkhd_size);
	WriteEditListContainer(mpWriteStream->m_edts_size);
	WriteMedia(mpWriteStream->m_mdia_size);

	return size;
}

// moov.trak.tkhd
avf_u32_t CMP4Writer::WriteTrackHeader(avf_u32_t size)
{
	if (size == 0) {
		return 92;
	}

	write_full_box(size, "tkhd", 0, 0x0F);
	write_time();
	write_be_32(mTrackID);
	write_be_32(0);			// reserved

	avf_u32_t duration = mpWriteStream->GetGlobalDuration();
	write_be_32(duration);

	write_zero(8);			// reserved
	write_be_16(0);			// layer
	write_be_16(0);			// alternate_group

	// volumn
	if (mpWriteStream->IsAudio())
		write_be_16(256);
	else
		write_be_16(0);

	write_be_16(0);			// reserved
	write_matrix();

	avf_u32_t width, height;
	mpWriteStream->GetSize(&width, &height);

	write_be_16(width);
	write_be_16(0);
	write_be_16(height);
	write_be_16(0);

	return size;
}

// moov.trak.edts
avf_u32_t CMP4Writer::WriteEditListContainer(avf_u32_t size)
{
	if (size == 0) {
		return 8 + WriteEditList(0);
	}

	write_box(size, "edts");
	WRITE_BOX(WriteEditList);

	return size;
}

// moov.trak.edts.elst
avf_u32_t CMP4Writer::WriteEditList(avf_u32_t size)
{
	if (size == 0) {
		return 16 + 12*1;
	}

	write_full_box(size, "elst", 0, 0);
	write_be_32(1);	// entry count

	write_be_32((avf_u32_t)mpWriteStream->GetGlobalDuration());	// segment_duration
	write_be_32(0);						// media_time
	write_be_16(1);						// media_rate_integer
	write_be_16(0);						// media_rate_fraction

	return size;
}

// moov.trak.mdia
avf_u32_t CMP4Writer::WriteMedia(avf_u32_t size)
{
	if (size == 0) {
		size = 8;
		size += mpWriteStream->m_mdhd_size = WriteMediaHeader(0);
		size += mpWriteStream->m_hdlr_size = WriteHandlerReference(0);
		size += mpWriteStream->m_minf_size = WriteMediaInformation(0);
		return size;
	}

	write_box(size, "mdia");
	WriteMediaHeader(mpWriteStream->m_mdhd_size);
	WriteHandlerReference(mpWriteStream->m_hdlr_size);
	WriteMediaInformation(mpWriteStream->m_minf_size);

	return size;
}

// moov.trak.mdia.mdhd
avf_u32_t CMP4Writer::WriteMediaHeader(avf_u32_t size)
{
	if (size == 0) {
		return 32;
	}

	write_full_box(size, "mdhd", 0, 0);
	write_time();

	write_be_32(mpWriteStream->mTimeScale);
	write_be_32(mpWriteStream->mDuration);

	//write_be_16(0);		// language
	write_be_16(0x55c4);	// und
	write_be_16(0);			// pre_defined

	return size;
}

// moov.trak.mdia.hdlr
avf_u32_t CMP4Writer::WriteHandlerReference(avf_u32_t size)
{
	if (size == 0) {
		if (mpWriteStream->IsVideo())
			return 32 + 1 + g_video_handler_len;
		else if (mpWriteStream->IsAudio())
			return 32 + 1 + g_audio_handler_len;
		else if (mpWriteStream->IsSubtitle())
			return 32 + 1 + g_subtitle_handler_len;
	}

	write_full_box(size, "hdlr", 0, 0);
	write_be_32(0);		// pre_defined

	if (mpWriteStream->IsVideo()) {
		write_fcc("vide");
		write_zero(12);		// reserved
		write_be_8(g_video_handler_len);
		write_mem((const uint8_t*)g_video_handler, g_video_handler_len);
	} else if (mpWriteStream->IsAudio()) {
		write_fcc("soun");
		write_zero(12);		// reserved
		write_be_8(g_audio_handler_len);
		write_mem((const uint8_t*)g_audio_handler, g_audio_handler_len);
	} else if (mpWriteStream->IsSubtitle()) {
		write_fcc("text");
		write_zero(12);		// reserved
		write_be_8(g_subtitle_handler_len);
		write_mem((const uint8_t*)g_subtitle_handler, g_subtitle_handler_len);
	}

	return size;
}

// moov.trak.mdia.minf
avf_u32_t CMP4Writer::WriteMediaInformation(avf_u32_t size)
{
	if (size == 0) {
		size = 8;

		if (mpWriteStream->IsVideo()) {
			size += mpWriteStream->m_vshd_size = WriteVideoMediaHeader(0);
		} else if (mpWriteStream->IsAudio()) {
			size += mpWriteStream->m_vshd_size = WriteAudioMediaHeader(0);
		} else if (mpWriteStream->IsSubtitle()) {
			size += mpWriteStream->m_vshd_size = WriteSubtitleMediaHeader(0);
		}

		size += mpWriteStream->m_dinf_size = WriteDataInformation(0);
		size += mpWriteStream->m_stbl_size = WriteSampleTable(0);

		return size;
	}

	write_box(size, "minf");

	if (mpWriteStream->IsVideo()) {
		WriteVideoMediaHeader(mpWriteStream->m_vshd_size);
	} else if (mpWriteStream->IsAudio()) {
		WriteAudioMediaHeader(mpWriteStream->m_vshd_size);
	} else if (mpWriteStream->IsSubtitle()) {
		WriteSubtitleMediaHeader(mpWriteStream->m_vshd_size);
	}

	WriteDataInformation(mpWriteStream->m_dinf_size);
	WriteSampleTable(mpWriteStream->m_stbl_size);

	return size;
}

// moov.trak.mdia.minf.vmhd
avf_u32_t CMP4Writer::WriteVideoMediaHeader(avf_u32_t size)
{
	if (size == 0) {
		return 20;
	}

	write_full_box(size, "vmhd", 0, 1);
	write_be_16(0);		// graphics_mode
	write_zero(6);		// opcolor

	return size;
}

// moov.trak.mdia.minf.smhd
avf_u32_t CMP4Writer::WriteAudioMediaHeader(avf_u32_t size)
{
	if (size == 0) {
		return 16;
	}

	write_full_box(size, "smhd", 0, 0);
	write_be_16(0);	// balance
	write_be_16(0);	// reserved

	return size;
}

// moov.trak.mdia.minf.smhd
avf_u32_t CMP4Writer::WriteSubtitleMediaHeader(avf_u32_t size)
{
	if (g_use_text_box)
		return WriteBaseMediaInformationHeader(size);
	else
		return WriteNullMediaHeader(size);
}

avf_u32_t CMP4Writer::WriteNullMediaHeader(avf_u32_t size)
{
	if (size == 0) {
		return 12;
	}

	write_box(size, "nmhd");
	write_be_32(0);	// flags

	return size;
}

avf_u32_t CMP4Writer::WriteBaseMediaInformationHeader(avf_u32_t size)
{
	if (size == 0) {
		return 8 + 24;
	}

	// gmhd
	write_box(8, "gmhd");

	// gmin
	write_full_box(24, "gmin", 0, 1);
	write_be_16(0);	// graphics mode
	write_zero(6);	// op color
	write_be_16(0);	// balance
	write_be_16(0);	// reserved

	return size;
}

// moov.trak.mdia.minf.dinf
avf_u32_t CMP4Writer::WriteDataInformation(avf_u32_t size)
{
	if (size == 0) {
		return 8 + WriteDataReference(0);
	}

	write_box(size, "dinf");
	WRITE_BOX(WriteDataReference);

	return size;
}

// moov.trak.mdia.minf.dinf.dref
avf_u32_t CMP4Writer::WriteDataReference(avf_u32_t size)
{
	if (size == 0) {
		return 16 + 12*1;
	}

	write_full_box(size, "dref", 0, 0);
	write_be_32(1);	// entry_count

	write_full_box(12, "url ", 0, 1);

	return size;
}

// moov.trak.mdia.minf.stbl
avf_u32_t CMP4Writer::WriteSampleTable(avf_u32_t size)
{
	if (size == 0) {
		size = mpWriteStream->m_sample_table_size;
		if (size == 0) {
			size = 8;
			size += mpWriteStream->m_stsd_size = WriteSampleDescription(0);
			size += mpWriteStream->m_stts_size = WriteDecodingTimeToSample(0);
			size += mpWriteStream->m_ctts_size = WriteCompositionTimeToSample(0);
			if (mpWriteStream->IsVideo())
				size += mpWriteStream->m_stss_size = WriteSyncSampleTable(0);
			else	// audio and subtitle
				size += mpWriteStream->m_stss_size = 0;
			size += mpWriteStream->m_stsc_size = WriteSampleToChunk(0);
			size += mpWriteStream->m_stsz_size = WriteSampleSize(0);
			size += mpWriteStream->m_stco_size = WriteChunkOffset(0);
			mpWriteStream->m_sample_table_size = size;
		}
		return size;
	}

	write_box(size, "stbl");
	WriteSampleDescription(mpWriteStream->m_stsd_size);
	WriteDecodingTimeToSample(mpWriteStream->m_stts_size);
	WriteCompositionTimeToSample(mpWriteStream->m_ctts_size);
	if (mpWriteStream->IsVideo())
		WriteSyncSampleTable(mpWriteStream->m_stss_size);
	WriteSampleToChunk(mpWriteStream->m_stsc_size);
	WriteSampleSize(mpWriteStream->m_stsz_size);
	WriteChunkOffset(mpWriteStream->m_stco_size);

	return size;
}

// moov.trak.mdia.minf.stbl.stsd
avf_u32_t CMP4Writer::WriteSampleDescription(avf_u32_t size)
{
	if (size == 0) {
		size = 16;
		if (mpWriteStream->IsVideo())
			size += WriteVideoSampleEntry(0);
		else if (mpWriteStream->IsAudio())
			size += WriteAudioSampleEntry(0);
		else if (mpWriteStream->IsSubtitle())
			size += WriteTextSampleEntry(0);
		return size;
	}

	write_full_box(size, "stsd", 0, 0);
	write_be_32(1);	// entry_count

	if (mpWriteStream->IsVideo())
		WRITE_BOX(WriteVideoSampleEntry);
	else if (mpWriteStream->IsAudio())
		WRITE_BOX(WriteAudioSampleEntry);
	else if (mpWriteStream->IsSubtitle())
		WRITE_BOX(WriteTextSampleEntry);

	return size;
}

// moov.trak.mdia.minf.stbl.stts
avf_u32_t CMP4Writer::WriteDecodingTimeToSample(avf_u32_t size)
{
	if (size == 0) {
		return ProcessDecodingTimeToSample(16, false);
	}

	write_full_box(size, "stts", 0, 0);
	write_be_32((size - 16) / 8);

	ProcessDecodingTimeToSample(size, true);

	return size;
}

avf_u32_t CMP4Writer::ProcessDecodingTimeToSample(avf_size_t size, bool bWrite)
{
	CMP4Stream::sample_block_t *pBlock = mpWriteStream->m_first_sample_block;
	if (pBlock == NULL || pBlock->m_count == 0)
		return size;

	media_sample_t *pStartSample = pBlock->m_samples;
	avf_u32_t last_duration = pStartSample->m_duration;
	avf_u32_t last_sample_index = 0;
	avf_u32_t sample_index = 0;

	for (; pBlock; pBlock = pBlock->GetNext()) {
		media_sample_t *sample = pBlock->m_samples;
		avf_u32_t i = pBlock->m_count;

		for (; i; i--, sample_index++, sample++) {
			if (sample->m_duration != last_duration) {
				if (bWrite) {
					write_be_32(sample_index - last_sample_index);
					write_be_32(last_duration);
				} else {
					size += 8;
				}
				last_sample_index = sample_index;
				last_duration = sample->m_duration;
			}
		}
	}

	if (bWrite) {
		write_be_32(sample_index - last_sample_index);
		write_be_32(last_duration);
	} else {
		size += 8;
	}

	return size;
}

void CMP4Writer::WriteMediaDataBox()
{
	if (IsBigFile()) {
		write_large_box(m_data_pos - m_data_start + 16, "mdat");
	} else {
		write_box(8, "wide");
		write_box((avf_u32_t)(m_data_pos - m_data_start) + 8, "mdat");
	}
}

// moov.trak.mdia.minf.stbl.ctts
avf_u32_t CMP4Writer::WriteCompositionTimeToSample(avf_u32_t size)
{
	if (!mpWriteStream->mb_write_ctts)
		return 0;

	if (size == 0) {
		return 16 + mpWriteStream->mSampleCount * 8;
	}

	AVF_LOGI(C_YELLOW " ctts " C_NONE);

	write_full_box(size, "ctts", 0, 0);
	write_be_32(mpWriteStream->mSampleCount);

	CMP4Stream::sample_block_t *pBlock = mpWriteStream->m_first_sample_block;
	for (; pBlock; pBlock = pBlock->GetNext()) {
		media_sample_t *pSample = pBlock->m_samples;
		for (avf_u32_t i = pBlock->m_count; i; i--, pSample++) {
			write_be_32(1);
			write_be_32(pSample->m_pts_diff);
		}
	}

	return size;
}

// moov.trak.mdia.minf.stbl.stss
avf_u32_t CMP4Writer::WriteSyncSampleTable(avf_u32_t size)
{
	if (size == 0) {
		return 16 + mpWriteStream->mSyncSampleCount * 4;
	}

	write_full_box(size, "stss", 0, 0);
	write_be_32(mpWriteStream->mSyncSampleCount);

	CMP4Stream::sample_block_t *pBlock = mpWriteStream->m_first_sample_block;
	avf_u32_t index = 1;
	for (; pBlock; pBlock = pBlock->GetNext()) {
		media_sample_t *pSample = pBlock->m_samples;
		for (avf_u32_t i = pBlock->m_count; i; i--) {
			if (pSample->m_flags & CBuffer::F_KEYFRAME) {
				write_be_32(index);
			}
			index++;
			pSample++;
		}
	}

	return size;
}

// moov.trak.mdia.minf.stbl.stsc
avf_u32_t CMP4Writer::WriteSampleToChunk(avf_u32_t size)
{
	if (size == 0) {
		if (!mb_flat_chunks)
			return 16 + mpWriteStream->m_sample_to_chunk_entries * 12;
		else
			return 16 + 12;
	}

	write_full_box(size, "stsc", 0, 0);

	if (!mb_flat_chunks) {
		write_be_32(mpWriteStream->m_sample_to_chunk_entries);
		mpWriteStream->WriteSampleToChunkEntries();
	} else {
		write_be_32(1);	// entry_count
		write_be_32(1);	// first chunk
		write_be_32(1);	// samples per chunk
		write_be_32(1);	// sample description index
	}

	return size;
}

// moov.trak.mdia.minf.stbl.stsz
avf_u32_t CMP4Writer::WriteSampleSize(avf_u32_t size)
{
	if (size == 0) {
		return 20 + mpWriteStream->mSampleCount * 4;
	}

	write_full_box(size, "stsz", 0, 0);
	write_be_32(0);	// default sample size
	write_be_32(mpWriteStream->mSampleCount);

	CMP4Stream::sample_block_t *pBlock = mpWriteStream->m_first_sample_block;
	for (; pBlock; pBlock = pBlock->GetNext()) {
		media_sample_t *pSample = pBlock->m_samples;
		for (avf_u32_t i = pBlock->m_count; i; i--) {
			write_be_32(pSample->m_size);
			pSample++;
		}
	}

	return size;
}

// moov.trak.mdia.minf.stbl.stco
avf_u32_t CMP4Writer::WriteChunkOffset(avf_u32_t size)
{
	avf_u32_t entries;
	
	if (!mb_flat_chunks) {
		entries = mpWriteStream->m_chunk_offset_entries;
	} else {
		entries = mpWriteStream->mSampleCount;
	}

	if (size == 0) {
		size = 16;
		if (IsBigFile())
			size += 8 * entries;
		else
			size += 4 * entries;
		return size;
	}

	write_full_box(size, IsBigFile() ? "co64" : "stco", 0, 0);
	write_be_32(entries);

	mpWriteStream->WriteChunkOffsetEntries();

	return size;
}

// moov.trak.mdia.minf.stbl.stsd.avc1
avf_u32_t CMP4Writer::WriteVideoSampleEntry(avf_u32_t size)
{
	if (size == 0) {
		if (mpWriteStream->IsAVCFormat())
			return 86 + WriteAVCConfiguration(0);
		return 86 + 0;
	}

	if (!mpWriteStream->IsAVCFormat()) {
		write_zero(86);
		return size;
	}

	write_box(size, "avc1");
	write_zero(6);		// reserved
	write_be_16(1);		// data_reference_index
	write_be_16(0);		// predefined
	write_be_16(0);		// reserved
	write_zero(12);		// reserved

	avf_u32_t width, height;
	mpWriteStream->GetSize(&width, &height);

	write_be_16(width);
	write_be_16(height);
	write_be_32(0x00480000);	// h 72dpi
	write_be_32(0x00480000);	// v 72dpi

	write_be_32(0);		// reserved
	write_be_16(1);		// frame count per sample

	write_zero(32);		// compressor name

	write_be_16(24);		// depth
	write_be_16(0xffff);	// predefined

	WRITE_BOX(WriteAVCConfiguration);

	return size;
}

// moov.trak.mdia.minf.stbl.stsd.avc1.avcC
avf_u32_t CMP4Writer::WriteAVCConfiguration(avf_size_t size)
{
	if (size == 0) {
		size = 8;
		if (mpWriteStream->IsAVCFormat())
			size += WriteAVCConfigurationRecord(0);
		return size;
	}

	if (mpWriteStream->IsAVCFormat()) {
		write_box(size, "avcC");
		WRITE_BOX(WriteAVCConfigurationRecord);
	} else {
		write_box(size, "free");
	}

	return size;
}

avf_u32_t CMP4Writer::WriteAVCConfigurationRecord(avf_size_t size)
{
	raw_data_t *sps = mpWriteStream->m_sps;
	raw_data_t *pps = mpWriteStream->m_pps;

	int sps_size = sps ? sps->mSize : 0;
	int pps_size = pps ? pps->mSize : 0;

	if (size == 0) {
		size = 7;
		if (sps_size)
			size += 2 + sps_size;
		if (pps_size)
			size += 2 + pps_size;
		return size;
	}

	write_be_8(1);		// configuration version
	write_be_8(sps ? sps->GetData()[1] : 77);	// profile
	write_be_8(sps ? sps->GetData()[2] : 64);	// profile compatibility
	write_be_8(sps ? sps->GetData()[3] : 30);	// level
	write_be_8(0xFF);	// 6-bit 1, length_size = 11

	if (sps == NULL) {
		write_be_8(0xE0);
	} else {
		write_be_8(0xE1);
		write_be_16(sps->mSize);
		write_mem(sps->GetData(), sps->mSize);
	}

	if (pps == NULL) {
		write_be_8(0);
	} else {
		write_be_8(1);
		write_be_16(pps->mSize);
		write_mem(pps->GetData(), pps->mSize);
	}

	return size;
}

// moov.trak.mdia.minf.stbl.stsd.mp4a
avf_u32_t CMP4Writer::WriteAudioSampleEntry(avf_u32_t size)
{
	if (size == 0) {
		return 36 + WriteAudioESDesc(0);
	}

	write_box(size, "mp4a");		// todo
	write_be_32(0);		// reserved
	write_be_16(0);		// reserved
	write_be_16(1);		// data_reference_index
	write_be_16(0);		// version
	write_be_16(0);		// revision
	write_be_32(0);		// vendor

	CAudioFormat *pFormat = static_cast<CAudioFormat*>(mpWriteStream->mMediaFormat.get());

	write_be_16(pFormat->mNumChannels);
	write_be_16(pFormat->mBitsPerSample);
	write_be_16(0xFFFE);	// todo - what's is
	write_be_16(0);			// packet_size
	write_be_16(pFormat->mSampleRate);
	write_be_16(0);			// reserved

	WriteAudioESDesc(size - 36);

	return size;
}

// moov.trak.mdia.minf.stbl.stsd.stts
avf_u32_t CMP4Writer::WriteTextSampleEntry(avf_u32_t size)
{
	if (g_use_text_box)
		return WriteTextSampleEntry_text(size);
	else
		return WriteTextSampleEntry_tx3g(size);
}

avf_u32_t CMP4Writer::WriteTextSampleEntry_text(avf_u32_t size)
{
	if (size == 0) {
		return 60 + (sizeof(g_subtitle_font_text) - 1);
	}

	write_box(size, "text");
	write_zero(6);	// reserved
	write_be_16(1);	// data reference index
	write_be_32(0);	// display flags
	write_be_32(0);	// text justification
	write_zero(6);	// background color
	write_zero(8);	// default text box
	write_zero(8);	// reserved
	write_be_16(0);	// font number
	write_be_16(0);	// font face
	write_be_8(0);	// reserved
	write_be_16(0);	// reserved
	write_be_16(0x00FF);
	write_be_16(0x00FF);
	write_be_16(0x00FF);	// foreground color
	write_be_8(sizeof(g_subtitle_font_text) - 1);
	write_mem((const avf_u8_t*)g_subtitle_font_text, sizeof(g_subtitle_font_text) - 1);

	return size;
}

avf_u32_t CMP4Writer::WriteTextSampleEntry_tx3g(avf_u32_t size)
{
	if (size == 0) {
		return 46 + WriteFontTable(0);
	}

	write_box(size, "tx3g");
	write_zero(6);	// reserved
	write_be_16(1);	// data reference index

	write_be_32(0);	// display flags
	write_be_8(1);	// horizontal justification: center
	write_be_8(0xff);	// vertical justification: bottom
	write_be_32(0);	// background color

	avf_u32_t width, height;
	mpWriteStream->GetSize(&width, &height);

	write_be_16(0);		// top
	write_be_16(0);		// left
	write_be_16(height);	// bottom
	write_be_16(width);		// right

	write_be_16(0);		// start char
	write_be_16(0);		// end char
	write_be_16(1);		// font id
	write_be_8(0);		// font face
	write_be_8(12);		// font size
	write_be_32(0xffffffff);	// text color

	WriteFontTable(size - 46);

	return size;
}

avf_u32_t CMP4Writer::WriteFontTable(avf_u32_t size)
{
	if (size == 0) {
		return 8 + 2 + WriteFontRecord(0);
	}

	write_box(size, "ftab");
	write_be_16(1);	// entry count

	WRITE_BOX(WriteFontRecord);

	return size;
}

avf_u32_t CMP4Writer::WriteFontRecord(avf_u32_t size)
{
	if (size == 0) {
		return 3 + sizeof(g_subtitle_font_tx3g) - 1;
	}

	write_be_16(1);	// font id
	write_be_8(sizeof(g_subtitle_font_tx3g) - 1);
	write_mem((const avf_u8_t*)g_subtitle_font_tx3g, sizeof(g_subtitle_font_tx3g) - 1);

	return size;
}

// moov.trak.mdia.minf.stbl.stsd.mp4a.esds
avf_u32_t CMP4Writer::WriteAudioESDesc(avf_size_t size)
{
	if (size == 0) {
		return 12 + WriteAudioESDescriptor(0);
	}

	write_full_box(size, "esds", 0, 0);
	WriteAudioESDescriptor(size - 12);

	return size;
}

avf_u32_t CMP4Writer::WriteAudioESDescriptor(avf_size_t size)
{
	avf_size_t tmp;

	if (size == 0) {
		tmp = 3 + WriteAudioDecConfig(0) + WriteAudioSLConfig(0);
		return 1 + GetSizeSize(tmp) + tmp;
	}

	write_be_8(3);		// ES_DescrTag

	avf_size_t audio_desc_config_size = WriteAudioDecConfig(0);
	avf_size_t audio_sl_config_size = WriteAudioSLConfig(0);
	tmp = 3 + audio_desc_config_size + audio_sl_config_size;

	WriteSize(tmp);
	write_be_16(2);	// ES_ID - todo?
	write_be_8(0x1F);	// 

	WriteAudioDecConfig(audio_desc_config_size);
	WriteAudioSLConfig(audio_sl_config_size);

	return size;
}

avf_u32_t CMP4Writer::WriteAudioDecConfig(avf_size_t size)
{
	avf_size_t tmp;

	if (size == 0) {
		tmp = 13 + WriteAudioDecSpecificInfo(0);
		return 1 + GetSizeSize(tmp) + tmp;
	}

	write_be_8(4);		// DecoderConfigDescrTag

	avf_size_t dec_specific_info_size = WriteAudioDecSpecificInfo(0);
	tmp = 13 + dec_specific_info_size;

	WriteSize(tmp);

	int obj_type = 0x40;	// Audio ISO/IEC 14496-3
	if (mpWriteStream->IsMp3Format()) {
		obj_type = 0x6B;	// 0x69: 13818-3; 0x6B: 11172-3
	}

	write_be_8(obj_type);
	write_be_8(0x15);	// AudioStream
	write_be_24(1024*1024);	// bufferSizeDB

	write_be_32(mpWriteStream->mMaxBitrate);
	write_be_32(mpWriteStream->GetAverageBitrate());

	WriteAudioDecSpecificInfo(dec_specific_info_size);

	return size;
}

avf_u32_t CMP4Writer::WriteAudioDecSpecificInfo(avf_size_t size)
{
	avf_size_t tmp;

	CAudioFormat *pFormat = static_cast<CAudioFormat*>(mpWriteStream->mMediaFormat.get());

	if (size == 0) {
		if (mpWriteStream->IsMp3Format()) {
			// TODO:
			return 0;
		} else {
			if (pFormat->HasExtraData()) {
				tmp = pFormat->mDecoderDataSize;
				return 1 + GetSizeSize(tmp) + tmp;
			} else {
				return 2;
			}
		}
	}

	if (mpWriteStream->IsMp3Format()) {
		// TODO:
	} else {
		write_be_8(5);		// DecSpecificInfoTag
		if (pFormat->HasExtraData()) {
			tmp = pFormat->mDecoderDataSize;
			WriteSize(tmp);
			write_mem(pFormat->mDecoderData, tmp);
		} else {
			write_be_8(0);
		}
	}

	return size;
}

avf_u32_t CMP4Writer::WriteAudioSLConfig(avf_size_t size)
{
	if (size == 0) {
		return 3;
	}

	write_be_8(6);		// SLConfigDescrTag
	write_be_8(1);
	write_be_8(2);		// sl_predefined

	return size;
}

avf_u32_t CMP4Writer::GetSizeSize(avf_size_t size)
{
	if (size < (1<<7))		// 0xxxxxxx
		return 1;

	if (size < (1<<14))		// 1xxxxxxx 0xxxxxxx
		return 2;

	if (size < (1<<21))		// 1xxxxxxx 1xxxxxxx 0xxxxxxx
		return 3;

	AVF_ASSERT(size < (1<<28));	// 256M
	return 4;
}

void CMP4Writer::WriteSize(avf_size_t size)
{
	AVF_ASSERT(size < (1<<28));

	if (size >= (1<<21)) {	// 1xxxxxxx 1xxxxxxx 0xxxxxxx
		write_be_8(0x80 | (size >> 21));
		size &= 0x1FFFFF;
		write_be_8(0x80 | (size >> 14));
		size &= 0x3FFF;
		write_be_8(0x80 | (size >> 7));
		size &= 0x7F;
		write_be_8(size);
		return;
	}

	if (size >= (1<<14)) {	// 1xxxxxxx 0xxxxxxx
		write_be_8(0x80 | (size >> 14));
		size &= 0x3FFF;
		write_be_8(0x80 | (size >> 7));
		size &= 0x7F;
		write_be_8(size);
		return;
	}

	if (size >= (1<<7)) {	// 0xxxxxxx
		write_be_8(0x80 | (size >> 7));
		size &= 0x7F;
		write_be_8(size);
		return;
	}

	write_be_8(size);
}


//-----------------------------------------------------------------------
//
//  CMP4Stream
//
//-----------------------------------------------------------------------
CMP4Stream::CMP4Stream(CMP4Writer *pWriter, avf_size_t index):
	mpWriter(pWriter),
	mIndex(index),

	//mFrameDuration(3003),	// fixed later
	mTimeScale(_90kHZ),	// fixed later

	mMaxPts(0),
	mStartPts(0),
	mDuration(0),
	mTotalBytes(0),
	mMaxBitrate(0),

	m_first_sample_block(NULL), m_last_sample_block(NULL),
	m_first_chunk_block(NULL), m_last_chunk_block(NULL),

	mb_write_ctts(false),
	mSampleCount(0),
	mSyncSampleCount(0),

	mpPrevBuffer(NULL),

	m_sps(NULL),
	m_pps(NULL),
	m_aac(NULL),

	m_sample_table_size(0)
{
}

CMP4Stream::~CMP4Stream()
{
	Reset();
}

avf_status_t CMP4Stream::SetRawData(raw_data_t *raw)
{
	if (raw->fcc == FCC_SPS) {
		if (m_sps == NULL) {
			m_sps = raw->acquire();
		}
	} else if (raw->fcc == FCC_PPS) {
		if (m_pps == NULL) {
			m_pps = raw->acquire();
		}
	}
	return E_OK;
}

avf_status_t CMP4Stream::CheckMediaFormat(CMediaFormat *pMediaFormat)
{
	if (pMediaFormat->mFormatType != MF_H264VideoFormat &&
		pMediaFormat->mFormatType != MF_AACFormat &&
		pMediaFormat->mFormatType != MF_Mp3Format &&
		pMediaFormat->mFormatType != MF_SubtitleFormat &&
		pMediaFormat->mFormatType != MF_NullFormat) {
		AVF_LOGE("MP4Muxer cannot accept this media '%c%c%c%c'", FCC_CHARS(pMediaFormat->mFormatType));
		return E_INVAL;
	}
	return E_OK;
}

avf_status_t CMP4Stream::SetMediaFormat(CMediaFormat *pMediaFormat)
{
	mMediaFormat = pMediaFormat;
	::memset(&mMp3Info, 0, sizeof(mMp3Info));
	mHeaderLen = 0;

	if (pMediaFormat) {
		//mFrameDuration = pMediaFormat->mFrameDuration;
		mTimeScale = pMediaFormat->mTimeScale;
	}

	return E_OK;
}

avf_status_t CMP4Stream::StopStream()
{
	avf_safe_release(mpPrevBuffer);
	return E_OK;
}

void CMP4Stream::Reset()
{
	if (m_first_sample_block) {
		FreeAllBlocks(m_first_sample_block);
		m_first_sample_block = NULL;
		m_last_sample_block = NULL;
	}

	if (m_first_chunk_block) {
		FreeAllBlocks(m_first_chunk_block);
		m_first_chunk_block = NULL;
		m_last_chunk_block = NULL;
	}

	avf_safe_release(mpPrevBuffer);

	avf_safe_release(m_sps);
	avf_safe_release(m_pps);
	avf_safe_release(m_aac);

	mMaxPts = 0;
	mStartPts = 0;
	mDuration = 0;
	mTotalBytes = 0;
	mMaxBitrate = 0;

	mb_write_ctts = false;
	mSampleCount = 0;
	mSyncSampleCount = 0;

	m_sample_table_size = 0;
}

void CMP4Stream::WriteSampleData(CBuffer *pBuffer)
{
	media_sample_t *pSample;

	if (m_last_sample_block == NULL || m_last_sample_block->m_count == SAMPLES_PER_BLOCK) {
		if (!NewSampleBlock())
			return;
	}

	if (mSampleCount == 0) {
		mStartPts = pBuffer->m_pts;
		mMaxPts = 0;
		mDuration = pBuffer->m_duration;
	}

	avf_u64_t pts = pBuffer->m_pts - mStartPts;
	if (mMaxPts < pts)
		mMaxPts = pts;

	mDuration = mMaxPts + pBuffer->m_duration;

	pSample = m_last_sample_block->m_samples + m_last_sample_block->m_count++;

	pSample->m_pos = mpWriter->m_data_pos;
	pSample->m_size = 0;
	pSample->m_duration = pBuffer->m_duration;
	pSample->m_flags = pBuffer->mFlags;

	mSampleCount++;
	if (pBuffer->mFlags & CBuffer::F_KEYFRAME)
		mSyncSampleCount++;

	if (IsAVCFormat()) {
		pSample->m_pts_diff = (avf_u32_t)(pBuffer->m_pts - pBuffer->m_dts);
		if (pSample->m_pts_diff) {
			mb_write_ctts = true;
		}
	}

	mpWriter->m_data_pos_inc = 0;

	if (pBuffer->mFlags & CBuffer::F_RAW) {
		mpWriter->mpIO->Write(pBuffer->GetData(), pBuffer->GetDataSize());
		pSample->m_size = pBuffer->GetDataSize();
	} else {
		if (IsAVCFormat()) {
			pSample->m_size = WriteAVCSample(pBuffer);
		} else if (IsAACFormat()) {
			pSample->m_size = WriteAACSample(pBuffer);
		} else if (IsMp3Format()) {
			pSample->m_size = WriteMp3Sample(pBuffer);
		} else if (IsSubtitle()) {
			pSample->m_size = WriteSubtitleSample(pBuffer);
		}
	}

	pSample->m_pos += mpWriter->m_data_pos_inc;

	// update bitrate
	if (pSample->m_duration > 0) {
		avf_u32_t bitrate = CalcBitrate(pSample->m_size, pSample->m_duration);
		if (bitrate > mMaxBitrate) {
			mMaxBitrate = bitrate;
		}
	}

	mTotalBytes += pSample->m_size;
	mpWriter->m_data_pos += pSample->m_size;
}

void CMP4Stream::PreWriteSubtitleSample(CBuffer *pBuffer)
{
	if (mpPrevBuffer == NULL) {
		mpPrevBuffer = pBuffer->acquire();
		return;
	}

	CBuffer *pPrevBuffer = mpPrevBuffer;
	if (pPrevBuffer->m_duration == 0) {
		pPrevBuffer->m_duration = pBuffer->m_pts - pPrevBuffer->m_pts;
		WriteSampleData(pPrevBuffer);
	} else {
		WriteSampleData(pPrevBuffer);
		avf_u64_t next_pts = pPrevBuffer->m_pts + pPrevBuffer->m_duration;
		if (next_pts < pBuffer->m_pts) {
			CBuffer buffer;
			buffer.mType = CBuffer::DATA;
			buffer.mFlags = CBuffer::F_KEYFRAME;
			buffer.m_data_size = 0;
			buffer.m_pts = next_pts;
			buffer.m_duration = pBuffer->m_pts - next_pts;
			WriteSampleData(&buffer);
		}
	}

	avf_assign(mpPrevBuffer, pBuffer);
}

avf_u32_t CMP4Stream::WriteNAL(const avf_u8_t *pNAL, avf_size_t size)
{
	int nal_type = pNAL[0] & 0x1F;

	if (nal_type == 7) {
		if (m_sps == NULL) {
			m_sps = avf_alloc_raw_data(size, FCC_SPS);
			::memcpy(m_sps->GetData(), pNAL, size);
		}
		return 0;
	}

	if (nal_type == 8) {
		if (m_pps == NULL) {
			m_pps = avf_alloc_raw_data(size, FCC_PPS);
			::memcpy(m_pps->GetData(), pNAL, size);
		}
		return 0;
	}

	IAVIO *io = mpWriter->mpIO;
	io->write_be_32(size);
	io->Write(pNAL, size);

	return size + 4;
}

avf_u32_t CMP4Stream::WriteAVCSample(CBuffer *pBuffer)
{
	avf_u8_t *pmem;
	avf_u32_t size;
	avf_u32_t totalSize = 0;

	if (pBuffer->mpNext) {
		CBuffer *tmp = pBuffer->mpNext;
		size = pBuffer->GetDataSize() + tmp->GetDataSize();
		pmem = avf_malloc_bytes(size);
		AVF_ASSERT(pmem);
		::memcpy(pmem, pBuffer->GetData(), pBuffer->GetDataSize());
		::memcpy(pmem + pBuffer->GetDataSize(), tmp->GetData(), tmp->GetDataSize());
	} else {
		pmem = pBuffer->GetData();
		size = pBuffer->GetDataSize();
	}

	CTSMap *map = mpWriter->mpMap;
	if (map) {
		int is_idr = (pBuffer->mFlags & CBuffer::F_AVC_IDR) != 0;
		CTSMap::sample_info_s info;

		avf_u64_t pts = pBuffer->pts_90k(pBuffer->m_pts);
		map->WriteSample_AVC(mpWriter->mpIO, pmem, size, pts, is_idr, &info);

		if (m_sps == NULL && info.sps) {
			m_sps = avf_alloc_raw_data(info.sps->size, FCC_SPS);
			::memcpy(m_sps->GetData(), info.sps->p, info.sps->size);
		}

		if (m_pps == NULL && info.pps) {
			m_pps = avf_alloc_raw_data(info.pps->size, FCC_PPS);
			::memcpy(m_pps->GetData(), info.pps->p, info.pps->size);
		}

		mpWriter->m_data_pos_inc = info.pos_inc;
		mpWriter->m_data_pos += info.pos_inc;

		totalSize = mpWriter->mpIO->GetPos() - mpWriter->m_data_pos;

	} else {

		avf_u8_t *ptr = pmem;
		avf_u8_t *ptr_end = pmem + size;

		const avf_u8_t *pNAL = avc_find_nal(ptr, ptr_end);
		const avf_u8_t *pNextNAL;

		if (pNAL < ptr_end) {

			while (true) {

#ifdef FAST_NAL
				int nal_type = *pNAL & 0x1F;
				if (nal_type == 1 || nal_type == 5) {
					// ignore the remaining bytes when non-IDR/IDR is found
					break;
				}
#endif
				pNextNAL = avc_find_nal(pNAL, ptr_end);
				if (pNextNAL >= ptr_end)
					break;

				const avf_u8_t *p = pNextNAL - 3;
				//if (p[-1] == 0)
				//	p--;

				totalSize += WriteNAL(pNAL, p - pNAL);

				pNAL = pNextNAL;
			}

			totalSize += WriteNAL(pNAL, ptr_end - pNAL);
		}
	}

	if (pBuffer->mpNext)
		avf_free(pmem);

	if (totalSize == 0) {
		AVF_LOGD("AVC sample size is 0, data size: %d", size);
	}

	return totalSize;
}

avf_u32_t CMP4Stream::WriteAACSample(CBuffer *pBuffer)
{
	avf_size_t len;

	CTSMap *map = mpWriter->mpMap;
	if (map) {
		CTSMap::sample_info_s info;

		map->WriteSample_AAC(mpWriter->mpIO, pBuffer->GetData(), pBuffer->GetDataSize(),
			pBuffer->pts_90k(pBuffer->m_pts), &info);

		mpWriter->m_data_pos_inc = info.pos_inc;
		mpWriter->m_data_pos += info.pos_inc;

		len = mpWriter->mpIO->GetPos() - mpWriter->m_data_pos;

	} else {

		aac_adts_header_t header;

		int res = aac_parse_header(pBuffer->GetData(), pBuffer->GetDataSize(), &header);
		if (res < 0) {
			AVF_LOGE("aac_parse_header: %d", res);
			return 0;
		}

		if (!header.crc_absent && header.num_aac_frames > 1) {
			AVF_LOGE("num_aac_frames: %d", header.num_aac_frames);
			return 0;
		}

		int size = 7 + 2 * !header.crc_absent;

		IAVIO *io = mpWriter->mpIO;
		len = pBuffer->GetDataSize() - size;
		io->Write(pBuffer->GetData() + size, len);
	}

	return len;
}

avf_u32_t CMP4Stream::WriteMp3Sample(CBuffer *pBuffer)
{
//	if (mHeaderLen == 0) {
		if (pBuffer->GetDataSize() < 6) {
			return 0;
		}

		avf_u32_t frame_header = load_be_32(pBuffer->GetData());
		if (mp3_parse_header(frame_header, &mMp3Info) < 0) {
			return 0;
		}

		mHeaderLen = 4 + 2 * !mMp3Info.protection_bit;
//	}

	IAVIO *io = mpWriter->mpIO;
	io->Write(pBuffer->GetData(), pBuffer->GetDataSize());

	return pBuffer->GetDataSize();
}

avf_u32_t CMP4Stream::WriteSubtitleSample(CBuffer *pBuffer)
{
	IAVIO *io = mpWriter->mpIO;

	io->write_be_16(pBuffer->GetDataSize());
	io->Write(pBuffer->GetData(), pBuffer->GetDataSize());

	return 2 + pBuffer->GetDataSize();
}

void CMP4Stream::CalcChunkOffsetEntries()
{
	m_chunk_offset_entries = 0;
	m_sample_to_chunk_entries = 0;

	if (m_first_sample_block == NULL || m_first_sample_block->m_count == 0)
		return;

	sample_block_t *pBlock = m_first_sample_block;
	media_sample_t *pStartSample = pBlock->m_samples;
	media_sample_t *pPrevSample = pStartSample;
	avf_u32_t last_sample_index = 0;
	avf_u32_t stsc_last_val = 0;	// samples in prev chunk
	avf_u32_t chunk_index = 0;		// chunk index from 1
	avf_u32_t sample_index = 0;
	avf_u32_t samples_in_chunk;

	for (; pBlock; pBlock = pBlock->GetNext()) {
		media_sample_t *sample = pBlock->m_samples;
		avf_u32_t i = pBlock->m_count;

		// start from the second sample
		if (sample_index == 0) {
			sample_index++;
			sample++;
			i--;
		}

		for (; i; i--, sample_index++, sample++) {
			if (pPrevSample->m_pos + pPrevSample->m_size != sample->m_pos) {

				chunk_index++;
				pStartSample->m_flags |= CHUNK_START;
				samples_in_chunk = sample_index - last_sample_index;
				if (samples_in_chunk != stsc_last_val) {
					stsc_last_val = samples_in_chunk;
					AddSampleToChunkEntry(chunk_index, samples_in_chunk);
				}

				pStartSample = sample;
				last_sample_index = sample_index;

				m_chunk_offset_entries++;

			}
			pPrevSample = sample;
		}
	}

	chunk_index++;
	pStartSample->m_flags |= CHUNK_START;
	samples_in_chunk = sample_index - last_sample_index;
	if (samples_in_chunk != stsc_last_val) {
		AddSampleToChunkEntry(chunk_index, samples_in_chunk);
	}

	m_chunk_offset_entries++;
}

void CMP4Stream::AddSampleToChunkEntry(avf_u32_t chunk_index, avf_u32_t samples_in_chunk)
{
	chunk_t *pChunk;

	if (m_last_chunk_block == NULL || m_last_chunk_block->m_count == CHUNKS_PER_BLOCK) {
		if (!NewChunkBlock())
			return;
	}

	pChunk = m_last_chunk_block->m_chunks + m_last_chunk_block->m_count++;
	pChunk->m_index = chunk_index;
	pChunk->m_samples_in_chunk = samples_in_chunk;

	m_sample_to_chunk_entries++;
}

void CMP4Stream::WriteChunkOffsetEntries()
{
	bool bBigFile = mpWriter->IsBigFile();
	CWriteBuffer *buffer = &mpWriter->m_wbuf;
	sample_block_t *pBlock = m_first_sample_block;

	if (!mpWriter->mb_flat_chunks) {
		for (; pBlock; pBlock = pBlock->GetNext()) {
			media_sample_t *sample = pBlock->m_samples;
			if (bBigFile) {
				for (avf_u32_t i = pBlock->m_count; i; i--, sample++) {
					if (sample->m_flags & CHUNK_START) {
						buffer->write_be_64(sample->m_pos);
					}
				}
			} else {
				for (avf_u32_t i = pBlock->m_count; i; i--, sample++) {
					if (sample->m_flags & CHUNK_START) {
						buffer->write_be_32(sample->m_pos);
					}
				}
			}
		}
	} else {
		for (; pBlock; pBlock = pBlock->GetNext()) {
			media_sample_t *sample = pBlock->m_samples;
			if (bBigFile) {
				for (avf_u32_t i = pBlock->m_count; i; i--, sample++) {
					buffer->write_be_64(sample->m_pos);
				}
			} else {
				for (avf_u32_t i = pBlock->m_count; i; i--, sample++) {
					buffer->write_be_32(sample->m_pos);
				}
			}
		}
	}
}

void CMP4Stream::WriteSampleToChunkEntries()
{
	chunk_block_t *pBlock = m_first_chunk_block;
	CWriteBuffer *buffer = &mpWriter->m_wbuf;
	for (; pBlock; pBlock = pBlock->GetNext()) {
		chunk_t *chunk = pBlock->m_chunks;
		for (avf_u32_t i = pBlock->m_count; i; i--, chunk++) {
			buffer->write_be_32(chunk->m_index);
			buffer->write_be_32(chunk->m_samples_in_chunk);
			buffer->write_be_32(1);	// sampe description index
		}
	}
}

avf_u32_t CMP4Stream::CalcBitrate(uint64_t size, uint64_t duration)
{
	CAudioFormat *pInFormat = static_cast<CAudioFormat*>(mMediaFormat.get());
	avf_u32_t result = (avf_u32_t)(size * (8 * pInFormat->mSampleRate) / duration);
	return result;
}

void CMP4Stream::GetSize(avf_u32_t *pWidth, avf_u32_t *pHeight)
{
	CVideoFormat *pFormat;

	if (mMediaFormat.get() == NULL)
		goto None;

	if (mMediaFormat->mMediaType != MEDIA_TYPE_VIDEO) {
		// use video's width/height for subtitle
		if (mMediaFormat->mMediaType == MEDIA_TYPE_SUBTITLE) {
			for (avf_uint_t i = 0; i < mpWriter->MAX_STREAMS; i++) {
				CMP4Stream *pStream = mpWriter->mpStreams[i];
				if (pStream && pStream->mMediaFormat.get() &&
					pStream->mMediaFormat->mMediaType == MEDIA_TYPE_VIDEO)
					return pStream->GetSize(pWidth, pHeight);
			}
		}
		goto None;
	}

	pFormat = (CVideoFormat*)mMediaFormat.get();
	*pWidth = pFormat->mWidth;
	*pHeight = pFormat->mHeight;

	return;

None:
	*pWidth = 0;
	*pHeight = 0;
	return;
}

bool CMP4Stream::NewBlock(avf_size_t size, block_t **pFirstBlock, block_t **pLastBlock)
{
	block_t *pBlock = (block_t*)avf_malloc(size);
	if (pBlock == NULL) {
		AVF_LOGE("Alloc new block failed");
		return false;
	}

	pBlock->m_next = NULL;
	pBlock->m_count = 0;

	if ((*pFirstBlock) == NULL) {
		*pFirstBlock = pBlock;
		*pLastBlock = pBlock;
	} else {
		(*pLastBlock)->m_next = pBlock;
		*pLastBlock = pBlock;
	}

	return true;
}

void CMP4Stream::FreeAllBlocks(block_t *block)
{
	while (block) {
		block_t *pNext = block->m_next;
		avf_free(block);
		block = pNext;
	}
}


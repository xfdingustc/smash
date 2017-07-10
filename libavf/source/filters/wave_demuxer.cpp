
#define LOG_TAG "wave_demuxer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"

#include "filter_if.h"
#include "wave_demuxer.h"

//-----------------------------------------------------------------------
//
//  CWaveDemuxer
//
//-----------------------------------------------------------------------

// after ck_id, size
static bool goto_wave_box(IAVIO *io, avf_u32_t fcc_val)
{
	if (io->Seek(12) != E_OK)
		return false;

	while (true) {
		avf_u32_t val = io->read_be_32();
		avf_u32_t size = io->read_le_32();

		if (val == fcc_val)
			return true;

		if (val == 0 || size == 0)
			return false;

		if (io->Seek(size, IAVIO::kSeekCur) != E_OK)
			return false;
	}
}

avf_int_t CWaveDemuxer::Recognize(IAVIO *io)
{
	if (io->Seek(0) != E_OK)
		return 0;
	if (io->read_be_32() != FCC_VAL("RIFF")) {
		//printf("not riff\n");
		return 0;
	}

	if (io->Seek(4, IAVIO::kSeekCur) != E_OK)
		return 0;
	if (io->read_be_32() != FCC_VAL("WAVE")) {
		//printf("not wave\n");
		return 0;
	}

	if (!goto_wave_box(io, FCC_VAL("fmt ")))
		return 0;

	if (io->read_le_16() != 1) {	// PCM
		//printf("not pcm\n");
		return 0;
	}

	return 100;
}

IFilter* CWaveDemuxer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CWaveDemuxer *result = avf_new CWaveDemuxer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

void* CWaveDemuxer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMediaSource)
		return static_cast<IMediaSource*>(this);
	return inherited::GetInterface(refiid);
}

CWaveDemuxer::CWaveDemuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpOutput(NULL),
	mpAudioFormat(NULL)
{
	mpOutput = avf_new CWaveDemuxerOutput(this);
	CHECK_STATUS(mpOutput);
	CHECK_OBJ(mpOutput);
}

CWaveDemuxer::~CWaveDemuxer()
{
	avf_safe_release(mpIO);
	avf_safe_release(mpOutput);
	avf_safe_release(mpAudioFormat);
}

avf_status_t CWaveDemuxer::InitFilter(avf_int_t index)
{
	return inherited::InitFilter(index);
}

avf_status_t CWaveDemuxer::GetMediaInfo(avf_media_info_t *info)
{
	info->length_ms = 0;	// todo
	info->num_videos = 0;
	info->num_audios = 1;
	info->num_subtitles = 0;
	return E_OK;
}

avf_status_t CWaveDemuxer::GetAudioInfo(avf_uint_t index, avf_audio_info_t *info)
{
	return E_OK;
}

avf_status_t CWaveDemuxer::SetRange(avf_u32_t start_time_ms, avf_s32_t length_ms, bool bLastFile)
{
	return E_ERROR;
}

avf_status_t CWaveDemuxer::SeekToTime(avf_u32_t ms)
{
	return E_ERROR;
}

bool CWaveDemuxer::CanFastPlay()
{
	return false;
}

avf_status_t CWaveDemuxer::SetFastPlay(int mode, int speed)
{
	return E_ERROR;
}

avf_status_t CWaveDemuxer::OpenMediaSource(IAVIO *io)
{
	AVF_ASSERT(mpAudioFormat == NULL);
	AVF_ASSERT(mpIO == NULL);

	avf_assign(mpIO, io);

	mpAudioFormat = avf_alloc_media_format(CAudioFormat);
	if (mpAudioFormat == NULL) {
		return E_NOMEM;
	}

	io->Seek(22);
	avf_size_t num_channels = io->read_le_16();
	avf_u32_t sample_rate = io->read_le_32();
	avf_u32_t bytes_per_sec = io->read_le_32();
	avf_u32_t block_align = io->read_le_16();
	avf_u32_t bits_per_sample = io->read_le_16();

	AVF_LOGI("num_channels: %d", num_channels);
	AVF_LOGI("sample_rate: %d", sample_rate);
	AVF_LOGI("bytes_per_sec: %d", bytes_per_sec);
	AVF_LOGI("block_align: %d", block_align);
	AVF_LOGI("bits_per_sample: %d", bits_per_sample);

	// CMediaFormat
	mpAudioFormat->mMediaType = MEDIA_TYPE_AUDIO;
	mpAudioFormat->mFormatType = MF_RawPcmFormat;
	mpAudioFormat->mFrameDuration = 1024;
	mpAudioFormat->mTimeScale = sample_rate;

	// CAudioFormat
	mpAudioFormat->mCoding = AudioCoding_PCM;
	mpAudioFormat->mNumChannels = num_channels;
	mpAudioFormat->mSampleRate = sample_rate;
	mpAudioFormat->mBitsPerSample = bits_per_sample;
	mpAudioFormat->mBitsFormat = 0;
	mpAudioFormat->mFramesPerChunk = mpAudioFormat->mFrameDuration;

	return E_OK;
}

IPin* CWaveDemuxer::GetOutputPin(avf_uint_t index)
{
	return index == 0 ? mpOutput : NULL;
}

void CWaveDemuxer::FilterLoop()
{
	ReplyCmd(E_OK);

	if (!goto_wave_box(mpIO, FCC_VAL("data"))) {
		PostErrorMsg(E_DEVICE);
		return;
	}

	avf_size_t buffer_size = mpAudioFormat->ChunkSize();
	avf_u64_t remain = mpIO->GetSize() - mpIO->GetPos();

	while (true) {
		CBuffer *pBuffer;

		if (!AllocOutputBuffer(mpOutput, pBuffer, 0))
			break;

		avf_size_t bytes = buffer_size;
		if (bytes > remain) {
			bytes = (avf_size_t)remain;
			remain = 0;
		} else {
			remain -= bytes;
		}

		if (mpIO->Read(pBuffer->mpData, bytes) < 0) {
			AVF_LOGD("Cannot read %d bytes", bytes);
			pBuffer->Release();
			PostErrorMsg(E_DEVICE);
			break;
		}

		pBuffer->mType = CBuffer::DATA;
		pBuffer->mFlags = CBuffer::F_KEYFRAME;
		//pBuffer->mpData = 
		//pBuffer->mSize = bytes;
		pBuffer->m_offset = 0;
		pBuffer->m_data_size = bytes;
		pBuffer->m_dts = 0;	// todo
		pBuffer->m_pts = 0;	// todo
		pBuffer->m_duration = 0;	// todo

		mpOutput->PostBuffer(pBuffer);

		if (remain == 0) {
			GenerateEOS(mpOutput);
			break;
		}
	}
}

bool CWaveDemuxer::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	return inherited::AOCmdProc(cmd, bInAOLoop);
}

//-----------------------------------------------------------------------
//
//  CWaveDemuxerOutput
//
//-----------------------------------------------------------------------
CWaveDemuxerOutput::CWaveDemuxerOutput(CWaveDemuxer *pFilter):
	inherited(pFilter, 0, "WaveDemuxerOutput")
{
}

CWaveDemuxerOutput::~CWaveDemuxerOutput()
{
}

CMediaFormat* CWaveDemuxerOutput::QueryMediaFormat()
{
	CAudioFormat *pFormat = ((CWaveDemuxer*)mpFilter)->mpAudioFormat;
	if (pFormat) {
		pFormat->AddRef();
	}
	return pFormat;
}

IBufferPool *CWaveDemuxerOutput::QueryBufferPool()
{
	CAudioFormat *pFormat = ((CWaveDemuxer*)mpFilter)->mpAudioFormat;

	if (pFormat == NULL)
		return NULL;

	IBufferPool *pBufferPool = CMemoryBufferPool::Create("WaveDemuxerBP", 16, 
		sizeof(CBuffer), pFormat->ChunkSize());

	return pBufferPool;
}


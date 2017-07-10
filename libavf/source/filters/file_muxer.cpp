
#define LOG_TAG "file_muxer"

#include <time.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"

#include "avio.h"
#include "file_io.h"

#include "avf_config.h"
#include "avf_media_format.h"
#include "file_muxer.h"

#include "writer_list.h"

//-----------------------------------------------------------------------
//
//  CFileMuxer
//
//-----------------------------------------------------------------------
IFilter* CFileMuxer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CFileMuxer *result = avf_new CFileMuxer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CFileMuxer::CFileMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mbEnableRecord(false),
	mbEnableAVSync(false),
	mbFileNameSet(false),	// filename is specified by the filter element
	mMuxerType(),
	mpWriter(NULL),
	mpSampleWriter(NULL),
	mpObserver(NULL),
	m_sync_counter(0),
	m_sync_counter2(0),
	m_frame_counter(0),
	mb_avsync_error_sent(false)
{
	SET_ARRAY_NULL(mpInputs);

	CConfigNode *pAttr;

	if (pFilterConfig == NULL) {
		mMuxerType = "mp4";
	} else {
		pAttr = pFilterConfig->FindChild("format");
		mMuxerType = pAttr ? pAttr->GetValue() : "mp4";

		pAttr = pFilterConfig->FindChild("index");
		mTypeIndex = pAttr ? ::atoi(pAttr->GetValue()) : 0;

		pAttr = pFilterConfig->FindChild("vdb");
		if (pAttr && ::atoi(pAttr->GetValue())) {
			mpObserver = GetRecordObserver();
		}

		pAttr = pFilterConfig->FindChild("filename");
		if (pAttr) {
			mVideoFileName = ap<string_t>(pAttr->GetValue());
			mbFileNameSet = true;
		}
	}

	CREATE_OBJ(mpWriter = avf_create_file_writer(mpEngine, "async", pFilterConfig, mTypeIndex, NULL));

	mbEnableAVSync = mpEngine->ReadRegBool(NAME_AVSYNC, mTypeIndex, VALUE_AVSYNC);
	AVF_LOGD(C_GREEN "%s av sync %d" C_NONE, mbEnableAVSync ? "enable" : "disable", mTypeIndex);
}

CFileMuxer::~CFileMuxer()
{
	avf_safe_release(mpSampleWriter);
	avf_safe_release(mpWriter);
	SAFE_RELEASE_ARRAY(mpInputs);
}

void *CFileMuxer::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ISegmentSaver)
		return static_cast<ISegmentSaver*>(this);
	return inherited::GetInterface(refiid);
}

avf_status_t CFileMuxer::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	SetRTPriority(CThread::PRIO_SINK);

	for (avf_uint_t i = 0; i < MAX_INPUTS; i++) {
		mpInputs[i] = avf_new CFileMuxerInput(this, i);
		CHECK_STATUS(mpInputs[i]);
		if (mpInputs[i] == NULL)
			return E_NOMEM;
	}

	return E_OK;
}

IPin* CFileMuxer::GetInputPin(avf_uint_t index)
{
	return index >= MAX_INPUTS ? NULL : mpInputs[index];
}

avf_status_t CFileMuxer::PreRunFilter()
{
	inherited::PreRunFilter();
	return mpWriter->CheckStreams();
}

void CFileMuxer::CalcStreamAttr(avf_stream_attr_s& stream_attr)
{
	::memset(&stream_attr, 0, sizeof(stream_attr));
	stream_attr.stream_version = CURRENT_STREAM_VERSION;
	stream_attr.video_version = 0;
	stream_attr.audio_version = 0;
	stream_attr.extra_size = 0;

	for (int i = 0; i < MAX_INPUTS; i++) {
		CFileMuxerInput *input = mpInputs[i];
		if (input && input->Connected()) {
			CMediaFormat *format = input->GetUsingMediaFormat();
			if (format) {
				switch (format->mFormatType) {
				case MF_H264VideoFormat: {
						CH264VideoFormat *pFormat = (CH264VideoFormat*)format;
						stream_attr.video_coding = VideoCoding_AVC;
						stream_attr.video_framerate = pFormat->mFrameRate;
						stream_attr.video_width = pFormat->mWidth;
						stream_attr.video_height = pFormat->mHeight;
					}
					AVF_LOGD(C_WHITE "video: coding=%d, framerate=%d, width=%d, height=%d" C_NONE,
						stream_attr.video_coding,
						stream_attr.video_framerate,
						stream_attr.video_width,
						stream_attr.video_height);
					break;

				case MF_AACFormat: {
						CAacFormat *pFormat = (CAacFormat*)format;
						stream_attr.audio_coding = AudioCoding_AAC;
						stream_attr.audio_num_channels = pFormat->mNumChannels;
						stream_attr.audio_sampling_freq = pFormat->mSampleRate;
					}
					AVF_LOGD(C_WHITE "audio: coding=%d, channels=%d, sample_freq=%d" C_NONE,
						stream_attr.audio_coding,
						stream_attr.audio_num_channels,
						stream_attr.audio_sampling_freq);
					break;

				default:
					AVF_LOGD("format_type ignored: " C_LIGHT_CYAN "%c%c%c%c" C_NONE, FCC_CHARS(format->mFormatType));
					break;
				}
			}
		}
	}
}

void CFileMuxer::FilterLoop()
{
	bool bEOS = false;
	avf_status_t status;

	int value = mpEngine->ReadRegInt32(NAME_RECORD, mTypeIndex, -1);
	if (value < 0) {
		// not set, use stream 0
		value = mpEngine->ReadRegInt32(NAME_RECORD, 0, 0);
	}
	mbEnableRecord = value == 1;
	mb_avsync_error_sent = false;

	AVF_LOGD(C_YELLOW "file muxer FilterLoop %d" C_NONE, mbEnableRecord);

	CalcStreamAttr(m_stream_attr);
	mpWriter->SetStreamAttr(m_stream_attr);

	if (mbEnableRecord) {
		if (mpObserver) {
			int video_type = (mMuxerType == "mp4") ? VIDEO_TYPE_MP4 : VIDEO_TYPE_TS;
			mpObserver->InitVideoStream(&m_stream_attr, video_type, mTypeIndex);
		}
	}

	if ((status = mpWriter->StartWriter(mbEnableRecord)) != E_OK) {
		goto Error;
	}

	ReplyCmd(E_OK);

	while (true) {
		if (mbEnableRecord) {
			if (!mbFileNameSet) {
				if (mpObserver) {
					GenerateFileName_vdb();
				} else {
					GenerateFileName();
				}
			} else {
				mGenTime = avf_get_curr_time(0);
			}
		}

		mpWriter->StartFile(mVideoFileName.get(), mGenTime,
			mPictureFileName.get(), mPictureGenTime, mBaseName.get());

		bool bContinue = MuxOneFile(&bEOS);
		if (bContinue) {
			mpWriter->EndFile(NULL, false, m_nextseg_ms);
		} else {
			mpWriter->EndFile(NULL, true, 0);
			break;
		}
	}

	mpWriter->EndWriter();

	if (bEOS) {
		// only send EOS after all are done
		PostEosMsg();
	}

	avf_safe_release(mpSampleWriter);

	return;

Error:
	ReplyCmd(status);
}

bool CFileMuxer::CanSave()
{
	return mpWriter->ProvideSamples();
}

avf_status_t CFileMuxer::HandleSaveCurrent(const CMD& cmd)
{
	avf_status_t status;

	if (mpSampleWriter != NULL) {
		AVF_LOGE("SampleWriter is busy");
		return E_BUSY;
	} 

	if (!CanSave()) {
		AVF_LOGE("cannot save");
		return E_UNIMP;
	}

	save_current_t *param = (save_current_t*)cmd.pExtra;
	avf_size_t before = param->before;
	avf_size_t after = param->after;
	ap<string_t> filename = param->filename;

	status = DoSaveCurrent(before, after, filename);
	if (status != E_OK) {
		avf_safe_release(mpSampleWriter);
	}

	return status;
}

bool CFileMuxer::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	switch (cmd.code) {
	case CMD_SAVE_CURRENT:
		ReplyCmd(HandleSaveCurrent(cmd));
		return true;

	default:
		return inherited::AOCmdProc(cmd, bInAOLoop);
	}
}

bool CFileMuxer::ProcessFlag(avf_int_t flag)
{
	if (flag == FLAG_SAMPLE_WRITER) {
		if (mpSampleWriter) {
			AVF_LOGI("SaveCurrent done");
			avf_safe_release(mpSampleWriter);
			PostFilterMsg(IEngine::MSG_DONE_SAVING, 0);
		}
	}
	return true;
}

avf_status_t CFileMuxer::DoSaveCurrent(avf_size_t before, avf_size_t after, const ap<string_t>& filename)
{
	ISampleProvider *pProvider = NULL;
	IMediaFileWriter *pWriter = NULL;
	avf_status_t status;
	avf_size_t num;

	if ((pProvider = mpWriter->CreateSampleProvider(before, after)) == NULL) {
		AVF_LOGE("CreateSampleProvider failed");
		return E_ERROR;
	}

	pWriter = avf_create_file_writer(mpEngine, mMuxerType->string(), NULL, -1, NULL);
	if (pWriter == NULL) {
		status = E_ERROR;
		goto Done;
	}

	// setup media format
	num = pProvider->GetNumStreams();
	for (avf_size_t i = 0; i < num; i++) {
		CMediaFormat *pFormat = pProvider->QueryMediaFormat(i);
		if (pFormat) {
			status = pWriter->CheckMediaFormat(i, pFormat);
			if (status == E_OK) {
				status = pWriter->SetMediaFormat(i, pFormat);
			}
			pFormat->Release();
			if (status != E_OK)
				goto Done;
		}
	}

	AVF_ASSERT(mpSampleWriter == NULL);
	mpSampleWriter = CSampleWriter::Create(this, filename, time(NULL), pWriter, pProvider);
	if (mpSampleWriter == NULL) {
		status = E_NOMEM;
		goto Done;
	}

	status = E_OK;

Done:
	avf_safe_release(pProvider);
	avf_safe_release(pWriter);
	return status;
}

// mVideoFileName - /tmp/2004-10-11-16:01:00
// mPictureFileName - same
// mGenTime
void CFileMuxer::GenerateFileName()
{
	char buffer[REG_STR_MAX];

	mpEngine->ReadRegString(NAME_MUXER_FILE, mTypeIndex, buffer, "");
	if (buffer[0] != 0) {
		mVideoFileName = buffer;
		mGenTime = avf_get_curr_time(0);
	} else {
		mpEngine->ReadRegString(NAME_MUXER_PATH, mTypeIndex, buffer, VALUE_MUXER_PATH);
		mVideoFileName = buffer;

		mGenTime = avf_get_curr_time(0);
		::sprintf(buffer, "%04d-%02d-%02d-%02d-%02d-%02d", SPLIT_TIME(mGenTime));
		mBaseName = buffer;

		mVideoFileName += mBaseName;
		mPictureFileName = mVideoFileName;
		mPictureGenTime = mGenTime;
	}
}

void CFileMuxer::GenerateFileName_vdb()
{
	string_t *name;

	mGenTime = AVF_INVALID_TIME;
	mPictureGenTime = AVF_INVALID_TIME;

	name = mpObserver->CreateVideoFileName(&mGenTime, mTypeIndex, mMuxerType == "mp4" ? ".mp4" : NULL);
	mVideoFileName = name;
	name->Release();

	if (mTypeIndex == 0) {
		name = mpObserver->CreatePictureFileName(&mPictureGenTime, mTypeIndex);
		mPictureFileName = name;
		name->Release();
	}
}

// find the pin which we should receive buffer from
// its lenght is the minimum, thus making all streams nearly equal
CFileMuxerInput *CFileMuxer::FindPinToReceive(bool *bVideoEOS, avf_u64_t *length_90k_video)
{
	avf_u64_t length_90k_min = -1ULL;
	CFileMuxerInput *pMinInput = NULL;

#ifdef DEBUG_AVSYNC
	avf_u64_t length_90k_max = 0;
	CFileMuxerInput *pMaxInput = NULL;
#endif

	for (avf_uint_t i = 0; i < MAX_INPUTS; i++) {
		CFileMuxerInput *pPin = mpInputs[i];
		if (pPin && pPin->Connected()) { // pPin->GetMediaFormat()->mMediaType == MEDIA_TYPE_NULL
			if (pPin->GetEOS()) {
				if (pPin->IsVideo()) {
					*bVideoEOS = true;
					*length_90k_video = pPin->m_length_90k;
				}
			} else {
				if (pPin->m_length_90k < length_90k_min) {
					length_90k_min = pPin->m_length_90k;
					pMinInput = pPin;
				}
#ifdef DEBUG_AVSYNC
				if (pPin->m_length_90k > length_90k_max) {
					length_90k_max = pPin->m_length_90k;
					pMaxInput = pPin;
				}
#endif
			}
		}
	}

#ifdef DEBUG_AVSYNC
	if (pMinInput && pMaxInput) {
		if (m_sync_counter2 == 0) {
			m_sync_counter2 = 120;
			avf_u64_t diff = pMaxInput->m_length_90k - pMinInput->m_length_90k;
			int record_ticks = (int)(avf_get_curr_tick() - mpEngine->ReadRegInt64(NAME_CAMER_START_RECORD_TICKS, 0, 0));
			AVF_LOGD("av sync info: " C_GREEN "%s(%d,%d)-%s(%d,%d) = %d" C_NONE,
				pMaxInput->GetMediaTypeName(), pMaxInput->GetNumInputBuffers(),
				(int)(pMaxInput->m_length_90k/90) - record_ticks,
				pMinInput->GetMediaTypeName(), pMinInput->GetNumInputBuffers(),
				(int)(pMinInput->m_length_90k/90) - record_ticks,
				(int)diff);
		}
		m_sync_counter2--;
	}
#endif

	return pMinInput;
}

CFileMuxerInput *CFileMuxer::GetMaxInputPin()
{
	avf_size_t min_buffers = -1U;
	avf_size_t max_buffers = 0;
	CFileMuxerInput *pMinInput = NULL;
	CFileMuxerInput *pMaxInput = NULL;

	for (avf_uint_t i = 0; i < MAX_INPUTS; i++) {
		CFileMuxerInput *pPin = mpInputs[i];
		if (pPin && pPin->Connected() && !pPin->GetEOS()) {
			avf_size_t num_buffers = pPin->GetNumInputBuffers();
			if (min_buffers > num_buffers) {
				min_buffers = num_buffers;
				pMinInput = pPin;
			}
			if (max_buffers <= num_buffers) {
				max_buffers = num_buffers;
				pMaxInput = pPin;
			}
		}
	}

	if (max_buffers >= 30 + min_buffers) {
		if (++m_sync_counter >= 100) {
			m_sync_counter = 0;
			AVF_LOGI(C_LIGHT_PURPLE "stream %d av not sync, diff: " LLD C_NONE,
				mTypeIndex, pMaxInput->m_length_90k - pMinInput->m_length_90k);
			AVF_LOGI("%s(%d): %d ms, %s(%d): %d ms, system: %d ms",
				pMaxInput->GetMediaTypeName(), max_buffers, (int)(pMaxInput->m_length_90k/90),
				pMinInput->GetMediaTypeName(), min_buffers, (int)(pMinInput->m_length_90k/90),
				(int)(avf_get_curr_tick() - mpEngine->ReadRegInt64(NAME_CAMER_START_RECORD_TICKS, 0, 0)));
		}
		if (pMaxInput->m_length_90k > pMinInput->m_length_90k + _90kHZ * 5) {
			if (!mb_avsync_error_sent) {
				mb_avsync_error_sent = true;
				AVF_LOGE("av diff too large: " LLD " ms, send E_DEVICE",
					(pMaxInput->m_length_90k - pMinInput->m_length_90k) / 90);
				PostErrorMsg(E_DEVICE);
			}
		}
	}

	return pMaxInput;
}

// returns true: new segment
bool CFileMuxer::MuxOneFile(bool *pbEOS)
{
	bool bAlignToVideo = mpEngine->ReadRegBool(NAME_ALIGN_TO_VIDEO, 0, VALUE_ALIGN_TO_VIDEO);
	bool bVideoEOS = false;

	while (true) {

		avf_u64_t length_90k_video;
		CFileMuxerInput *pInput;

		length_90k_video = -1ULL;
		pInput = FindPinToReceive(&bVideoEOS, &length_90k_video);

		// all EOS received ?
		if (pInput == NULL) {
			*pbEOS = true;
			return false;
		}

		CBuffer *pBuffer;

		if (mbEnableRecord) {
			while (true) {
				CQueueInputPin *pQP = static_cast<CQueueInputPin*>(pInput);
				if (GetPinWithMsg(pQP) != E_OK)
					return false;

				if (pQP != static_cast<CQueueInputPin*>(pInput)) {
					pInput = GetMaxInputPin();
					if (pInput == NULL) {
						*pbEOS = true;
						return false;
					}
				}

				// new seg ?
				if (pInput->mbNewSeg) {
					pInput->mbNewSeg = false;
					m_nextseg_ms = pInput->m_newseg_ms;
					return true;
				}

				if ((pBuffer = ReceiveBufferFrom(pInput)) != NULL)
					break;

				AVF_LOGW("no buffer received");
			}
		} else {
			CQueueInputPin *pQP = static_cast<CQueueInputPin*>(pInput);
			if (ReceiveInputBuffer(pQP, pBuffer) != E_OK) {
				return false;
			}
			pInput = static_cast<CFileMuxerInput*>(pQP);
		}

		switch (pBuffer->mType) {
		case CBuffer::DATA:
			if (bAlignToVideo && bVideoEOS && pInput->m_length_90k >= length_90k_video) {
				/*
				if (!bEndDemux) {
					bEndDemux = true;
					AVF_LOGD("send MSG_END_DEMUX");
					PostFilterMsg(IEngine::MSG_END_DEMUX, 0);
				}
				*/
			} else {
				if (pBuffer->mFlags & CBuffer::F_JPEG) {
					if (mTypeIndex == 0) {
						mpWriter->WritePoster(mPictureFileName.get(), mGenTime, pBuffer);
					}
				} else {
					mpWriter->WriteSample(pInput->mPinIndex, pBuffer);
				}
			}
			pInput->UpdateLength(pBuffer);
			pBuffer->Release();
			break;

		case CBuffer::EOS:
			pInput->SetEOS();
			pBuffer->Release();
			break;

		case CBuffer::NEW_SEG:
			pInput->mbNewSeg = true;
			pInput->m_newseg_ms = pBuffer->pts_to_ms(pBuffer->m_pts);
			pBuffer->Release();
			break;

		default:
			pBuffer->Release();
			break;
		}
	}
}

//-----------------------------------------------------------------------
//
//  CFileMuxerInput
//
//-----------------------------------------------------------------------
CFileMuxerInput::CFileMuxerInput(CFileMuxer* pFilter, avf_uint_t index):
	inherited(pFilter, index, "FileMuxerInput")
{
	mbCheckFull = pFilter->mbEnableAVSync;
}

CFileMuxerInput::~CFileMuxerInput()
{
}

avf_status_t CFileMuxerInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	IMediaFileWriter *writer = ((CFileMuxer*)mpFilter)->mpWriter;
	return writer->CheckMediaFormat(mPinIndex, pMediaFormat);
}

void CFileMuxerInput::SetMediaFormat(CMediaFormat *pMediaFormat)
{
	IMediaFileWriter *writer = ((CFileMuxer*)mpFilter)->mpWriter;
	writer->SetMediaFormat(mPinIndex, pMediaFormat);
	inherited::SetMediaFormat(pMediaFormat);
}

void CFileMuxerInput::UpdateLength(CBuffer *pBuffer)
{
	CMediaFormat *pFormat = pBuffer->mpFormat;
	AVF_ASSERT(pFormat);

	avf_u64_t end_pts_90k;

	if (!mbStarted) {
		mbStarted = true;
		m_start_pts = pBuffer->m_pts;
		end_pts_90k = pBuffer->m_duration;
	} else {
		end_pts_90k = (pBuffer->m_pts - m_start_pts) + pBuffer->m_duration;
	}

	if (pFormat->mTimeScale != _90kHZ) {
		end_pts_90k = end_pts_90k * _90kHZ / pFormat->mTimeScale;
	}

	m_length_90k = end_pts_90k;

	// for debug
	m_buffer_pts = pBuffer->m_pts;
	m_buffer_len = pBuffer->m_duration;
}

//-----------------------------------------------------------------------
//
//  CSampleWriter
//
//-----------------------------------------------------------------------
CSampleWriter* CSampleWriter::Create(CFileMuxer *pMuxer, const ap<string_t>& filename, avf_time_t gen_time,
		IMediaFileWriter *pWriter, ISampleProvider *pProvider)
{
	CSampleWriter *result = avf_new CSampleWriter(pMuxer, filename, gen_time, pWriter, pProvider);
	if (result) {
		result->RunThread();
	}
	CHECK_STATUS(result);
	return result;
}

CSampleWriter::CSampleWriter(CFileMuxer *pMuxer, const ap<string_t>& filename, avf_time_t gen_time,
		IMediaFileWriter *pWriter, ISampleProvider *pProvider):
	mpMuxer(pMuxer),
	mGenTime(gen_time),
	mFileName(filename),
	mpWriter(NULL),
	mpProvider(NULL),
	mpThread(NULL)
{
	mpWriter = pWriter->acquire();
	mpProvider = pProvider->acquire();
}

void CSampleWriter::RunThread()
{
	CREATE_OBJ(mpThread = CThread::Create("SampleWriter", CSampleWriter::ThreadEntry, this));
}

CSampleWriter::~CSampleWriter()
{
	avf_safe_release(mpThread);
	avf_safe_release(mpWriter);
	avf_safe_release(mpProvider);
}

void CSampleWriter::ThreadEntry(void *p)
{
	((CSampleWriter*)p)->ThreadLoop();
}

void CSampleWriter::ThreadLoop()
{
	IAVIO *mpIO = NULL;
	string_t *filename = NULL;
	CDynamicBufferPool *bp = NULL;
	ap<string_t> tmp;

	m_result = E_OK;
	AVF_LOGI("SampleWriter run");

	if ((mpIO = CFileIO::Create()) == NULL) {
		m_result = E_ERROR;
		goto Done;
	}

	bp = CDynamicBufferPool::CreateNonBlock("SampleWriter", NULL, 4);
	if (bp == NULL) {
		m_result = E_NOMEM;
		goto Done;
	}

	if ((m_result = mpWriter->StartFile(mFileName.get(), mGenTime, NULL, AVF_INVALID_TIME, tmp.get())) != E_OK)
		goto Done;

	while (true) {

		// get samples
		media_block_t *block = mpProvider->PopMediaBlock();
		if (block == NULL) {
			// EOS
			break;
		}

		// open the file
		if (filename == NULL || ::strcmp(filename->string(), block->filename->string())) {
			avf_safe_release(filename);
			mpIO->Close();

			filename = block->filename->acquire();

			if ((m_result = mpIO->OpenRead(filename->string())) != E_OK) {
				AVF_LOGE("cannot open %s, skip", filename->string());
				avf_safe_release(filename);
				block->Release();
				continue;
			}

			AVF_LOGI("reading %s", filename->string());
		}

		// raw data
		stream_raw_data_t *pRaw = block->m_stream_raw_data;
		for (avf_size_t i = block->m_num_raw; i; i--, pRaw++) {
			mpWriter->SetRawData(pRaw->m_index, pRaw->m_data);
		}

		// read/write samples
		raw_sample_t *sample = block->mpSamples;
		for (avf_size_t i = block->m_sample_count; i; i--, sample++) {
			avf_size_t size = sample->m_size;
			CBuffer *pBuffer;

			if (!bp->AllocBuffer(pBuffer, NULL, size)) {
				AVF_LOGE("cannot alloc buffer");
				m_result = E_ERROR;
				goto End;
			}

			mpIO->Seek(sample->m_pos);
			if (mpIO->Read(pBuffer->mpData, size) != (int)size) {
				m_result = E_IO;
				goto End;
			}

			pBuffer->mType = CBuffer::DATA;
			pBuffer->frame_type = sample->frame_type;
			pBuffer->mFlags = sample->mFlags;

			pBuffer->m_offset = 0;
			pBuffer->m_data_size = size;

			pBuffer->m_dts = sample->m_dts;
			pBuffer->m_pts = sample->m_pts;
			pBuffer->m_duration = sample->m_duration;

			m_result = mpWriter->WriteSample(sample->m_index, pBuffer);
			pBuffer->Release();

			if (m_result != E_OK)
				goto End;
		}

		block->Release();
	}

End:
	mpWriter->EndFile(NULL, true, 0);

Done:
	avf_safe_release(bp);
	avf_safe_release(mpIO);
	avf_safe_release(filename);

	AVF_LOGI("SampleWriter done, result: %d", m_result);
	mpMuxer->DoneSaveCurrent();
}


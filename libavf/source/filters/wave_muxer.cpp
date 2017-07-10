
#define LOG_TAG "wave_muxer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"
#include "buffer_io.h"

#include "avf_media_format.h"
#include "wave_muxer.h"

//-----------------------------------------------------------------------
//
//  CWaveMuxer
//
//-----------------------------------------------------------------------
IFilter* CWaveMuxer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CWaveMuxer *result = avf_new CWaveMuxer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CWaveMuxer::CWaveMuxer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	mpIO(NULL)
{
}

CWaveMuxer::~CWaveMuxer()
{
	avf_safe_release(mpIO);
	avf_safe_release(mpInput);
}

avf_status_t CWaveMuxer::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	mpInput = avf_new CWaveMuxerInput(this);
	CHECK_STATUS(mpInput);
	if (mpInput == NULL)
		return E_NOMEM;

	if ((mpIO = CBufferIO::Create()) == NULL)
		return E_NOMEM;

	char buffer[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_MUXER_FILE, 0, buffer, "/tmp/mmcblk0p1/wave001.wav");
	mFileName = buffer;

	return E_OK;
}

IPin* CWaveMuxer::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

void CWaveMuxer::FilterLoop()
{
	avf_status_t status;

	status = mpIO->CreateFile(mFileName->string());
	if (status != E_OK) {
		ReplyCmd(E_OK);
		return;
	}
	AVF_LOGD("Write to file %s", mFileName->string());

	ReplyCmd(E_OK);

	mpIO->Seek(44);

	while (true) {
		CQueueInputPin *pPin;
		CBuffer *pBuffer;

		if (ReceiveInputBuffer(pPin, pBuffer) != E_OK)
			break;

		switch (pBuffer->mType) {
		case CBuffer::DATA:
//			mpIO->write_mem(pBuffer->GetData(), pBuffer->GetDataSize());
			break;

		case CBuffer::EOS:
			pBuffer->Release();
			PostEosMsg();
			goto Done;

		default:
			break;
		}

		pBuffer->Release();
	}

Done:
	CompleteFile();
	mpIO->Close();
	AVF_LOGD("File %s completed", mFileName->string());
}

void CWaveMuxer::CompleteFile()
{/*
	avf_u32_t data_size = mpIO->GetPos() - 44;

	mpIO->Seek(0);
	mpIO->write_fcc("RIFF");
	mpIO->write_le_32(data_size + 36);
	mpIO->write_fcc("WAVE");

	mpIO->write_fcc("fmt ");
	mpIO->write_le_32(16);	// size
	mpIO->write_le_16(1);	// PCM

	CAudioFormat *pFormat = static_cast<CAudioFormat*>(mpInput->GetUsingMediaFormat());

	mpIO->write_le_16(pFormat->mNumChannels);
	mpIO->write_le_32(pFormat->mSampleRate);

	// bytes per sec: 44100 * 2 * 2
	mpIO->write_le_32(pFormat->mSampleRate * pFormat->mNumChannels * (pFormat->mBitsPerSample/8));

	// block align: 2*2 = 4
	mpIO->write_le_16(pFormat->mNumChannels * (pFormat->mBitsPerSample/8));

	mpIO->write_le_16(pFormat->mBitsPerSample);

	mpIO->write_fcc("data");
	mpIO->write_le_32(data_size); */
}

//-----------------------------------------------------------------------
//
//  CWaveMuxerInput
//
//-----------------------------------------------------------------------
CWaveMuxerInput::CWaveMuxerInput(CWaveMuxer *pFilter):
	inherited(pFilter, 0, "WaveMuxerInput")
{
}

CWaveMuxerInput::~CWaveMuxerInput()
{
}

avf_status_t CWaveMuxerInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (pMediaFormat->mMediaType == MEDIA_TYPE_AUDIO) {
		CAudioFormat *pFormat = static_cast<CAudioFormat*>(pMediaFormat);
		if (pFormat->mFormatType == MF_RawPcmFormat)
			return E_OK;
	}

	return E_ERROR;
}


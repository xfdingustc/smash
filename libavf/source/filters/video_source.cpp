
#define LOG_TAG "video_source"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_malloc.h"
#include "filter_if.h"
#include "avf_util.h"
#include "avio.h"
#include "buffer_io.h"

#include "avf_config.h"
#include "avf_media_format.h"
#include "still_capture.h"
#include "video_source.h"

#include "libipc.h"
#include "avf_plat_config.h"

//-----------------------------------------------------------------------
//
//  CVideoSource
//
//-----------------------------------------------------------------------

IFilter* CVideoSource::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CVideoSource *result = avf_new CVideoSource(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CVideoSource::CVideoSource(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mbStreamConfigured(false),
	mbDeviceInitDone(false),
	mbPreview(false),
	mbSetClockMode(true),
	mOverlay(pEngine),
	mpStillCap(NULL),
	mpObserver(NULL)
{
	mNumStream = 0;
	SET_ARRAY_NULL(mpOutputs);
	mpInput = NULL;

	if (mOverlay.Status() != E_OK) {
		mStatus = mOverlay.Status();
		return;
	}

	mbSaveNextPicture = false;

	if (vsrc_Init(&mDevice) < 0) {
		AVF_LOGE("vsrc_Init failed");
		mStatus = E_ERROR;
		return;
	}

	mbDeviceInitDone = true;
	mbSetClockMode = mpEngine->ReadRegBool(NAME_CLOCK_MODE_ENABLE, IRegistry::GLOBAL, VALUE_CLOCK_MODE_ENABLE);

	int maxDevNumStream = mDevice.GetMaxNumStream(&mDevice);
	if (maxDevNumStream > MAX_STREAM)
		maxDevNumStream = MAX_STREAM;

	mNumStream = maxDevNumStream;

	for (int i = 0; i < maxDevNumStream; i++) {
		mpOutputs[i] = avf_new CVideoSourceOutput(this, i);
		CHECK_STATUS(mpOutputs[i]);
		CHECK_OBJ(mpOutputs[i]);
	}

	mpInput = avf_new CVideoSourceInput(this, 0);
	CHECK_STATUS(mpInput);
	CHECK_OBJ(mpInput);

	bool bStillCap = false;
	if (pFilterConfig) {
		CConfigNode *pAttr = pFilterConfig->FindChild("stillcap");
		if (pAttr) {
			AVF_LOGI("stillcap: %s", pAttr->GetValue());
			bStillCap = ::atoi(pAttr->GetValue()) != 0;
		}
	}

	// still capture
	if (bStillCap && mDevice.SupportStillCapture(&mDevice)) {
		CREATE_OBJ(mpStillCap = CStillCapture::Create(mpEngine));
	}

	mpObserver = GetRecordObserver();

	GetMemLimit();
}

void CVideoSource::GetMemLimit()
{
	char buffer[REG_STR_MAX];

	mpEngine->ReadRegString(NAME_MEM_LIMIT, 0, buffer, "80,48,64");
	if (::sscanf(buffer, "%d,%d,%d", &m_mem_max, &m_mem_overrun, &m_mem_overflow) != 3) {
		m_mem_max = 80;
		m_mem_overrun = 48;
		m_mem_overflow = 64;
	}

	AVF_LOGD("mem: %d,%d,%d", m_mem_max, m_mem_overrun, m_mem_overflow);

	m_mem_max *= MB;
	m_mem_overrun *= MB;
	m_mem_overflow *= MB;

	if (mpObserver) {
		mb_debug_vmem = mpEngine->ReadRegBool(NAME_DBG_VMEM, IRegistry::GLOBAL, VALUE_DBG_VMEM) != 0;
	} else {
		mb_debug_vmem = false;
	}
}

CVideoSource::~CVideoSource()
{
	if (mbDeviceInitDone) {
		avf_safe_release(mpStillCap);
		EnterIdle(true);
		mDevice.Close(&mDevice);
		mDevice.Release(&mDevice);
	}
	SAFE_RELEASE_ARRAY(mpOutputs);
	avf_safe_release(mpInput);
}

void *CVideoSource::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ISourceFilter)
		return static_cast<ISourceFilter*>(this);
	if (refiid == IID_IImageControl)
		return static_cast<IImageControl*>(this);
	if (refiid == IID_IVideoControl)
		return static_cast<IVideoControl*>(this);
	return inherited::GetInterface(refiid);
}

avf_status_t CVideoSource::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	SetRTPriority(CThread::PRIO_SOURCE);

	if (mDevice.Open(&mDevice) < 0) {
		AVF_LOGE("mDevice.Open failed");
		return E_ERROR;
	}

	// disable it by force
	mbPreview = true;
	if ((status = EnterIdle(false)) != E_OK)
		return status;

	return E_OK;
}

IPin* CVideoSource::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

IPin* CVideoSource::GetOutputPin(avf_uint_t index)
{
	if (index >= mNumStream) {
		AVF_LOGE("No such pin %d", index);
		return NULL;
	}
	return mpOutputs[index];
}

// return false: is stopped
bool CVideoSource::SendEOS(CVideoSourceOutput *pPin)
{
	CBuffer *pBuffer;
	if (!AllocOutputBuffer(pPin, pBuffer, 0)) {
		return false;
	}

	SetEOS(pPin, pBuffer);
	pPin->PostBuffer(pBuffer);

	return true;
}

// return true: is stopped or all EOS sent
bool CVideoSource::SendAllEOS()
{
	for (avf_uint_t i = 0; i < MAX_STREAM; i++) {
		CVideoSourceOutput *pOutput = mpOutputs[i];
		if (pOutput && pOutput->Connected()) {
			if (!pOutput->GetEOS()) {
				return false;
			}
		}
	}

	// all eos have been sent
	PostEosMsg();
	return true;
}

avf_status_t CVideoSource::ConfigVin()
{
	char buffer[REG_STR_MAX];

	// vin config
	mpEngine->ReadRegString(NAME_VIN, 0, buffer, VALUE_VIN);
	if (buffer[0] != 0) {
		AVF_LOGI("SetVinConfig " C_CYAN "%s" C_NONE, buffer);
		int rval = mDevice.SetVinConfig(&mDevice, buffer);
		if (rval < 0) {
			AVF_LOGE("SetVinConfig failed!");
			return E_ERROR;
		}
	}

	if (mDevice.SetClockMode && mbSetClockMode) {
		mpEngine->ReadRegString(NAME_CLOCK_MODE, 0, buffer, VALUE_CLOCK_MODE);
		if (buffer[0] != 0) {
			AVF_LOGI("SetClockMode " C_CYAN "%s" C_NONE, buffer);
			int rval = mDevice.SetClockMode(&mDevice, buffer);
			if (rval < 0) {
				AVF_LOGE("SetClockMode failed!");
				return E_ERROR;
			}
		}
	}

	return E_OK;
}

avf_status_t CVideoSource::ConfigVideoStreams()
{
	char buffer[REG_STR_MAX];
	int rval;

	// video config
	for (avf_uint_t i = 0; i < MAX_STREAM; i++) {
		mpEngine->ReadRegString(NAME_VIDEO, i, buffer, VALUE_VIDEO);
		if (buffer[0] != 0) {
			AVF_LOGI("SetVideoConfig(" C_CYAN "%d,%s" C_NONE ")", i, buffer);
			rval = mDevice.SetVideoConfig(&mDevice, i, buffer);
			if (rval < 0) {
				AVF_LOGE("SetVideoConfig failed!");
				return E_ERROR;
			}
		}
	}

	// framerate config
	for (avf_uint_t i = 0; i < MAX_STREAM; i++) {
		mpEngine->ReadRegString(NAME_VIDEO_FRAMERATE, i, buffer, VALUE_VIDEO_FRAMERATE);
		if (buffer[0] != 0) {
			AVF_LOGI("SetVideoFrameRateConfig(" C_CYAN "%d,%s" C_NONE ")", i, buffer);
			rval = mDevice.SetVideoFrameRateConfig(&mDevice, i, buffer);
			if (rval < 0) {
				AVF_LOGE("SetVideoFrameRateConfig failed!");
				return E_ERROR;
			}
		}
	}

	return E_OK;
}

// config.video.overlay = 1:0,0,0,64:52
// enable:x_off,y_off,width,height:font_size
avf_status_t CVideoSource::SetOverlayConfig(avf_uint_t stream, const char *pConfig)
{
	CVideoOverlay::config_t config;
	const char *ptr = pConfig;
	char buffer[32];
	int size;

	AVF_LOGD("SetOverlayConfig(%d,%s)", stream, pConfig);

	::memset(&config, 0, sizeof(config));

	size = avf_get_word(ptr, buffer, sizeof(buffer), ':');
	if (size < 0)
		goto BadConfig;

	if (ptr[0] == '0') {
		// disabled
		goto Done;
	}

	config.valid = 1;
	ptr += size;

	if (::sscanf(ptr, "%d,%d,%d,%d:%d",
			&config.x_off, &config.y_off,
			&config.width, &config.height,
			&config.font_size) != 5) {
		goto BadConfig;
	}

Done:
	mOverlay.ConfigOverlay(stream, &config);
	return E_OK;

BadConfig:
	AVF_LOGE("Bad overlay config: %s", pConfig);
	return E_INVAL;
}

// config.video.logo.path = /usr/local/bin/
// config.video.logo = 1:0,0,0,0:logo_1920x1080.bmp
// enable:x_off,y_off,width,height:<bmp_name>
avf_status_t CVideoSource::SetLogoConfig(avf_uint_t stream, const char *pConfig)
{
	CVideoOverlay::config_t config;
	const char *ptr = pConfig;
	char path[REG_STR_MAX];
	char buffer[32];
	int size;
	string_t *logo_filename = NULL;
	bmp8_data_s *bmp = NULL;

	AVF_LOGD("SetLogoConfig(%d,%s)", stream, pConfig);

	::memset(&config, 0, sizeof(config));

	size = avf_get_word(ptr, buffer, sizeof(buffer), ':');
	if (size < 0)
		goto BadConfig;

	if (ptr[0] == '0') {
		// disabled
		return E_OK;
	}

	config.valid = 1;
	ptr += size;

	if (::sscanf(ptr, "%d,%d,%d,%d",
			&config.x_off, &config.y_off,
			&config.width, &config.height) != 4) {
		goto BadConfig;
	}

	ptr = ::strchr(ptr, ':');
	if (ptr == NULL)
		goto BadConfig;
	ptr++;	// bmp_name

	mpEngine->ReadRegString(NAME_VIDEO_LOGO_PATH, 0, path, VALUE_VIDEO_LOGO_PATH);
	if (path[0] == 0) {
		AVF_LOGE("%s not set", NAME_VIDEO_LOGO_PATH);
		goto BadConfig;
	}

	logo_filename = string_t::Add(path, ::strlen(path), ptr, ::strlen(ptr),
		NULL, 0, NULL, 0);
	if (logo_filename == NULL)
		goto BadConfig;

	bmp = bmp8_data_s::LoadFromFile(logo_filename->string());
	if (bmp == NULL)
		goto BadConfig;

	mOverlay.ConfigLogo(stream, &config, bmp);

	avf_safe_release(logo_filename);
	avf_safe_release(bmp);
	return E_OK;

BadConfig:
	AVF_LOGE("Bad logo config: %s", pConfig);
	avf_safe_release(logo_filename);
	avf_safe_release(bmp);
	return E_INVAL;
}

avf_status_t CVideoSource::ConfigOverlay()
{
	char buffer[REG_STR_MAX];

//	avf_u64_t tick = avf_get_curr_tick();

	if (mOverlay.InitOverlay(&mDevice) < 0) {
		AVF_LOGE("InitOverlay failed");
		return E_ERROR;
	}

	for (avf_uint_t i = 0; i < MAX_STREAM; i++) {
		if (mpOutputs[i] == NULL)
			continue;

		mpOutputs[i]->mbOverlayEnabled = false;
		mpOutputs[i]->mbLogoEnabled = false;

		vsrc_stream_info_t streamInfo;
		if (mDevice.GetStreamInfo(&mDevice, i, &streamInfo) != E_OK) {
			continue;
		}

		if (streamInfo.format == VSRC_FMT_DISABLED) {
			continue;
		}
		
#ifdef ARCH_S2L
		if (mpEngine->ReadRegInt32(NAME_INTERNAL_LOGO, IRegistry::GLOBAL|i, VALUE_INTERNAL_LOGO)) {
			vsrc_stream_info_t streamInfo;
			if (mDevice.GetStreamInfo(&mDevice, i, &streamInfo) == 0) {
				int logo_size = CVideoOverlay::LOGO_48;
				if (streamInfo.height >= 1080) {
					logo_size = CVideoOverlay::LOGO_96;
				} else if (streamInfo.height >= 576) {
					logo_size = CVideoOverlay::LOGO_64;
				}
				mOverlay.EnableInternalLogo(i, logo_size);
				mpOutputs[i]->mbLogoEnabled = true;
			}
		} else {
			mpEngine->ReadRegString(NAME_VIDEO_LOGO, i, buffer, VALUE_VIDEO_LOGO);
			if (buffer[0] != 0) {
				if (SetLogoConfig(i, buffer) == E_OK) {
					mpOutputs[i]->mbLogoEnabled = true;
				}
			}
		}
#endif

		mpEngine->ReadRegString(NAME_VIDEO_OVERLAY, i, buffer, VALUE_VIDEO_OVERLAY);
		if (buffer[0] != 0) {
			if (SetOverlayConfig(i, buffer) == E_OK) {
				mpOutputs[i]->mbOverlayEnabled = true;
			}
		}

		mOverlay.UpdateOverlayConfig(i);
	}

//	AVF_LOGI("ConfigOverlay used %d ticks", (avf_u32_t)(avf_get_curr_tick() - tick));

	return E_OK;
}

bool CVideoSource::CalcPosters(vsrc_bits_buffer_t *bitsBuffer)
{
	avf_size_t num = 0;
	for (avf_uint_t i = 0; i < MAX_STREAM; i++) {
		CVideoSourceOutput *pOutput = mpOutputs[i];
		if (pOutput && pOutput->mPosterLength && !pOutput->mbSendPoster && 
				bitsBuffer->pts64 >= pOutput->m_next_poster_pts) {
			pOutput->m_next_poster_pts += 90000ULL * pOutput->mPosterLength;
			pOutput->mbSendPoster = true;
			num++;
		}
	}
	return num > 0;
}

void CVideoSource::SendPoster(CBuffer *pBuffer)
{
	for (avf_uint_t i = 0; i < MAX_STREAM; i++) {
		CVideoSourceOutput *pOutput = mpOutputs[i];
		if (pOutput && pOutput->mbSendPoster &&
				pOutput->mUsingMediaFormat->mFormatType != MF_MJPEGVideoFormat) {
			pOutput->m_next_poster_pts = pBuffer->m_pts + 90000ULL * pOutput->mPosterLength - 200;
			pOutput->mbSendPoster = false;
			pBuffer->AddRef();
			pOutput->PostBuffer(pBuffer);
			break;
		}
	}
}

avf_status_t CVideoSource::PrepareForEncoding()
{
	avf_status_t status;

	if ((status = ConfigVin()) != E_OK)
		return status;

	if ((status = ConfigVideoStreams()) != E_OK)
		return status;
	
	if ((status = EnterPreview()) != E_OK)
		return status;

	if (mDevice.UpdateVideoConfigs(&mDevice) < 0) {
		AVF_LOGE("UpdateVideoConfigs failed");
		return E_ERROR;
	}

	if ((status = ConfigOverlay()) != E_OK)
		return status;

	mbStreamConfigured = true;

	for (int i = 0; i < MAX_STREAM; i++) {
		CVideoSourceOutput *output = mpOutputs[i];
		if (output && output->Connected()) {
			output->mSegLength = mpEngine->ReadRegInt32(NAME_VIDEO_SEGMENT, i, 60);
			output->mPosterLength = mpEngine->ReadRegInt32(NAME_PICTURE_SEGMENT, i, 1);
			AVF_LOGD("set stream segment to %d, poster length to %d", output->mSegLength, output->mPosterLength);
		}
	}

	return E_OK;
}

avf_status_t CVideoSource::StartEncoding()
{
	if (mDevice.StartEncoding(&mDevice) < 0) {
		AVF_LOGE("StartEncoding failed");
		return E_ERROR;
	}
	return E_OK;
}

avf_status_t CVideoSource::StopEncoding()
{
	if (mDevice.StopEncoding(&mDevice) < 0) {
		AVF_LOGE("StopEncoding failed");
		return E_ERROR;
	}
	// keep overlay after stop encoding
	return E_OK;
}

avf_status_t CVideoSource::EnterPreview()
{
	if (mbPreview) {
		if (!mDevice.NeedReinitPreview(&mDevice))
			return E_OK;

		if (mDevice.EnterIdle(&mDevice) < 0) {
			return E_ERROR;
		}
	}

	// enter preview
	if (mDevice.EnterPreview(&mDevice) < 0) {
		return E_ERROR;
	}

	if (mpStillCap) {// && mDevice.IsStillCaptureMode(&mDevice)) {
		mpStillCap->Start();
	}

	mbPreview = true;
	return E_OK;
}

avf_status_t CVideoSource::EnterIdle(bool bStopDevice)
{
	if (!mbPreview)
		return E_OK;

	if (mpStillCap) {
		mpStillCap->Stop();
	}

	if (mDevice.EnterIdle(&mDevice) < 0) {
		AVF_LOGE("EnterIdle failed");
		return E_ERROR;
	}

#ifdef ARCH_S2L
	if (bStopDevice && mbSetClockMode) {
		avf_platform_set_clock_mode("idle");
	}
#endif

	mbPreview = false;
	return E_OK;
}

bool CVideoSource::ProcessSubtitle(bool& bStopped, bool bFirst)
{
	CBuffer *pBuffer;
	CQueueInputPin *pInput;

	if (bFirst) {
		if (ReceiveInputBuffer(pInput, pBuffer) != E_OK) {
			return false;
		}
	} else {
		if (!PeekInputBuffer(pInput, pBuffer, bStopped)) {
			return false;
		}
	}

	if (pBuffer->mType == CBuffer::DATA) {
		const char *pText = (const char*)pBuffer->GetData();
		avf_size_t size = pBuffer->GetDataSize();
//		if (size > 0) {
			for (avf_uint_t i = 0; i < MAX_STREAM; i++) {
				CVideoSourceOutput *pOutput = mpOutputs[i];
				if (pOutput && pOutput->mbOverlayEnabled)
					mOverlay.DrawText(i, 1, pText, size);
			}
//		}
	}

	pBuffer->Release();
	return true;
}

#ifdef KERNEL_MEMCPY
	extern "C" void *kernel_memcpy(avf_u8_t *dst, const avf_u8_t *src, avf_size_t size);
#endif

void CVideoSource::CopyMem(avf_u8_t *dst, const avf_u8_t *src, avf_size_t size)
{
#ifdef KERNEL_MEMCPY
	::kernel_memcpy(dst, src, size);
#else
	::memcpy(dst, src, size);
#endif
}

void CVideoSource::FilterLoop()
{
	vsrc_bits_buffer_t bitsBuffer;
	bool bPoster = false;
	bool bStopped;
	bool bDiskErrorSent = false;
	bool bDiskSlowSent = false;

	for (int i = 0; i < MAX_STREAM; i++) {
		CVideoSourceOutput *output = mpOutputs[i];
		if (output) {
			output->m_next_seg_pts = 90000ULL * output->mSegLength;
			output->m_next_poster_pts = 0;
			output->mbSendPoster = false;
		}
	}

	mbStopped = false;

	ReplyCmd(E_OK);

	// render the first subtitle on overlay before start encoding
	if (!ProcessSubtitle(bStopped, true)) {
		if (bStopped)
			goto Done;
	}

	if (StartEncoding() < 0) {
		PostErrorMsg(E_DEVICE);
		goto Done;
	}

	tick_info_init(&m_tick_info);

	if (mpObserver && mDevice.GetVinConfig) {
		raw_data_t *raw = avf_alloc_raw_data(sizeof(vin_config_info_t), IRecordObserver::VIN_CONFIG);
		vin_config_info_t *info = (vin_config_info_t*)raw->GetData();
		if (mDevice.GetVinConfig(&mDevice, info) == 0) {
			mpObserver->SetConfigData(raw);
		}
		raw->Release();
	}

	while (true) {

		// delayed action from AOCmdProc
		// if it is executed in AOCmdProc, it will cause 
		// AllocOutputBuffer() too slow, thus bits buffer corrupted
		if (mbStopped) {
			mbStopped = false;
			if (StopEncoding() != E_OK) {
				PostErrorMsg(E_IO);
			}
		}

		// process input subtitles
		while (true) {
			if (!ProcessSubtitle(bStopped, false)) {
				if (bStopped)
					goto Done;

				break;
			}
		}

		// fetch a new bits buffer
		bitsBuffer.size1 = bitsBuffer.size2 = 0;
		if (mDevice.ReadBits(&mDevice, &bitsBuffer) < 0) {
			AVF_LOGE("read bits failed");
			PostErrorMsg(E_FATAL);
			goto Done;
		}

#ifdef DEBUG_AVSYNC
		if (bitsBuffer.stream_id == 0) {
			tick_info_update(&m_tick_info, 60);
		}
#endif

		if (bitsBuffer.stream_id >= mNumStream) {
			AVF_LOGE("bad stream_id: %d", bitsBuffer.stream_id);
			PostErrorMsg(E_FATAL);
			goto Done;
		}

		// check size
		avf_u32_t total_size = bitsBuffer.size1 + bitsBuffer.size2;
		if ((bitsBuffer.pic_type == PIC_JPEG && total_size > MAX_FRAME_SIZE_JPEG) ||
				total_size > MAX_FRAME_SIZE_H264) {
			AVF_LOGE("bad bits buffer size, stream = %d, size = %d,%d, eos = %d", 
				bitsBuffer.stream_id, bitsBuffer.size1, bitsBuffer.size2, bitsBuffer.is_eos);
			avf_dump_mem(bitsBuffer.buffer1, 16);
			avf_dump_mem(bitsBuffer.buffer1 + bitsBuffer.size1 - 16, 16);
			PostErrorMsg(E_FATAL);
			goto Done;
		}

		CVideoSourceOutput *pPin = mpOutputs[bitsBuffer.stream_id];
		if (pPin == NULL || !pPin->Connected()) {
			continue;
		}

		if (pPin->GetEOS()) {
			continue;
		}

		if (bitsBuffer.is_eos) {

			AVF_LOGD(C_BROWN "stream %d EOS, used_mem: %d KB" C_NONE, pPin->GetPinIndex(),
				pPin->GetUsingBufferPool()->GetAllocatedMem() >> 10);

			if (!SendEOS(pPin))
				break;

			if (SendAllEOS())
				break;

			continue;

		}

//		if (bitsBuffer.pic_type == PIC_H264_IDR && bitsBuffer.stream_id == 0) {
//			AVF_LOGD("pts: " LLD, bitsBuffer.pts64);
//		}

		// handle segment
		if (bitsBuffer.pic_type == PIC_H264_IDR) {

			bool bNewSeg = false;

			if (pPin->mSegLength && bitsBuffer.pts64 >= pPin->m_next_seg_pts) {
				//pPin->m_next_seg_pts += 90000ULL * pPin->mSegLength;
				bNewSeg = true;
			} else if (pPin->GetPinIndex() == 0) {
				if (mpEngine->ReadRegBool(NAME_MUXER_LARGE_FILE, 0, 0)) {
					mpEngine->WriteRegBool(NAME_MUXER_LARGE_FILE, 0, 0);
					bNewSeg = true;
				}
			}

			if (bNewSeg) {
				pPin->m_next_seg_pts = bitsBuffer.pts64 + 90000ULL * pPin->mSegLength - 200;

				CBuffer *pBuffer;
				if (!AllocOutputBuffer(pPin, pBuffer, 0))
					break;

				pBuffer->SetNewSeg(bitsBuffer.pts64);
				pPin->PostBuffer(pBuffer);
			}

			bPoster = CalcPosters(&bitsBuffer);

			if (mb_debug_vmem && pPin->GetPinIndex() == 0) {
				raw_data_t *raw = avf_alloc_raw_data(sizeof(vmem_usage_s), IRecordObserver::VMEM_DATA);
				vmem_usage_s *vmem = (vmem_usage_s*)raw->GetData();

				vmem->max = m_mem_max >> 20;
				vmem->overrun = m_mem_overrun >> 20;
				vmem->overflow = m_mem_overflow >> 20;
				vmem->reserved = 0;
				vmem->curr_kb = pPin->GetUsingBufferPool()->GetAllocatedMem() >> 10;

				raw->ts_ms = bitsBuffer.pts64 / 90;
				mpObserver->AddRawData(raw, 0);
				raw->Release();
			}
		}

		avf_u32_t flags;

		switch (bitsBuffer.pic_type) {
		case PIC_H264_IDR:
			flags = CBuffer::F_KEYFRAME | CBuffer::F_AVC_IDR;
			break;

		case PIC_I:
			flags = CBuffer::F_KEYFRAME;
			break;

		case PIC_JPEG:
			flags = CBuffer::F_KEYFRAME | CBuffer::F_JPEG;
			break;

		default:
			flags = 0;
			break;
		}

		// only adjust H264
		if (bitsBuffer.pic_type == PIC_H264_IDR) {
#ifdef AVF_DEBUG
			if (pPin->mDTS + 100 < bitsBuffer.pts64 || pPin->mDTS > bitsBuffer.pts64 + 100) {
				AVF_LOGW("stream %d bad pts, prev idr: " LLD ", this: " LLD ", diff: " LLD,
					bitsBuffer.stream_id, pPin->m_last_pts_idr, bitsBuffer.pts64,
					pPin->m_last_pts_idr - bitsBuffer.pts64);
			}
#endif
			pPin->mDTS = bitsBuffer.pts64;
			pPin->m_last_pts_idr = bitsBuffer.pts64;
		}

		if (bitsBuffer.stream_id == 0) {
			avf_u32_t used_mem = pPin->GetUsingBufferPool()->GetAllocatedMem();
			if (used_mem >= m_mem_overflow && !bDiskErrorSent) {
				// todo: CRingMalloc::MAX_MEM is 80MB
				AVF_LOGW("used_mem: %d KB, send io error", used_mem >> 10);
				bDiskErrorSent = true;
				__IO_ERROR("");
			}

			if (used_mem >= m_mem_overrun && !bDiskSlowSent) {
				AVF_LOGW("used_mem: %d KB, send io slow", used_mem >> 10);
				bDiskSlowSent = true;
				__IO_SLOW("");
			}
		}

		CBuffer *pBuffer = NULL;

		// copy/set data
		if (pPin->GetBufferPoolFlags() & IBufferPool::F_HAS_MEMORY) {

			avf_size_t size = bitsBuffer.size1 + bitsBuffer.size2;
			if (!AllocOutputBuffer(pPin, pBuffer, size))
				break;

			CopyMem(pBuffer->mpData, bitsBuffer.buffer1, bitsBuffer.size1);
			if (bitsBuffer.size2 > 0) {
				CopyMem(pBuffer->mpData + bitsBuffer.size1, bitsBuffer.buffer2, bitsBuffer.size2);
			}

			pBuffer->mType = CBuffer::DATA;
			pBuffer->mFlags = flags;

			//pBuffer->mpData = bitsBuffer.buffer1;
			//pBuffer->mSize = size;
			pBuffer->m_offset = 0;
			pBuffer->m_data_size = size;

			pBuffer->m_dts = bitsBuffer.dts64;
			pBuffer->m_pts = bitsBuffer.pts64;
			pBuffer->m_duration = bitsBuffer.length;

		} else {

			if (!AllocOutputBuffer(pPin, pBuffer, bitsBuffer.size1))
				break;

			pBuffer->mType = CBuffer::DATA;
			pBuffer->mFlags = flags;

			pBuffer->mpData = bitsBuffer.buffer1;
			pBuffer->mSize = bitsBuffer.size1;
			pBuffer->m_offset = 0;
			pBuffer->m_data_size = bitsBuffer.size1;

			pBuffer->m_dts = bitsBuffer.dts64;
			pBuffer->m_pts = bitsBuffer.pts64;
			pBuffer->m_duration = bitsBuffer.length;

			if (bitsBuffer.buffer2) {
				//AVF_LOGD("Buffer wrap back");

				CBuffer *pBuffer2;
				if (!AllocOutputBuffer(pPin, pBuffer2, bitsBuffer.size2)) {
					pBuffer->Release();
					break;
				}

				pBuffer2->mType = CBuffer::DATA;
				pBuffer2->mFlags = flags;

				pBuffer2->mpData = bitsBuffer.buffer2;
				pBuffer2->mSize = bitsBuffer.size2;
				pBuffer2->m_offset = 0;
				pBuffer2->m_data_size = bitsBuffer.size2;

				pBuffer2->m_dts = bitsBuffer.dts64;
				pBuffer2->m_pts = bitsBuffer.pts64;
				pBuffer2->m_duration = bitsBuffer.length;

				pBuffer->mpNext = pBuffer2;
			}
		}

		// generate poster
		if (bitsBuffer.pic_type == PIC_JPEG) {
			if (mbSaveNextPicture) {
				mbSaveNextPicture = false;
				IAVIO *io = CBufferIO::Create();
				if (io) {
					io->CreateFile(mNextPictureFileName->string());
					io->Write(pBuffer->GetData(), pBuffer->GetDataSize());
					if (pBuffer->mpNext) {
						io->Write(pBuffer->mpNext->GetData(), pBuffer->mpNext->GetDataSize());
					}
					io->Release();
					PostFilterMsg(IEngine::MSG_DONE_WRITE_NEXT_PICTURE, 0);
				}
			}
			if (bPoster) {
				bPoster = false;
				SendPoster(pBuffer);
			}
		}

		// update 
		pPin->mDTS += bitsBuffer.length;
		pPin->PostBuffer(pBuffer);
	}

Done:
	StopEncoding();
}

bool CVideoSource::AOCmdProc(const CMD& cmd, bool bInAOLoop)
{
	avf_status_t status;

	switch (cmd.code) {
	case CMD_START_SOURCE:
		*(bool*)cmd.pExtra = false;
		ReplyCmd(E_OK);

		status = PrepareForEncoding();
		if (status != E_OK) {
			PostErrorMsg(status);
		} else {
			PostStartedMsg();
		}

		return true;

	case CMD_STOP_SOURCE:
		ReplyCmd(E_OK);
		mbStopped = true;
		return true;

	case CMD_SET_IMAGE_CONFIG: {
			name_value_t *param = (name_value_t*)cmd.pExtra;
			ReplyCmd(mDevice.SetImageControl(&mDevice, param->name, param->value) == 0 ? E_OK : E_ERROR);
		}
		return true;

	case CMD_ENABLE_PREVIEW:
		status = EnterPreview();
		ReplyCmd(status);
		return true;

	case CMD_DISABLE_PREVIEW:
		if (bInAOLoop) {
			AVF_LOGE("Cannot disable preview when encoding");
			ReplyCmd(E_PERM);
		} else {
			status = EnterIdle(false);
			ReplyCmd(status);
		}
		return true;

	case CMD_SAVE_NEXT_PICTURE:
		mNextPictureFileName = (const char*)cmd.pExtra;
		mbSaveNextPicture = true;
		ReplyCmd(E_OK);
		return true;

	case CMD_ChangeVoutVideo: {
			if (mDevice.ChangeVoutVideo == NULL) {
				AVF_LOGW("ChangeVoutVideo not supported");
				status = E_ERROR;
			} else {
				status = mDevice.ChangeVoutVideo(&mDevice, (const char*)cmd.pExtra) >= 0 ? E_OK : E_ERROR;
			}
			ReplyCmd(status);
		}
		return true;

	default:
		return inherited::AOCmdProc(cmd, bInAOLoop);
	}
}

// run in media control thread
avf_status_t CVideoSource::StillCapture(int action, int flags)
{
	if (mpStillCap == NULL) {
		AVF_LOGE("still capture not supported");
		return E_BADCALL;
	}
	return mpStillCap->StillCapture(action, flags);
}

avf_status_t CVideoSource::GetStillCaptureInfo(stillcap_info_s *info)
{
	if (mpStillCap == NULL) {
		AVF_LOGE("still capture not supported");
		return E_BADCALL;
	}
	return mpStillCap->GetStillCaptureInfo(info);
}

//-----------------------------------------------------------------------
//
//  CVideoSourceOutput
//
//-----------------------------------------------------------------------

CVideoSourceOutput::CVideoSourceOutput(CVideoSource *pFilter, avf_uint_t index):
	inherited(static_cast<CFilter*>(pFilter), index, "VideoSourceOutput"),
	mbOverlayEnabled(false),
	mbLogoEnabled(false),
	mSegLength(0),
	mPosterLength(0)
{
	mStreamIndex = index;
	mpBufferPool = NULL;
	mpDevice = &pFilter->mDevice;
}

CVideoSourceOutput::~CVideoSourceOutput()
{
	avf_safe_release(mpBufferPool);
}

CMediaFormat* CVideoSourceOutput::QueryMediaFormat()
{
	vsrc_stream_info_t streamInfo;

	if (!((CVideoSource*)mpFilter)->mbStreamConfigured) {
		streamInfo.format = VSRC_FMT_DISABLED;
	} else {
		if (mpDevice->GetStreamInfo(mpDevice, mStreamIndex, &streamInfo) < 0) {
			AVF_LOGE("Cannot get stream %d info", mStreamIndex);
			return NULL;
		}
	}

	switch (streamInfo.format) {
	case VSRC_FMT_DISABLED: {
			CMediaFormat *pFormat = avf_alloc_media_format(CMediaFormat);
			if (pFormat) {
				pFormat->mMediaType = MEDIA_TYPE_NULL;
				pFormat->mFormatType = MF_NullFormat;
				pFormat->mFrameDuration = 0;
				pFormat->mTimeScale = _90kHZ;
			}
			return pFormat;
		}

	case VSRC_FMT_H264: {
			CH264VideoFormat *pFormat;

			if (streamInfo.fcc == FCC_AMBA) {
				pFormat = (CH264VideoFormat*)__avf_alloc_media_format(sizeof(CAmbaAVCFormat));
			} else {
				pFormat = (CH264VideoFormat*)__avf_alloc_media_format(sizeof(CH264VideoFormat));
			}

			if (pFormat == NULL)
				return NULL;

			// CMediaFormat
			pFormat->mMediaType = MEDIA_TYPE_VIDEO;
			pFormat->mFormatType = MF_H264VideoFormat;
			pFormat->mFrameDuration = streamInfo.rate;
			pFormat->mTimeScale = streamInfo.scale;

			// CVideoFormat
			pFormat->mFrameRate = streamInfo.framerate;
			pFormat->mWidth = streamInfo.width;
			pFormat->mHeight = streamInfo.height;

			// CH264VideoFormat
			pFormat->mDataFourcc = streamInfo.fcc;

			// AMBA
			if (streamInfo.fcc == FCC_AMBA) {
				CAmbaAVCFormat *pAmbaFormat = (CAmbaAVCFormat*)pFormat;
				pAmbaFormat->data.info.ar_x = streamInfo.extra.ar_x;
				pAmbaFormat->data.info.ar_y = streamInfo.extra.ar_y;
				pAmbaFormat->data.info.mode = streamInfo.extra.mode;
				pAmbaFormat->data.info.M = streamInfo.extra.M;
				pAmbaFormat->data.info.N = streamInfo.extra.N;
				pAmbaFormat->data.info.gop_model = streamInfo.extra.gop_model;
				pAmbaFormat->data.info.idr_interval = streamInfo.extra.idr_interval;
				pAmbaFormat->data.info.rate = streamInfo.extra.rate;
				pAmbaFormat->data.info.scale = streamInfo.extra.scale;
				pAmbaFormat->data.info.bitrate = streamInfo.extra.bitrate;
				pAmbaFormat->data.info.bitrate_min = streamInfo.extra.bitrate_min;
				pAmbaFormat->data.info.idr_size = streamInfo.extra.idr_size;
				::memset(pAmbaFormat->data.reserved, 0, sizeof(pAmbaFormat->data.reserved));
			}

			return pFormat;
		}

	case VSRC_FMT_MJPEG: {
			CMJPEGVideoFormat *pFormat = avf_alloc_media_format(CMJPEGVideoFormat);
			if (pFormat) {
				// CMediaFormat
				pFormat->mMediaType = MEDIA_TYPE_VIDEO;
				pFormat->mFormatType = MF_MJPEGVideoFormat;
				pFormat->mFrameDuration = streamInfo.rate;
				pFormat->mTimeScale = streamInfo.scale;

				// CVideoFormat
				pFormat->mFrameRate = streamInfo.framerate;
				pFormat->mWidth = streamInfo.width;
				pFormat->mHeight = streamInfo.height;

				// Mjpeg
			}
			return pFormat;
		}

	default:
		AVF_LOGE("Unknown stream format %d", streamInfo.format);
		return NULL;
	}
}

IBufferPool* CVideoSourceOutput::QueryBufferPool()
{
	if (mpBufferPool == NULL) {
		IAllocMem *malloc = CRingMalloc::Create(mPinIndex, ((CVideoSource*)mpFilter)->m_mem_max);
		CDynamicBufferPool *pBufferPool = CDynamicBufferPool::CreateNonBlock("VideoSrcOut", malloc, 32);
		malloc->Release();
		mpBufferPool = static_cast<IBufferPool*>(pBufferPool);
	}

	mpBufferPool->AddRef();
	return mpBufferPool;
}

//-----------------------------------------------------------------------
//
//  CVideoSourceOutput
//
//-----------------------------------------------------------------------
CVideoSourceInput::CVideoSourceInput(CVideoSource *pFilter, avf_uint_t index):
	inherited(pFilter, index, "VideoSourceInput"),
	mpSource(pFilter)
{
}
	
CVideoSourceInput::~CVideoSourceInput()
{
}

avf_status_t CVideoSourceInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (pMediaFormat->mMediaType != MEDIA_TYPE_SUBTITLE)
		return E_ERROR;
	return E_OK;
}


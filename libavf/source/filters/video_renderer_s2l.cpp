
#define LOG_TAG "video_renderer"

#include <fcntl.h>
#include <sys/ioctl.h>

#include "avf_common.h"
#include "avf_new.h"
#include "avf_util.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avf_media_format.h"
#include "filter_if.h"

#include "video_renderer.h"

//-----------------------------------------------------------------------
//
//  CVideoRenderer
//
//-----------------------------------------------------------------------
avf_status_t CVideoRenderer::RecognizeFormat(CMediaFormat *pFormat)
{
	if (pFormat->mFormatType != MF_H264VideoFormat)
		return E_ERROR;

	CH264VideoFormat *pAVCFormat = (CH264VideoFormat*)pFormat;
	if (pAVCFormat->mDataFourcc != MKFCC('A','M','B','A')) {
		AVF_LOGD("not amba video");
		return E_ERROR;
	}

	return E_OK;
}

IFilter* CVideoRenderer::Create(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig)
{
	CVideoRenderer *result = avf_new CVideoRenderer(pEngine, pName, pFilterConfig);
	CHECK_STATUS(result);
	return result;
}

CVideoRenderer::CVideoRenderer(IEngine *pEngine, const char *pName, CConfigNode *pFilterConfig):
	inherited(pEngine, pName),
	mpInput(NULL),
	m_iav_fd(-1),
	mbFrameSeek(false)
{
}

CVideoRenderer::~CVideoRenderer()
{
}

avf_status_t CVideoRenderer::InitFilter()
{
	return inherited::InitFilter();
}

IPin* CVideoRenderer::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

bool CVideoRenderer::OnPauseRenderer()
{
	return true;
}

bool CVideoRenderer::WaitForResume()
{
	return true;
}

void CVideoRenderer::FilterLoop()
{
}

bool CVideoRenderer::WaitForStart()
{
	return true;
}

bool CVideoRenderer::WaitEOS(avf_u64_t eos_pts, bool *pbAudio)
{
	return false;
}

void CVideoRenderer::__SetPlaySpeed(avf_u32_t speed)
{
}

avf_u32_t CVideoRenderer::GetDisplayPts()
{
	return INVALID_PTS;
}

avf_u32_t CVideoRenderer::UpdatePlayPosition()
{
	return 0;
}

avf_status_t CVideoRenderer::StartDecoder()
{
	return E_OK;
}

avf_status_t CVideoRenderer::StopDecoder()
{
	return E_OK;
}

avf_status_t CVideoRenderer::WaitDecoderBuffer(avf_u32_t size)
{
	return E_OK;
}

void CVideoRenderer::FillGopHeader(avf_u32_t pts)
{
}

void CVideoRenderer::WriteBuffer(CBuffer *pBuffer)
{
}

void CVideoRenderer::CopyToBSB(const avf_u8_t *ptr, avf_size_t size)
{
}

avf_status_t CVideoRenderer::DecodeBuffer(avf_u8_t *prev_ptr, int num_frames, avf_u64_t start_pts)
{
	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CVideoRendererInput
//
//-----------------------------------------------------------------------
CVideoRendererInput::CVideoRendererInput(CVideoRenderer *pFilter):
	inherited(pFilter, 0, "VideoRendererInput"),
	mp_video_format(NULL)
{
}

CVideoRendererInput::~CVideoRendererInput()
{
}

avf_status_t CVideoRendererInput::AcceptMedia(CMediaFormat *pMediaFormat,
	IBufferPool *pUpStreamBufferPool, IBufferPool** ppBufferPool)
{
	if (CVideoRenderer::RecognizeFormat(pMediaFormat) != E_OK) {
		return E_ERROR;
	}

	mp_video_format = (CAmbaAVCFormat*)pMediaFormat;
	return E_OK;
}


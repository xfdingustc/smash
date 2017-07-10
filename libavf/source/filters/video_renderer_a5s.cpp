
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


#include <basetypes.h>
#include <ambas_common.h>
#include <ambas_vin.h>
#include <ambas_vout.h>
#include <iav_drv.h>


#include "video_renderer.h"

static iav_mmap_info_t g_bsb_map;

static int map_bsb(int iav_fd)
{
	int rval;

	if (g_bsb_map.addr != NULL)
		return 0;

	rval = ::ioctl(iav_fd, IAV_IOC_MAP_DECODE_BSB, &g_bsb_map);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_MAP_BSB");
		return rval;
	}

	return 0;
}

static int unmap_bsb(int iav_fd)
{
	if (g_bsb_map.addr == NULL)
		return 0;

	int rval = ::ioctl(iav_fd, IAV_IOC_UNMAP_DECODE_BSB, 0);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_UNMAP_BSB");
	}

	g_bsb_map.addr = NULL;
	return 0;
}

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
	mpInput = avf_new CVideoRendererInput(this);
	CHECK_STATUS(mpInput);
	CHECK_OBJ(mpInput);
	mbFrameSeek = mpEngine->ReadRegBool(NAME_PB_FRAME_SEEK, 0, VALUE_PB_FRAME_SEEK);
	m_last_pts = INVALID_PTS;
}

CVideoRenderer::~CVideoRenderer()
{
	avf_safe_release(mpInput);
	if (m_iav_fd > 0) {
		unmap_bsb(m_iav_fd);
		avf_close_file(m_iav_fd);
	}
}

avf_status_t CVideoRenderer::InitFilter()
{
	avf_status_t status = inherited::InitFilter();
	if (status != E_OK)
		return status;

	SetRTPriority(CThread::PRIO_RENDERER);

	m_iav_fd = avf_open_file("/dev/iav", O_RDWR, 0);
	if (m_iav_fd < 0) {
		AVF_LOGP("/dev/iav");
		return E_IO;
	}

	if (map_bsb(m_iav_fd) < 0) {
		return E_IO;
	}

	int rval = ::ioctl(m_iav_fd, IAV_IOC_ENTER_IDLE, 0);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_ENTER_IDLE");
		return E_IO;
	}

	return E_OK;
}

IPin* CVideoRenderer::GetInputPin(avf_uint_t index)
{
	return index == 0 ? mpInput : NULL;
}

bool CVideoRenderer::OnPauseRenderer()
{
	::ioctl(m_iav_fd, IAV_IOC_DECODE_PAUSE, 0);
	UpdatePlayPosition();
	return WaitForResume();
}

bool CVideoRenderer::WaitForResume()
{
	while (true) {
		if (!ProcessOneCmd(AVF_POLL_DELAY))
			return false;
		//UpdatePlayPosition();
		if (!mbPaused)
			break;
	}

	if (!SyncWithAudio())
		return false;

	::ioctl(m_iav_fd, IAV_IOC_DECODE_RESUME, 0);

	return true;
}

void CVideoRenderer::FilterLoop()
{
	static u8 eos[] = {0x00, 0x00, 0x00, 0x01, 0x0A};

	avf_u64_t from_pts = 0;
	avf_u64_t last_pts = 0;
	avf_status_t status;

	if ((status = StartDecoder()) != E_OK) {
		ReplyCmd(status);
		return;
	}

	mbPaused = true;
	mbStarting = true;
	ReplyCmd(E_OK);

	SetPlaySpeed(0x100/8);
	UpdatePlayPosition();

	mb_decode_started = false;
	m_written_bytes = 0;
	m_written_frames = 0;

	while (true) {
		CQueueInputPin *pInPin;
		CBuffer *pBuffer;

		UpdatePlayPosition();

		if (mbStarting) {
			// 1. if decode is started
			// 2. if input buffer is ready

			bool bWait = m_written_frames >= 60 || 
				m_written_bytes >= (m_bsb_size / 2);

			while (true) {

				while (true) {
					if (!WaitForStart()) {
						goto Done;
					}
					if (!mbStarting) {
						goto Buffer;
					}
					if (!bWait) {
						break;
					}
					if (!ProcessAnyCmds(AVF_POLL_DELAY))
						goto Done;
				}

				// need ReceiveInputBufferTimeout
				bool bStopped;
				if (PeekInputBuffer(pInPin, pBuffer, bStopped))
					break;

				if (bStopped)
					goto Done;

				if (!ProcessAnyCmds(AVF_POLL_DELAY))
					goto Done;
			}

		} else {
Buffer:
			if (ReceiveInputBuffer(pInPin, pBuffer) != E_OK)
				goto Done;
		}

		switch (pBuffer->mType) {
		case CBuffer::EOS:
			pBuffer->Release();

			while (mbStarting) {
				if (!WaitForStart())
					goto Done;
				if (!ProcessAnyCmds(AVF_POLL_DELAY))
					goto Done;
			}

			UpdatePlayPosition();

			status = WaitDecoderBuffer(sizeof eos);
			if (status == E_OK) {
				avf_u8_t *prev_ptr = m_bsb_ptr;
				CopyToBSB(eos, sizeof eos);
				if ((status = DecodeBuffer(prev_ptr, 0, 0)) == E_OK) {
					bool bAudio;
					if (WaitEOS(last_pts, &bAudio)) {
						if (!bAudio) {
							avf_u32_t eos_wait = mpEngine->ReadRegInt32(NAME_PB_EOS_WAIT, 0, 0);
							if (eos_wait > 0) {
								AVF_LOGD("video: sleep %d ms for EOS", eos_wait);
								if (!ProcessAnyCmds(eos_wait))
									goto Done;
							}
						}
						PostEosMsg();
					}
				}
			}
			goto Done;

		// frame accurate
		case CBuffer::START_DTS:
			last_pts = from_pts = pBuffer->m_dts;
			pBuffer->Release();
			AVF_LOGD("from_pts: %d", (avf_u32_t)from_pts);
			break;

		case CBuffer::DATA:
			m_written_frames++;
			last_pts = pBuffer->m_pts;
			status = WaitDecoderBuffer(pBuffer->GetDataSize() + GOP_HEADER_SIZE);
			if (status == E_OK) {
				avf_u8_t *prev_ptr = m_bsb_ptr;
				WriteBuffer(pBuffer);
				status = DecodeBuffer(prev_ptr, 1, from_pts - m_first_pts + m_start_pts);
			}

			pBuffer->Release();

			if (status != E_OK) {
				PostErrorMsg(status);
				goto Done;
			}

			break;

		default:
			pBuffer->Release();
			break;
		}
	}

Done:
	StopDecoder();
}

bool CVideoRenderer::WaitForStart()
{
	avf_u32_t pts = GetDisplayPts();
	if (pts < m_start_pts || pts >= m_start_pts + PTS_GAP) {
		// pts not changed, the renderer has not started. need feed more data
		return true;
	}

	::ioctl(m_iav_fd, IAV_IOC_DECODE_PAUSE, 0);

	AVF_LOGD("started, curr_pts: %d, start_pts: %d, last_pts: %d",
		pts, m_start_pts, m_last_pts);

	mbStarting = false;
	PostStartedMsg();

	// wait until start or stopped
	while (mbPaused) {
		if (!ProcessOneCmd())
			return false;
	}

	// sync with audio
	if (!SyncWithAudio())
		return false;

	SetPlaySpeed(0x100);
	::ioctl(m_iav_fd, IAV_IOC_DECODE_RESUME, 0);

	return true;
}

bool CVideoRenderer::WaitEOS(avf_u64_t eos_pts, bool *pbAudio)
{
	avf_u64_t tick_prev = -1;
	avf_u64_t tick_curr;
	avf_u64_t pts_prev = -1;
	avf_u64_t pts_curr;
	int state = 0;

	// if no audio, the value will be -1
	bool bHasAudio = mpEngine->ReadRegInt32(NAME_PB_AUDIO_POS, 0, -1) >= 0;
	*pbAudio = false;

	while (true) {
		pts_curr = UpdatePlayPosition();

		// 1. check if eos pts is reached
		if (pts_curr >= eos_pts) {
			AVF_LOGD("video eos reached, curr: " LLD ", target: " LLD, pts_curr, eos_pts);
			return true;
		}

		// 2. check if audio is stopped
		if (bHasAudio) {
			// if audio if not present, it will return 2
			if (mpEngine->ReadRegInt32(NAME_PB_AUDIO_STOPPED, 0, 2) == 1) {
				AVF_LOGD("stop video after audio");
				*pbAudio = true;
				return true;
			}
		} else {
			// 3. check if wait too long (if there's no audio)
			switch (state) {
			case 0:		// start
				pts_prev = pts_curr;
				state = 1;
				break;

			case 1:		// wait for pts no change
				if (pts_curr == pts_prev) {
					tick_prev = GetTickCount();
					state = 2;
				}
				break;

			case 2:
				tick_curr = GetTickCount();
				if (pts_curr == pts_prev) {
					if (tick_curr - tick_prev > 100) {
						AVF_LOGD("wait for video EOS timeout");
						return true;
					}
				} else {
					pts_prev = pts_curr;
					tick_prev = tick_curr;
				}
				break;
			}
		}

		if (!ProcessAnyCmds(AVF_POLL_DELAY))
			return false;
	}
}

void CVideoRenderer::__SetPlaySpeed(avf_u32_t speed)
{
	iav_trick_play_t param;
	param.speed = speed;
	param.scan_mode = 0;
	param.direction = 0;
	::ioctl(m_iav_fd, IAV_IOC_TRICK_PLAY, &param);
}

avf_u32_t CVideoRenderer::GetDisplayPts()
{
	iav_decode_info_t info = {0,0,0};
	int rval = ::ioctl(m_iav_fd, IAV_IOC_GET_DECODE_INFO, &info);
	return rval == 0 ? info.curr_pts : INVALID_PTS;
}

avf_u32_t CVideoRenderer::UpdatePlayPosition()
{
	avf_u32_t ms;
	avf_u32_t pts;

	if (mbStarting) {
		ms = m_start_ms;
		pts = m_first_pts;
	} else {
		pts = GetDisplayPts() - m_start_pts + m_first_pts;
		ms = (avf_u32_t)((avf_u64_t)pts * 1000 / mpInput->mUsingMediaFormat->mTimeScale);
	}

	mpEngine->WriteRegInt32(NAME_PB_VIDEO_POS, 0, ms);
	return pts;
}

avf_status_t CVideoRenderer::StartDecoder()
{
	int rval;

	if (mpInput->mp_video_format == NULL)
		return E_ERROR;

	rval = ::ioctl(m_iav_fd, IAV_IOC_START_DECODE, 0);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_START_DECODE");
		return E_IO;
	}

	iav_config_decoder_t config;
	config.flags = 0;
	config.decoder_type = IAV_DEFAULT_DECODER;
	config.pic_width = mpInput->mp_video_format->mWidth;
	config.pic_height = mpInput->mp_video_format->mHeight;

	rval = ::ioctl(m_iav_fd, IAV_IOC_CONFIG_DECODER, &config);
	if (rval < 0) {
		AVF_LOGP("IAV_IOC_CONFIG_DECODER");
		return E_DEVICE;
	}

	m_bsb_ptr = g_bsb_map.addr;
	m_bsb_end = g_bsb_map.addr + g_bsb_map.length;
	m_bsb_room = g_bsb_map.length;
	m_bsb_size = g_bsb_map.length;

	if (m_last_pts > 2*PTS_GAP) {
		m_start_pts = PTS_GAP;
	} else {
		m_start_pts = 3*PTS_GAP;
	}
	AVF_LOGD("StartDecoder, start_pts: %d, last_pts: %d", m_start_pts, m_last_pts);

	return E_OK;
}

avf_status_t CVideoRenderer::StopDecoder()
{
	if (mpEngine->ReadRegBool(NAME_PB_PAUSE, 0, VALUE_PB_PAUSE)) {
		// with picture on screen
		::ioctl(m_iav_fd, IAV_IOC_STOP_DECODE, 1);
	} else {
		// clear screen
		::ioctl(m_iav_fd, IAV_IOC_STOP_DECODE, 0);
	}

	UpdatePlayPosition();
	if (!mbStarting) {
		m_last_pts = GetDisplayPts();
		AVF_LOGD("StopDecoder, last_pts: %d", m_last_pts);
	}

	return E_OK;
}

avf_status_t CVideoRenderer::WaitDecoderBuffer(avf_u32_t size)
{
	if (size < m_bsb_room)
		return E_OK;

	while (true) {
		iav_wait_decoder_t wait;
		wait.emptiness.room = size;
		wait.emptiness.start_addr = m_bsb_ptr;
		wait.flags = IAV_WAIT_BSB;

		if (::ioctl(m_iav_fd, IAV_IOC_WAIT_DECODER, &wait) < 0) {
			AVF_LOGP("IAV_IOC_WAIT_DECODER");
			return E_DEVICE;
		}

		if (wait.flags == IAV_WAIT_BSB) {
			m_bsb_room = wait.emptiness.room;
			return E_OK;
		}
	}
}

void CVideoRenderer::FillGopHeader(avf_u32_t pts)
{
	if (!mb_decode_started) {
		mb_decode_started = true;
		m_first_pts = pts;
		pts = m_start_pts;
	} else {
		pts -= m_first_pts;
		pts += m_start_pts;
	}

	avf_u8_t buf[GOP_HEADER_SIZE];
	CAmbaAVCFormat *pFormat = mpInput->mp_video_format;

	avf_u32_t tick_high = AVF_HIGH(pFormat->data.info.rate);
	avf_u32_t tick_low = AVF_LOW(pFormat->data.info.rate);
	avf_u32_t scale_high = AVF_HIGH(pFormat->data.info.scale);
	avf_u32_t scale_low = AVF_LOW(pFormat->data.info.scale);
	avf_u32_t pts_high = AVF_HIGH(pts);
	avf_u32_t pts_low = AVF_LOW(pts);
	avf_u32_t M = pFormat->data.info.M;
	avf_u32_t N = pFormat->data.info.N;

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 1;

	buf[4] = 0x7a;
	buf[5] = 0x01;
	buf[6] = 0x01;

	buf[7] = tick_high >> 10;
	buf[8] = tick_high >> 2;
	buf[9] = (tick_high << 6) | (1 << 5) | (tick_low >> 11);
	buf[10] = tick_low >> 3;

	buf[11] = (tick_low << 5) | (1 << 4) | (scale_high >> 12);
	buf[12] = scale_high >> 4;
	buf[13] = (scale_high << 4) | (1 << 3) | (scale_low >> 13);
	buf[14] = scale_low >> 5;

	buf[15] = (scale_low << 3) | (1 << 2) | (pts_high >> 14);
	buf[16] = pts_high >> 6;

	buf[17] = (pts_high << 2) | (1 << 1) | (pts_low >> 15);
	buf[18] = pts_low >> 7;
	buf[19] = (pts_low << 1) | 1;

	buf[20] = N;
	buf[21] = (M << 4) & 0xF0;

	CopyToBSB(buf, GOP_HEADER_SIZE);
}

void CVideoRenderer::WriteBuffer(CBuffer *pBuffer)
{
	if (pBuffer->mFlags & CBuffer::F_AVC_IDR) {
		FillGopHeader(pBuffer->m_pts);
	}
	CopyToBSB(pBuffer->GetData(), pBuffer->GetDataSize());
}

void CVideoRenderer::CopyToBSB(const avf_u8_t *ptr, avf_size_t size)
{
	m_written_bytes += size;
	m_bsb_room -= size;

	if (m_bsb_ptr + size <= m_bsb_end) {
		::memcpy(m_bsb_ptr, ptr, size);
		m_bsb_ptr += size;
	} else {
		avf_size_t bsb_remain = m_bsb_end - m_bsb_ptr;
		::memcpy(m_bsb_ptr, ptr, bsb_remain);

		ptr += bsb_remain;
		size -= bsb_remain;

		::memcpy(g_bsb_map.addr, ptr, size);
		m_bsb_ptr = g_bsb_map.addr + size;
	}

	if (m_bsb_ptr == m_bsb_end)
		m_bsb_ptr = g_bsb_map.addr;
}

avf_status_t CVideoRenderer::DecodeBuffer(avf_u8_t *prev_ptr, int num_frames, avf_u64_t start_pts)
{
	iav_h264_decode_t decode;

	decode.start_addr = prev_ptr;
	decode.end_addr = m_bsb_ptr;
	decode.first_display_pts = start_pts;
	decode.num_pics = num_frames;
	decode.next_size = 0;
	decode.pic_width = 0;
	decode.pic_height = 0;

	if (::ioctl(m_iav_fd, IAV_IOC_DECODE_H264, &decode) < 0) {
		AVF_LOGP("IAV_IOC_DECODE_H264");
		return E_DEVICE;
	}

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


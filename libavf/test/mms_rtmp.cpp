
#define LOG_TAG	"rtmp"

#include "rtmp.h"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "mpeg_utils.h"

#include "mms_if.h"

//{6718F388-B516-4F85-8CA0-712A5D1726C2}
AVF_DEFINE_IID(IID_IMMRtmpStream,
0x6718F388, 0xB516, 0x4F85, 0x8C, 0xA0, 0x71, 0x2A, 0x5D, 0x17, 0x26, 0xC2);

//-----------------------------------------------------------------------
//
// CRtmpStream
//
//-----------------------------------------------------------------------
class CRtmpStream: public CObject, public IMMRtmpStream
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

public:
	static CRtmpStream* Create(IMMSource *pSource, const char *server_url);

private:
	CRtmpStream(IMMSource *pSource, const char *server_url);
	virtual ~CRtmpStream();

// IMMRtmpStream
public:
	virtual avf_status_t Start();
	virtual avf_status_t Stop();

private:
	static void ThreadEntry(void *p);
	void ThreadLoop();

private:
	bool Connect();
	bool SendStreamInfo();
	bool SendVideoFrame(IMMSource::FrameInfo& frameInfo);
	bool SendAudioFrame(IMMSource::FrameInfo& frameInfo);

private:
	void AllocPacket(RTMPPacket *packet, uint32_t packetType, uint32_t size, uint32_t timestamp);
	bool SendPacket(RTMPPacket *packet, uint32_t size);

	static uint8_t *put_string(uint8_t *p, const char *str);
	static uint8_t *put_double(uint8_t *p, double d);

	INLINE unsigned cal_nal_size(const uint8_t *pnal, const uint8_t *pnal_next, const uint8_t *pend) {
		return pnal_next >= pend ? pend - pnal : pnal_next - pnal - 3;
	}

	double get_video_framerate();

private:
	IMMSource *mpSource;
	char *mp_server_url;
	CThread *mpThread;
	RTMP *m_rtmp;

private:
	bool mb_stream_info_sent;
	bool mb_avc_info_sent;
	bool mb_aac_info_sent;
	bool mb_avc_valid;
	bool mb_aac_valid;
	avf_stream_attr_t m_stream_attr;
};

//-----------------------------------------------------------------------
//
// CRtmpStream
//
//-----------------------------------------------------------------------
CRtmpStream* CRtmpStream::Create(IMMSource *pSource, const char *server_url)
{
	CRtmpStream *result = avf_new CRtmpStream(pSource, server_url);
	CHECK_STATUS(result);
	return result;
}

void *CRtmpStream::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IMMRtmpStream)
		return static_cast<IMMRtmpStream*>(this);
	return inherited::GetInterface(refiid);
}

CRtmpStream::CRtmpStream(IMMSource *pSource, const char *server_url):
	mpSource(pSource),
	mp_server_url(NULL),
	mpThread(NULL),
	m_rtmp(NULL)
{
	mp_server_url = avf_strdup(server_url);
	m_rtmp = RTMP_Alloc();
	RTMP_Init(m_rtmp);
}

CRtmpStream::~CRtmpStream()
{
	avf_safe_free(mp_server_url);
	avf_safe_release(mpThread);
	if (m_rtmp) {
		RTMP_Close(m_rtmp);
		RTMP_Free(m_rtmp);
	}
}

avf_status_t CRtmpStream::Start()
{
	if (mpThread) {
		return E_OK;
	}

	if ((mpThread = CThread::Create("rtmp-stream", ThreadEntry, this)) == NULL) {
		return E_NOMEM;
	}

	return E_OK;
}

avf_status_t CRtmpStream::Stop()
{
	return E_OK;
}

void CRtmpStream::ThreadEntry(void *p)
{
	CRtmpStream *self = (CRtmpStream*)p;
	self->ThreadLoop();
}

void CRtmpStream::ThreadLoop()
{
	if (!Connect()) {
		return;
	}

	mb_stream_info_sent = false;
	mb_avc_info_sent = false;
	mb_aac_info_sent = false;
	mb_avc_valid = false;
	mb_aac_valid = false;
	::memset(&m_stream_attr, 0, sizeof(m_stream_attr));

	for (;;) {
		IMMSource::FrameInfo frameInfo;

		if (mpSource->Read(NULL, 0, &frameInfo) < 0) {
			AVF_LOGD("read source failed");
			return;
		}

		// AVF_LOGD("frame: %p, size: %d", frameInfo.pframe, frameInfo.frame_size);

		switch (frameInfo.data_type) {
		case VDB_VIDEO_FRAME:
			//AVF_LOGD("VDB_VIDEO_FRAME");
			if (!SendVideoFrame(frameInfo))
				return;
			break;

		case VDB_AUDIO_FRAME:
			//AVF_LOGD("VDB_AUDIO_FRAME");
			if (!SendAudioFrame(frameInfo))
				return;
			break;

		case VDB_STREAM_ATTR:
			//AVF_LOGD("VDB_STREAM_ATTR");
			if (frameInfo.frame_size >= sizeof(m_stream_attr)) {
				::memcpy(&m_stream_attr, frameInfo.pframe, sizeof(m_stream_attr));

				if (m_stream_attr.stream_version != 2) {
					AVF_LOGW("unknown stream version %d", m_stream_attr.stream_version);
				} else {
					// video
					if (m_stream_attr.video_version != 0 || m_stream_attr.video_coding != VideoCoding_AVC) {
						AVF_LOGW("unknown video, version: %d, coding: %d",
							m_stream_attr.video_version, m_stream_attr.video_coding);
					} else {
						mb_avc_valid = true;
					}

					// audio
					if (m_stream_attr.audio_version != 0 || m_stream_attr.audio_coding != AudioCoding_AAC) {
						AVF_LOGW("unknown audio, version: %d, coding: %d",
							m_stream_attr.audio_version, m_stream_attr.audio_coding);
					} else {
						mb_aac_valid = true;
					}

					if (!mb_stream_info_sent) {
						if (!SendStreamInfo())
							return;
						mb_stream_info_sent = true;
					}
				}
			} else {
				AVF_LOGW("bad VDB_STREAM_ATTR");
			}
			break;

		default:
			AVF_LOGI("skip frame, type %d, size %d", frameInfo.data_type, frameInfo.frame_size);
			break;
		}
	}
}

bool CRtmpStream::Connect()
{
	if (RTMP_SetupURL(m_rtmp, mp_server_url) < 0) {
		return false;
	}

	RTMP_EnableWrite(m_rtmp);

	AVF_LOGD("RTMP_Connect: %s...", mp_server_url);
	if (RTMP_Connect(m_rtmp, NULL) < 0) {
		return false;
	}

	AVF_LOGD("RTMP_ConnectStream: %s...", mp_server_url);
	if (RTMP_ConnectStream(m_rtmp, 0) < 0) {
		return false;
	}

	AVF_LOGD("connected to %s", mp_server_url);
	return true;
}

uint8_t *CRtmpStream::put_string(uint8_t *p, const char *str)
{
	int len = ::strlen(str);
	SAVE_BE_16(p, len);
	::memcpy(p, str, len);
	return p + len;
}

uint8_t *CRtmpStream::put_double(uint8_t *p, double d)
{
	*p++ = AMF_NUMBER;
	uint8_t *pd = (uint8_t*)&d;

	p[0] = pd[7];
	p[1] = pd[6];
	p[2] = pd[5];
	p[3] = pd[4];
	p[4] = pd[3];
	p[5] = pd[2];
	p[6] = pd[1];
	p[7] = pd[0];

	return p + 8;
}

void CRtmpStream::AllocPacket(RTMPPacket *packet, uint32_t packetType, uint32_t size, uint32_t timestamp)
{
	RTMPPacket_Reset(packet);
	RTMPPacket_Alloc(packet, size);

	packet->m_packetType = packetType;
	packet->m_nChannel = 0x04;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nTimeStamp = timestamp;
	packet->m_nInfoField2 = m_rtmp->m_stream_id;
	packet->m_nBodySize = size;
}

bool CRtmpStream::SendPacket(RTMPPacket *packet, uint32_t size)
{
	packet->m_nBodySize = size;
	bool rval = RTMP_SendPacket(m_rtmp, packet, 0);
	RTMPPacket_Free(packet);
	return rval;
}

double CRtmpStream::get_video_framerate()
{
	switch (m_stream_attr.video_framerate) {
	case FrameRate_12_5: return 12.5;
	case FrameRate_6_25: return 6.25;
	case FrameRate_23_976: return 23.976;
	case FrameRate_24: return 24;
	case FrameRate_25: return 25;
	case FrameRate_29_97: return 29.97;
	case FrameRate_30: return 30;
	case FrameRate_50: return 50;
	case FrameRate_59_94: return 59.94;
	case FrameRate_60: return 60;
	case FrameRate_120: return 120;
	case FrameRate_240: return 240;
	case FrameRate_20: return 20;
	case FrameRate_15: return 15;
	case FrameRate_14_985: return 14.985;
	default:
		AVF_LOGW("unknown framerate type %d", m_stream_attr.video_framerate);
		return 30;
	}
}

bool CRtmpStream::SendStreamInfo()
{
	RTMPPacket packet;

	AllocPacket(&packet, RTMP_PACKET_TYPE_INFO, 256, 0);
	uint8_t *p = (uint8_t*)packet.m_body;

	*p++ = AMF_STRING;
	p = put_string(p, "@setDataFrame");

	*p++ = AMF_STRING;
	p = put_string(p, "onMetaData");

	*p++ = AMF_OBJECT;
	p = put_string(p, "copyright");
	*p++ = AMF_STRING;
	p = put_string(p, "transee");

	p = put_string(p, "width");
	p = put_double(p, m_stream_attr.video_width);

	p = put_string(p, "height");
	p = put_double(p, m_stream_attr.video_height);

	p = put_string(p, "framerate");
	p = put_double(p, get_video_framerate());

	p = put_string(p, "videocodecid");
	p = put_double(p, 7);	// FLV_CODECID_H264

	p = put_string(p, "");
	*p++ = AMF_OBJECT_END;

	AVF_LOGD("video width %d, height %d", m_stream_attr.video_width, m_stream_attr.video_height);
	AVF_LOGD("send stream info total %d", p - (uint8_t*)packet.m_body);

	if (!SendPacket(&packet, p - (uint8_t*)packet.m_body))
		return false;

	return true;
}

bool CRtmpStream::SendVideoFrame(IMMSource::FrameInfo& frameInfo)
{
	RTMPPacket packet;

	const uint8_t *pend = frameInfo.pframe + frameInfo.frame_size;
	const uint8_t *pnal = avc_find_nal(frameInfo.pframe, pend);
	const uint8_t *pnal_first = pnal;
	const uint8_t *pnal_next;

	if (pnal >= pend) {
		AVF_LOGW("no nal");
		return true;
	}

	if (!mb_avc_info_sent) {

		if (m_stream_attr.stream_version == 0) {
			// not received yet
			AVF_LOGD("no stream attr");
			return true;
		}

		if (!mb_avc_valid) {
			return true;
		}

		const uint8_t *sps = NULL;
		int sps_len = 0;

		const uint8_t *pps = NULL;
		int pps_len = 0;

		// search for sps & pps
		for (;;) {
			int nal_type = pnal[0] & 0x1F;

			if (nal_type == 1 || nal_type == 5) {
				// non-IDR/IDR; ignore remaining
				pnal_next = pend;
			} else {
				pnal_next = avc_find_nal(pnal, pend);
			}

			if (nal_type == 7) {
				sps = pnal;
				sps_len = cal_nal_size(pnal, pnal_next, pend);
			} else if (nal_type == 8) {
				pps = pnal;
				pps_len = cal_nal_size(pnal, pnal_next, pend);
			}

			if (sps && pps)
				break;

			if (pnal_next >= pend)
				break;

			pnal = pnal_next;
		}

		if (sps == NULL || pps == NULL) {
			AVF_LOGW("sps or pps not found");
			return true;
		}

		AllocPacket(&packet, RTMP_PACKET_TYPE_VIDEO, 256, 0);
		uint8_t *p = (uint8_t*)packet.m_body;

		// VIDEODATA

		// FrameType UB[4]
		//		1: keyframe (for AVC, a seekable frame)
		//		2: inter frame (for AVC, a nonseekable frame)
		//		3: disposable inter frame (H.263 only)
		//		4: generated keyframe (reserved for server use only)
		//		5: video info/command frame
		// CodecID UB[4]
		//		1: JPEG (currently unused)
		//		2: Sorenson H.263
		//		3: Screen video
		//		4: On2 VP6
		//		5: On2 VP6 with alpha channel
		//		6: Screen video version 2
		//		7: AVC
		// VideoData : AVCVIDEOPACKET

		p[0] = 0x17;

		// AVCPacketType UI8
		//		0: AVC sequence header
		//		1: AVC NALU
		//		2: AVC end of sequence (lower level NALU sequence ender is not required or supported)
		p[1] = 0;

		// CompositionTime SI24
		// if AVCPacketType == 1
		//		Composition time offset
		// else
		//		0
		p[2] = 0;
		p[3] = 0;
		p[4] = 0;
		p += 5;

		// AVCDecoderConfigurationRecord
		p[0] = 0x01;	// configurationVersion
		p[1] = sps[1];	// AVCProfileIndication
		p[2] = sps[2];	// profile_compatibility
		p[3] = sps[3];	// AVCLevelIndication
		p[4] = 0xff;	// lengthSizeMinusOne: 3
		p += 5;

		// sps nums
		p[0] = 0xE1;	// & 0x1F
		p[1] = sps_len >> 8;
		p[2] = sps_len;
		p += 3;

		// sps data
		::memcpy(p, sps, sps_len);
		p += sps_len;

		// pps nums
		p[0] = 0x01;	// & 0x1F
		p[1] = pps_len >> 8;
		p[2] = pps_len;
		p += 3;

		// pps data
		::memcpy(p, pps, pps_len);
		p += pps_len;

		AVF_LOGD("send avc info, sps %d, pps %d, total %d", sps_len, pps_len, p - (uint8_t*)packet.m_body);

		if (!SendPacket(&packet, p - (uint8_t*)packet.m_body))
			return false;

		mb_avc_info_sent = true;
	}

	// send NALs
	pnal = pnal_first;
	for (;;) {
		int nal_type = pnal[0] & 0x1F;

		if (nal_type == 1 || nal_type == 5) {
			uint32_t nal_len = pend - pnal;

			AllocPacket(&packet, RTMP_PACKET_TYPE_VIDEO, nal_len + 9, frameInfo.timestamp_ms);
			uint8_t *p = (uint8_t*)packet.m_body;

			p[0] = (nal_type == 5) ? 0x17 : 0x27;	// FrameType, CodecID
			p[1] = 0x01;	// AVCPacketType
			p[2] = 0x00;	// CompositionTime ?
			p[3] = 0x00;
			p[4] = 0x00;
			p += 5;

			// NALU size
			p[0] = nal_len >> 24;
			p[1] = nal_len >> 16;
			p[2] = nal_len >> 8;
			p[3] = nal_len;
			p += 4;

			// NALU data
			::memcpy(p, pnal, nal_len);
			p += nal_len;

			//AVF_LOGD("send video frame, size: %d", p - buf);

			bool rval = SendPacket(&packet, nal_len + 9);
			return rval;
		}

		pnal_next = avc_find_nal(pnal, pend);
		if (pnal_next >= pend)
			break;

		pnal = pnal_next;
	}

	return true;
}

bool CRtmpStream::SendAudioFrame(IMMSource::FrameInfo& frameInfo)
{
	RTMPPacket packet;

	if (!mb_aac_info_sent) {

		if (m_stream_attr.stream_version == 0) {
			// not received yet
			AVF_LOGD("no stream attr");
			return true;
		}

		if (!mb_aac_valid) {
			return true;
		}

		AllocPacket(&packet, RTMP_PACKET_TYPE_AUDIO, 16, 0);
		uint8_t *p = (uint8_t*)packet.m_body;

		// AUDIODATA

		// SoundFormat	UB[4]
		//		0 = Linear PCM, platform endian
		//		1 = ADPCM
		//		2 = MP3
		//		3 = Linear PCM, little endian
		//		4 = Nellymoser 16-kHz mono
		//		5 = Nellymoser 8-kHz mono
		//		6 = Nellymoser
		//		7 = G.711 A-law logarithmic PCM
		//		8 = G.711 mu-law logarithmic PCM
		//		9 = reserved
		//		10 = AAC
		//		11 = Speex
		//		14 = MP3 8-Khz
		//		15 = Device-specific sound
		// SoundRate	UB[2]	For AAC: always 3
		//		0 = 5.5-kHz
		//		1 = 11-kHz
		//		2 = 22-kHz
		//		3 = 44-kHz
		// SoundSize	UB[1]
		//		0 = snd8Bit
		//		1 = snd16Bit
		// SoundType	UB[1]	For AAC: always 1
		//		0 = sndMono
		//		1 = sndStereo
		// SoundData: AACAUDIODATA

		p[0] = (10 << 4) | (3 << 2) | (1 << 1) | 1;

		// AACAUDIODATA
		//	AACPacketType UI8
		//		0: AAC sequence header
		//		1: AAC raw
		//	Data UI8[n]
		//		if AACPacketType == 0
		//			AudioSpecificConfig
		//		else if AACPacketType == 1
		//			Raw AAC frame data
		p[1] = 0;

		uint32_t audio_cfg = aac_gen_specific_config(
			m_stream_attr.audio_sampling_freq,
			m_stream_attr.audio_num_channels, 0);	// version should be 0
		p[2] = audio_cfg >> 8;
		p[3] = audio_cfg;
		p += 4;

		if (!SendPacket(&packet, p - (uint8_t*)packet.m_body))
			return false;

		mb_aac_info_sent = true;
	}

	aac_adts_header_t header;
	if (aac_parse_header(frameInfo.pframe, frameInfo.frame_size, &header) < 0) {
		AVF_LOGE("aac_parse_header failed");
		return true;
	}

	int header_size = 7 + 2 * !header.crc_absent;
	int frame_size = frameInfo.frame_size - header_size;

	AllocPacket(&packet, RTMP_PACKET_TYPE_AUDIO, 2 + frame_size, frameInfo.timestamp_ms);
	uint8_t *p = (uint8_t*)packet.m_body;

	p[0] = (10 << 4) | (3 << 2) | (1 << 1) | 1;
	p[1] = 1;
	p += 2;

	::memcpy(p, frameInfo.pframe + header_size, frame_size);
	p += frame_size;

	if (!SendPacket(&packet, p - (uint8_t*)packet.m_body))
		return false;

	return true;
}

//-----------------------------------------------------------------------

extern "C" IMMRtmpStream* mms_create_rtmp_stream(IMMSource *pSource, const char *server_url)
{
	return CRtmpStream::Create(pSource, server_url);
}


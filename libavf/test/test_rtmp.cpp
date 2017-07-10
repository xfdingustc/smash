
#define LOG_TAG	"rtmp"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avf_util.h"
#include "mpeg_utils.h"

#include "filemap.h"

#include <getopt.h>
#include "test_common.cpp"

#include "rtmp.h"

typedef struct AVC_Info_s
{
	uint32_t	width;
	uint32_t	height;
	uint32_t	framerate;
	uint32_t	videoDataRate;

	uint32_t	sps_len;
	uint8_t		sps[256];

	uint32_t	pps_len;
	uint32_t	pps[256];
} AVC_Info_t;

typedef struct AAC_Info_s
{
	uint32_t	audioSampleRate;
	uint32_t	audioSampleSize;
	uint32_t	audioChannels;
	uint32_t	audioSpecCfg;
	uint32_t	audioSpecCfgLen;
} AAC_Info_t;

class CRTMPStream
{
public:
	CRTMPStream();
	~CRTMPStream();

public:
	bool Connect(const char *url);
	void Close();
	bool SendH264File(const char *pFileName, int width, int height);

private:
	bool SendAVCInfo(AVC_Info_t *info);
	bool SendAACInfo(AAC_Info_t *info);
	bool SendNal(const uint8_t *ptr, unsigned size, unsigned timestamp);
	int SendPacket(uint32_t packetType, uint8_t *data, uint32_t size, uint32_t timestamp);

private:
	RTMP *m_rtmp;
	CFileMap *mpFileMap;
	uint8_t *mpFilePtr;
};

CRTMPStream::CRTMPStream():
	m_rtmp(NULL),
	mpFileMap(NULL),
	mpFilePtr(NULL)
{
	m_rtmp = RTMP_Alloc();
	RTMP_Init(m_rtmp);
	mpFileMap = CFileMap::Create();
}

CRTMPStream::~CRTMPStream()
{
	Close();
}

bool CRTMPStream::Connect(const char *url)
{
	if (RTMP_SetupURL(m_rtmp, (char*)url) < 0) {
		return false;
	}

	RTMP_EnableWrite(m_rtmp);

	if (RTMP_Connect(m_rtmp, NULL) < 0) {
		return false;
	}

	if (RTMP_ConnectStream(m_rtmp, 0) < 0) {
		return false;
	}

	return true;
}

void CRTMPStream::Close()
{
	if (m_rtmp) {
		RTMP_Close(m_rtmp);
		RTMP_Free(m_rtmp);
		m_rtmp = NULL;
	}
	avf_safe_release(mpFileMap);
}

int CRTMPStream::SendPacket(uint32_t packetType, uint8_t *data, uint32_t size, uint32_t timestamp)
{
	RTMPPacket packet;
	RTMPPacket_Reset(&packet);
	RTMPPacket_Alloc(&packet, size);

	packet.m_packetType = packetType;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nTimeStamp = timestamp;
	packet.m_nInfoField2 = m_rtmp->m_stream_id;
	packet.m_nBodySize = size;
	memcpy(packet.m_body, data, size);

	int rval = RTMP_SendPacket(m_rtmp, &packet, 0);

	RTMPPacket_Free(&packet);

	return rval;
}

uint8_t *put_amf_string(uint8_t *p, const char *str)
{
	int len = strlen(str);
	SAVE_BE_16(p, len);
	memcpy(p, str, len);
	return p + len;
}

uint8_t *put_amf_double(uint8_t *p, double d)
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

bool CRTMPStream::SendAVCInfo(AVC_Info_t *info)
{
	uint8_t body[1024] = {0};
	uint8_t *p = body;

	*p++ = AMF_STRING;
	p = put_amf_string(p, "@setDataFrame");

	*p++ = AMF_STRING;
	p = put_amf_string(p, "onMetaData");

	*p++ = AMF_OBJECT;
	p = put_amf_string(p, "copyright");
	*p++ = AMF_STRING;
	p = put_amf_string(p, "transee");

	p = put_amf_string(p, "width");
	p = put_amf_double(p, info->width);

	p = put_amf_string(p, "height");
	p = put_amf_double(p, info->height);

	p = put_amf_string(p, "framerate");
	p = put_amf_double(p, info->framerate);

	p = put_amf_string(p, "videocodecid");
	p = put_amf_double(p, 7);	// FLV_CODECID_H264

	p = put_amf_string(p, "");
	*p++ = AMF_OBJECT_END;

	SendPacket(RTMP_PACKET_TYPE_INFO, (uint8_t*)body, p - body, 0);

	p = body;
	p[0] = 0x17;	// 1: keyframe; 7: AVC
	p[1] = 0x00;	// AVC sequence header
	p += 2;

	p[0] = 0;
	p[1] = 0;
	p[2] = 0;	// 
	p += 3;

	// AVCDecoderConfigurationRecord
	p[0] = 0x01;	// configurationVersion
	p[1] = info->sps[1];	// AVCProfileIndication
	p[2] = info->sps[2];	// profile_compatibility
	p[3] = info->sps[3];	// AVCLevelIndication
	p[4] = 0xff;	// lengthSizeMinusOne: 3
	p += 5;

	// sps nums
	p[0] = 0xE1;	// & 0x1F
	// sps length
	p[1] = info->sps_len >> 8;
	p[2] = info->sps_len;
	p += 3;

	// sps data
	memcpy(p, info->sps, info->sps_len);
	p += info->sps_len;

	// pps nums
	p[0] = 0x01;	// & 0x1F
	p[1] = info->pps_len >> 8;
	p[2] = info->pps_len;
	p += 3;

	// pps data
	memcpy(p, info->pps, info->pps_len);
	p += info->pps_len;

	return SendPacket(RTMP_PACKET_TYPE_VIDEO, (uint8_t*)body, p - body, 0);
}

bool CRTMPStream::SendAACInfo(AAC_Info_t *info)
{
	return false;
}

bool CRTMPStream::SendH264File(const char *pFileName, int width, int height)
{
	if (mpFileMap->OpenFileRead(pFileName) != E_OK) {
		return false;
	}

	const uint8_t *ptr = mpFileMap->Map(0, (uint32_t)mpFileMap->GetFileSize());
	if (ptr == NULL) {
		return false;
	}

	AVC_Info_t avc;
	::memset(&avc, 0, sizeof(avc));

	// find sps & pps
	const uint8_t *p = ptr;
	const uint8_t *pend = ptr + (uint32_t)mpFileMap->GetFileSize();
	const uint8_t *pnal = avc_find_nal(p, pend);
	const uint8_t *pfirst = pnal;
	if (pnal >= pend) {
		AVF_LOGE("cannot find nal");
		return false;
	}

	bool b_done = false;
	const uint8_t *pnext;
	for (;;) {
		pnext = avc_find_nal(pnal, pend);
		if (pnext >= pend)
			break;

		int nal_type = pnal[0] & 0x1F;
		if (nal_type == 7) {
			if (avc.sps_len == 0) {
				int len = pnext - pnal - 3;
				if (len > (int)sizeof(avc.sps)) {
					AVF_LOGE("size of sps too larg: %d", len);
					return false;
				}
				AVF_LOGI("found sps, %d bytes", len);
				avc.sps_len = len;
				memcpy(avc.sps, pnal, len);
			}
		} else if (nal_type == 8) {
			if (avc.pps_len == 0) {
				int len = pnext - pnal - 3;
				if (len > (int)sizeof(avc.pps)) {
					AVF_LOGE("size of pps too larg: %d", len);
					return false;
				}
				AVF_LOGI("found pps, %d bytes", len);
				avc.pps_len = len;
				memcpy(avc.pps, pnal, len);
			}
		}

		if (avc.sps_len > 0 && avc.pps_len > 0) {
			b_done = true;
			break;
		}

		pnal = pnext;
	}

	if (!b_done) {
		AVF_LOGE("sps or pps not found");
		return false;
	}

	avc.width = width;
	avc.height = height;
	avc.framerate = 25;

	SendAVCInfo(&avc);

	unsigned tick = 0;

	for (;;) {
		pnal = pfirst;
		for (;;) {
			pnext = avc_find_nal(pnal, pend);
			if (pnext >= pend)
				break;

			if (SendNal(pnal, pnext - pnal - 3, tick)) {
				//avf_msleep(40);
				tick += 40;
			}

			pnal = pnext;
		}

		SendNal(pnal, pend - pnal, tick);
		AVF_LOGI("loop");
	}

	return true;
}

bool CRTMPStream::SendNal(const uint8_t *ptr, unsigned size, unsigned timestamp)
{
	int nal_type = ptr[0] & 0x1F;

	if (nal_type != 1 && nal_type != 5) {
		AVF_LOGD("skip nal type %d, size %d", nal_type, size);
		return false;
	}

	uint8_t *body = new uint8_t[size + 9];
	uint8_t *p = body;

	if (nal_type == 5) {
		p[0] = 0x17;	// 1: Iframe; 7: AVC
	} else {
		p[0] = 0x27;	// 2: Pframe; 7: AVC
	}
	p[1] = 0x01;
	p[2] = 0x00;
	p[3] = 0x00;
	p[4] = 0x00;
	p += 5;

	// NALU size
	p[0] = size >> 24;
	p[1] = size >> 16;
	p[2] = size >> 8;
	p[3] = size;
	p += 4;

	// NALU data
	memcpy(p, ptr, size);
	p += size;

	bool rval = SendPacket(RTMP_PACKET_TYPE_VIDEO, body, size + 9, timestamp);
	AVF_LOGD("sent nal type %d, size %d", nal_type, size);

	delete[] body;

	return rval;
}

static char g_server_url[512] = "rtmp://90.0.0.51/live";
static char g_h264_file[512] = "/usr/local/bin/aa.264";

static int g_video_width = 1920;
static int g_video_height = 1080;

static struct option long_options[] = {
	{"url",		HAS_ARG,	0,	'u'},
	{"file",	HAS_ARG,	0,	'f'},
	{"width",	HAS_ARG,	0,	'w'},
	{"height",	HAS_ARG,	0,	'h'},
	{NULL,	NO_ARG, 0,	0}
};
static const char *short_options = "f:u:w:h:";

static int init_param(int argc, char **argv)
{
	int ch;
	int option_index = 0;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'f':
			strcpy(g_h264_file, optarg);
			break;

		case 'u':
			strcpy(g_server_url, optarg);
			break;

		case 'w':
			g_video_width = atoi(optarg);
			break;

		case 'h':
			g_video_height = atoi(optarg);
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (init_param(argc, argv) < 0)
		return -1;

	CRTMPStream rtmp;

	AVF_LOGI("connecting to %s...", g_server_url);
	if (!rtmp.Connect(g_server_url)) {
		AVF_LOGE("failed");
		return -1;
	}
	AVF_LOGI("connected to %s", g_server_url);

	rtmp.SendH264File(g_h264_file, g_video_width, g_video_height);

	rtmp.Close();

	return 0;
}



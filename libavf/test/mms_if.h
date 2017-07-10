
#ifndef __MM_IF_H__
#define __MM_IF_H__

extern const avf_iid_t IID_IMMSource;
extern const avf_iid_t IID_IMMRtspServer;
extern const avf_iid_t IID_IMMRtmpStream;

class IMMSource;
class IMMRtspServer;
class IMMRtmpStream;

//-----------------------------------------------------------------------
//
// IMMSource
//
//-----------------------------------------------------------------------
class IMMSource: public IInterface
{
public:
	DECLARE_IF(IMMSource, IID_IMMSource);

public:
	struct FrameInfo {
		uint64_t timestamp_ms;
		uint32_t duration_ms;
		uint8_t new_frame;
		uint8_t data_type;	// avf_std_media.h
		const uint8_t *pframe;
		uint32_t frame_size;
	};

	enum {
		MODE_TS = 0,
		MODE_FRAME = 1,
	};

public:
	virtual int GetFileNo() = 0;
	virtual int Read(uint8_t *buf, uint32_t size, FrameInfo *pFrameInfo) = 0;
	virtual void Close() = 0;
	virtual avf_status_t UpdateWrite(int write_index, uint32_t write_pos, uint32_t framecount) = 0;
};

//-----------------------------------------------------------------------
//
// IMMRtspServer
//
//-----------------------------------------------------------------------
class IMMRtspServer: public IInterface
{
public:
	DECLARE_IF(IMMRtspServer, IID_IMMRtspServer);

public:
	virtual avf_status_t Start() = 0;
	virtual avf_status_t Stop() = 0;
};

//-----------------------------------------------------------------------
//
// IMMRtmpStream
//
//-----------------------------------------------------------------------
class IMMRtmpStream: public IInterface
{
public:
	DECLARE_IF(IMMRtmpStream, IID_IMMRtmpStream);

public:
	virtual avf_status_t Start() = 0;
	virtual avf_status_t Stop() = 0;
};


//-----------------------------------------------------------------------

extern "C" IMMSource* mms_create_source(const char *path, int mode);
extern "C" IMMRtspServer* mms_create_rtsp_server(IMMSource *pSource);
extern "C" IMMRtmpStream* mms_create_rtmp_stream(IMMSource *pSource, const char *server_url);


#endif


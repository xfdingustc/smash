
#ifndef __AVF_CAMERA_IF_H__
#define __AVF_CAMERA_IF_H__

class IMediaControl;
class IVideoControl;

#include "filter_if.h"

//-----------------------------------------------------------------------
//
//  Camera - please do not subclass it
//
//-----------------------------------------------------------------------
class Camera
{
public:
	Camera(IMediaControl *pMediaControl) {
		mpMediaControl = pMediaControl;
	}
	~Camera() {}

private:
	IMediaControl *mpMediaControl;

public:
	enum {
		CMD_SetImageControl,
		CMD_EnablePreview,
		CMD_DisablePreview,
		CMD_SaveCurrent,
		CMD_SaveNextPicture,
		CMD_StillCapture,
		CMD_GetStillCaptureInfo,
		CMD_ChangeVoutVideo,
	};

	struct save_current_t {
		avf_size_t before;
		avf_size_t after;
		const char *filename;
	};

	struct still_capture_s {
		int action;
		int flags;
	};

public:
	avf_status_t SetImageControl(const char *name, const char *value) {
		name_value_t param = {name, value};
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_SetImageControl, (avf_intptr_t)&param);
	}
	avf_status_t EnablePreview() {
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_EnablePreview, 0);
	}
	avf_status_t DisablePreview() {
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_DisablePreview, 0);
	}
	avf_status_t SaveCurrent(avf_size_t before, avf_size_t after, const char *filename) {
		save_current_t param = {before, after, filename}; 
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_SaveCurrent, (avf_intptr_t)&param);
	}
	avf_status_t SaveNextPicture(const char *filename) {
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_SaveNextPicture, (avf_intptr_t)filename);
	}
	avf_status_t StillCapture(int action, int flags) {
		still_capture_s param = {action, flags};
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_StillCapture, (avf_intptr_t)&param);
	}
	avf_status_t GetStillCaptureInfo(IVideoControl::stillcap_info_s *info) {
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_GetStillCaptureInfo, (avf_intptr_t)info);
	}
	avf_status_t ChangeVoutVideo(const char *config) {
		return mpMediaControl->Command(CMP_ID_CAMERA, CMD_ChangeVoutVideo, (avf_intptr_t)config);
	}
};

#endif


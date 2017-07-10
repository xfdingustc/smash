/*****************************************************************************
 * ccam_domain_record.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2014, linnsong.
 *
 *
 *****************************************************************************/

#ifndef __H_ccam_domain_record__
#define __H_ccam_domain_record__

#include "ClinuxThread.h"
#include "sd_general_description.h"
#include "class_TCPService.h"
#include "ccam_cmds.h"

//#include "class_camera_property.h"
//#include "class_camera_property.h"
#include "class_ipc_rec.h"
//#include "upnp_sdcc.h"

enum RecProps
{
    RecProp_resolution = 0,
    RecProp_quality,
    RecProp_colormode,
    RecProp_recMode,
    RecProps_num,
};

SCMD_DOMIAN_CLASS(DomainRec)

SCMD_CMD_CLASS(StartRecord1, CMD_Rec_Start)
SCMD_CMD_CLASS(StopRecord1, CMD_Rec_Stop)

SCMD_CMD_CLASS(ListResolution, CMD_Rec_List_Resolutions)
SCMD_CMD_CLASS(GetResolution, CMD_Rec_get_Resolution)
SCMD_CMD_CLASS(SetResolution, CMD_Rec_Set_Resolution)

SCMD_CMD_CLASS(ListQuality, CMD_Rec_List_Qualities)
SCMD_CMD_CLASS(SetQuality, CMD_Rec_Set_Quality)
SCMD_CMD_CLASS(GetQuality, CMD_Rec_get_Quality)

SCMD_CMD_CLASS(ListRecMode, CMD_Rec_List_RecModes)
SCMD_CMD_CLASS(SetRecMode, CMD_Rec_Set_RecMode)
SCMD_CMD_CLASS(GetRecMode, CMD_Rec_get_RecMode)

SCMD_CMD_CLASS(ListColorMode, CMD_Rec_List_ColorModes)
SCMD_CMD_CLASS(SetColorMode, CMD_Rec_Set_ColorMode)
SCMD_CMD_CLASS(GetColorMode, CMD_Rec_get_ColorMode)

SCMD_CMD_CLASS(SetOverlay, CMD_Rec_setOverlay)
SCMD_CMD_CLASS(GetOverlay, CMD_Rec_getOverlayState)

SCMD_CMD_CLASS(SetDualStream, CMD_Rec_setDualStream)
SCMD_CMD_CLASS(GetDualStreamState, CMD_Rec_getDualStreamState)

SCMD_CMD_CLASS(GetRotateMode, CMD_Rec_Get_Rotate_Mode)
SCMD_CMD_CLASS(SetRotateMode, CMD_Rec_Set_Rotate_Mode)
SCMD_CMD_CLASS(MarkLiveVideo, CMD_Rec_Mark_Live_Video)
SCMD_CMD_CLASS(SetMarkTime, CMD_Rec_Set_Mark_Time)
SCMD_CMD_CLASS(GetMarkTime, CMD_Rec_Get_Mark_Time)

SCMD_CMD_CLASS(GetVideoQuality, CMD_Rec_Get_Video_Quality)
SCMD_CMD_CLASS(SetVideoQuality, CMD_Rec_Set_Video_Quality)

SCMD_CMD_CLASS(SetStillMode, CMD_Rec_SetStillMode)
SCMD_CMD_CLASS(StartStillCapture, CMD_Rec_StartStillCapture)
SCMD_CMD_CLASS(StopStillCapture, CMD_Rec_StopStillCapture)

#endif


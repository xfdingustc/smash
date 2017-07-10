/*****************************************************************************
 * sd_general_cmds.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 - , linnsong.
 *
 *
 *****************************************************************************/
#ifndef __H_sd_general_cmds__
#define __H_sd_general_cmds__

enum SD_General_CMDs
{
	SD_General_CMDs_None = 0,
	SD_General_CMDs_StartControl,
	SD_General_CMDs_StopControl,
	SD_General_CMDs_GetStatus,
	SD_General_CMDs_P2PConnect,
	SD_General_CMDs_StartStream,
	SD_General_CMDs_StopStream,
	SD_General_CMDs_Num,
};

enum SD_General_Events
{
	SD_General_Events_None = 0,
	SD_General_Events_Cmd_Return,
	SD_General_Events_CameraStatus,
	SD_General_Events_Num,
};

//static char* cmd_start_p2p = "startP2P";


#endif

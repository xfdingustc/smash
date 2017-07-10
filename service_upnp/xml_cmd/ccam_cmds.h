/*****************************************************************************
 * ccam_cmds.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 - , linnsong.
 *
 *
 *****************************************************************************/
#ifndef __H_ccam_cmds__
#define __H_ccam_cmds__
//#include "sd_general_description.h"

enum CMD_Domain
{
	CMD_Domain_cam = 0,
	CMD_Domain_p2p,
	CMD_Domain_rec,
	CMD_Domain_decode,
	CMD_Domain_network,
	CMD_Domain_power,
	CMD_Domain_storage,
	CMD_Domain_stream,
	CMD_Domain_MotorControl,
	CMD_Domain_num,
};
#define DomainCamera camera
#define DomainRec record
#define DomainDec decode
#define DomainNet network
#define DomainPower power
#define DomainStorage storage
#define DomainStream stream
#define DomainMotoControl MotorControl

enum CMD_Cam
{
	CMD_Cam_getMode				= 0,
	CMD_Cam_getMode_result		= 1,
	CMD_Cam_getAPIVersion		= 2,
	CMD_Cam_isAPISupported		= 3,
	CMD_Cam_get_Name			= 4,
	CMD_Cam_get_Name_result		= 5,
	CMD_Cam_set_Name			= 6,
	CMD_Cam_set_Name_result		= 7,
	CMD_Cam_get_State			= 8,
	CMD_Cam_get_State_result	= 9,
	CMD_Cam_start_rec			= 10,
	CMD_Cam_stop_rec			= 11,
	CMD_Cam_get_time			= 12,
	CMD_Cam_get_time_result		= 13,

	CMD_Cam_get_getAllInfor		= 14,
	CMD_Cam_get_getStorageInfor	= 15,
	CMD_Cam_msg_Storage_infor	= 16,	// enum State_storage
	CMD_Cam_msg_StorageSpace_infor		= 17,	//all, free
	CMD_Cam_msg_Battery_infor	= 18,	// votage(int mv),  int percentage
	CMD_Cam_msg_power_infor		= 19,	//(char* charging.. full), (int online 0, 1)
	CMD_Cam_msg_BT_infor		= 20,	//
	CMD_Cam_msg_GPS_infor		= 21,
	CMD_Cam_msg_Internet_infor	= 22,
	CMD_Cam_msg_Mic_infor		= 23,	//enum State_Mic , gain (int)
	CMD_Cam_set_StreamSize		= 24,

	CMD_Cam_PowerOff			= 25,
	CMD_Cam_Reboot				= 26,

	CMD_Network_GetWLanMode		= 27,
	CMD_Network_GetHostNum		= 28,
	CMD_Network_GetHostInfor	= 29,
	CMD_Network_AddHost			= 30,
	CMD_Network_RmvHost			= 31,
	CMD_Network_ConnectHost		= 32,

	CMD_Network_Synctime		= 33,
	CMD_Network_GetDevicetime	= 34,

	CMD_Rec_error				= 35,

	CMD_audio_setMic			= 36,
	CMD_audio_getMicState		= 37,

	CMD_fw_getVersion			= 38,
	CMD_fw_newVersion			= 39,
	CMD_fw_doUpgrade			= 40,
	//1.1

	CMD_Cam_num,
};

enum Rec_Error_code
{
	Error_code_none = 0,
	Error_code_notfcard,
	Error_code_tfFull,
	Error_code_tfError,
};

enum State_Cam_Mode
{
	State_Cam_Mode_idle = 0,
	State_Cam_Mode_Record,
	State_Cam_Mode_Decode,
	State_Cam_Mode_Duplex,
	State_Cam_Mode_Num,
};

enum State_Rec
{
	State_Rec_stoped = 0,
	State_Rec_stopping,
	State_Rec_starting,
	State_Rec_recording,
	//State_Rec_close,
	//State_Rec_open,
	State_Rec_Num,
};

enum State_storage
{
	State_storage_noStorage = 0,
	State_storage_loading,
	State_storage_ready,
	State_storage_error,
	State_storage_usbdisc,
	State_storage_num,
};

enum State_power
{
	State_battery_nobattery = 0,
	State_battery_battery,
	State_storage_charging,
};

enum State_bluetooth
{
	State_bluetooth_diconnect = 0,
	State_bluetooth_paring,
	State_bluetooth_connected,
};

enum State_GPS
{
	State_GPS_on = 0,
	State_GPS_ready,
	State_GPS_off,
};

enum State_internet
{
	State_internet_nointernet = 0,
	State_internet_connecting,
	State_internet_connected,
	State_internet_tracing,
	State_internet_tracingOff,
};

enum State_Mic
{
	State_Mic_ON = 0,
	State_Mic_MUTE,
};

enum CMD_Rec
{
	CMD_Rec_Start				= 0,
	CMD_Rec_Stop				= 1,
	CMD_Rec_List_Resolutions	= 2,
	CMD_Rec_Set_Resolution		= 3,
	CMD_Rec_get_Resolution		= 4,
	CMD_Rec_List_Qualities		= 5,
	CMD_Rec_Set_Quality			= 6,
	CMD_Rec_get_Quality			= 7,
	CMD_Rec_List_RecModes		= 8,
	CMD_Rec_Set_RecMode			= 9,
	CMD_Rec_get_RecMode			= 10,
	CMD_Rec_List_ColorModes		= 11,
	CMD_Rec_Set_ColorMode		= 12,
	CMD_Rec_get_ColorMode		= 13,
	CMD_Rec_List_SegLens		= 14,
	CMD_Rec_Set_SegLen			= 15,
	CMD_Rec_get_SegLen			= 16,
	CMD_Rec_get_State			= 17,
	EVT_Rec_state_change		= 18,
	CMD_Rec_getTime				= 19,
	CMD_Rec_getTime_result		= 20,

	CMD_Rec_setDualStream		= 21,
	CMD_Rec_getDualStreamState	= 22,

	CMD_Rec_setOverlay			= 23, // time gps
	CMD_Rec_getOverlayState		= 24,
	//1.1

	CMD_Rec_num,
};

enum Video_Resolution
{
	Video_Resolution_1080p30 = 0,
	Video_Resolution_1080p60,
	Video_Resolution_720p30,
	Video_Resolution_720p60,
	Video_Resolution_4Kp30,
	Video_Resolution_4Kp60,
	Video_Resolution_480p30,
    Video_Resolution_480p60,
	Video_Resolution_num,
};

enum Video_Qauality
{
	Video_Qauality_Supper = 0,
	Video_Qauality_HI,   
	Video_Qauality_Mid,
	Video_Qauality_LOW,
	Video_Qauality_num,
};

enum Rec_Mode
{
	Rec_Mode_Manual = 0,	//all manually
	Rec_Mode_AutoStart,		// start record when power on
	Rec_Mode_Manual_circle, //circle buffer
	Rec_Mode_AutoStart_circle, //auto start and circle buffer
	Rec_Mode_num,
};

enum Color_Mode
{
	Color_Mode_NORMAL = 0,	
	Color_Mode_SPORT,
	Color_Mode_CARDV,
	Color_Mode_SCENE,
	Color_Mode_num,
};

enum CMD_Stream
{
	CMD_Stream_Start = 0,
	CMD_Stream_Stop,
	CMD_Stream_get_Address,
	CMD_Stream_List_Formats,
	CMD_Stream_Set_Format,
	CMD_Stream_get_Format,
};

enum CMD_Storage
{
	CMD_Storage_GetNum = 0,
	CMD_Storage_GetSize,
	CMD_Storage_GetFreeSpace,
};
/*
enum CMD_Network
{
	CMD_Network_GetIfNum = 0,
	CMD_Network_GetIfState,
	CMD_Network_GetIfIpaddress,
	CMD_Network_GetWLanAPState,
	CMD_Network_GetWLanCLTState,
	CMD_Network_GetWLanMode,
	CMD_Network_GetHostNameList,
	CMD_Network_AddHost,
	CMD_Network_RmvHost,
};*/

enum CMD_Movement
{
	CMD_Forward_angles = 0,
	CMD_Backward_angles,
	CMD_Forward_steps,
	CMD_Backward_steps,
	CMD_Go_ABSPosition,
};


#endif

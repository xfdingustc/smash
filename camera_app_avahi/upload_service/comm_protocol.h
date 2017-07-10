/*==============================================================================
FILE    NAME    :   comm_protocol.h

FILE    DESC    :   common protocol define 

AUTHOR          :   zhouningdan

DATE            :   20141229
===============================================================================*/

#ifndef  __COMM_PROTOCOL_H_20141229
#define  __COMM_PROTOCOL_H_20141229 1

#include "comm_types.h"
#ifdef WIN32
#include <winsock2.h>
#else
#include <string.h>
#include <arpa/inet.h>
#endif

#ifdef __cplusplus
extern "C"{
#endif //__cplusplus


#define WAYLENS_VERSION 0x0101



//define max device_id size
#define MAX_JID_SIZE 72
#define MAX_JID_EXT_SIZE	MAX_JID_SIZE + 56
#define MAX_GUID_SIZE 64
#define MAX_TITLE_SIZE 512
#define MAX_DESCRIPTION_SIZE	1024
#define MAX_FRAGMENT_COUNT	64
//define max background music size
#define MAX_BGM_SIZE 256

//the fixed length of hash_value
#define CRS_HASH_VALUE_SIZE 33
//the block size
#define CAM_TRAN_BLOCK_SIZE 4096
//define max token size
#define MAX_TOKEN_SIZE 64
//the fixed length of SHA1
#define UPLOAD_DATA_SHA1_SIZE 20
//define each block size while the file for block transmission
#define FILE_EACH_BLOCK_SIZE	4 * 1024 * 1024

#define WAYLENS_RESOURCE_TYPE_ANDROID	"android"
#define WAYLENS_RESOURCE_TYPE_IOS		"ios"
#define WAYLENS_RESOURCE_TYPE_PC		"pc"
#define WAYLENS_RESOURCE_TYPE_CAM		"cam"


enum HTTP_IID_COMMAND
{
	//video server ==>  web
	INIT_MOMENT			= 0x0001, //create moment
	INIT_MOMENT_ACK		= 0x0002, 
	
	ADD_RESOURCE		= 0x0003, //upload moment complete
	ADD_RESOURC_ACK		= 0x0004, //web response
	
	UPDATE_AVATAR_INFO		= 0x0005, //update avater info in db
	UPDATE_AVATAR_INFO_ACK	= 0x0006, //web response

	//web ==> video server
	QUERY_UPLOAD_SVR			= 0x1001,	//query upload server (ip, port, private key)
	QUERY_UPLOAD_SVR_ACK		= 0x1002, 	//cfs manager server response
	UPLOAD_AVATAR_BY_LINK		= 0x1003,  //
	UPLOAD_AVATAR_BY_LINK_ACK	= 0x1004,
};

enum TS_STREAM_TYPE
{
    TS_STREAM_1920X1080         = 1,
    TS_STREAM_1280X720          = 2,
    TS_STREAM_720X480           = 4,
    TS_STREAM_352X288           = 8,
};

enum COMM_IID_ERR
{
	RES_SLICE_TRANS_COMPLETE	= 0x0001,
	RES_FILE_TRANS_COMPLETE		= 0x0002,

	RES_STATE_OK			=  0x0000,	//OK
	RES_STATE_FAIL			= -0x0001,	//failed
	RES_STATE_NO_DEVICE		= -0x0002,	//no device
	RES_STATE_NO_PERMISSION = -0x0003,	//no permission
	RES_STATE_NO_SPACE		= -0x0004,
	RES_STATE_WRITE_ERR		= -0x0005,
	RES_STATE_NO_CLIPS		= -0x0006,
	RES_STATE_INVALID_HTTP_REQUEST 		= -0x0007,
	RES_STATE_INVALID_USER_ID			= -0x0008,
	RES_STATE_MULTI_DEVICE_UPLOADING	= -0x0009,
	RES_STATE_TOO_MUCH_UNFINSIH_MOMENT  = -0x000A,
	RES_STATE_INVALID_MOMENT_ID         = -0x000B,
	RES_STATE_INVALID_RESOLUTION        = -0x000C,
	RES_STATE_INVALID_DATA_TYPE         = -0x000D,
};

enum VIDIT_DATA_TYPE
{
	VIDIT_RAW_DATA		= 1,
	VIDIT_VIDEO_DATA	= 2,
	VIDIT_PICTURE_DATA	= 4,
    VIDIT_RAW_GPS       = 8, 
    VIDIT_RAW_OBD       = 16,
    VIDIT_RAW_ACC       = 32,
    VIDIT_VIDEO_DATA_LOW= 64,
    VIDIT_VIDEO_DATA_TRANSFER = 128,
	VIDIT_THUMBNAIL_JPG	= 256,
};

enum OTHER_DATA_TYPE
{
	RAW_GPS			= 1,
	AUDIO_AAC		= 2,
    AUDIO_MP3       = 3,
	MP4_DATA		= 8,
	JPEG_DATA		= 16,
	JPEG_AVATAR		= 17,
	PNG_DATA		= 32,
	GIF_DATA		= 64,
};

enum CLIENT_UPLOAD_STREAM_TYPE
{
	LIVE_DATA		= 1,
	HISTORY_DATA	= 2,
};

enum WAYLENS_DEVICE_TYPE
{
	DEVICE_VIDIT	= 1,
	DEVICE_OTHER	= 2,
};

enum WAYLENS_SERVER_TYPE
{
	CFS_MANAGER_SERVER_TYPE	= 1,
	DB_PROXY_SERVER_TYPE	= 2,
	CAM_MESSAGE_QUEUE_TYPE	= 3,
	CAM_FILE_SERVER_TYPE	= 4,
	CRS_SERVER_TYPE			= 5,
	CTS_SERVER_TYPE			= 6,
	CAM3_FILE_SERVER_TYPE	= 7,

    VIDEO_TRANSCODE_SVR_TYPE    = 10,
    VIDEO_OVERLAY_SVR_TYPE      = 11,
};

//memory alignment
#pragma  pack(1) 

//common header
typedef struct WaylensCommHead
{
	sint16	size;	//packet size
	sint16	cmd;	//command word
	sint32	version;//the version number
}WaylensCommHead;
#pragma pack()


/* ======================================================
 * encode packet and decode packet functions,
 * including network byte order and host byte order conversion,to memory serialization
 * ======================================================*/

//common header encode packet and decode packet functions
inline int EncodePkgWaylensCommHead(WaylensCommHead *comm_head, char  *out_buf, int out_buf_len, int *rlen)
{
	if(NULL == comm_head || NULL == out_buf || out_buf_len < (int)sizeof(WaylensCommHead))
		return -1;

	WaylensCommHead *ph	= (WaylensCommHead*)out_buf;
	ph->size	= htons(comm_head->size);
	ph->cmd		= htons(comm_head->cmd);
	ph->version	= htonl(comm_head->version);

	if(NULL != rlen)
		(*rlen) = (int)sizeof(WaylensCommHead);
	return 0;
}

inline int DecodePkgWaylensCommHead(char *in_buf, int in_len, WaylensCommHead *comm_head)
{
	if(NULL == in_buf || NULL == comm_head || in_len < (int)sizeof(WaylensCommHead))
		return -1;

	WaylensCommHead *ph	= (WaylensCommHead*)in_buf;
	comm_head->size = ntohs(ph->size);
	comm_head->cmd	= ntohs(ph->cmd);
	comm_head->version = ntohl(ph->version);

	return 0;
}

inline sint64 htonl_64(sint64 num)
{
	sint64 ret = 0;   
	uint32 high,low;
	low 	=   num & 0xFFFFFFFF;
	high   	=  (num >> 32) & 0xFFFFFFFF;
	low   	=   htonl(low);   
	high   	=   htonl(high);   
	ret   	=   low; 
	ret   <<= 32;ret   	|=   high;   
	return   ret;   
}

inline sint64 ntohl_64(sint64 num)
{
	sint64 ret = 0;   
	uint32 high,low;
	low 	= num & 0xFFFFFFFF;
	high   	= (num >> 32) & 0xFFFFFFFF;
	low   	= ntohl(low);   
	high   	= ntohl(high);   
	ret   	= low;
	ret   	<<= 32; ret   |=   high;   
	return   ret;   
}


#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__COMM_PROTOCOL_H_20141229

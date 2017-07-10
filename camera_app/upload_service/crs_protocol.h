#ifndef __CRS_SERVER_PROTOCOL_H__
#define __CRS_SERVER_PROTOCOL_H__	1
#include "comm_types.h"
#include "comm_protocol.h"

#ifdef __cplusplus
extern "C"{
#endif //__cplusplus


//protocol command
enum CRS_IID_COMMAND
{
	//=======================================================================
	//CRS server
	//=======================================================================
	CRS_C2S_LOGIN				= 0x1000,	//login server
	CRS_S2C_LOGIN_ACK			= 0x1000,	//server response login

	CRS_C2S_LOGOUT				= 0x1001,	//logout server
	
	CRS_C2S_START_UPLOAD		= 0x1002,	//start upload
	CRS_S2C_STRAT_UPLOAD_ACK	= 0x1002,	//server response upload
	
	CRS_UPLOADING_DATA			= 0x1003,	//uploading data
	
	CRS_C2S_STOP_UPLOAD			= 0x1004,	//stop upload
	CRS_S2C_STOP_UPLOAD_ACK		= 0x1004,	//server response stop uplaod
	
	CRS_SEND_COMM_RESULT_TO_CLIENT	= 0x1005,	//server send common result to client

	CRS_C2S_UPLOAD_MOMENT_DESC		= 0x1006,
	CRS_S2C_UPLOAD_MOMENT_DESC_ACK	= 0x1007,
};

enum CRS_ENCODE_TYPE
{
	ENCODE_TYPE_OPEN	= 0,
	ENCODE_TYPE_AES		= 1,
};

//memory alignment
#pragma  pack(1) 

//encode comm head
typedef struct EncodeCommHead
{
	sint16	size;
	sint16 	encode_type; //enum CRS_ENCODE_TYPE; only support aes now
}EncodeCommHead;

//client login
typedef struct CrsClientLogin
{
	char	jid[MAX_JID_SIZE];	 //userid/resource
	sint64	moment_id;
	char	device_type; //vidit camera\mobile,etc.
	char	hash_value[CRS_HASH_VALUE_SIZE];//md5(jid+moment_id+device_type+pri_key); 
										//for example jid="test/ios",moment_id= 0, device_type = 1,pri_key="key";
										//so hash_value = md5("test/ios01key"); 
}CrsClientLogin;

//client logout
typedef struct CrsClientLogout
{
	char	jid[MAX_JID_SIZE]; //userid/resource
}CrsClientLogout;

//client start upload
typedef struct CrsClientStartUpload
{
	char	jid_ext[MAX_JID_EXT_SIZE]; //userid/resource/guid
	sint64	moment_id;
	char	file_sha1[UPLOAD_DATA_SHA1_SIZE];	//upload file sha1
	uint32	data_type;	//raw\video\picture
	uint64	file_size;
	sint64	begin_time;
	sint64	offset;
	uint32	duration; //only for mp4, the mp4 total time(unit:second)
}CrsClientStartUpload;

typedef struct CrsClientTranData
{
	char	jid_ext[MAX_JID_EXT_SIZE];
	sint64	moment_id;
	char	file_sha1[UPLOAD_DATA_SHA1_SIZE];	//upload file sha1
	char	block_sha1[UPLOAD_DATA_SHA1_SIZE];	//upload block sha1
	uint32	data_type;	//raw\video\picture
	uint32 	seq_num;	//each packet +1
	uint32	block_num;	//0  
	sint16 	length;
	char	buf[CAM_TRAN_BLOCK_SIZE];
}CrsClientTranData;

//client stop upload
typedef struct CrsClientStopUpload
{
	char	jid_ext[MAX_JID_EXT_SIZE]; //user_id/resource/guid
}CrsClientStopUpload;

//server response
typedef struct CrsCommResponse
{
	char	jid_ext[MAX_JID_EXT_SIZE];
	sint64	moment_id;
	sint64	offset;
	sint16	ret;
}CrsCommResponse;

typedef struct CrsFragment
{
	char guid[MAX_GUID_SIZE];
	char clip_capture_time[64];
	sint64	begin_time;
	sint64	offset;
	sint32	duration;
	double	frame_rate;
	uint32	resolution;
	uint32	data_type;
}CrsFragment;
typedef struct CrsMomentDescription
{
	char 	jid[MAX_JID_SIZE];
	sint64	moment_id;
	char 	background_music[MAX_BGM_SIZE];
	CrsFragment fragments[MAX_FRAGMENT_COUNT];
}CrsMomentDescription;

#pragma pack()

/* ======================================================
 * encode packet and decode packet functions,
 * including network byte order and host byte order conversion,to memory serialization
 * ======================================================*/
//encode/decode EncodeCommHead
int EncodePkgEncodeCommHead(EncodeCommHead *encode_head, char *outbuf,int outbufsize, int *rlen);
int DecodePkgEncodeCommHead(char *inbuf, int inbufsize, EncodeCommHead *encode_head, int *rlen);

//encode/decode message
int EncodePkgEncodeMessage(EncodeCommHead* comm_head,char*msg, int msg_len, char *outbuf,int outbufsize, int *rlen);
int DecodePkgEncodeMessage(char *inbuf, int inbufsize, EncodeCommHead*comm_head,char *msg, int &msg_len);

// encode/decode CrsClientLogin
int EncodePkgCrsClientLogin(WaylensCommHead *comm_head, CrsClientLogin *login, char *out_buf, int out_buf_len, int *rlen);
int DecodePkgCrsClientLogin(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsClientLogin *login);

// encode/decode CrsClientLogout
int EncodePkgCrsClientLogout(WaylensCommHead *comm_head, CrsClientLogout *logout, char *out_buf,	int out_buf_len, int *rlen);
int DecodePkgCrsClientLogout(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsClientLogout *logout);

// encode/decode CrsClientStartUpload
int EncodePkgCrsClientStartUpload(WaylensCommHead *comm_head, CrsClientStartUpload *start_upload, char *out_buf,	int out_buf_len, int *rlen);
int DecodePkgCrsClientStartUpload(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsClientStartUpload *start_upload);

//encode/decode CrsClientTranData
int EncodePkgCrsClientTranData(WaylensCommHead *comm_head, CrsClientTranData *dv, char *outbuf,int outbufsize, int *rlen);
int DecodePkgCrsClientTranData(char *inbuf, int inbufsize, WaylensCommHead *comm_head, CrsClientTranData *dv);

// encode/decode CrsClientStopUpload
int EncodePkgCrsClientStopUpload(WaylensCommHead *comm_head, CrsClientStopUpload *stop_upload, char *out_buf, int out_buf_len, int *rlen);
int DecodePkgCrsClientStopUpload(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsClientStopUpload *stop_upload);

// encode/decode server response
int EncodePkgCrsCommResponse(WaylensCommHead *comm_head, CrsCommResponse *rsp, char *out_buf, int out_buf_len, int *rlen);
int DecodePkgCrsCommResponse(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsCommResponse *rsp);

//encode/decode CrsMomentDescription 
int EncodePkgCrsMomentDescription(WaylensCommHead *comm_head, CrsMomentDescription *moment, char *out_buf, int out_buf_len, int *rlen);
int DecodePkgCrsMomentDescription(char *in_buf, int in_len, WaylensCommHead *comm_head,CrsMomentDescription*moment);


#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__CRS_SERVER_PROTOCOL_H__

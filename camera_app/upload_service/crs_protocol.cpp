/* ======================================================================
 * FILE	NAME	:	crs_protocol.inl
 * FILE	DESC	:	crs server protocol encode packet and decode packet functions
 * AUTHOR		:   zhouningdan
 * DATE			:   20141230
 *
 * protocol involving all data IP, Port's are stored in network byte order
 * ======================================================================*/

#ifdef WIN32_OS
#include <winsock2.h>
#else
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#endif
#include "crs_protocol.h"

static int EncodeShortStringMessage(const char *info, char *outbuf, int outbufsize, int *rlen)
{
	int str_len = (int)strlen(info), size = str_len + 1;
	if((size % 4)) size += (4 - (size % 4));

	if(size > outbufsize) return -1;
	char *pbuf = outbuf;
	(*(char*)pbuf) = str_len; pbuf += 1; 

	if(0 < str_len)
	{
		memcpy(pbuf, info, str_len); pbuf += str_len; 
	}

	if(rlen) (*rlen) = size;
	return 0;
}

static int DecodeShortStringMesssage(char *inbuf, int inbufsize, char *outbuf, int outbufsize, int *ilen)
{
	if(1 > inbufsize) return -1;
	int size = (*(char*)inbuf); 
	if(size > inbufsize || size >= outbufsize || size < 0) return -2; 
	if(0 < size) memcpy(outbuf, inbuf+1, size);

	size += 1;
	if((size % 4)) size += (4 - (size % 4));
	if(ilen) (*ilen) = size;
	return 0;
}

static int EncodeStringMessage(const char *info, char *outbuf, int outbufsize, int *rlen)
{
	int str_len = (int)strlen(info), size = str_len + 2;
	if((size % 4)) size += (4 - (size % 4));

	if(size > outbufsize) return -1;
	char *pbuf = outbuf;
	(*(short*)pbuf) = htons(str_len); pbuf += 2; 
	if(0 < str_len)
	{
		memcpy(pbuf, info, str_len); pbuf += str_len; 
	}

	if(rlen) (*rlen) = size;
	return 0;
}

static int DecodeStringMessage(char *inbuf, int inbufsize, char *outbuf, int outbufsize, int *ilen)
{
	if(2 > inbufsize) return -1;
	int size = ntohs(*(short*)inbuf); 
	if(size > inbufsize) return -2; 
	if(size > inbufsize || size >= outbufsize || 0 > size) return -3;
	if(0 < size) memcpy(outbuf, inbuf + 2, size);

	size += 2;
	if((size % 4)) size += (4 - (size % 4));
	if(ilen) (*ilen) = size;

	return 0;
}

//encode/decode EncodeCommHead
int EncodePkgEncodeCommHead(EncodeCommHead *encode_head, char *outbuf,int outbufsize, int *rlen)
{
	if(!encode_head || !outbuf) return -1;

	int left = outbufsize, len = 0;
	char *pbuf = outbuf;

	if(left < 2) return -2;
	(*(sint16*)pbuf) = htons(encode_head->size); pbuf += 2; len += 2; left -= 2;

	if(left < 2) return -3;
	(*(sint16*)pbuf) = htons(encode_head->encode_type); pbuf += 2; len += 2; left -= 2;

	if(rlen) (*rlen) = len;
	EncodeCommHead *ph = (EncodeCommHead*)outbuf;
	ph->size = htons((short)len);

	return 0;
}

int DecodePkgEncodeCommHead(char *inbuf, int inbufsize, EncodeCommHead *encode_head, int *rlen)
{
	if(!inbuf || !encode_head) return -1;

	int left = inbufsize, len = 0;
	char *pbuf = inbuf;

	if(2 > left) return -2;
	encode_head->size = ntohs(*(sint16*)pbuf); pbuf += 2; len += 2; left -= 2;

	if(2 > left) return -3;
	encode_head->encode_type = ntohs(*(sint16*)pbuf); pbuf += 2; len += 2; left -= 2;

	if(rlen) (*rlen) = len;
	return 0;
}

//encode/decode message
int EncodePkgEncodeMessage(EncodeCommHead *comm_head,char*msg, int msg_len, char *outbuf,int outbufsize, int *rlen)
{
	if(!comm_head || !msg || !outbuf) return -1;

	int len = 0;
	if(0 != EncodePkgEncodeCommHead(comm_head, outbuf, outbufsize, &len))
		return -2;
	
	int left = outbufsize - len;
	char *pbuf = outbuf + len;

	if(left < msg_len) return -3;
	memcpy(pbuf, msg, msg_len); pbuf += msg_len; len += msg_len; left -= msg_len;

	if(rlen) (*rlen) = len;
	EncodeCommHead *ph = (EncodeCommHead*)outbuf;
	ph->size = htons((short)len);
	return 0;
}

int DecodePkgEncodeMessage(char *inbuf, int inbufsize, EncodeCommHead *comm_head,char *msg, int &msg_len)
{
	if(!inbuf || !comm_head || !msg) return -1;

	int len = 0;
	if(0 != DecodePkgEncodeCommHead(inbuf, inbufsize, comm_head, &len))
		return -2;

	int left = comm_head->size - len;
	char *pbuf = inbuf + len;
	if(left > msg_len || left <= 0) return -3;
	memcpy(msg, pbuf, left);
	msg_len = left;
	return 0;
}

//client login encode packet and decode packet functions
int EncodePkgCrsClientLogin(WaylensCommHead *comm_head, CrsClientLogin *login, char *out_buf,
		int out_buf_len, int *rlen)
{
	if(NULL == comm_head || NULL == login || NULL == out_buf)
		return -1;

	int data_len = 0;
	if(0 != EncodePkgWaylensCommHead(comm_head, out_buf, out_buf_len, &data_len))
		return -2;

	char *pbuf = out_buf + data_len; out_buf_len -= data_len;

	//jid: 1 bytes len + string (must multipile of 4)
	int ret = 0, size = 0;
	ret = EncodeShortStringMessage(login->jid, pbuf, out_buf_len, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; data_len += size; out_buf_len -= size;

	//moment_id
	size = (int)sizeof(sint64);
	if(out_buf_len < size) return -4;
	(*(sint64*)pbuf) = htonl_64(login->moment_id); pbuf += size; data_len += size; out_buf_len -= size;
		
	//device type
	if(1 > out_buf_len)  return -5;
	(*(char*)pbuf) = login->device_type; pbuf += 1; data_len += 1; out_buf_len -= 1;

	//hash_value: fixed length
	size = (int)sizeof(login->hash_value)-1;
	if(out_buf_len < size) return -6;
	memcpy(pbuf, login->hash_value, size); pbuf += size; data_len += size; out_buf_len -= size;
		
	if(NULL != rlen) (*rlen) = data_len;

	WaylensCommHead *ph = (WaylensCommHead*)out_buf;
	ph->size = htons((short)data_len);
	return 0;
}

int DecodePkgCrsClientLogin(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsClientLogin *login)
{
	if(NULL == in_buf || NULL == comm_head || NULL == login)
		return -1;

	if(0 != DecodePkgWaylensCommHead(in_buf, in_len, comm_head))
		return -2;

	char *pbuf = in_buf + sizeof(WaylensCommHead);
	int len = (int)(comm_head->size - sizeof(WaylensCommHead));

	memset(login, 0x00, sizeof(*login));
	//jid: 1 bytes len + string (must multipile of 4)
	int ret = 0, size = 0, tmp_size = 0;
	tmp_size = (int)sizeof(login->jid);
	ret = DecodeShortStringMesssage(pbuf, len,login->jid, tmp_size, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; len -= size;

	//moment_id
	size = (int)sizeof(sint64);
	if(len <= size) return (ret - 4);
	login->moment_id = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;
	
	//device type
	if(1 > len) return (ret - 5);
	login->device_type = (*(char*)pbuf); pbuf += 1; len -= 1;

	//hash_value: fixed length
	size = (int)sizeof(login->hash_value)-1;
	if(size > len) return -6;
	memcpy(login->hash_value, pbuf, size); pbuf += size; len -=  size;
	
	return 0;
}

//client logout
int EncodePkgCrsClientLogout(WaylensCommHead *comm_head, CrsClientLogout *logout, char *out_buf, int out_buf_len, int *rlen)
{
	if(NULL == comm_head || NULL == logout || NULL == out_buf)
		return -1;

	int data_len = 0;
	if(0 != EncodePkgWaylensCommHead(comm_head, out_buf, out_buf_len, &data_len))
		return -2;

	char *pbuf = out_buf + data_len; out_buf_len -= data_len;

	//jid: 1 bytes len + string (must multipile of 4)
	int ret = 0, size = 0;
	ret = EncodeShortStringMessage(logout->jid, pbuf, out_buf_len, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; data_len += size; out_buf_len -= size;
		
	if(NULL != rlen) (*rlen) = data_len;

	WaylensCommHead *ph = (WaylensCommHead*)out_buf;
	ph->size = htons((short)data_len);
	return 0;
}

int DecodePkgCrsClientLogout(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsClientLogout *logout)
{
	if(NULL == in_buf || NULL == comm_head || NULL == logout)
		return -1;

	if(0 != DecodePkgWaylensCommHead(in_buf, in_len, comm_head))
		return -2;

	char *pbuf = in_buf + sizeof(WaylensCommHead);
	int len = (int)(comm_head->size - sizeof(WaylensCommHead));

	memset(logout, 0x00, sizeof(*logout));
	//device_id: 1 bytes len + string (must multipile of 4) 
	int ret = 0, size = 0, tmp_size = 0;
	tmp_size = (int)sizeof(logout->jid);
	ret = DecodeShortStringMesssage(pbuf, len, logout->jid, tmp_size, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; len -= size;

	return 0;
}

//client start upload
int EncodePkgCrsClientStartUpload(WaylensCommHead *comm_head, CrsClientStartUpload *start_upload, char *out_buf,
		int out_buf_len, int *rlen)
{
	if(!comm_head || !start_upload || !out_buf) return -1;

	int len = 0;
	if(0 != EncodePkgWaylensCommHead(comm_head, out_buf,out_buf_len, &len)) return -2;

	int left = out_buf_len - len;
	char *pbuf = out_buf + len;

	//jid_ext: 1 byte len + string (must multipile of 4)
	int size = 0;
	int ret = EncodeShortStringMessage(start_upload->jid_ext, pbuf, left, &size);
	if(0 != ret) return (ret - 1);
	pbuf += size; len += size; left -= size;
	
	//moment_id
	size = (int)sizeof(sint64);
	if(left < size) return -2;
	(*(sint64*)pbuf) = htonl_64(start_upload->moment_id); pbuf += size; len += size; left -= size;

	//file sha1: fixed length
	size = (int)sizeof(start_upload->file_sha1);
	if(left < (int)size) return (ret-3);
	memcpy(pbuf, start_upload->file_sha1, size); pbuf += size; len += size; left -= size;
	
	//data type
	size = (int)sizeof(uint32);
	if(size > left) return (ret - 4);
	(*(uint32*)pbuf) = htonl(start_upload->data_type); pbuf += size; len += size; left -= size;
	
	//file_size
	size = (int)sizeof(sint64);
	if(left < size) return -5;
	(*(sint64*)pbuf) = htonl_64(start_upload->file_size); pbuf += size; len += size; left -= size;
	
	//begin_time
	size = (int)sizeof(sint64);
	if(left < size) return -6;
	(*(sint64*)pbuf) = htonl_64(start_upload->begin_time); pbuf += size; len += size; left -= size;

	//offset
	size = (int)sizeof(sint64);
	if(size > left) return (ret - 7);
	sint64 offset = htonl_64(start_upload->offset);
    memcpy(pbuf, reinterpret_cast<uint8*>(&offset), sizeof(sint64));
	pbuf += size; len += size; left -= size;
	
	//duration
	size = (int)sizeof(uint32);
	if(size > left) return (ret - 8);
	(*(uint32*)pbuf) = htonl(start_upload->duration); pbuf += size; len += size; left -= size;
	
	if(rlen) (*rlen) = len;

	WaylensCommHead *ph = (WaylensCommHead*)out_buf;
	ph->size = htons((short)len);
	return 0;
}

int DecodePkgCrsClientStartUpload(char *in_buf, int in_len, WaylensCommHead *comm_head,
		CrsClientStartUpload *start_upload)
{
	if(!in_buf || !comm_head || !start_upload) return -1;

	if(0 != DecodePkgWaylensCommHead(in_buf, in_len, comm_head)) return -2;

	int len = comm_head->size - (int)sizeof(*comm_head);
	char *pbuf = in_buf + sizeof(*comm_head);

	//jid_ext: 1 byte len + string (must multipile of 4)
	int ret = 0, size = 0, tmp_size = 0;
	tmp_size = (int)sizeof(start_upload->jid_ext); memset(start_upload->jid_ext, 0x00, tmp_size);
	ret = DecodeShortStringMesssage(pbuf, len, start_upload->jid_ext, tmp_size, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; len -= size;
	
	//moment_id
	size = (int)sizeof(sint64);
	if(len < size) return (ret - 4);
	start_upload->moment_id = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;

	//data sha1: fixed length
	size = (int)sizeof(start_upload->file_sha1);
	if((int)size > len) return (ret - 5);
	memcpy(start_upload->file_sha1, pbuf, size); pbuf += size; len -=  size;
	
	//data type
	size = (int)sizeof(uint32);
	if(size > len) return (ret -6);
	start_upload->data_type = ntohl(*(uint32*)pbuf); pbuf += size; len -= size;
	
	//file_size
	size = (int)sizeof(uint64);
	if(size > len) return (ret - 7);
	start_upload->file_size = ntohl_64(*(uint64*)pbuf); pbuf += size; len -= size;
	
	//begin_time
	size = (int)sizeof(sint64);
	if(len < size) return (ret - 8);
	start_upload->begin_time = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;
	
	//offset
	size = (int)sizeof(sint64);
	if(size > len) return (ret - 9);
	start_upload->offset = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;
	
	//duration
	size = sizeof(uint32);
	if((int)size > len) return (ret - 10);
	start_upload->duration = ntohl(*(uint32*)pbuf); pbuf += size; len -= size;
	
	return 0;
}

//encode/decode CrsClientTranData
int EncodePkgCrsClientTranData(WaylensCommHead *comm_head, CrsClientTranData *dv, char *outbuf,int outbufsize, int *rlen)
{
	if(!comm_head || !dv || !outbuf) return -1;

	int len = 0;
	if(0 != EncodePkgWaylensCommHead(comm_head, outbuf, outbufsize, &len))
		return -2;
	
	int left = outbufsize - len;
	char *pbuf = outbuf + len;

	//jid_ext: 1 byte len + string (must multipile of 4)
	int ret = 0, size = 0;
	ret = EncodeShortStringMessage(dv->jid_ext, pbuf, left, &size);
	if(0 != ret) return (ret - 2);
	pbuf += size; len += size; left -= size;
	
	//moment_id
	size = (int)sizeof(sint64);
	if(left < size) return -3;
	(*(sint64*)pbuf) = htonl_64(dv->moment_id); pbuf += size; len += size; left -= size;

	//file_sha1: fixed length
	size = (int)sizeof(dv->file_sha1);
	if(left < (int)size) return ret - 4;
	memcpy(pbuf, dv->file_sha1, size); pbuf += size; len += size; left -= size;
	
	//block_sha1: fixed length
	size = (int)sizeof(dv->block_sha1);
	if(left < (int)size) return ret - 5;
	memcpy(pbuf, dv->block_sha1, size); pbuf += size; len += size; left -= size;
	
	//data type
	size = (int)sizeof(uint32);
	if(size > left) return ret - 6;
	(*(uint32*)pbuf) = htonl(dv->data_type); pbuf += size; len += size; left -= size;
	
	//seq_num
	size = (int)sizeof(uint32);
	if((int)size > left) return ret - 7;
	(*(uint32*)pbuf) = htonl(dv->seq_num); pbuf += size; len += size; left -= size;

	//block num
	size = (int)sizeof(uint32);
	if((int)size > left) return ret -8;
	(*(uint32*)pbuf) = htonl(dv->block_num); pbuf += size; len += size; left -= size;

	//length
	size = (int)sizeof(sint16);
	if((int)size > left) return ret - 9;
	(*(sint16*)pbuf) = htons(dv->length); pbuf += size; len += size; left -= size;

	//buf
	size = dv->length;
	if((int)size > left) return ret - 10;
	memcpy(pbuf, dv->buf, size); pbuf += size; len += size; left -= size;

	if(rlen) (*rlen) = len;
	WaylensCommHead *ph = (WaylensCommHead*)outbuf;
	ph->size = htons((short)len);

	return 0;
}

int DecodePkgCrsClientTranData(char *inbuf, int inbufsize, WaylensCommHead *comm_head, CrsClientTranData *dv)
{
	if(!inbuf || !comm_head || !dv) return -1;

	if(0 != DecodePkgWaylensCommHead(inbuf, inbufsize, comm_head))
		return -2;
	
	int len = comm_head->size - (int)sizeof(*comm_head);
	char *pbuf = inbuf + sizeof(*comm_head);

	//jid_ext: 1 byte len + string (must multipile of 4)
	int ret = 0, size = 0, tmp_size = 0;
	tmp_size = (int)sizeof(dv->jid_ext); memset(dv->jid_ext, 0x00, tmp_size);
	ret = DecodeShortStringMesssage(pbuf, len, dv->jid_ext, tmp_size, &size);
	if(0 != ret) return (ret - 2);
	pbuf += size; len -= size;
	
	//moment_id
	size = (int)sizeof(sint64);
	if(len < size) return (ret - 3);
	dv->moment_id = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;

	//file_sha1: fixed length
	size = (int)sizeof(dv->file_sha1);
	if(size > len) return -4;
	memcpy(dv->file_sha1, pbuf, size); pbuf += size; len -=  size;
	
	//block_sha1: fixed length
	size = (int)sizeof(dv->block_sha1);
	if(size > len) return -5;
	memcpy(dv->block_sha1, pbuf, size); pbuf += size; len -=  size;
	
	//data type
	size = (int)sizeof(uint32);
	if(size > len) return -7;
	dv->data_type = ntohl(*(uint32*)pbuf); pbuf += size; len -= size;
	
	//seq_num
	size = sizeof(uint32);
	if((int)size > len) return -11;
	dv->seq_num = ntohl(*(uint32*)pbuf); pbuf += size; len -= size;

	//block num
	size = sizeof(uint32);
	if((int)size > len) return -12;
	dv->block_num= ntohl(*(uint32*)pbuf); pbuf += size; len -= size;

	//length
	size = sizeof(sint16);
	if((int)size > len) return -13;
	dv->length= ntohs(*(sint16*)pbuf); pbuf += size; len -= size;

	//buf
	size = dv->length;
	if((int)size > len || size > (int)sizeof(dv->buf)) return -14;
	memcpy(dv->buf, pbuf, size); pbuf += size; len -= size;

	return 0;
}

//stop upload
int EncodePkgCrsClientStopUpload(WaylensCommHead *comm_head, CrsClientStopUpload *stop_upload, char *out_buf,
		int out_buf_len, int *rlen)
{
	if(NULL == comm_head || NULL == stop_upload|| NULL == out_buf)
		return -1;

	int data_len = 0;
	if(0 != EncodePkgWaylensCommHead(comm_head, out_buf, out_buf_len, &data_len))
		return -2;

	char *pbuf = out_buf + data_len; out_buf_len -= data_len;

	//jid_ext: 1 bytes len + string (must multipile of 4)
	int ret = 0, size = 0;
	ret = EncodeShortStringMessage(stop_upload->jid_ext, pbuf, out_buf_len, &size);
	if(0 != ret) return (ret -3);
	pbuf += size; data_len += size; out_buf_len -= size;
		
	if(NULL != rlen) (*rlen) = data_len;

	WaylensCommHead *ph = (WaylensCommHead*)out_buf;
	ph->size = htons((short)data_len);
	return 0;
}

int DecodePkgCrsClientStopUpload(char *in_buf, int in_len, WaylensCommHead *comm_head,
		CrsClientStopUpload *stop_upload)
{
	if(NULL == in_buf || NULL == comm_head || NULL == stop_upload)
		return -1;

	if(0 != DecodePkgWaylensCommHead(in_buf, in_len, comm_head))
		return -2;

	memset(stop_upload, 0x00, sizeof(*stop_upload));
	char *pbuf = in_buf + sizeof(WaylensCommHead);
	int len = (int)(comm_head->size - sizeof(WaylensCommHead));

	//jid_ext: 1 bytes len + string (must multipile of 4)
	int ret = 0, size = 0, tmp_size = 0;
	tmp_size = (int)sizeof(stop_upload->jid_ext);
	ret = DecodeShortStringMesssage(pbuf, len, stop_upload->jid_ext, tmp_size, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; len -= size;

	return 0;
}

//server response
int EncodePkgCrsCommResponse(WaylensCommHead *comm_head, CrsCommResponse *rsp, char *out_buf,
		int out_buf_len, int *rlen)
{
	if(!comm_head || !rsp || !out_buf) return -1;

	int len = 0;
	if(0 != EncodePkgWaylensCommHead(comm_head, out_buf,out_buf_len, &len)) return -2;

	int left = out_buf_len - len;
	char *pbuf = out_buf + len;

	//jid_ext: 1 byte len + string (must multipile of 4)
	int ret = 0, size = 0;
	ret = EncodeShortStringMessage(rsp->jid_ext, pbuf, left, &size);
	if(0 != ret) return (ret - 1);
	pbuf += size; len += size; left -= size;
	
	//moment_id
	size = (int)sizeof(sint64);
	if(size > left) return ret-2;
	(*(sint64*)pbuf) = htonl_64(rsp->moment_id); pbuf += size; len += size; left -= size;

	//offset
	size = (int)sizeof(sint64);
	if(size > left) return ret-3;
	(*(sint64*)pbuf) = htonl_64(rsp->offset); pbuf += size; len += size; left -= size;
	
	//ret
	size = (int)sizeof(sint16);
	if(size > left) return ret-4;
	(*(sint16*)pbuf) = htons(rsp->ret); pbuf += size; len += size; left -= size;

	if(rlen) (*rlen) = len;
	WaylensCommHead *ph = (WaylensCommHead*)out_buf;
	ph->size = htons(len);

	return 0;
}

int DecodePkgCrsCommResponse(char *in_buf, int in_len, WaylensCommHead *comm_head, CrsCommResponse *rsp)
{
	if(!in_buf || !comm_head || !rsp) return -1;

	if(0 != DecodePkgWaylensCommHead(in_buf, in_len, comm_head)) return -2;

	int len = comm_head->size - (int)sizeof(*comm_head);
	char *pbuf = in_buf + sizeof(*comm_head);

	memset(rsp, 0x00, sizeof(*rsp));
	//jid_ext: 1 byte len + string (must multipile of 4)
	int ret = 0, size = 0, tmp_size = 0;
	tmp_size = (int)sizeof(rsp->jid_ext);
	ret = DecodeShortStringMesssage(pbuf, len, rsp->jid_ext, tmp_size, &size);
	if(0 != ret) return (ret - 2);
	pbuf += size; len -= size;
	
	//moment_id
	size = sizeof(sint64);
	if((int)size > len) return -3;
	rsp->moment_id = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;
	
	//offset
	size = sizeof(sint64);
	if((int)size > len) return -3;
	rsp->offset = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;
	
	//ret
	size = sizeof(sint16);
	if((int)size > len) return -4;
	rsp->ret = ntohs(*(sint16*)pbuf); pbuf += size; len -= size;

	return 0;
}
//encode/decode CrsFragment
static int EncodePkgCrsFragment(CrsFragment *fragment, char *out_buf, int out_buf_len, int *rlen)
{
	if(!fragment || !out_buf) return -1;

	char *pbuf = out_buf;
	int left = out_buf_len, ilen = 0;
	
	int ret = 0, size = 0;
	//guid: 1 byte len + string (must multipile of 4)
	ret = EncodeShortStringMessage(fragment->guid, pbuf, left, &size);
	if(0 != ret) return (ret - 2);
	pbuf += size; ilen += size; left -= size;

	//clip_capture_time: 1 byte len + string (must multipile of 4)
	ret = EncodeShortStringMessage(fragment->clip_capture_time, pbuf, left, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; ilen += size; left -= size;

	//begin_time
	size = (int)sizeof(sint64);
	if(left < size) return (ret - 4);
	(*(sint64*)pbuf) = htonl_64(fragment->begin_time); pbuf += size; left -= size; ilen += size;

	//offset
	size = (int)sizeof(sint64);
	if(left < size) return (ret - 5);
	(*(sint64*)pbuf) = htonl_64(fragment->offset); pbuf += size; left -= size; ilen += size;

	//duration
	size = (int)sizeof(sint32);
	if(left < size) return (ret - 6);
	(*(sint32*)pbuf) = htonl(fragment->duration); pbuf += size; left -= size; ilen += size;
	
	//frame_rate;
	size = (int)sizeof(double);
	sint64 frame_rate = (*((sint64*)&fragment->frame_rate));
	if(size > left) return ret - 7;
	(*(sint64*)pbuf) = htonl_64(frame_rate); pbuf += size; left -= size; ilen += size;
	
	//resolution;
	size = (int)sizeof(uint32);
	if(size > left) return ret-8;
	(*(uint32*)pbuf) = htonl(fragment->resolution); pbuf += size; left -= size; ilen += size;
	
	//data_type
	size = (int)sizeof(uint32);
	if(left < size) return (ret - 9);
	(*(uint32*)pbuf) = htonl(fragment->data_type); pbuf += size; left -= size; ilen += size;

	if(rlen) (*rlen) = ilen;

	return 0;
}
int DecodePkgCrsFragment(char *in_buf, int in_len, CrsFragment *fragment, int *rlen)
{
	if(!in_buf || !fragment) return -1;
	char *pbuf = in_buf;
	int left = in_len, ilen = 0;

	int ret = 0, tmp_size = 0, size = 0;
	//guid
	tmp_size = (int)sizeof(fragment->guid);
	ret = DecodeShortStringMesssage(pbuf, left, fragment->guid, tmp_size, &size);
	if(0 != ret) return (ret - 2);
	pbuf += size; left -= size; ilen += size;

	//clip_capture_time: 1 byte len + string (must multipile of 4)
	tmp_size = (int)sizeof(fragment->clip_capture_time);
	ret = DecodeShortStringMesssage(pbuf, left, fragment->clip_capture_time, tmp_size, &size);
	if(0 != ret) return (ret - 3);
	pbuf += size; left -= size; ilen += size;
	
	//begin_time
	size = (int)sizeof(sint64);
	if(left < size) return (ret - 4);
	fragment->begin_time = ntohl_64(*(sint64*)pbuf); pbuf += size; left -= size; ilen += size;

	//offset
	size = (int)sizeof(sint64);
	if(left < size) return (ret - 5);
	fragment->offset = ntohl_64(*(sint64*)pbuf); pbuf += size; left -= size; ilen += size;

	//duration
	size = (int)sizeof(sint32);
	if(left < size) return (ret - 6);
	fragment->duration = ntohl(*(sint32*)pbuf); pbuf += size; left -= size; ilen += size;
	
	//frame_rate;
	size = sizeof(double);
	if((int)size > left) return -7;
	sint64 frame_rate = ntohl_64(*(sint64*)pbuf);
	fragment->frame_rate = (*(double*)&frame_rate); pbuf += size; left -= size; ilen += size;
	
	//resolution;
	size = sizeof(uint32);
	if((int)size > left) return -8;
	fragment->resolution = ntohl(*(uint32*)pbuf); pbuf += size; left -= size; ilen += size;
	
	//data_type
	size = (int)sizeof(uint32);
	if(left < size) return (ret - 9);
	fragment->data_type = ntohl(*(uint32*)pbuf); pbuf += size; left -= size; ilen += size;

	if(rlen) (*rlen) = ilen;

	return 0;
}
//encode/decode CrsMomentDescription 
int EncodePkgCrsMomentDescription(WaylensCommHead *comm_head, CrsMomentDescription*moment, char *out_buf, int out_buf_len, int *rlen)
{
	if(!comm_head || !moment || !out_buf) return -1;

	int len = 0;
	if(0 != EncodePkgWaylensCommHead(comm_head, out_buf,out_buf_len, &len)) return -2;

	int left = out_buf_len - len;
	char *pbuf = out_buf + len;

	//jid: 1 byte len + string (must multipile of 4)
	int ret = 0, size = 0;
	ret = EncodeShortStringMessage(moment->jid, pbuf, left, &size);
	if(0 != ret) return (ret - 1);
	pbuf += size; len += size; left -= size;

	//moment_id
	size = (int)sizeof(sint64);
	if(size > left) return ret-2;
	(*(sint64*)pbuf) = htonl_64(moment->moment_id); pbuf += size; len += size; left -= size;
	
	//background_music: 1 byte len + string (must multipile of 4)
	ret = EncodeShortStringMessage(moment->background_music, pbuf, left, &size);
	if(0 != ret) return (ret - 4);
	pbuf += size; len += size; left -= size;

	//fragments[MAX_FRAGMENT_COUNT];
	char *pcount = pbuf; pbuf += 1; len += 1; left -= 1; 
	short count = 0;
	for(int i = 0; i < MAX_FRAGMENT_COUNT; ++i)
	{
		if(0 >= strlen(moment->fragments[i].guid)) break;
		ret = EncodePkgCrsFragment(&moment->fragments[i], pbuf, left, &size);
		if(0 != ret) return (ret - 10 - i);
		pbuf += size; len += size; left -= size; ++count;
	}
	(*pcount) = (char)count;

	if(rlen) (*rlen) = len;
	WaylensCommHead *ph = (WaylensCommHead*)out_buf;
	ph->size = htons(len);
	return 0;
}
int DecodePkgCrsMomentDescription(char *in_buf, int in_len, WaylensCommHead *comm_head,CrsMomentDescription*moment)
{
	if(!in_buf || !comm_head || !moment) return -1;

	if(0 != DecodePkgWaylensCommHead(in_buf, in_len, comm_head)) return -2;

	int len = comm_head->size - (int)sizeof(*comm_head);
	char *pbuf = in_buf + sizeof(*comm_head);

	memset(moment, 0x00, sizeof(*moment));
	//jid_ext: 1 byte len + string (must multipile of 4)
	int ret = 0, size = 0, tmp_size = 0;
	tmp_size = (int)sizeof(moment->jid);
	ret = DecodeShortStringMesssage(pbuf, len, moment->jid, tmp_size, &size);
	if(0 != ret) return (ret - 2);
	pbuf += size; len -= size;
	
	//moment_id
	size = sizeof(sint64);
	if((int)size > len) return -3;
	moment->moment_id = ntohl_64(*(sint64*)pbuf); pbuf += size; len -= size;
	
	//background_music: 1 byte len + string (must multipile of 4)
	tmp_size = (int)sizeof(moment->background_music);
	ret = DecodeShortStringMesssage(pbuf, len, moment->background_music, tmp_size, &size);
	if(0 != ret) return (ret - 5);
	pbuf += size; len -= size;

	//fragments[MAX_FRAGMENT_COUNT];
	if(len < 1) return (ret - 7);
	short count = (*pbuf); pbuf += 1; len -= 1;
	if(count >= MAX_FRAGMENT_COUNT || 0 > count) return (ret - 21);
	if(0 < count)
	{
		for(int i = 0; i < count; ++i)
		{
			ret = DecodePkgCrsFragment(pbuf, len, &moment->fragments[i], &size);
			if(0 != ret) return (ret - 10 - i);
			pbuf += size; len -= size;
		}
	}

	return 0;
}

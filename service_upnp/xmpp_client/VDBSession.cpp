#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "VDBSession.h"
#include "class_xmpp_client.h"

TCP_VDB_SessionClient::TCP_VDB_SessionClient
(int rcvBufferLen
 ): StandardTcpClient(rcvBufferLen, "TCP_VDB_SessionClient")
,_fd(-1)
{
    
}

TCP_VDB_SessionClient::~TCP_VDB_SessionClient()
{
    
}

int TCP_VDB_SessionClient::Send(char* buffer, int len)
{
    int rt = 0;
    if(_fd > 0)
    {
        while (rt < len) {
            int sendlen = 0;
            sendlen = ::send(_fd, buffer + rt, len - rt, MSG_NOSIGNAL);
            if(sendlen >= 0)
                rt += sendlen;
            else
                break;
        }
    }
    return rt;
}

void TCP_VDB_SessionClient::onReadBufferProcess(char* buffer, int len, int fd)
{
    StandardTcpClient::onReadBufferProcess(buffer, len, fd);
}

void TCP_VDB_SessionClient::onFdNew(int fd)
{
    //printf("---onFdNew : %d\n", fd);
    _fd = fd;
}

void TCP_VDB_SessionClient::onFdClose(int fd)
{
    printf("---onFdClose\n");
    _fd = -1;
}

TCP_VDB_Session::TCP_VDB_Session
    (char* address
    , StringRemoteCamera* camera
     ):_buffSize(VDB_Data_BufferLen)
    ,_rcvedLen(0)
    ,_cmdSequence(0)
    ,_pCamera(camera)
{
    _pVDBCmdSession = new TCP_VDB_SessionClient(10240);
    _pVDBCmdSession->SetDelegate(this);
    bool b = _pVDBCmdSession->Connect(address, VDB_CMD_PORT);
	if(b)
	{
		SwitchRawDataMsgOn(true);
	}
    printf("---TCP_VDB_Session connect : %d\n", b);
    _bMoreNeedRcv = false;
    _dataBuffer = new char[_buffSize];

}

TCP_VDB_Session::~TCP_VDB_Session()
{
    delete _pVDBCmdSession;
    delete[] _dataBuffer;
}

void TCP_VDB_Session::OnRecievedBuffer
    (char* buffer
     , int len
     , int fd)
{
	//printf("---OnRecievedBuffer : %d\n", len);
    if(_pVDBCmdSession->isFd(fd))
    {
        if(_buffSize - _rcvedLen < len)
        {
            printf("--data rcv buffer overflow\n");
            // error process.
        }
        memcpy(_dataBuffer + _rcvedLen, buffer, len);
        _rcvedLen += len;
        if(_rcvedLen >= (int)sizeof(vdb_ack_s))
        {
            vdb_ack_s* pAck_s = (vdb_ack_s*)_dataBuffer;
            int actLen = pAck_s->extra_bytes + VDB_ACK_SIZE;
            if(_rcvedLen >= actLen)
            {
            	onMsg(_dataBuffer, actLen);
                //onCMDResult(_dataBuffer, actLen);
                memcpy(_dataBuffer, _dataBuffer + actLen, _rcvedLen - actLen);
                _rcvedLen -= actLen;
            }
        }
    }/*
    else if(_pVDBMsgSession->isFd(fd))
    {
        if(_msgBufferSize - _msgRcvLen < len)
        {
            printf("--msg rcv buffer overflow\n");
            // error process.
        }
        memcpy(_msgBuffer + _msgRcvLen, buffer, len);
        _msgRcvLen += len;
        if(_msgRcvLen >= (int)sizeof(vdb_ack_s))
        {
            vdb_ack_s* pAck_s = (vdb_ack_s*)_msgBuffer;
            int actLen = pAck_s->extra_bytes + VDB_MSG_SIZE;
            if(_msgRcvLen >= actLen)
            {
                onMsg(_msgBuffer, actLen);
                memcpy(_msgBuffer, _msgBuffer + actLen, _msgRcvLen - actLen);
                _msgRcvLen -= actLen;
                //printf("remain len : %d\n", _msgRcvLen);
            }
        }
    }*/
}

void TCP_VDB_Session::onCMDResult(char* buffer, int len)
{
    vdb_ack_s* pAck_s = (vdb_ack_s*)buffer;
    switch (pAck_s->cmd_code) {
        case VDB_CMD_GetClipSetInfo:
            
            break;
        case VDB_CMD_GetIndexPicture:
            
            break;
        case VDB_CMD_GetPlaybackUrl:
            
            break;
        case VDB_CMD_GetRawData:
			
            break;
        default:
            printf("---unkonw cmd result: %d\n", pAck_s->cmd_code);
            break;
    }

}

void TCP_VDB_Session::onMsg(char* buffer, int len)
{
    
    vdb_ack_s* pAck_s = (vdb_ack_s*)buffer;
    //printf("---msg : %d\n", pAck_s->cmd_code);
    switch (pAck_s->cmd_code) {
        case VDB_MSG_ClipCreated:
            
            break;
        case VDB_MSG_ClipChanged:
            // update current clip infor.
            break;
        case VDB_MSG_BufferSpaceLow:
            
            break;
        case VDB_MSG_BufferFull:
            
            break;
        case VDB_MSG_ClipRemoved:
            
            break;
        case VDB_MSG_RawData:
			//if(pAck_s->ret_code == 0)
            {
            	char ttmp[256];
				char* pointer;
                vdb_msg_RawData_s *data = (vdb_msg_RawData_s*)(buffer);
				//printf("-- on msg raw data recved: %d\n", data->data_type);
				if (data->data_type == kRawData_GPS) {
					//printf("kRawData_GPS data recieve\n");
                    pointer = buffer + sizeof(vdb_msg_RawData_s);
                    if(data->data_size == 0)
                    {
                        // no valid gps infor.
                    }
                    else
                    {
                        gps_raw_data_s *gpsInfor = (gps_raw_data_s*)pointer;
                        memset(ttmp, 0, sizeof(ttmp));
						snprintf(ttmp,sizeof(ttmp),"EW%f ,NS%f ,H%f ,SP%f ", gpsInfor->longitude, gpsInfor->latitude, gpsInfor->altitude, gpsInfor->speed);
                        _pCamera->OnCameraInformation(CameraInforType_GPS, ttmp);
                    }
                }
				else if(data->data_type == kRawData_ODB)
                {
                    //break;
                   	pointer = buffer + sizeof(vdb_msg_RawData_s);
                    if(data->data_size == 0)
                    {
                        // no valid gps infor.
                    }
                    else
                    {
                    	int _OBD_speed = 0, _OBD_temp = 0, _OBD_rpm = 0;
                    	struct odb_raw_data_s* obdData = (struct odb_raw_data_s*)pointer;
					    int speed = 0;
					    struct odb_index_s* indexArray = (struct odb_index_s *)(pointer + sizeof(struct odb_raw_data_s));
					    int indexNum = obdData->pid_info_size / sizeof(odb_index_t);
					    unsigned char *data = (unsigned char *)(pointer + sizeof(struct odb_raw_data_s) + obdData->pid_info_size);
					    if(indexNum > 0x0d)
					    {
					        if (indexArray[0x0d].flag & 0x1) {
					            _OBD_speed = data[indexArray[0x0d].offset];
					        }
					        if (indexArray[0x05].flag & 0x1) {
					            _OBD_temp = data[indexArray[0x05].offset];
					        }
					        if (indexArray[0x0c].flag & 0x1) {
					            _OBD_rpm = data[indexArray[0x0c].offset];
					        }
					    }
						
						memset(ttmp, 0, sizeof(ttmp));
						snprintf(ttmp,sizeof(ttmp),"RP%d ,TP%d ,SP%d ",_OBD_rpm,  _OBD_temp, _OBD_speed);
						//printf("OBD data : %x\n", _pCamera);
						if(_pCamera)
							_pCamera->OnCameraInformation(CameraInforType_VDB, ttmp);
                    }
                }
            }
            break;  
        default:
            printf("---unkonw msb result\n");
            break;
    }
}

void TCP_VDB_Session::SwitchRawDataMsgOn(bool bOn)
{
    if(!_pVDBCmdSession->isConnected())
        return;
    char tmp[VDB_CMD_SIZE];
    vdb_cmd_SetRawDataOption_s* cmd = (vdb_cmd_SetRawDataOption_s*)tmp;
    cmd->header.cmd_code = VDB_CMD_SetRawDataOption;
	if(bOn)
    	cmd->data_types = (1<<kRawData_GPS)|(1<<kRawData_ODB) ; // | (1<<kRawData_ACC) |;
	else
		cmd->data_types = 0;
	_pVDBCmdSession->Send(tmp, VDB_CMD_SIZE);
}

/*int TCP_VDB_Session::GetVDBInfor(int dir)
{
    //NSLog(@"---GetVDBInfor 0");
    if(!_pVDBCmdSession->isConnected())
        return -2;
    char tmp[VDB_CMD_SIZE];
    vdb_cmd_GetClipSetInfo_s* cmd = (vdb_cmd_GetClipSetInfo_s*)tmp;
    cmd->header.cmd_code = VDB_CMD_GetClipSetInfo;
    cmd->clip_type = dir;
    _pVDBCmdSession->Send(tmp, VDB_CMD_SIZE);
    //int rt = _pCmdSync->Take(3000000);
    //NSLog(@"---GetVDBInfor- %d", [_pClips count]);
    [_pCondition lock];
    NSLog(@"wait vdb infor");
    [_pCondition wait];
    [_pCondition unlock];
    //return  rt;
    return 0;
}

void TCP_VDB_Session::GetSubNail(int dir, unsigned int clip, double timePoint, unsigned int sequence)
{
    if(!_pVDBCmdSession->isConnected())
        return;
    char tmp[VDB_CMD_SIZE];
    _cmdSequence ++;
    vdb_cmd_GetIndexPicture_s* cmd = (vdb_cmd_GetIndexPicture_s*)tmp;
    cmd->header.cmd_code = VDB_CMD_GetIndexPicture;
    cmd->header.cmd_tag = sequence;
    cmd->clip_type = dir;
    cmd->clip_id = clip;
    cmd->clip_time_ms_hi = ((unsigned long long)(timePoint * 1000) / 0xffffffff);
    cmd->clip_time_ms_lo = ((unsigned long long)(timePoint * 1000) % 0xffffffff);
    _pVDBCmdSession->Send(tmp, VDB_CMD_SIZE);
}

void TCP_VDB_Session::GetAdditonalData(int dir, unsigned int clip, double timePoint)
{
    if(!_pVDBCmdSession->isConnected())
        return;
    char tmp[VDB_CMD_SIZE];
    vdb_cmd_GetRawData_s* cmd = (vdb_cmd_GetRawData_s*)tmp;
    cmd->header.cmd_code = VDB_CMD_GetRawData;
    cmd->clip_type = dir;
    cmd->clip_id = clip;
    cmd->clip_time_ms_hi = ((unsigned long long)(timePoint * 1000) / 0xffffffff);
    cmd->clip_time_ms_lo = ((unsigned long long)(timePoint * 1000) % 0xffffffff);
    cmd->data_types = (1<<kRawData_GPS)|(1<<kRawData_ODB) ; // | (1<<kRawData_ACC) |;
    _pVDBCmdSession->Send(tmp, VDB_CMD_SIZE);
}

void TCP_VDB_Session::GetGsensorBlock(int dir, unsigned int clip, double timePoint, double range)
{
    //NSLog(@"Get GensorBlock ");
    if(!_pVDBCmdSession->isConnected())
        return;
    [_gsensorDataLock lock];
    if ([_gsensorDateBlock count] > 0) {
        CGensorValue* pFirst = [_gsensorDateBlock objectAtIndex:0];
        CGensorValue* pLast = [_gsensorDateBlock objectAtIndex:[_gsensorDateBlock count] -1];
        
        if((timePoint + range < [pFirst timePoint])||(timePoint > [pLast timePoint]))
        {
            [_gsensorDateBlock removeAllObjects];
        }
        else
        {
            //NSLog(@"time: %f, %f, %f, %f", timePoint, timePoint + range, [pFirst timePoint], [pLast timePoint]);
            if((timePoint < [pFirst timePoint])&&(timePoint + range > [pFirst timePoint]))
            {
                //NSLog(@"count : %d",[_gsensorDateBlock count]);
                int i = 0;
                for(i = 0; i < [_gsensorDateBlock count]; i++)
                {
                    CGensorValue* pObj = [_gsensorDateBlock objectAtIndex:i];
                    if ([pObj timePoint] > timePoint + range) {
                        break;
                    }
                }
                NSRange rg ;
                rg.location = i;
                rg.length = [_gsensorDateBlock count] - i;
                //NSLog(@"remove: %d, %d, %@", rg.location, rg.length, _gsensorDateBlock);
                [_gsensorDateBlock removeObjectsInRange:rg];
                range = [pFirst timePoint] - timePoint;
            }
            else if((timePoint < [pLast timePoint])&&(timePoint + range > [pLast timePoint]))
            {
                int i = 0;
                for(i = [_gsensorDateBlock count] -1 ; i >= 0 ;i--)
                {
                    CGensorValue* pObj = [_gsensorDateBlock objectAtIndex:i];
                    if ([pObj timePoint] < timePoint) {
                        break;
                    }
                }
                NSRange rg ;
                rg.location = 0;
                rg.length = i + 1;
                [_gsensorDateBlock removeObjectsInRange:rg];
                range = timePoint + range - [pLast timePoint];
                timePoint = [pLast timePoint];
            }
            else
            {
                [_gsensorDataLock unlock];
                return;
            }
        }
    }
    [_gsensorDataLock unlock];
    //NSLog(@"cmd time : %f --- %f", timePoint, timePoint + range);
    char tmp[VDB_CMD_SIZE];
    vdb_cmd_GetRawDataBlock_s* cmd = (vdb_cmd_GetRawDataBlock_s*)tmp;
    cmd->header.cmd_code = VDB_CMD_GetRawDataBlock;
    cmd->clip_type = dir;
    cmd->clip_id = clip;
    cmd->clip_time_ms_hi = ((unsigned long long)(timePoint * 1000) / 0xffffffff);
    cmd->clip_time_ms_lo = ((unsigned long long)(timePoint * 1000) % 0xffffffff);
    cmd->length_ms = (UINT32)ceil(range * 1000);
    cmd->data_type = kRawData_ACC;
    //NSLog(@"start time: %f, range: %d", timePoint, cmd->length_ms);
    _pVDBCmdSession->Send(tmp, VDB_CMD_SIZE);
}

void TCP_VDB_Session::GetGpsData(int dir, unsigned int clip, double timePoint)
{
    if(!_pVDBCmdSession->isConnected())
        return;
    char tmp[VDB_CMD_SIZE];
    vdb_cmd_GetRawData_s* cmd = (vdb_cmd_GetRawData_s*)tmp;
    cmd->header.cmd_code = VDB_CMD_GetRawData;
    cmd->clip_type = dir;
    cmd->clip_id = clip;
    cmd->clip_time_ms_hi = ((unsigned long long)(timePoint * 1000) / 0xffffffff);
    cmd->clip_time_ms_lo = ((unsigned long long)(timePoint * 1000) % 0xffffffff);
    cmd->data_types = (1<<kRawData_GPS)|(1<<kRawData_ODB);
    int rt = _pVDBCmdSession->Send(tmp, VDB_CMD_SIZE);
    NSLog(@"- %d", rt);
}


void TCP_VDB_Session::GetFirstUrl(int dir, unsigned int clip, double timePoint, char* buffer, int len)
{
    if(!_pVDBCmdSession->isConnected())
        return;
    _urlBuffer = buffer;
    _urlBufferlen = len;
    char tmp[VDB_CMD_SIZE];
    vdb_cmd_GetPlaybackUrl_s* cmd = (vdb_cmd_GetPlaybackUrl_s*)tmp;
    cmd->header.cmd_code = VDB_CMD_GetPlaybackUrl;
    cmd->clip_type = dir;
    cmd->clip_id = clip;
    cmd->url_type = URL_TYPE_HLS;
    cmd->clip_time_ms_hi = ((unsigned long long)(timePoint * 1000) / 0xffffffff);
    cmd->clip_time_ms_lo = ((unsigned long long)(timePoint * 1000) % 0xffffffff);
    _pVDBCmdSession->Send(tmp, VDB_CMD_SIZE);
    //NSLog(@"timePoint : %f, hi: %d, low: %d", timePoint, cmd->clip_time_ms_hi, cmd->clip_time_ms_lo);
    //_pCmdSync->Take(3000000);
    [_pCondition lock];
    NSLog(@"wait url");
    [_pCondition wait];
    [_pCondition unlock];
}*/


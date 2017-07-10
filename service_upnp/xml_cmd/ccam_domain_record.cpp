/*****************************************************************************
 * ccam_domain_record.cpp
 *****************************************************************************
 * Author: linnsong <llsong@cam2cloud.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 *
 *****************************************************************************/
#include "ccam_domain_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef isServer
#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"
#endif

SCMD_DOMIAN_CONSTRUCTOR(DomainRec, CMD_Domain_rec, CMD_Rec_num)
{
#ifdef isServer
	_iPropNum = RecProps_num;
	_pProperties = new Property* [RecProps_num];
	_pProperties[RecProp_resolution] = new Property(PropName_rec_resoluton,Video_Resolution_num);
	_pProperties[RecProp_resolution]->setString(Video_Resolution_1080p30, "1080p30");
	_pProperties[RecProp_resolution]->setString(Video_Resolution_720p60, "720p60");
	_pProperties[RecProp_resolution]->setString(Video_Resolution_720p30, "720p30");
	_pProperties[RecProp_resolution]->setString(Video_Resolution_480p30, "480p30");

	_pProperties[RecProp_quality] = new Property(PropName_rec_quality,Video_Qauality_num);
	_pProperties[RecProp_quality]->setString(Video_Qauality_Supper, "HQ");
	_pProperties[RecProp_quality]->setString(Video_Qauality_HI, "NORMAL");
	_pProperties[RecProp_quality]->setString(Video_Qauality_Mid, "SOSO");
	_pProperties[RecProp_quality]->setString(Video_Qauality_LOW, "LONG");

	_pProperties[RecProp_recMode] = new Property(PropName_rec_recMode,Rec_Mode_num);
	_pProperties[RecProp_recMode]->setString(Rec_Mode_Manual, "NORMAL");
	_pProperties[RecProp_recMode]->setString(Rec_Mode_AutoStart, "AutoStart");
	_pProperties[RecProp_recMode]->setString(Rec_Mode_Manual_circle, "Circle");
	_pProperties[RecProp_recMode]->setString(Rec_Mode_AutoStart_circle, "CAR");

	_pProperties[RecProp_colormode] = new Property(PropName_rec_colorMode,Color_Mode_num);
	_pProperties[RecProp_colormode]->setString(Color_Mode_NORMAL, "NORMAL");
	_pProperties[RecProp_colormode]->setString(Color_Mode_SPORT, "SPORT");
	_pProperties[RecProp_colormode]->setString(Color_Mode_CARDV, "CARDV");
	_pProperties[RecProp_colormode]->setString(Color_Mode_SCENE, "SCENE");

	SCMD_CMD_NEW(StartRecord1, this);
	SCMD_CMD_NEW(StopRecord1, this);
	SCMD_CMD_NEW(ListResolution, this);
	SCMD_CMD_NEW(GetResolution, this);
	SCMD_CMD_NEW(SetResolution, this);

	SCMD_CMD_NEW(ListQuality, this);
	SCMD_CMD_NEW(SetQuality, this);
	SCMD_CMD_NEW(GetQuality, this);

	SCMD_CMD_NEW(ListRecMode, this);
	SCMD_CMD_NEW(SetRecMode, this);
	SCMD_CMD_NEW(GetRecMode, this);

	SCMD_CMD_NEW(ListColorMode, this);
	SCMD_CMD_NEW(SetColorMode, this);
	SCMD_CMD_NEW(GetColorMode, this);


	SCMD_CMD_NEW(SetOverlay, this);
	SCMD_CMD_NEW(GetOverlay, this);

	SCMD_CMD_NEW(SetDualStream, this);
	SCMD_CMD_NEW(GetDualStreamState, this);
	//SCMD_CMD_NEW()
#else
	SCMD_CMD_NEW(EvtStateChanged, this);
	SCMD_CMD_NEW(ListResolution, this);

	SCMD_CMD_NEW(ListQuality, this);
	SCMD_CMD_NEW(GetQuality, this);

	SCMD_CMD_NEW(ListColorMode, this);
	SCMD_CMD_NEW(GetColorMode, this);
#endif
}


SCMD_DOMIAN_DISTRUCTOR(DomainRec)
{
	#ifdef isServer
	#endif
}

#ifdef isServer
SCMD_CMD_EXECUTE(StartRecord1)
{
	//char tmp[128];
	//IPC_AVF_Client::getObject()->StartRecording(tmp, sizeof(tmp));
	printf("SCMD_CMD_EXECUTE StartRecord1\n");
	return 0;
}

SCMD_CMD_EXECUTE(StopRecord1)
{
	//char tmp[128];
	//IPC_AVF_Client::getObject()->StopRecording();
	return 0;
}

SCMD_CMD_EXECUTE(ListResolution)
{
	EnumedStringCMD* cmd = p;
	Property* pr = this->getDomainObj()->getProperty(RecProp_resolution);
	char tmp[64];
	sprintf(tmp, "%lld", pr->GetAvalible());
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_List_Resolutions, tmp,NULL);
	StringCMD *pp = &result;
	StringEnvelope ev(&pp, 1);
	printf("send : %s\n", ev.getBuffer());
	if(cmd != NULL)
	{
		cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
	}
	else // evnet broad cast
	{
		BroadCaster* b = this->getDomainObj()->getBroadCaster();
		if(b)
			b->BroadCast(&ev);
	}
	return 0;
}

SCMD_CMD_EXECUTE(GetResolution)
{
	EnumedStringCMD* cmd = p;
	int rt = 0;
	Property* pr = this->getDomainObj()->getProperty(RecProp_resolution);
	char tmp[64];
	sprintf(tmp, "%d",pr->load());
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_Resolution, tmp,NULL);
	result.Send(cmd);
	return 0;
}

SCMD_CMD_EXECUTE(SetResolution)
{
	EnumedStringCMD* cmd = p;
	int rt = atoi(cmd->getPara1());
	Property* pp = this->getDomainObj()->getProperty(RecProp_resolution);
	pp->save(rt);
	
	return 0;
}


SCMD_CMD_EXECUTE(ListQuality)
{
	EnumedStringCMD* cmd = p;
	Property* pp = this->getDomainObj()->getProperty(RecProp_quality);
	char tmp[64];
	sprintf(tmp, "%lld", pp->GetAvalible());
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_List_Qualities, tmp,NULL);
    result.Send(cmd);
	return 0;
}


SCMD_CMD_EXECUTE(SetQuality)
{
	printf("--server set quality\n");
	EnumedStringCMD* cmd = p;
	int rt = atoi(cmd->getPara1());
	Property* pp = this->getDomainObj()->getProperty(RecProp_quality);
	pp->save(rt);
	return 0;
}

SCMD_CMD_EXECUTE(GetQuality)
{
	EnumedStringCMD* cmd = p;
	int rt = 0;
	Property* pp = this->getDomainObj()->getProperty(RecProp_quality);
	char tmp[64];
	sprintf(tmp, "%d",pp->load());
	
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_Quality, tmp,NULL);
    result.Send(cmd);
	return 0;
}

SCMD_CMD_EXECUTE(ListRecMode)
{
	EnumedStringCMD* cmd = p;
	Property* pp = this->getDomainObj()->getProperty(RecProp_recMode);
	char tmp[64];
	sprintf(tmp, "%lld", pp->GetAvalible());
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_List_RecModes, tmp,NULL);
    result.Send(cmd);
	return 0;
}

SCMD_CMD_EXECUTE(SetRecMode)
{
	EnumedStringCMD* cmd = p;
	int rt = atoi(cmd->getPara1());
	Property* pp = this->getDomainObj()->getProperty(RecProp_recMode);
	pp->save(rt);
	return 0;
}

SCMD_CMD_EXECUTE(GetRecMode)
{
	EnumedStringCMD* cmd = p;
	int rt = 0;
	Property* pp = this->getDomainObj()->getProperty(RecProp_recMode);
	char tmp[64];
	sprintf(tmp, "%d",pp->load());
	
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_RecMode, tmp,NULL);
    result.Send(cmd);
	return 0;
}

SCMD_CMD_EXECUTE(ListColorMode)
{
	EnumedStringCMD* cmd = p;
	Property* pp = this->getDomainObj()->getProperty(RecProp_colormode);
	char tmp[64];
	sprintf(tmp, "%lld", pp->GetAvalible());
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_List_ColorModes, tmp,NULL);
    result.Send(cmd);
	return 0;
}

SCMD_CMD_EXECUTE(SetColorMode)
{
	EnumedStringCMD* cmd = p;
	int rt = atoi(cmd->getPara1());
	Property* pp = this->getDomainObj()->getProperty(RecProp_colormode);
	pp->save(rt);
	return 0;
}

SCMD_CMD_EXECUTE(GetColorMode)
{
	EnumedStringCMD* cmd = p;
	int rt = 0;
	Property* pp = this->getDomainObj()->getProperty(RecProp_colormode);
	char tmp[64];
	sprintf(tmp, "%d",pp->load());
	
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_ColorMode, tmp,NULL);
	result.Send(cmd);
	return 0;
}

SCMD_CMD_EXECUTE(SetOverlay)
{
	EnumedStringCMD* cmd = p;
	int rt = atoi(cmd->getPara1());
	if(rt & 0x01)
	{	
		agcmd_property_set("system.ui.namestamp", "On");
	}
	else
	{
		agcmd_property_set("system.ui.namestamp", "Off");
	}

	if(rt & 0x02)
	{	
		agcmd_property_set("system.ui.timestamp", "On");
	}
	else
	{
		agcmd_property_set("system.ui.timestamp", "Off");
	}

	if(rt & 0x04)
	{	
		agcmd_property_set("system.ui.coordstamp", "On");
	}
	else
	{
		agcmd_property_set("system.ui.coordstamp", "Off");
	}

	if(rt & 0x08)
	{	
		agcmd_property_set("system.ui.speedstamp", "On");
	}
	else
	{
		agcmd_property_set("system.ui.speedstamp", "Off");
	}
	printf("set overlay : %d\n", rt);
	IPC_AVF_Client::getObject()->UpdateSubtitleConfig();
	return 0;
}

SCMD_CMD_EXECUTE(GetOverlay)
{
	EnumedStringCMD* cmd = p;
	char tmp[16];
	int c = 0;
	agcmd_property_get("system.ui.namestamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x01;
	}
	agcmd_property_get("system.ui.timestamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x02;
	}
	agcmd_property_get("system.ui.coordstamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x04;
	}
	agcmd_property_get("system.ui.speedstamp", tmp, "On");
	if(strcmp(tmp, "On") == 0)
	{
		c = c | 0x08;
	}	
	sprintf(tmp, "%d", c);
	EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_getOverlayState, tmp,NULL);
	result.Send(cmd);
	return 0;

}

SCMD_CMD_EXECUTE(SetDualStream)
{


}
SCMD_CMD_EXECUTE(GetDualStreamState)
{


}

#else

SCMD_CMD_EXECUTE(EvtStateChanged)
{
	

	return 0;
}

SCMD_CMD_EXECUTE(ListResolution)
{
	EnumedStringCMD* cmd = p;
	unsigned long long rt = 0;
	if(cmd)
	{
		rt = atoll(cmd->getPara1());
        printf("get list : %d\n", rt);
	}
	return 0;
}

SCMD_CMD_EXECUTE(GetResolution)
{
	EnumedStringCMD* cmd = p;
	if(cmd)
	{
		int rt = atoi(cmd->getPara1()); 
		printf("get resolution : %d\n", rt);
	}
	return 0;
}

SCMD_CMD_EXECUTE(ListQuality)
{
	
	return 0;
}

SCMD_CMD_EXECUTE(GetQuality)
{
	
	return 0;
}

SCMD_CMD_EXECUTE(ListColorMode)
{
	
	return 0;
}

SCMD_CMD_EXECUTE(GetColorMode)
{
	
	return 0;
}

#endif
//IPC_Obj::getObject()->getRecordingTime();


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

#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"

SCMD_DOMIAN_CONSTRUCTOR(DomainRec, CMD_Domain_rec, CMD_Rec_num)
{
    _iPropNum = RecProps_num;
    _pProperties = new Property* [RecProps_num];
    memset(_pProperties, 0x00, sizeof(Property*) * RecProps_num);

    _pProperties[RecProp_resolution] = new Property(PropName_rec_resoluton, Video_Resolution_num);
    _pProperties[RecProp_resolution]->setString(Video_Resolution_1080p30, "1080p30");
    _pProperties[RecProp_resolution]->setString(Video_Resolution_720p60, "720p60");
    _pProperties[RecProp_resolution]->setString(Video_Resolution_720p30, "720p30");
#ifdef ARCH_S2L
    _pProperties[RecProp_resolution]->setString(Video_Resolution_QXGAp30, "QXGAp30");
    _pProperties[RecProp_resolution]->setString(Video_Resolution_1080p60, "1080p60");
    _pProperties[RecProp_resolution]->setString(Video_Resolution_Still, "stillCapture");
    // 720p120 is only available with 178.
    char tmp_prop[AGCMD_PROPERTY_SIZE];
    agcmd_property_get("prebuild.board.vin0", tmp_prop, "imx178");
    if (strncasecmp(tmp_prop, "imx178", strlen("imx178")) == 0) {
        _pProperties[RecProp_resolution]->setString(Video_Resolution_720p120, "720p120");
    }
#endif
#ifdef ARCH_A5S
    _pProperties[RecProp_resolution]->setString(Video_Resolution_480p30, "480p30");
#endif

    _pProperties[RecProp_quality] = new Property(PropName_rec_quality, Video_Qauality_num);
    _pProperties[RecProp_quality]->setString(Video_Qauality_Supper, "HQ");
    _pProperties[RecProp_quality]->setString(Video_Qauality_HI, "NORMAL");
    _pProperties[RecProp_quality]->setString(Video_Qauality_Mid, "SOSO");
    _pProperties[RecProp_quality]->setString(Video_Qauality_LOW, "LOW");

    _pProperties[RecProp_recMode] = new Property(PropName_rec_recMode, Rec_Mode_num);
    _pProperties[RecProp_recMode]->setString(Rec_Mode_Manual, "NORMAL");
    _pProperties[RecProp_recMode]->setString(Rec_Mode_AutoStart, "AutoStart");
    _pProperties[RecProp_recMode]->setString(Rec_Mode_Manual_circle, "Circle");
    _pProperties[RecProp_recMode]->setString(Rec_Mode_AutoStart_circle, "CAR");

    _pProperties[RecProp_colormode] = new Property(PropName_rec_colorMode, Color_Mode_num);
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

    SCMD_CMD_NEW(GetRotateMode, this);
    SCMD_CMD_NEW(SetRotateMode, this);
    SCMD_CMD_NEW(MarkLiveVideo, this);
    SCMD_CMD_NEW(SetMarkTime, this);
    SCMD_CMD_NEW(GetMarkTime, this);

    SCMD_CMD_NEW(GetVideoQuality, this);
    SCMD_CMD_NEW(SetVideoQuality, this);

    SCMD_CMD_NEW(SetStillMode, this);
    SCMD_CMD_NEW(StartStillCapture, this);
    SCMD_CMD_NEW(StopStillCapture, this);
}


SCMD_DOMIAN_DISTRUCTOR(DomainRec)
{
    for (int i = 0; i < RecProps_num; i++) {
        delete _pProperties[i];
    }
    delete[] _pProperties;
}

SCMD_CMD_EXECUTE(StartRecord1)
{
    //IPC_AVF_Client::getObject()->StartRecording();
    CAMERALOG("SCMD_CMD_EXECUTE StartRecord1");
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
    snprintf(tmp, 64, "%lld", pr->GetAvalible());
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_List_Resolutions, tmp, NULL);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    CAMERALOG("send : %s", ev.getBuffer());
    if (cmd != NULL) {
        cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    } else {// evnet broad cast
        BroadCaster* b = this->getDomainObj()->getBroadCaster();
        if (b) {
            b->BroadCast(&ev);
        }
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetResolution)
{
    EnumedStringCMD* cmd = p;
    Property* pr = this->getDomainObj()->getProperty(RecProp_resolution);
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%d", pr->load());
    char resolution[64] = {0x00};
    agcmd_property_get(PropName_rec_resoluton, resolution, "1080p30");
    if (strcasecmp(resolution, "1080p23976") == 0) {
        snprintf(tmp, sizeof(tmp), "%d", Video_Resolution_1080p30);
    }
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_Resolution, tmp, NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(SetResolution)
{
    EnumedStringCMD* cmd = p;
    int rt = atoi(cmd->getPara1());
    Property* pp = this->getDomainObj()->getProperty(RecProp_resolution);
    pp->save(rt);

    char tmp[] = "0";
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Set_Resolution, tmp, NULL);
    result.Send(cmd);

    return 0;
}


SCMD_CMD_EXECUTE(ListQuality)
{
    EnumedStringCMD* cmd = p;
    Property* pp = this->getDomainObj()->getProperty(RecProp_quality);
    char tmp[64];
    snprintf(tmp, 64, "%lld", pp->GetAvalible());
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_List_Qualities, tmp, NULL);
    result.Send(cmd);
    return 0;
}


SCMD_CMD_EXECUTE(SetQuality)
{
    CAMERALOG("--server set quality");
    EnumedStringCMD* cmd = p;
    int rt = atoi(cmd->getPara1());
    Property* pp = this->getDomainObj()->getProperty(RecProp_quality);
    pp->save(rt);

    char tmp[] = "0";
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Set_Quality, tmp, NULL);
    result.Send(cmd);
    return 0;
}

#ifdef PLATFORMHACHI

SCMD_CMD_EXECUTE(GetQuality)
{
    EnumedStringCMD* cmd = p;

    char bitrate[32];
    char second_bitrate[32];
    snprintf(bitrate, 32, "%d", IPC_AVF_Client::getObject()->getBitrate());
    snprintf(second_bitrate, 32, "%d", IPC_AVF_Client::getObject()->getSecondVideoBitrate());

    EnumedStringCMD result(CMD_Domain_cam, CMD_Rec_get_Quality, bitrate, second_bitrate);
    result.Send(cmd);
    return 0;
}

#else

SCMD_CMD_EXECUTE(GetQuality)
{
    EnumedStringCMD* cmd = p;
    Property* pp = this->getDomainObj()->getProperty(RecProp_quality);
    char tmp[64];
    snprintf(tmp, 64, "%d", pp->load());

    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_Quality, tmp, NULL);
    result.Send(cmd);
    return 0;
}

#endif

SCMD_CMD_EXECUTE(ListRecMode)
{
    EnumedStringCMD* cmd = p;
    Property* pp = this->getDomainObj()->getProperty(RecProp_recMode);
    char tmp[64];
    snprintf(tmp, 64, "%lld", pp->GetAvalible());
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_List_RecModes, tmp,NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(SetRecMode)
{
    EnumedStringCMD* cmd = p;
    int rt = atoi(cmd->getPara1());

#ifdef PLATFORMHACHI
    if ((rt != Rec_Mode_Mannual_Recording)
        && (rt != Rec_Mode_Continous_Recording))
    {
        rt = Rec_Mode_Continous_Recording;
    }
#endif

//Support Auto power on in Car mode
#if !defined(PLATFORMHACHI)
    Property* pp = this->getDomainObj()->getProperty(RecProp_recMode);
    // TODO: enable record mode change in future for hachi
    pp->save(rt);

{
    int ret_val = -1;
    int auto_boot = 1;
    int udc_power_detect = 0;
    int udc_power_delay = 60; // in Car mode, auto power off after the delay if unplug USB
    const char *ab_args[8];
    char ab_str[AGCMD_PROPERTY_SIZE];
    char ab_prop_str[AGCMD_PROPERTY_SIZE];

    if (pp->load() == Rec_Mode_AutoStart_circle) {
        auto_boot = 2;
        udc_power_detect = 1;
        char delay_sec[AGCMD_PROPERTY_SIZE];
        memset(delay_sec, 0x00, sizeof(delay_sec));
        agcmd_property_get(PropName_Udc_Power_Delay, delay_sec, Udc_Power_Delay_Default);
        // TODO: 
        udc_power_delay = atoi(delay_sec);
    }
    snprintf(ab_str, sizeof(ab_str), "%d", auto_boot);
    agcmd_property_get("temp.hw.auto_boot", ab_prop_str, "1");
    if (strncmp(ab_str, ab_prop_str, strlen("ab_str")) != 0) {
        CAMERALOG("AutoBoot:ab_str = %s, ab_prop_str = %s", ab_str, ab_prop_str);
        ab_args[0] = "agboard_helper";
        ab_args[1] = "--auto_boot";
        ab_args[2] = ab_str;
        ab_args[3] = NULL;
        ret_val = agcmd_agexecvp(ab_args[0], (char * const *)ab_args);
        if (ret_val) {
            CAMERALOG("agboard_helper(auto_boot, %s) = %d.",
                ab_str, ret_val);
        }

        char value[8];
        memset(value, 0x00, 8);
        snprintf(value, 8, "%d", udc_power_detect);
        agcmd_property_set("system.config.udc_power_detect", value);
        memset(value, 0x00, 8);
        snprintf(value, 8, "%d", udc_power_delay);
        agcmd_property_set("system.config.udc_power_delay", value);
    }
}
#endif

    char tmp[] = "0";
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Set_RecMode, tmp, NULL);
    result.Send(cmd);

    return 0;
}

SCMD_CMD_EXECUTE(GetRecMode)
{
    EnumedStringCMD* cmd = p;
    Property* pp = this->getDomainObj()->getProperty(RecProp_recMode);
    char tmp[64];

    int rec_mode = pp->load();
#ifdef PLATFORMHACHI
    if ((rec_mode != Rec_Mode_Mannual_Recording)
        && (rec_mode != Rec_Mode_Continous_Recording))
    {
        rec_mode = Rec_Mode_Continous_Recording;
        pp->save(rec_mode);
    }
#endif

    snprintf(tmp, 64, "%d", rec_mode);
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_RecMode, tmp, NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(ListColorMode)
{
    EnumedStringCMD* cmd = p;
    Property* pp = this->getDomainObj()->getProperty(RecProp_colormode);
    char tmp[64];
    snprintf(tmp, 64, "%lld", pp->GetAvalible());
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

    char tmp[] = "0";
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Set_ColorMode, tmp, NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(GetColorMode)
{
    EnumedStringCMD* cmd = p;
    Property* pp = this->getDomainObj()->getProperty(RecProp_colormode);
    char tmp[64];
    snprintf(tmp, 64, "%d", pp->load());

    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_get_ColorMode, tmp,NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(SetOverlay)
{
    EnumedStringCMD* cmd = p;
    int rt = atoi(cmd->getPara1());
    if (rt & 0x01) {
        agcmd_property_set("system.ui.namestamp", "On");
    } else {
        agcmd_property_set("system.ui.namestamp", "Off");
    }

    if (rt & 0x02) {
        agcmd_property_set("system.ui.timestamp", "On");
    } else {
        agcmd_property_set("system.ui.timestamp", "Off");
    }

    if (rt & 0x04) {
        agcmd_property_set("system.ui.coordstamp", "On");
    } else {
        agcmd_property_set("system.ui.coordstamp", "Off");
    }

    if (rt & 0x08) {
        agcmd_property_set("system.ui.speedstamp", "On");
    } else {
        agcmd_property_set("system.ui.speedstamp", "Off");
    }
    CAMERALOG("set overlay : %d", rt);
    IPC_AVF_Client::getObject()->UpdateSubtitleConfig();

    char tmp[] = "0";
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_setOverlay, tmp, NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(GetOverlay)
{
    EnumedStringCMD* cmd = p;
    char tmp[16];
    int c = 0;
#if defined(PLATFORMHACHI)
    const char *defaultVal = "Off";
#else
    const char *defaultVal = "On";
#endif
    agcmd_property_get("system.ui.namestamp", tmp, defaultVal);
    if (strcasecmp(tmp, "On") == 0) {
        c = c | 0x01;
    }
    agcmd_property_get("system.ui.timestamp", tmp, defaultVal);
    if (strcasecmp(tmp, "On") == 0) {
        c = c | 0x02;
    }
    agcmd_property_get("system.ui.coordstamp", tmp, defaultVal);
    if (strcasecmp(tmp, "On") == 0) {
        c = c | 0x04;
    }
    agcmd_property_get("system.ui.speedstamp", tmp, defaultVal);
    if (strcasecmp(tmp, "On") == 0) {
        c = c | 0x08;
    }
    snprintf(tmp, 16, "%d", c);
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_getOverlayState, tmp,NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(SetDualStream)
{
    return 0;
}
SCMD_CMD_EXECUTE(GetDualStreamState)
{
    return 0;
}

SCMD_CMD_EXECUTE(GetRotateMode)
{
    EnumedStringCMD* cmd = p;

    // current mode
    char rotate_mode[256];
    memset(rotate_mode, 0x00, 256);
    agcmd_property_get(PropName_Rotate_Mode, rotate_mode, Rotate_Mode_Normal);

    // current rotation
    char *rotation = NULL;
    char horizontal[256];
    char vertical[256];
    agcmd_property_get(PropName_Rotate_Horizontal, horizontal, "0");
    agcmd_property_get(PropName_Rotate_Vertical, vertical, "0");
    if ((strcasecmp(horizontal, "0") == 0)
        && (strcasecmp(vertical, "0") == 0))
    {
        rotation = (char*)Rotate_Mode_Normal;
    } else if ((strcasecmp(horizontal, "1") == 0)
         && (strcasecmp(vertical, "1") == 0))
    {
        rotation = (char*)Rotate_Mode_180;
    }

    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Get_Rotate_Mode, rotate_mode, rotation);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    return 0;
}
SCMD_CMD_EXECUTE(SetRotateMode)
{
    EnumedStringCMD* cmd = p;
    if (strcasecmp(cmd->getPara1(), Rotate_Mode_Auto) == 0) {
        agcmd_property_set(PropName_Rotate_Mode, Rotate_Mode_Auto);
    } else if (strcasecmp(cmd->getPara1(), Rotate_Mode_Normal) == 0) {
        if (IPC_AVF_Client::getObject()) {
            if (IPC_AVF_Client::getObject()->setOSDRotate(0)) {
                agcmd_property_set(PropName_Rotate_Mode, Rotate_Mode_Normal);
            }
        }
    } else if (strcasecmp(cmd->getPara1(), Rotate_Mode_180) == 0) {
        if (IPC_AVF_Client::getObject()) {
            if (IPC_AVF_Client::getObject()->setOSDRotate(1)) {
                agcmd_property_set(PropName_Rotate_Mode, Rotate_Mode_180);
            }
        }
    }

    // the result will be broadcast to all clients through property change
    return 0;
}

SCMD_CMD_EXECUTE(MarkLiveVideo)
{
    EnumedStringCMD* cmd = p;
    bool ret = false;
    if (IPC_AVF_Client::getObject()) {
        ret = IPC_AVF_Client::getObject()->markVideo();
    }

    char tmp[16];
    snprintf(tmp, 16, "%d", ret ? 0 : -1);
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Mark_Live_Video, tmp, NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(SetMarkTime)
{
    EnumedStringCMD* cmd = p;
    if ((cmd->getPara1() != NULL) && (cmd->getPara2() != NULL)) {
        agcmd_property_set(PropName_Mark_before, cmd->getPara1());
        agcmd_property_set(PropName_Mark_after, cmd->getPara2());
    } else {
        return 0;
    }

    // broadcast the result to all clients
    char before[256];
    char after[256];
    memset(before, 0x00, 256);
    memset(after, 0x00, 256);
    agcmd_property_get(PropName_Mark_before, before, Mark_before_time);
    agcmd_property_get(PropName_Mark_after, after, Mark_after_time);
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Get_Mark_Time, before, after);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    BroadCaster* b = this->getDomainObj()->getBroadCaster();
    if (b) {
        b->BroadCast(&ev);
    }
    return 0;
}

SCMD_CMD_EXECUTE(GetMarkTime)
{
    EnumedStringCMD* cmd = p;
    char before[256];
    char after[256];
    memset(before, 0x00, 256);
    memset(after, 0x00, 256);
    agcmd_property_get(PropName_Mark_before, before, Mark_before_time);
    agcmd_property_get(PropName_Mark_after, after, Mark_after_time);
    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Get_Mark_Time, before, after);
    StringCMD *pp = &result;
    StringEnvelope ev(&pp, 1);
    cmd->SendBack(ev.getBuffer(), ev.getBufferLen());
    return 0;
}

SCMD_CMD_EXECUTE(GetVideoQuality)
{
    EnumedStringCMD* cmd = p;

    char tmp[64];
    agcmd_property_get(PropName_rec_quality, tmp, Quality_Normal);
    char quality[64];
    if (strcasecmp(tmp, Quality_Normal) == 0) {
        snprintf(quality, sizeof(quality), "Normal");
    } else {
        snprintf(quality, sizeof(quality), "High");
    }

    EnumedStringCMD result(CMD_Domain_cam, CMD_Rec_get_Quality, quality, NULL);
    result.Send(cmd);
    return 0;
}

SCMD_CMD_EXECUTE(SetVideoQuality)
{
    CAMERALOG("--server set quality");
    EnumedStringCMD* cmd = p;
    const char *quality = cmd->getPara1();
    const char *ret = "0";
    if (strcmp(quality, "High") == 0) {
        agcmd_property_set(PropName_rec_quality, Quality_High);
        ret = "1";
    } else if (strcmp(quality, "Normal") == 0) {
        agcmd_property_set(PropName_rec_quality, Quality_Normal);
        ret = "1";
    }

    EnumedStringCMD result(CMD_Domain_rec, CMD_Rec_Set_Quality, (char *)ret, NULL);
    result.Send(cmd);
    return 0;
}


SCMD_CMD_EXECUTE(SetStillMode)
{
#ifdef ENABLE_STILL_CAPTURE
    EnumedStringCMD* cmd = p;
    int rt = atoi(cmd->getPara1());
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->setStillCaptureMode(rt);
    }
#endif
    return 0;
}

SCMD_CMD_EXECUTE(StartStillCapture)
{
#ifdef ENABLE_STILL_CAPTURE
    EnumedStringCMD* cmd = p;
    int rt = atoi(cmd->getPara1());
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->startStillCapture(rt);
    }
#endif
    return 0;
}

SCMD_CMD_EXECUTE(StopStillCapture)
{
#ifdef ENABLE_STILL_CAPTURE
    if (IPC_AVF_Client::getObject()) {
        IPC_AVF_Client::getObject()->stopStillCapture();
    }
#endif
    return 0;
}


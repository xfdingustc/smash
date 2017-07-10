
/*****************************************************************************
 * ccam_mirror.cpp
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
 * 
 *****************************************************************************/
#include "ccam_mirror.h"
#include "string.h"
#include "unistd.h"

CCAM_mirror::CCAM_mirror(char* address, int16_t port)
    : _port(port)
{
    _ccamService = new TCPSessionService
        (_port
        , 10240
        , 24
        , 10240);
    _processor = new CloudCameraCMDProcessor(this);
    _ccamService->SetDelegate(_processor);
    _processor->Go();
    _ccamService->Go();

    memset(_address, 0x00, sizeof(_address));
    if (address) {
        memcpy(_address, address, strlen(address));
    }
}

CCAM_mirror::~CCAM_mirror()
{
    _ccamService->Stop();
    _processor->Stop();
    delete _processor;
    delete _ccamService;
}

void CCAM_mirror::BroadCast(StringEnvelope* ev)
{
    if ((_ccamService) && (ev))
    {
        _ccamService->BroadCast(ev->getBuffer(), strlen(ev->getBuffer()));
    }
}

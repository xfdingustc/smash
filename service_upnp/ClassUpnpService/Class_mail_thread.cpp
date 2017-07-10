/*****************************************************************************
 * class_mail_thread.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 -, linnsong.
 *
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <string>
#include "class_ipc_rec.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/select.h">
#include <sys/inotify.h>
#include <sys/time.h>

#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"

#include "class_mail_thread.h"

//#include "class_upnp_description.h"
/***************************************************************
CMail
*/
CMail::CMail
	(TrigerMailEvent event
	, char* movieClipPath
	, char* posterPath
	):_event(event)
{
	printf("-- init mail\n");
	memset(_clipPath, 0, sizeof(_clipPath));
	strcpy(_clipPath, movieClipPath);

	memset(_posterPath, 0, sizeof(_clipPath));
	strcpy(_posterPath, posterPath);

	// get current time event + time = ID
	//time_t t, tRemote = 0;
	//time(&t);
	switch(_event)
	{
		case TrigerMailEvent_Alert:
			sprintf(_id, "Alert_");
			break;
		case TrigerMailEvent_DoorBell:
			sprintf(_id, "DoorBell_");
			break;
		default:
			sprintf(_id, "Unknow_");
			break;
	}
	GetLocalTimeString(_id + strlen(_id), sizeof(_id) - strlen(_id));

}

CMail::~CMail()
{

}

void CMail::GetLocalTimeString(char* tmp, int length)
{
	//printf("-- get local time \n");
	char * weekDay[]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	time_t timep;
	struct tm *p;
	time(&timep);
	p = localtime(&timep);
	snprintf(tmp, length,"%d-%d-%d_",(1900+ p->tm_year), (1 + p->tm_mon), (p->tm_mday));
	int i = strlen(tmp);
	if(i < length)
	{
		snprintf(tmp + i, length-i,"%s_%d-%d-%d", weekDay[p->tm_wday],(p->tm_hour), (p->tm_min), (p->tm_sec));
	}
	//printf("-- get local time : %s \n", tmp);
}

void CMail::AddContent(char* name, char* value)
{

}
char* CMail::getSubject()
{
	return "IS_TEST_Mail";
}

char* CMail::getAttachedMovie()
{
	return _clipPath;
}

char* CMail::getAttachedPicture()
{
	return _posterPath;
}
char* CMail::getConetentPath()
{
	return NULL;
}

char* CMail::getID()
{
	return _id;
}


/***************************************************************
CMailQueue
*/
CMailQueue::CMailQueue
	(int deepth
	):_deepth(deepth)
	,_remainNum(0)
	,_head(0)
	,_mutex("mail queue protection")
{
	_queue = new CMail*[_deepth];
}

CMailQueue::~CMailQueue()
{
	delete[] _queue;
}

bool CMailQueue::AppendMail
	(CMail::TrigerMailEvent event
	, char* movieClipPath
	, char* posterPath)
{
	if(_remainNum < _deepth)
	{
		_mutex.Take();
		CMail* pMail = new CMail(event, movieClipPath, posterPath);
		_queue[(_head + _remainNum)%_deepth] = pMail;
		_remainNum ++;
		_mutex.Give();
		printf("-- in mail queue waiting send out\n");
		return true;
	}
	else
		return false;	
}

void CMailQueue::ReleaseMail(CMail* mail)
{
	_mutex.Take();
	delete _queue[_head];
	_head++;
	_head = _head % _deepth;
	_remainNum --;
	_mutex.Give();
}

CMail* CMailQueue::GetMail()
{
	CMail *pMail = NULL;
	_mutex.Take();
	if(_remainNum > 0)
		pMail = _queue[_head];
	_mutex.Give();
	return pMail;
}

/***************************************************************
CMailServerThread
*/
CMailServerThread* CMailServerThread::_pInstance = NULL;
void CMailServerThread::destroy()
{
	if(_pInstance != NULL)
	{
		_pInstance->Stop();
		delete _pInstance;
	}
}
void CMailServerThread::create()
{
	if(_pInstance == NULL)
	{
		_pInstance = new CMailServerThread();
		_pInstance->Go();
	}
}

CMailServerThread::CMailServerThread
	():CThread("MailServerThread", CThread::NORMAL, 0, NULL)
{
	_pMailSignal = new CSemaphore(0, 1, "mail signal");
	_pQueue = new CMailQueue(20);
}

CMailServerThread::~CMailServerThread()
{
	delete _pMailSignal;
	delete _pQueue;
}

bool CMailServerThread::TrigerMail
	(CMail::TrigerMailEvent event
	, char* movieClipPath
	, char* posterPath
	)
{
	bool rt = _pQueue->AppendMail(event, movieClipPath, posterPath);
	if(!rt)
		printf("-- add mail task failed\n");
	else
	{
		_pMailSignal->Give();
	}
	return rt;
}

bool CMailServerThread::ReadConfig()
{
	char key[256];
	sprintf(key, "%s.%s", MailPropPreFix, MailPropAccount);
	agcmd_property_get(key, _mailAccount, "llsong@cam2cloud.com");
	
	sprintf(key, "%s.%s", MailPropPreFix, MailPropSendTo);
	agcmd_property_get(key, _mailSendTo, "llsong@cam2cloud.com");
	
	sprintf(key, "%s.%s", MailPropPreFix, MailPropServer);
	agcmd_property_get(key, _mailSMTP, "smtp.exmail.qq.com");
	
	sprintf(key, "%s.%s", MailPropPreFix, MailPropPwd);
	agcmd_property_get(key, _mailPwd, "leg0oken");

	return true;
}

int CMailServerThread::SendMail(CMail* pMail)
{
	int ret_val = 0;
	//struct agms_media_sendmail_config_info_s config_info;
	//struct agms_media_sendmail_file_info_s *pfile_info;
	char* makemime_args[16];
	char makemime_subject_arg[128];
	char makemime_to_arg[128];
	char makemime_from_arg[128];
	char makemime_attachment_arg[128];
	char makemime_output_arg[128];
	char makemime_input_arg[128];
	char *sendmail_args[16];
	char sendmail_sender_arg[128];
	char sendmail_smtp_arg[128];
	char sendmail_au_arg[128];
	char sendmail_ap_arg[128];
	unsigned int makemime_output_size;
	char *makemime_output_buffer;

	//pfile_info = AGMS_COMMAND_CMD_DATA(pin_msghdr);

	//AGLOG_DEBUG("%s_%d:%s\n", __func__, __LINE__, pfile_info->filename);

	makemime_args[0] = (char*) "makemime";
	makemime_args[1] = (char*) "-a";

	// event time as subject
	snprintf(makemime_subject_arg, sizeof(makemime_subject_arg),
		"Subject: %s", pMail->getSubject());
	makemime_args[2] = makemime_subject_arg;
	makemime_args[3] = (char*) "-a";
	snprintf(makemime_to_arg, sizeof(makemime_to_arg),
		"To: %s", _mailSendTo);
	makemime_args[4] = makemime_to_arg;
	makemime_args[5] = (char*) "-a";
	snprintf(makemime_from_arg, sizeof(makemime_from_arg),
		"From: %s", _mailAccount);
	makemime_args[6] = makemime_from_arg;
	makemime_args[7] = (char*) "-a";
	snprintf(makemime_attachment_arg, sizeof(makemime_attachment_arg),
		"Content-Disposition: attachment");
	makemime_args[8] = makemime_attachment_arg;
	makemime_args[9] = (char*) "-o";
	snprintf(makemime_output_arg, sizeof(makemime_output_arg),
		"/tmp/sendmail_%s.msg", pMail->getID());
	makemime_args[10] = makemime_output_arg;
	snprintf(makemime_input_arg, sizeof(makemime_input_arg),
		pMail->getAttachedMovie());
	makemime_args[11] = makemime_input_arg;
	makemime_args[12] = NULL;
	ret_val = agcmd_agexecvp(makemime_args[0], makemime_args);
	if (ret_val) {
		printf("make mime error return\n");
		goto __agms_command_sendmail_proc_exit;
	}

	makemime_output_buffer = (char*)agcmd_read_file(makemime_output_arg, &makemime_output_size);
	if (ret_val) {
		AGLOG_ERR("Can't open %s\n", makemime_output_arg);
		goto __agms_command_sendmail_proc_exit;
	}

	sendmail_args[0] = (char*) "sendmail";
	sendmail_args[1] = (char*) "-t";
	sendmail_args[2] = (char*) "-f";
	snprintf(sendmail_sender_arg, sizeof(sendmail_sender_arg),
		"%s", _mailAccount);
	sendmail_args[3] = sendmail_sender_arg;
	sendmail_args[4] = (char*) "-S";
	snprintf(sendmail_smtp_arg, sizeof(sendmail_smtp_arg),
		"%s", _mailSMTP);
	sendmail_args[5] = sendmail_smtp_arg;
	snprintf(sendmail_au_arg, sizeof(sendmail_au_arg),
		"-au%s", _mailAccount);
	sendmail_args[6] = sendmail_au_arg;
	snprintf(sendmail_ap_arg, sizeof(sendmail_ap_arg),
		"-ap%s", _mailPwd);
	sendmail_args[7] = sendmail_ap_arg;
	sendmail_args[8] = NULL;
	ret_val = agcmd_agexecvp_pipe_write(sendmail_args[0],
		(const char *const *)sendmail_args,
		makemime_output_buffer, makemime_output_size);
	free(makemime_output_buffer);
	if (ret_val != (int)makemime_output_size) {
		ret_val = -1;
		goto __agms_command_sendmail_proc_exit;
	}

__agms_command_sendmail_proc_exit:
	remove(makemime_output_arg);
	//pout_msghdr->length = AGMS_COMMAND_RET_SIZE(0);
	//*AGMS_COMMAND_RET(pout_msghdr) = ret_val;

	return ret_val;
}

void CMailServerThread::main(void * p)
{
	while(1)
	{
		_pMailSignal->Take(-1);

		// read mail config
		if(!ReadConfig())
			continue;

		// get mail
		CMail* pMail = _pQueue->GetMail();
		int count = 0;
		while(pMail != NULL)
		{
			if(SendMail(pMail))
			{
				//log mail send ok
				printf("--send mail ok for %s\n", pMail->getID());

				_pQueue->ReleaseMail(pMail);
				//get next mail
				pMail = _pQueue->GetMail();
				count = 0;
			}
			else
			{
				//log mail send err
				printf("-- send mail err : %s for %d times\n", pMail->getID(), count);
				//wait 5 seconds
				sleep(5);
				count++;
			}
		}
	}
}



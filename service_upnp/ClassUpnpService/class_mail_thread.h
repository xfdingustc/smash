/*****************************************************************************
 * class_mail_thread.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2013, linnsong.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef __H_class_mail_thread__
#define __H_class_mail_thread__

#include "GobalMacro.h"
#include "ClinuxThread.h"

#define MailPropPreFix "system.mail"
#define MailPropAccount "account"
#define MailPropPwd		"pwd"
#define MailPropServer	"server"
#define MailPropSendTo	"sendTo"

class CMail
{
public: 
	enum TrigerMailEvent
	{
		TrigerMailEvent_DoorBell = 0,
		TrigerMailEvent_Alert,
		TrigerMailEvent_Num,
	};
	CMail(TrigerMailEvent event, char* movieClipPath, char* posterPath);
	~CMail();
	//void SetMail();
	void AddContent(char* name, char* value);
	char* getSubject();
	char* getAttachedMovie();
	char* getAttachedPicture();
	char* getConetentPath();
	char* getID();
	void GetLocalTimeString(char* tmp, int length);
private:
	TrigerMailEvent _event;
	char _clipPath[MaxPathStringLength];
	char _posterPath[MaxPathStringLength];
	char _id[256];
	char* _content;
};

class CMailQueue
{
public:
	CMailQueue(int deep);
	~CMailQueue();
	bool AppendMail(CMail::TrigerMailEvent event
	, char* movieClipPath
	, char* posterPath);
	void ReleaseMail(CMail* mail);
	CMail* GetMail();
	
private:
	int			_deepth;
	int			_remainNum;
	int			_head;
	CMutex		_mutex;
	CMail** 	_queue;
};

class CMailServerThread : public CThread
{
public:
	static CMailServerThread* getInstance(){return _pInstance;};
	static void destroy();
	static void create();
	bool TrigerMail(CMail::TrigerMailEvent event, char* movieClipPath, char* posterPath);
	
protected:	
	virtual void main(void * p); // wait mail send mail. check network status.
	int SendMail(CMail* pMail);
		
private:
	static CMailServerThread* 	_pInstance;
	CMailServerThread(); // read propertys. init send structure. 
	virtual ~CMailServerThread(); 
	bool ReadConfig();
	
private:
	CMailQueue *_pQueue;
	CSemaphore *_pMailSignal;

	char	_mailAccount[256];
	char	_mailPwd[256];
	char	_mailSMTP[256];
	char	_mailSendTo[256];
};

#endif

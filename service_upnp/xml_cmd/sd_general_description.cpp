
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include ""
#ifdef isServer
#include "agbox.h"
#include "aglog.h"
#include "agcmd.h"
#endif

#include "sd_general_description.h"

/***************************************************************
StringCMD::StringCMD
*/
StringCMD::StringCMD
		(char* name
		,char* para1
		,char* para2
		//, StringController* controller
		):_name(name)
		,_para1(para1)
		,_para2(para2)
		//,_sequence(controller->GetSequence())
		,_bOut(false)
		,_bLock(false)
		,_from(0)
{
	// get sequency
	if(_name != NULL)
		_bOut = true;
}

StringCMD::~StringCMD()
{
	if(!_bOut)
	{
		if(_name)
			delete[] _name;
		if(_para1)
			delete[] _para1;
		if(_para2)
			delete[] _para2;
		if(_from)
			delete[] _from;
		{
			_name = _para1 = _para2 = _from = 0;
		}
	}
}

void StringCMD::copyName(char* name)
{
	_name = new char[strlen(name)+1];
	strcpy(_name, name);
}

void StringCMD::copyPara1(char* para1)
{
	_para1 = new char[strlen(para1)+1];
	strcpy(_para1, para1);
}

void StringCMD::copyPara2(char* para2)
{
	_para2 = new char[strlen(para2)+1];
	strcpy(_para2, para2);
}

void StringCMD::setSequence(char* seq)
{
	_sequence = atoi(seq);
}

void StringCMD::copyFromID(char* from)
{
	_from = new char[strlen(from)+1];
	strcpy(_from, from);
}


/***************************************************************
StringEnvelope::StringEnvelope
*/
StringEnvelope::StringEnvelope
	(StringCMD** cmds
	,int num
	//,StringController* pController
	) : _num(num)
	,_buffer(0)
{
	int rc = 0;
    xmlTextWriterPtr writer;
    xmlBufferPtr buf;
    //xmlChar *tmp;
    

    /* Create a new XML buffer, to which the XML document will be
     	* written */
    buf = xmlBufferCreate();
	if (buf == NULL) {
        printf("cmd construct failed\n");
    }
	else
	{
	    writer = xmlNewTextWriterMemory(buf, 0);
	    if (writer == NULL) {
	        printf("testXmlwriterMemory: Error creating the xml writer\n");
	    }
		else
		{
			//rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
			//if (rc < 0) {
			//	printf("testXmlwriterMemory: Error at xmlTextWriterStartDocument\n");
			//}
			//else
			{
				rc += xmlTextWriterStartElement(writer, BAD_CAST envelop_root_element_name);
				//rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST cmd_attr_id, "%d", _pController->GetID());
				for(int i = 0; i < _num; i++)
				{
					_cmds[i] = cmds[i];
					rc += xmlTextWriterStartElement(writer, BAD_CAST cmd_element_name);
					rc += xmlTextWriterWriteAttribute(writer, BAD_CAST cmd_attr_action, BAD_CAST _cmds[i]->getName());
					rc += xmlTextWriterWriteAttribute(writer, BAD_CAST cmd_attr_para1, BAD_CAST _cmds[i]->getPara1());
					rc += xmlTextWriterWriteAttribute(writer, BAD_CAST cmd_attr_para2, BAD_CAST _cmds[i]->getPara2());
					rc += xmlTextWriterEndElement(writer);
				}
				rc += xmlTextWriterEndElement(writer);
				rc += xmlTextWriterEndDocument(writer);
			}
			xmlFreeTextWriter(writer);
			_bufferLen = buf->use;
			_buffer = new char[_bufferLen + 1];
			strncpy(_buffer, (const char *)buf->content, _bufferLen);
			_buffer[_bufferLen] = 0;
		}
		xmlBufferFree(buf);
		_bOut = true;
	}
}

StringEnvelope::~StringEnvelope()
{
	if(!_bOut)
	{
		for(int i = 0; i < _num; i++)
		{
			//if(!_cmds[i]->isLock())
			{
				//delete _cmds[i];
				//_cmds[i] = NULL;
			}
		}
	}
	else if(_buffer != 0)
	{
		delete[] _buffer;
	}
}

void StringEnvelope::SendOut()
{

}



bool StringEnvelope::isEnvelope(char* buf, int len)
{
	char tmp[20];
	sprintf(tmp, "<%s", envelop_root_element_name);
	const char* p1 = strstr(buf, tmp);
	sprintf(tmp, "/%s>", envelop_root_element_name);
	const char* p2 = strstr(buf, tmp);
	if((p1 > 0)&&( p2 > 0)&&(p2 > p1))
		return true;
	else
		return false;

}

void StringEnvelope::ParseCmd(xmlNode* root_element)
{
	xmlNode *cur_node = NULL;
	xmlNode *tmp = NULL;
	xmlAttr *cur_attr = NULL;
	StringCMD *cmd;
	//int num;
	for (cur_node = root_element; cur_node; cur_node = cur_node->next) 
	{
	    if (cur_node->type == XML_ELEMENT_NODE) 
		{
			if(strcmp((char*)cur_node->name, envelop_root_element_name) == 0)
			{
				for (cur_attr = cur_node->properties; cur_attr; cur_attr = cur_attr->next)
				{
					tmp = cur_attr->children;
					if(strcmp((char*)cur_attr->name, envelop_attr_type_name) == 0)
					{
						_type = atoi((char*)tmp->content);
					}else if(strcmp((char*)cur_attr->name, envelop_attr_time_name) == 0)
					{
						_sendtime = atoi((char*)tmp->content);
					}
				}
			}
			else if(strcmp((char*)cur_node->name, cmd_element_name) == 0)
			{
				cmd = new StringCMD();
				 
				// set para table
				for (cur_attr = cur_node->properties; cur_attr; cur_attr = cur_attr->next)
				{
					tmp = cur_attr->children;
					//printf("%s  attr: %s=%s\n",tt, cur_attr->name, tmp->content);
					if(strcmp((char*)cur_attr->name, cmd_attr_action) == 0)
					{
						cmd->copyName((char*)tmp->content);
					}else if(strcmp((char*)cur_attr->name, cmd_attr_para1) == 0)
					{
						cmd->copyPara1((char*)tmp->content);
					}
					else if(strcmp((char*)cur_attr->name, cmd_attr_para2) == 0)
					{
						cmd->copyPara2((char*)tmp->content);
					}
					else if(strcmp((char*)cur_attr->name, cmd_attr_sn) == 0)
					{
						cmd->setSequence((char*)tmp->content);
					}
				}
				_cmds[_num] = cmd;
				_num++;
				if(_num >=in_envelope_max_cmd_num)
					return;
			}
			else
			{
				printf("not know node: %s\n",(char*)cur_node->name);
			}
			ParseCmd(cur_node->children);
		}
		else if(cur_node->type == XML_TEXT_NODE)
		{
			//printf("%content: \n", cur_node->content);
		}
		else
		{
			//printf("type:\n",cur_node->type);
		}
	}
}

StringEnvelope::StringEnvelope
	(char* buf
	,int length
	): _num(0)
{
	char tmp[20];
	sprintf(tmp, "<%s", envelop_root_element_name);
	char* p1 = strstr(buf, tmp);
	sprintf(tmp, "/%s>", envelop_root_element_name);
	char* p2 = strstr(buf, tmp);
	if((p1 > 0)&&( p2 > 0)&&(p2 > p1))
	{
    	xmlDocPtr doc; /* the resulting document tree */
    	doc = xmlReadMemory(p1, p2 + strlen(envelop_root_element_name) + 2 - p1, "noname.xml", NULL, 0);
	    if (doc == NULL) {
	        printf("Failed to parse document\n");
			//return;
	    }
		else
		{
			xmlNode *root_element = NULL;
			root_element = xmlDocGetRootElement(doc);
		    ParseCmd(root_element);
    		xmlFreeDoc(doc);
		}
	}
	_bOut = false; 
}

bool StringEnvelope::isNotEmpty()
{
	return _num > 0;
}
void StringEnvelope::print()
{
	printf("type : %d\n", _type);
	printf("num : %d\n", _num);
	for(int i = 0; i < _num; i++)
	{
		printf("cmd:%d : %s\n", i, _cmds[i]->getName());
		printf("cmd:%d para1: %s\n", i, _cmds[i]->getPara1());
		printf("cmd:%d para2: %s\n", i, _cmds[i]->getPara2());
		printf("cmd:%d seq: %d\n", i, _cmds[i]->getSeq());
	}
	printf("end : %d\n", _num);
}


/***************************************************************
StringCMDProcessor::StringCMDProcessor()
*/
StringCMDProcessor1::StringCMDProcessor1
	(): CThread("CMD Processor Thread", CThread::NORMAL, 0, NULL)
	,_head(0)
	,_num(0)
    ,_bRun(1)
{
	_lock = new CMutex("xmpp cmd queue lock");
	_pSem = new CSemaphore(0,1, "cmdProcess");
    _pStopSem = new CSemaphore(0,1, "cmdProcessStop");
}

StringCMDProcessor1::~StringCMDProcessor1()
{
	delete _lock;
	delete _pSem;
    delete _pStopSem;
}

int StringCMDProcessor1::ProcessCmd()
{
	int rt = 0;
	while(_bRun)
	{
		_pSem->Take(-1);
        //printf("---%d -----\n", _num);
		while(_num > 0)
		{
			_lock->Take();
			StringCMD* cmd = PopProcessCmd();
			_lock->Give();
			inProcessCmd(cmd);
			PostProcessCmd(cmd);
		}
	}
    _pStopSem->Give();
    
	return rt;
}

void StringCMDProcessor1::Stop()
{
    _lock->Take();
    while(_num > 0)
	{
		StringCMD* cmd = PopProcessCmd();
		PostProcessCmd(cmd);
	}
    _lock->Give();
    _bRun = 0;
    _pSem->Give();
    _pStopSem->Take(-1);
    CThread::Stop();
}

StringCMD* StringCMDProcessor1::PopProcessCmd()
{
	StringCMD* cmd = _cmdQueue[_head];
	_head = (_head + 1)% MaxCMDQueueDeapth;
	_num--;
	return cmd;
}
void StringCMDProcessor1::PostProcessCmd(StringCMD* cmd)
{
	if(cmd->isLock())
		delete cmd;
}
int StringCMDProcessor1::removeCmdsInQfor(SessionResponseSendBack* sendback)
{
	int last = _head + _num - 1;
	for (int i = _head; i < _head + _num; i++) {
		StringCMD* cmd = _cmdQueue[i%MaxCMDQueueDeapth];
		if(cmd->getResponsSendBack() == sendback) {
			if(cmd->isLock()) delete cmd;
			for (int j = i; j < _head + _num - 1; j++) {
				_cmdQueue[j%MaxCMDQueueDeapth] = _cmdQueue[(j+1)%MaxCMDQueueDeapth];
			}
			_num--;
		}
	}
	return 0;
}

int StringCMDProcessor1::ProecessEvelope
	(StringEnvelope* pEv
	,SessionResponseSendBack* resultBack)
{
	// if in control state, check sender id, then append cmd
	//TBD
	for(int i = 0; i < pEv->getNum(); i++)
	{
		pEv->GetCmd(i)->setSendBack(resultBack);
		if(appendCMD(pEv->GetCmd(i)) < 0)
			break;
	}
	_pSem->Give();
	//if not in control state, check connect cmd, check control id and password
	// if password correct,  set as connect state
	//TBD 
	return 0;
}

int StringCMDProcessor1::appendCMD
	(StringCMD* cmd)
{
	int rt = -1;
	_lock->Take();
	if(_num < MaxCMDQueueDeapth)
	{
		_cmdQueue[(_head + _num) % MaxCMDQueueDeapth] = cmd;
		cmd->setLock();
		_num++;
		//printf("append cmd : %s, %s\n",cmd->getName(), cmd->getPara1());
		rt = _num;
	}
	else
	{
		printf("StringCMDProcessor1::appendCMD overflow\n ");
		delete cmd;
	}
	_lock->Give();
	return rt;
}

void StringCMDProcessor1::main(void * p)
{
	//_pConnectSession = NULL;
	this->ProcessCmd();
}


/***************************************************************
EnumedStringCMD::EnumedStringCMD()
*/
#define EnumCMDPreFix "ECMD"
#define EnumCMDBreak "."
EnumedStringCMD::EnumedStringCMD
	(SCMD_Domain* pDomain
	, int cmd
	):inherited(_cmdBuffer, NULL, NULL)
	,_cmd(cmd)
	,_pDomain(pDomain)
{
	//printf("+++ new cmd : %d,  %d\n", pDomain->getDomain(), _cmd);
	sprintf(_cmdBuffer, "%s%d%s%d",EnumCMDPreFix , pDomain->getDomain(),EnumCMDBreak,_cmd);
	pDomain->Register(this);
}

EnumedStringCMD::EnumedStringCMD
	(int domain
	, int cmd
	, char* para1
	, char* para2
	):inherited(_cmdBuffer, para1, para2)
	,_cmd(cmd)
	,_pDomain(NULL)
{
	sprintf(_cmdBuffer, "%s%d%s%d",EnumCMDPreFix , domain,EnumCMDBreak,cmd);
	//pDomain->Register(this);
}

EnumedStringCMD::~EnumedStringCMD()
{

}

int	EnumedStringCMD::getDomain()
{
	int rt = -1;
	char *p;
	if((p = strstr(getName(), EnumCMDPreFix)) > 0)
	{
		rt = atoi(p + strlen(EnumCMDPreFix));
	}
	return rt;
}

int EnumedStringCMD::getCMD()
{
	int rt = -1;
	char *p;
	if((p = strstr(getName(), EnumCMDBreak)) > 0)
	{
		//printf("-- get cmd %s, %s\n", getName(), p);
		rt = atoi(p + strlen(EnumCMDBreak));
	}
	return rt;
}

int EnumedStringCMD::execute(EnumedStringCMD* p)
{
	printf("---not implemented cmd\n");
	return 0;
}

void EnumedStringCMD::Send(EnumedStringCMD* tricker)
{
	StringCMD *pp = this;
	StringEnvelope ev(&pp, 1);
	if(tricker != NULL)
	{
		tricker->SendBack(ev.getBuffer(), ev.getBufferLen());
	}
	else
	{
		BroadCaster* b = this->getDomainObj()->getBroadCaster();
		if(b)
			b->BroadCast(&ev);
	}

}

/***************************************************************
SCMD_Domain::SCMD_Domain()
*/
SCMD_Domain::SCMD_Domain
	(int domain
	, int cmdNum)
	:_domain(domain)
	,_CmdsListLenght(cmdNum)
	,_addedNum(0)
	,_pBroadCaster(NULL)
{
	_ppRegistedCmds = new EnumedStringCMD*[_CmdsListLenght];
    for(int i = 0; i < _CmdsListLenght; i++)
    {
        _ppRegistedCmds[i] = 0;
    }
}

SCMD_Domain::~SCMD_Domain()
{
    for(int i = 0; i < _addedNum; i++)
    {
        if(_ppRegistedCmds[i])
            delete _ppRegistedCmds[i];
    }
	delete[] _ppRegistedCmds;
}


EnumedStringCMD* SCMD_Domain::searchCMD(int cmd)
{
	int i = 0;
	for(i = 0; i < _addedNum; i++)
	{
		if(_ppRegistedCmds[i]->isCmd(cmd))
			break;
	}
	if(i < _addedNum)
	{
		return _ppRegistedCmds[i];
	}
	else
		return NULL;
}

void SCMD_Domain::Register(EnumedStringCMD* p)
{
	_ppRegistedCmds[_addedNum] = p;
	_addedNum++;
}

void SCMD_Domain::Event(int cmdID)
{
	if(_ppRegistedCmds[cmdID]!=NULL)
	{
		_ppRegistedCmds[cmdID]->execute(NULL);
	}
}

void SCMD_Domain::BroadCast(StringEnvelope* ev)
{
	if(_pBroadCaster)
		_pBroadCaster->BroadCast(ev);
}


Property::Property(char* propName
	, int OptionNumber
	//, unsigned long long avalible
	): _propername(propName)
	,_valueNumber(OptionNumber)
	,_avaliable(0)
	,_currentValue(0)
{
	_values = new const char*[OptionNumber];
	for(int i = 0; i < _valueNumber; i++)
	{
		_values[i] = NULL;
	}
};
Property::~Property()
{
	delete[] _values;
};

void Property::save(int value)
{
#ifdef isServer
	unsigned int check = 0x01<<value ;
	if(check & _avaliable)
	{
		_currentValue = value;
		printf("-- save prop: %s, %s\n", _propername, _values[_currentValue]);
		agcmd_property_set(_propername, _values[_currentValue]);
	}
#endif
};
int Property::load()
{
#ifdef isServer
	char val[256];
	memset (val, 0, sizeof(val));
	agcmd_property_get(_propername, val , "");
	for(int i = 0; i < _valueNumber; i++)
	{
		if(_values[i] != NULL)
		{
			if(strcmp(_values[i], val) == 0)
			{
				_currentValue = i; 
				break;
			}
		}
	}
	return _currentValue;
#endif
    return 0; 
};


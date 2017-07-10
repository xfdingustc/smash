/*****************************************************************************
 * class_upnp_thread.cpp:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 2012, linnsong.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include "ClinuxThread.h"
#include "class_upnp_description.h"

#include "agbox.h"
#include "GobalMacro.h"

/***************************************************************
CNodeAttribute
*/
int CNodeAttribute::getStringLength()
{
	return strlen(name) + strlen(value) + 4;
}

/***************************************************************
CUpnpDescriptionNode
*/
CUpnpDescriptionNode::CUpnpDescriptionNode
	(const char*nodeTag
	,const char* nodeValue
	,int maxSubNodeNum
	, int maxAttriNum
	) :  _maxNodNum(maxSubNodeNum)
	,_addedNodeNum(0)
	,_tag(nodeTag)
	,_value(nodeValue)
	,_maxAttriNum(maxAttriNum)
	,_addedAttriNum(0)
{
	if(_maxNodNum > 0)
		_ppUpnpNodeList = new CUpnpDescriptionNode* [_maxNodNum];
	if(_maxAttriNum > 0)
		_pAttributes = new CNodeAttribute [_maxAttriNum];
}

CUpnpDescriptionNode::CUpnpDescriptionNode
	(const char*nodeTag
	,const char* nodeValue
	,int maxSubNodeNum
	, int maxAttriNum
	,CUpnpDescriptionNode* parent
	):  _maxNodNum(maxSubNodeNum)
	,_addedNodeNum(0)
	,_tag(nodeTag)
	,_value(nodeValue)
	,_maxAttriNum(maxAttriNum)
	,_addedAttriNum(0)
	,_bNumberValue(false)
	,_floatNumber(0.0)
{
	if(_maxNodNum > 0)
		_ppUpnpNodeList = new CUpnpDescriptionNode* [_maxNodNum];
	if(_maxAttriNum > 0)
		_pAttributes = new CNodeAttribute [_maxAttriNum];
	parent->AddSubNode(this);
}


CUpnpDescriptionNode::~CUpnpDescriptionNode()
{
	if(_maxNodNum > 0)
		delete[] _ppUpnpNodeList;
	if(_maxAttriNum > 0)
		delete[] _pAttributes;
}

void CUpnpDescriptionNode::AddAttribute(const char* name, const char* value)
{
	if(_addedAttriNum < _maxAttriNum)
	{
		_pAttributes[_addedAttriNum].name = (char*)name;
		_pAttributes[_addedAttriNum].value =  (char*)value;
		_addedAttriNum++;
	}
	else
	{
		printf("added too much attributes!!!!!\n");
	}

}

void CUpnpDescriptionNode::SetNumber(double value)
{
	_bNumberValue = true;
	_floatNumber = value;
}

void CUpnpDescriptionNode::GenerateXML(char* buffer, int length, int level)
{
	int sl = 0;
	int checklen = strlen(_tag) +4 + level*4;
	for(int i = 0; i < _addedAttriNum; i++)
	{
		checklen += _pAttributes[i].getStringLength();
	}

	if(length < checklen)
	{
		printf("---short of buffer!! xml print not completed.\n");
		return;
	}
	
	for(int i =0; i < level; i++)
	{		
		sl = strlen(buffer);
		sprintf(buffer + sl, "	");	
	}
	
	sl = strlen(buffer);
	sprintf(buffer + sl, "<%s", _tag);
	for(int i = 0; i < _addedAttriNum; i++)
	{
		sl = strlen(buffer);
		sprintf(buffer + sl, " %s=\"%s\"", _pAttributes[i].name, _pAttributes[i].value);
	}
	sl = strlen(buffer);
	sprintf(buffer + sl, ">");
	
	if(_addedNodeNum > 0)
	{
		sl = strlen(buffer);
		sprintf(buffer + sl, "\n");
		for(int i = 0; i < _addedNodeNum; i++)
		{			
			sl = strlen(buffer);
			_ppUpnpNodeList[i]->GenerateXML(buffer + sl, length - sl, level + 1);
		}
	}	
	else if(_value != NULL)
	{
		if((length -sl) < (strlen(_value)))
		{
			printf("---short of buffer!! xml print not completed.\n");
			return;
		}		
		sl = strlen(buffer);
		sprintf(buffer + sl, "%s", _value);
	}
	else if(_bNumberValue)
	{
		if((length -sl) < (20))
		{
			printf("---short of buffer!! xml print not completed.\n");
			return;
		}
		sl = strlen(buffer);
		sprintf(buffer + sl, "%lf", _floatNumber);
	}
	
	
	if((length -sl) < (strlen(_tag)+(level + 1)*4))
	{
		printf("---short of buffer!! xml print not completed.\n");
		return;
	}	
	if(_addedNodeNum > 0)
	{
		for(int i =0; i < level; i++)
		{		
			sl = strlen(buffer);
			sprintf(buffer + sl, "	");	
		}
	}
	sl = strlen(buffer);
	sprintf(buffer + sl, "</%s>\n", _tag);
	return;
}

void CUpnpDescriptionNode::AddSubNode(CUpnpDescriptionNode* pNode)
{
	if(_addedNodeNum < _maxNodNum)
	{
		_ppUpnpNodeList[_addedNodeNum] = pNode;
		_addedNodeNum++;
	}
	else
	{
		printf("add too much subNod!!!!!\n");
	}
}	

void CUpnpDescriptionNode::SetValue(char* value)
{
	_value = value;
}


/***************************************************************
CUpnpService
*/
CUpnpService::CUpnpService
	(const char* ShortName
	,const char* serviceType
	,const char* serviceId
	,const char* SCPDURL
	,const char* controlURL
	,const char* eventSubURL
	,CUpnpDevice* pDevice
	,int maxStateNum
	,int masActionNum	
	):CUpnpDescriptionNode("service", NULL, 5, 0)
	,_pDevice(pDevice)
	,_pShortName(ShortName)
{
	_serviceType = new CUpnpDescriptionNode("serviceType", serviceType, 0, 0, this);
	_serviceId = new CUpnpDescriptionNode("serviceId",serviceId, 0, 0, this);
	_SCPDURL = new CUpnpDescriptionNode("SCPDURL",SCPDURL, 0, 0, this);
	_controlURL = new CUpnpDescriptionNode("controlURL", controlURL, 0, 0, this);
	_eventSubURL = new CUpnpDescriptionNode("eventSubURL", eventSubURL, 0, 0, this);

	_pServiceDoc = new CUpnpServiceDoc(pDevice->getDoc(), maxStateNum, masActionNum);

	_pUpdateAction = new CUpnpActionUpdate(this);
}

CUpnpService::~CUpnpService()
{
	delete _serviceType;
	delete _serviceId;
	delete _SCPDURL;
	delete _controlURL;
	delete _eventSubURL;
	delete _pServiceDoc;
}

int CUpnpService::UpnpActionProcess(UpnpActionRequest *par_event)
{
	const char * actionName = UpnpString_get_String(UpnpActionRequest_get_ActionName(par_event));
	CUpnpAction* pAction = _pServiceDoc->GetAction(actionName);
	if(pAction)
	{
		//printf("----action : %s\n", actionName);
		return pAction->action(par_event);
	}
	else
	{
		return UPNP_E_INVALID_ACTION;
		printf("--action not found! : %s\n", actionName);
	}
}

int CUpnpService::getEventStates(char** name, char**value, int num)
{
	int ret = 0;
	if((num) < (_pServiceDoc->GetStateNum()))
	{
		return -1;
	}
	else
	{	
		for(int i = 0; i < _pServiceDoc->GetStateNum(); i++)
		{
			if(_pServiceDoc->GetState(i)->isEventState())
			{
				name[ret] = (char*)_pServiceDoc->GetState(i)->GetNameNode()->getValue();
				value[ret] = (char*)_pServiceDoc->GetState(i)->getCurrentState();
				ret ++;
			}
		}
	}
	//printf("---getEventStates : pp\n");
	return ret;
}


char* CUpnpService::getServiceType()
{
	return (char*)this->_serviceType->getValue();
}

void CUpnpService::EventPropertyUpdate()
{
	const char* names[20];
	const char* values[20];
	int c = 0;
	for(int i = 0; (i < _pServiceDoc->GetStateNum())&&(c < 20); i++)
	{
		if(_pServiceDoc->GetState(i)->isEventState())
		{
			_pServiceDoc->GetState(i)->Update(false);
			names[c] = _pServiceDoc->GetState(i)->GetNameNode()->getValue();
			values[c] = _pServiceDoc->GetState(i)->getCurrentState();
			c++;
		}
	}
#ifndef noupnp
	UpnpNotify(this->getDevice()->getUpnpDeviceHandle(),
		this->getDevice()->getUDN(),
		this->getServiceID(),
		names,
		values,
		c);
#endif
}

/***************************************************************
CUpnpDevice
*/

CUpnpDevice::CUpnpDevice
	(const char* UDN
	,const char* id
	,const char* type
	,const char* friendlyName
	,const char* model
	,const char* modelNum
	,const char* serialNumber
	,int iconNum
	,int serviceNum
	,const char* versionMajor
	,const char* versionMinor
	): CUpnpDescriptionNode("device", NULL, 15, 0)
{
	//printf("max num: this->_maxNodNum :%d ",  this->_maxNodNum);
	_pUDN = new CUpnpDescriptionNode("UDN", UDN, 0, 0, this);
	_pdeviceType = new CUpnpDescriptionNode("deviceType", type, 0, 0, this);
	_pfriendlyName = new CUpnpDescriptionNode("friendlyName", friendlyName, 0, 0, this);
	_pmanufacturer = new CUpnpDescriptionNode("manufacturer", Device_manufatory, 0, 0, this);
	_pmanufacturerURL= new CUpnpDescriptionNode("manufacturerURL", Device_manufatory_url, 2, 0, this);
	_pmodelDescription = new CUpnpDescriptionNode("modelDescription", "NA", 0, 0, this);
	_pmodelName= new CUpnpDescriptionNode("modelName", model, 0, 0, this);
	_pmodelNumber = new CUpnpDescriptionNode("modelNumber", modelNum,0, 0, this);
	_pmodelURL = new CUpnpDescriptionNode("modelURL", Device_manufatory_url, 0, 0, this);
	_pSerialNumber = new CUpnpDescriptionNode("serialNumber", serialNumber, 0, 0, this);	
	_pIconList = new CUpnpDescriptionNode("iconList", NULL, iconNum, 0, this);	
	_pServiceList = new CUpnpDescriptionNode("serviceList", NULL, serviceNum, 0, this);	
	_pPresentationURL = new CUpnpDescriptionNode("presentationURL", "index.html", 0, 0, this);

	_pDoc = new CUpnpDeviceDoc(versionMajor, versionMinor);
	_pDoc->AddSubNode(this);	
}

CUpnpDevice::~CUpnpDevice()
{
	delete _pUDN;
	delete _pdeviceType;
	delete _pfriendlyName;
	delete _pmanufacturer;
	delete _pmanufacturerURL;
	delete _pmodelDescription;
	delete _pmodelName;
	delete _pmodelNumber;
	delete _pmodelURL;	
	delete _pSerialNumber;	
	delete _pIconList;	
	delete _pServiceList;
	delete _pPresentationURL;

	delete _pDoc;
}

void CUpnpDevice::AddService(CUpnpService* pService)
{
	_pServiceList ->AddSubNode(pService);
}

void CUpnpDevice::AddIcon(CUpnpIcon* picon)
{
	_pIconList->AddSubNode(picon);
}

void CUpnpDevice::testSetState(char* name, char* state)
{
	CUpnpService *pService;
	CUpnpServiceDoc* pDoc;
	for(int i = 0; i < _pServiceList->GetSubNodeNumber(); i++)
	{
		pService = (CUpnpService*)_pServiceList->GetSubNode(i);
		pDoc = pService->getDoc();
		if(pDoc->testSetState(name , state))
			break;
	}

}

int CUpnpDevice::upnp_action_request_handler(UpnpActionRequest *par_event)
{
	const char* devUDN = UpnpString_get_String(UpnpActionRequest_get_DevUDN(par_event));
	const char* pUDN = _pUDN->getValue();
	if((pUDN != NULL)&&(strcmp(pUDN, devUDN) == 0))
	{
		// search service, pass to service process
		const char* psvcID = UpnpString_get_String(UpnpActionRequest_get_ServiceID(par_event));
		int i;
		for(i = 0; i < _pServiceList->GetSubNodeNumber(); i++)
		{			
			const char *pID = ((CUpnpService*)(_pServiceList->GetSubNode(i)))->getServiceID();  
			if(strcmp(pID, psvcID) == 0)
			{
				break;
			}
		}
		if(i < _pServiceList->GetSubNodeNumber())
		{
			return ((CUpnpService*)(_pServiceList->GetSubNode(i)))->UpnpActionProcess(par_event);
		}
		else
		{
			printf("--SID not match!!\n");
			return UPNP_E_INVALID_SERVICE;
		}
	}
	else
	{
		printf("--UDN not match!!\n");
		return UPNP_E_INVALID_DEVICE;
	}
		
}

int CUpnpDevice::upnp_getvar_request_handler(UpnpStateVarRequest *pvr_event)
{
	printf("upnp_getvar_request_handler TBD \n");
	return UPNP_E_SUCCESS;
}

int CUpnpDevice::upnp_subscription_request_handler(UpnpSubscriptionRequest *psr_event)
{

	//const char	*Sid = NULL;
	const char* devUDN = UpnpSubscriptionRequest_get_UDN_cstr(psr_event);
	const char* pUDN = _pUDN->getValue();
	if((pUDN != NULL)&&(strcmp(pUDN, devUDN) == 0))
	{
		int i;
		const char* serviceID = UpnpString_get_String(	UpnpSubscriptionRequest_get_ServiceId(psr_event));
		for(i = 0; i < _pServiceList->GetSubNodeNumber(); i++)
		{			
			const char *pID = ((CUpnpService*)(_pServiceList->GetSubNode(i)))->getServiceID();  
			if(strcmp(pID, serviceID) == 0)
			{
				break;
			}
		}
		if(i < _pServiceList->GetSubNodeNumber())
		{
			char* ppName[MAX_STATES_IN_SERVICE];
			char* ppValue[MAX_STATES_IN_SERVICE];
			int num = ((CUpnpService*)(_pServiceList->GetSubNode(i)))->getEventStates(ppName, ppValue, MAX_STATES_IN_SERVICE);	
			//printf("---event number : %d\n", num);
			const char* Sid = UpnpSubscriptionRequest_get_SID_cstr(psr_event);
			//printf("---event sid : %s\n", Sid);
			UpnpAcceptSubscription(getUpnpDeviceHandle()
				,devUDN
				,serviceID,
				(const char **)ppName,
				(const char **)ppValue,
				num,
				Sid);
			return UPNP_E_SUCCESS;
		}
		else
		{
			printf("--SID not match!!\n");
			return UPNP_E_INVALID_SERVICE;
		}
	}
	else
	{
		printf("--UDN not match!!\n");
		return UPNP_E_INVALID_DEVICE;
	}
}

int CUpnpDevice::upnp_event_handler(Upnp_EventType EventType, const void *Event)
{

	int	ret_val = UPNP_E_SUCCESS;
	
	switch (EventType) {
	case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		//printf("---UPNP_EVENT_SUBSCRIPTION_REQUEST \n");
		ret_val = upnp_subscription_request_handler((UpnpSubscriptionRequest *)Event);
		break;

	case UPNP_CONTROL_GET_VAR_REQUEST:
		ret_val = upnp_getvar_request_handler((UpnpStateVarRequest *)Event);
		break;

	case UPNP_CONTROL_ACTION_REQUEST:
		ret_val = this->upnp_action_request_handler((UpnpActionRequest *)Event);
		break;

	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		printf("--UPNP_DISCOVERY_ADVERTISEMENT_ALIVE\n");
		break;
	case UPNP_DISCOVERY_SEARCH_RESULT:
		printf("--UPNP_DISCOVERY_SEARCH_RESULT\n");
		break;
	case UPNP_DISCOVERY_SEARCH_TIMEOUT:
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
	case UPNP_CONTROL_ACTION_COMPLETE:
	case UPNP_CONTROL_GET_VAR_COMPLETE:
	case UPNP_EVENT_RECEIVED:
	case UPNP_EVENT_RENEWAL_COMPLETE:
	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		printf("upnp_event_handleer : %d\n", EventType);
		break;
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		printf("--UPNP_EVENT_UNSUBSCRIBE_COMPLETE\n");
		//printf("%s(%d, %p, %p) not supported!\n", __func__, EventType, Event, this);
		break;

	default:
		printf("--UPNP_NODEFINE\n");
		//printf("%s(%d, %p, %p) unknown!\n", __func__, EventType, Event, Cookie);
		break;
	}

	return ret_val;
	return 0;
}
/***************************************************************
CUpnpDeviceRoot
*/
CUpnpDeviceDoc::CUpnpDeviceDoc
	(const char* versionMajor
	,const char* versionMinor
	):inherited("root", NULL , 2, 1)
{
	_pVersion = new CUpnpDescriptionNode("specVersion", NULL, 2, 0, this);
	_pMajor = new CUpnpDescriptionNode("major", versionMajor, 0, 0, _pVersion);
	_pMinor = new CUpnpDescriptionNode("minor", versionMinor, 0, 0, _pVersion);

	//this->AddSubNode(_pVersion);
	this->AddAttribute("xmlns", "urn:schemas-upnp-org:device-1-0");	
}

CUpnpDeviceDoc::~CUpnpDeviceDoc()
{
	delete _pVersion;
	delete _pMajor;
	delete _pMinor;	
}

void CUpnpDeviceDoc::GenerateXML(char* buffer, int length, int level)
{
	if(length < 100)
		return;
	
	sprintf(buffer, "<?xml version=\"1.0\"?>\n");
	int len = strlen(buffer);
	inherited::GenerateXML(buffer +len, length - len, 0);
}


void CUpnpDeviceDoc::printDoc(char* tmp, int length)
{
	memset(tmp, 0, length);
	this->GenerateXML(tmp, length, 0);
	printf("--printDoc:\n%s\n", tmp);
}

void CUpnpDeviceDoc::writeToDocument(char* path, char* tmp, int length)
{
	CFile *file = new CFile(path, CFile::FILE_WRITE);
	memset(tmp, 0, length);
	this->GenerateXML(tmp, length, 0);
	file->write((BYTE*)tmp, strlen(tmp));	
	delete file;
}


/***************************************************************
CUpnpState
*/
CUpnpState::CUpnpState
	(const char* name
	,StateVariableType type
	,bool event
	,bool broadcast
	,CUpnpService* pService
	) : inherited("stateVariable", NULL , 4, 2)
	,_type(type)
	,_event(event)
	,_broadCast(broadcast)
	,_pService(pService)
{
	_pName = new CUpnpDescriptionNode("name", name, 0, 0, this);
	_pRelatedNod = new CUpnpDescriptionNode("relatedStateVariable", name, 0, 0);
	_pMutex = new CMutex("upnp state");
	if(_event)
	{
		this->AddAttribute("sendEvents", "yes");
	}
	else
	{
		this->AddAttribute("sendEvents", "no");
	}
	
	if(_broadCast)
	{
		this->AddAttribute("multicast", "yes");
	}
	_pService->getDoc()->AddState(this);

}

CUpnpState::~CUpnpState()
{
	delete _pName;
	delete _pRelatedNod;
	delete _pMutex;
}

int CUpnpState::sendUpnpEvent()
{
	int ret_val = -1;
	if(_event)
	{
		char* name = (char*)this->GetNameNode()->getValue();
		char* value = (char*)this->getCurrentState();
		ret_val = UpnpNotify(_pService->getDevice()->getUpnpDeviceHandle(),
			_pService->getDevice()->getUDN(),
			_pService->getServiceID(),
			(const char**)&name,
			(const char**)&value,
			1);
	}
	return ret_val;
}
int CUpnpState::saveProperty()
{
	char key[256];
	memset (key, 0, sizeof(key));
	sprintf(key, "upnp.%s.%s",_pService->getSName(), _pName->getValue());
	//printf("-!-property save: %s, %s\n", key, getCurrentState());
	_pMutex->Take();
	agcmd_property_set(key, getCurrentState());
	_pMutex->Give();
	this->sendUpnpEvent();
	return 0;
}

int CUpnpState::loadProperty(char* def)
{
	char key[256];
	char tmp[256];
	memset (key, 0, sizeof(key));
	memset (tmp, 0, sizeof(tmp));
	sprintf(key, "upnp.%s.%s",_pService->getSName(), _pName->getValue());
	_pMutex->Take();
	if(strcmp(def, "")!=0)
		agcmd_property_get(key, tmp , def);//"def"_pDefaultValue->getValue());
	else
		agcmd_property_get(key, tmp , _pDefaultValue->getValue());
	_pMutex->Give();
	this->SetCurrentState(tmp);
	return 0;
}

void CUpnpState::Update(bool bSendEvent)
{
	//should do some thing in sub class.
	if(bSendEvent)
		this->sendUpnpEvent();
}

/***************************************************************
CUpnpStringFreeState
*/
CUpnpStringFreeState::CUpnpStringFreeState
	(const char* name
	,bool event
	,bool broadcast
	,CUpnpService* pService
	) : inherited(name , StateVariableType_freestring, event, broadcast, pService)
{
	_pDataType = new CUpnpDescriptionNode("dataType", "string", 0, 0, this);	
	_pDefaultValue = new CUpnpDescriptionNode("defaultValue", "NA", 0, 0, this);
	_currentValue= new char [MAX_STRING_STATE_CHAR_LENGTH];
	_wishValue= new char [MAX_STRING_STATE_CHAR_LENGTH];
	memset(_wishValue, 0, MAX_STRING_STATE_CHAR_LENGTH);
	memset(_currentValue, 0, MAX_STRING_STATE_CHAR_LENGTH);
	sprintf(_currentValue, "no value");
}

CUpnpStringFreeState::~CUpnpStringFreeState()
{
	delete _pDataType;
	delete _pDefaultValue;
	delete[] _wishValue;
	delete[] _currentValue;
}

void CUpnpStringFreeState::SetState(char* value)
{
	if(strlen(value) < MAX_STRING_STATE_CHAR_LENGTH)
	{
		memset(_wishValue, 0, MAX_STRING_STATE_CHAR_LENGTH);
		strncpy(_wishValue, value, strlen(value) );
	}
	else
		strncpy(_wishValue, value, MAX_STRING_STATE_CHAR_LENGTH);
}

char* CUpnpStringFreeState::getCurrentState()
{
	return _currentValue;
}

void CUpnpStringFreeState::setWishState(const char* wishValue)
{
	if(strlen(wishValue) < MAX_STRING_STATE_CHAR_LENGTH)
	{
		memset(_wishValue, 0, MAX_STRING_STATE_CHAR_LENGTH);
		strncpy(_wishValue, wishValue, strlen(wishValue) );
	}
	else
		strncpy(_wishValue, wishValue, MAX_STRING_STATE_CHAR_LENGTH);
}

void CUpnpStringFreeState::SetCurrentState(const char* value)
{
	if(strlen(value) < MAX_STRING_STATE_CHAR_LENGTH)
	{
		memset(_currentValue, 0, MAX_STRING_STATE_CHAR_LENGTH);
		strncpy(_currentValue, value, strlen(value) );
	}
	else
		strncpy(_currentValue, value, MAX_STRING_STATE_CHAR_LENGTH);
	sendUpnpEvent();
}

void CUpnpStringFreeState::setWishAsCurrent()
{
	memcpy(_currentValue, _wishValue, MAX_STRING_STATE_CHAR_LENGTH);
	//sendUpnpEvent();
}
char* CUpnpStringFreeState::getWishState()
{
	return _wishValue;
}

void CUpnpStringFreeState::GenerateCodeFiles(char* path
							, char* buffer
							, int bufferlen
							, CFile* ih
							, CFile* im
							,  CFile* iffh
							,  CFile* iffm)
{
	return;
}

void CUpnpStringFreeState::GenerateCodeFiles2(char* path
							, char* buffer
							, int bufferlen
							,  CFile* iffh
							,  CFile* iffm)
{
	return;
}

/***************************************************************
CUpnpStringLimitedState
*/
CUpnpStringLimitedState::CUpnpStringLimitedState
	(const char* name	
	,bool event
	,bool broadcast
	,CUpnpService* pService
	) : inherited(name , StateVariableType_limitedstring, event, broadcast, pService)
	,_allowedValueNum(0)
	,_wishValueIndex(0)
	,_currentValueIndex(1)
	,_ppAllowedValueList(NULL)
{	
	_pDataType = new CUpnpDescriptionNode("dataType", "string", 0, 0, this);	
	_pDefaultValue = new CUpnpDescriptionNode("defaultValue", "NA", 0, 0, this);
}

void CUpnpStringLimitedState::SetAllowedList
	(int allowedNum
	,char** allowedValueList)
{
	_allowedValueNum = allowedNum;
	if(_allowedValueNum > 0)
	{
		_ppAllowedValueList = new CUpnpDescriptionNode*[_allowedValueNum];
		_pAllowedList = new CUpnpDescriptionNode("allowedValueList", NULL, _allowedValueNum, 0, this);
		for(int i = 0; i < _allowedValueNum; i++)
		{
			_ppAllowedValueList[i] = new CUpnpDescriptionNode("allowedValue", allowedValueList[i], 0, 0, _pAllowedList);
		}				
	}
	_pDefaultValue->SetValue(allowedValueList[0]);
}


CUpnpStringLimitedState::~CUpnpStringLimitedState()
{
	//delete _pName;
	//if(_wishValue != NULL)
	//	delete _wishValue;
	delete _pDataType;
	if(_allowedValueNum > 0)
	{
		for(int i = 0; i < _allowedValueNum; i++)
		{
			delete _ppAllowedValueList[i];
		}		
		delete[] _ppAllowedValueList;		
		delete _pAllowedList;
	}
	delete _pDefaultValue;
}

void CUpnpStringLimitedState::SetState(char* value)
{	
	int i;
	for(i = 0; i < _allowedValueNum; i ++)
	{
		if(strcmp(_ppAllowedValueList[i]->getValue(), value) == 0)
			break;
	}
	if(i < _allowedValueNum)
		_currentValueIndex = i;
	else
	{
		//printf("---argument not in allowedList!!\n");
	}
/*
	else
	{
		if(_wishValue == NULL)
		{
			_currentValue = new char[MAX_STRING_STATE_CHAR_LENGTH];			
		}
		memset(_currentValue, 0, MAX_STRING_STATE_CHAR_LENGTH);
		int length = (MAX_STRING_STATE_CHAR_LENGTH > strlen(value)) ? strlen(value) : MAX_STRING_STATE_CHAR_LENGTH;
		strcpy(_currentValue, value, length);
	}*/
}

char* CUpnpStringLimitedState::getCurrentState()
{
	if(_ppAllowedValueList  != NULL)
		return (char*)_ppAllowedValueList[_currentValueIndex]->getValue();
	else
		return NULL;
}

void CUpnpStringLimitedState::SetState(int listIndex)
{
	// set varable
	// if event, call event call back here
	if(listIndex< _allowedValueNum)
	{
		_currentValueIndex = listIndex;
		sendUpnpEvent();
	}
}

void CUpnpStringLimitedState::setWishState(const char* wishValue)
{
	int i;	
	for(i = 0; i < _allowedValueNum; i ++)
	{
		if(strcmp(_ppAllowedValueList[i]->getValue(), wishValue) == 0)
			break;
	}
	if(i < _allowedValueNum)
		_wishValueIndex = i;
	else
	{
		_wishValueIndex = _currentValueIndex;
		//printf("---argument not in allowedList!!\n");
	}
	//printf("--setWishState set %s: %d\n", _pName->getValue(), _wishValueIndex);
}

void CUpnpStringLimitedState::SetCurrentState(const char* value)
{
	int i;	
	for(i = 0; i < _allowedValueNum; i ++)
	{
		if(strcmp(_ppAllowedValueList[i]->getValue(), value) == 0)
			break;
	}
	if(i < _allowedValueNum)
	{
		_currentValueIndex= i;
		sendUpnpEvent();
	}
	else
	{
		_wishValueIndex = _currentValueIndex;
		printf("---argument not in allowedList!! : |%s|\n", value);
	}
}

void CUpnpStringLimitedState::setWishAsCurrent()
{
	//if(_currentValueIndex != _wishValueIndex)
	//{
		_currentValueIndex = _wishValueIndex;
		//sendUpnpEvent();
	//}
}

char* CUpnpStringLimitedState::getWishState()
{
	return (char*)_ppAllowedValueList[_wishValueIndex]->getValue();
}

void CUpnpStringLimitedState::GenerateCodeFiles(char* path
							, char* buffer
							, int bufferlen
							, CFile* ih
							, CFile* im
							,  CFile* iffh
							,  CFile* iffm)
{
	char tmp[256];
	if(_event)
	{
		for(int i = 0; i < _allowedValueNum; i++)
		{
			memset(tmp, 0, 256);
			sprintf(tmp, "%s/state_%s_%s.png",path, GetNameNode()->getValue(), _ppAllowedValueList[i]->getValue());
			CFile file = CFile(tmp, CFile::FILE_WRITE);
			file.write((BYTE*)tmp, 256);					
		}
		
		memset(buffer, 0, bufferlen);
		sprintf(buffer,"\n@interface SD%sUIItems : CCSDStateUIItems\n@end\n", GetNameNode()->getValue());
		ih->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer,"\n@implementation SD%sUIItems\n-(id)init\n{\n	self=[super init];\n	if(self){\n", GetNameNode()->getValue());
		im->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "	self.stateicons = [NSArray arrayWithObjects:");
		im->write((BYTE*)buffer, strlen(buffer));
		for(int i = 0; i < _allowedValueNum; i++)
		{
			memset(buffer, 0, bufferlen);
			sprintf(buffer, "@\"state_%s_%s.png\", ", GetNameNode()->getValue(), _ppAllowedValueList[i]->getValue());
			im->write((BYTE*)buffer, strlen(buffer));
		}
		memset(buffer, 0, bufferlen);
		sprintf(buffer, "nil];\n");
		im->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "	self.states = [NSArray arrayWithObjects:");
		im->write((BYTE*)buffer, strlen(buffer));
		for(int i = 0; i < _allowedValueNum; i++)
		{
			memset(buffer, 0, bufferlen);
			sprintf(buffer, "@\"%s\", ",_ppAllowedValueList[i]->getValue());
			im->write((BYTE*)buffer, strlen(buffer));
		}
		memset(buffer, 0, bufferlen);
		sprintf(buffer, "nil];\n");
		im->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "	self.statelables = [NSArray arrayWithObjects:");
		im->write((BYTE*)buffer, strlen(buffer));
		for(int i = 0; i < _allowedValueNum; i++)
		{
			memset(buffer, 0, bufferlen);
			sprintf(buffer, "@\"%s\", ",_ppAllowedValueList[i]->getValue());
			im->write((BYTE*)buffer, strlen(buffer));
		}
		memset(buffer, 0, bufferlen);
		sprintf(buffer, "nil];\n");
		im->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "	self.stateName = @\"%s\";\n", GetNameNode()->getValue());
		im->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "	self.lable = @\"%s\";\n", GetNameNode()->getValue());
		im->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "	}\n	return self;\n}\n@end\n");
		im->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "@property (strong, nonatomic) SD%sUIItems *pSD%sUIItems;\n", GetNameNode()->getValue(), GetNameNode()->getValue());
		iffh->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "@synthesize pSD%sUIItems = _pSD%sUIItems;\n", GetNameNode()->getValue(), GetNameNode()->getValue());
		iffm->write((BYTE*)buffer, strlen(buffer));
	}
}

void CUpnpStringLimitedState::GenerateCodeFiles2(char* path
							, char* buffer
							, int bufferlen
							,  CFile* iffh
							,  CFile* iffm)
{
	if((_event)&&(iffm))
	{
		memset(buffer, 0, bufferlen);
		sprintf(buffer, "		_pSD%sUIItems = [[SD%sUIItems alloc]init];\n", GetNameNode()->getValue(), GetNameNode()->getValue());
		iffm->write((BYTE*)buffer, strlen(buffer));

		memset(buffer, 0, bufferlen);
		sprintf(buffer, "		[_pSDDescriptor AddEventObserver:_pSD%sUIItems];\n", GetNameNode()->getValue());
		iffm->write((BYTE*)buffer, strlen(buffer));
	}
	return;
}


/***************************************************************
CUpnpNumberState
*/
CUpnpNumberState::CUpnpNumberState
	(const char* name
	,bool limited
	,double maxValue
	,double minValue
	,double step
	,bool event
	,bool broadcast
	,CUpnpService* pService
	) : inherited(name , StateVariableType_number, event, broadcast, pService)
	,_limited(limited)
	,_maxValue(maxValue)
	,_minValue(minValue)
	,_step(step)
	//,_event(event)
	//,_broadCast(broadcast)
	,_currentValue(0)
	,_wishValue(0)
{
	//_pName = new CUpnpDescriptionNode("name", name, 0, 0, this);
	_pDataType = new CUpnpDescriptionNode("dataType", "number", 0, 0, this);	
	if(_limited)
	{
		_pAllowedValueRange = new CUpnpDescriptionNode("allowedValueRange", NULL, 3, 0, this);
		_pDefaultValue = new CUpnpDescriptionNode("defaultValue", "0.0", 0, 0, this);

		_pMaxValue = new CUpnpDescriptionNode("maximum", NULL, 0, 0, _pAllowedValueRange);
		_pMaxValue->SetNumber(_maxValue);
		_pMinValue = new CUpnpDescriptionNode("minimum", NULL, 0, 0, _pAllowedValueRange);
		_pMinValue->SetNumber(_minValue);
		_pStep = new CUpnpDescriptionNode("step", NULL, 0, 0, _pAllowedValueRange);
		_pStep->SetNumber(_step);
	}
	else
		_pAllowedValueRange = NULL;

}
CUpnpNumberState::~CUpnpNumberState()
{
	//delete _pName;
	delete _pDataType;
	if(_pAllowedValueRange)
	{
		delete _pAllowedValueRange;
		delete _pDefaultValue;
		delete _pMaxValue;
		delete _pMinValue;
		delete _pStep;
	}
}

void CUpnpNumberState::SetState(double value)
{
	//double tmp =atof(wishValue);
	if(_limited)
	{
		if((value > _maxValue)||(value<_minValue))
		{
			printf("--setWishState set value outof valid range : %lf.\n", value);
			return;
		}
	}
	_currentValue= value;
}

double CUpnpNumberState::getState()
{
	return _currentValue;
}

double CUpnpNumberState::getWish()
{
	return _wishValue;
}

void CUpnpNumberState::StepIncrease()
{

}

void CUpnpNumberState::StepDecrease()
{

}

void CUpnpNumberState::setWishState(const char* wishValue)
{
	double tmp =atof(wishValue);
	if(_limited)
	{
		if((tmp > _maxValue)||(tmp<_minValue))
		{
			printf("--setWishState set value outof valid range : %lf.\n", tmp);
			return;
		}
	}
	//printf("--setWishState set : %lf.\n", tmp);
	_wishValue = tmp;
}

char* CUpnpNumberState::getCurrentState()
{
	memset(_tmp, 0, MAX_STRING_STATE_CHAR_LENGTH);
	gcvt(_currentValue, MAX_STRING_STATE_CHAR_LENGTH, _tmp);
	return _tmp;
}

void CUpnpNumberState::SetCurrentState(const char* value)
{
	double tmp =atof(value);
	if(_limited)
	{
		if((tmp > _maxValue)||(tmp<_minValue))
		{
			printf("--setWishState set value outof valid range : %lf.\n", tmp);
			return;
		}
	}
	//printf("--setWishState set : %lf.\n", tmp);
	_currentValue = tmp;
	sendUpnpEvent();
}

void CUpnpNumberState::setWishAsCurrent()
{
	_currentValue = _wishValue;
	//sendUpnpEvent();
}

char* CUpnpNumberState::getWishState()
{
	memset(_tmp, 0, MAX_STRING_STATE_CHAR_LENGTH);
	gcvt(_wishValue, MAX_STRING_STATE_CHAR_LENGTH, _tmp);
	return _tmp;
}

void CUpnpNumberState::GenerateCodeFiles(char* path
							, char* buffer
							, int bufferlen
							, CFile* ih
							, CFile* im
							,  CFile* iffh
							,  CFile* iffm)
{
	return;
}

void CUpnpNumberState::GenerateCodeFiles2(char* path
							, char* buffer
							, int bufferlen
							,  CFile* iffh
							,  CFile* iffm)
{
	return;
}

/***************************************************************
CUpnpAction
*/
CUpnpDescriptionNode CUpnpAction::directionIn("direction", "in", 0, 0);
CUpnpDescriptionNode CUpnpAction::directionOut("direction", "out", 0, 0);

CUpnpAction::CUpnpAction
	(const char* name
	,CUpnpService* svc
	,int inArguNum
	,CUpnpState** inList
	,int outArguNum
	,CUpnpState** outList
	) : inherited("action", NULL , 2, 0)
	,_pOwnerService(svc)
	,_inNum(inArguNum)
	,_outNum(outArguNum)
{
	_pName = new CUpnpDescriptionNode("name", name, 0, 0, this);
	SetArguList(inArguNum, inList, outArguNum, outList);
	_pOwnerService->getDoc()->AddAction(this);
}

CUpnpAction::~CUpnpAction()
{
	if(_inNum + _outNum > 0)
	{
		if(_inNum > 0)
			delete[] _ppInStates;
		if(_outNum > 0)
			delete[] _ppOutStates;
		
		for(int i = 0; i < _inNum + _outNum; i++)
		{
			delete _ppArgus[i];
		}
		delete[] _ppArgus;
		delete _pArgulist;
	}
	delete _pName;	
}

CUpnpAction::CUpnpAction
	(const char* name
	,CUpnpService* svc
	): inherited("action", NULL , 2, 0)
	,_pOwnerService(svc)
	,_inNum(0)
	,_outNum(0)
{
	_pName = new CUpnpDescriptionNode("name", name, 0, 0, this);
	_pOwnerService->getDoc()->AddAction(this);
}

void CUpnpAction::SetArguList(int inArguNum
					,CUpnpState** inList
					,int outArguNum
					,CUpnpState** outList)
{
	_inNum = inArguNum;
	_outNum = outArguNum;

	if(_inNum + _outNum > 0)
	{
		if(_inNum > 0)
			_ppInStates = new CUpnpState* [_inNum];
		if(_outNum > 0)
			_ppOutStates = new CUpnpState* [_outNum];
		_pArgulist = new CUpnpDescriptionNode("argumentList", NULL, inArguNum + outArguNum, 0, this);
		_ppArgus = new CUpnpDescriptionNode* [inArguNum + outArguNum];
		for(int i = 0; i < _inNum; i++)
		{
			_ppArgus[i] = new CUpnpDescriptionNode("argument", NULL, 4, 0, _pArgulist);
			_ppArgus[i]->AddSubNode(inList[i]->GetNameNode());
			_ppArgus[i]->AddSubNode(&directionIn);
			_ppArgus[i]->AddSubNode(inList[i]->GetRelatedNode());
			_ppInStates[i] = inList[i];
		}
		for(int i = 0;  i < _outNum; i++)
		{
			_ppArgus[i + _inNum] = new CUpnpDescriptionNode("argument", NULL, 4, 0, _pArgulist);
			_ppArgus[i + _inNum]->AddSubNode(outList[i]->GetNameNode());
			_ppArgus[i + _inNum]->AddSubNode(&directionOut);
			_ppArgus[i + _inNum]->AddSubNode(outList[i]->GetRelatedNode());
			_ppOutStates[i] = outList[i];
		}
	}
}

bool CUpnpAction::checkName(char* name)
{
	if(strcmp(name, _pName->getValue()) == 0)
		return true;
	else return false;
}

int CUpnpAction::action(UpnpActionRequest *par_event)
{
	// parse paras, if ok, go on else return para error
	//for(int )
	//if(_inNum != 0)
	const char* name;
	const char* ret;
	for(int i = 0; i < _inNum; i++)
	{
		name = _ppInStates[i]->GetNameNode()->getValue();
		ret = IXML_get_first_document_item(UpnpActionRequest_get_ActionRequest(par_event), name);
		if(strcmp(ret, "NULL") != 0)
		{
			//printf("--action arg for %s : %s\n", name, ret);
			_ppInStates[i]->setWishState(ret);
		}
		else
		{
			printf("--action arg for %s : %s\n", name, ret);
		}
	}
	
	// action , translate paras to curresponding number, call virtual action 
	execution();	

	IXML_Document *ActionResult = NULL;
	// check return result, translate return value and send back.
	//printf("_outNum : %d\n", _outNum);
	int ret_value;
	for(int i = 0; i < _outNum; i++)
	{
		ret_value = UpnpAddToActionResponse
			(&ActionResult
			,this->_pName->getValue()
			,_pOwnerService->getServiceType()
			, _ppOutStates[i]->GetNameNode()->getValue()
			, _ppOutStates[i]->getCurrentState());
		if (ret_value != UPNP_E_SUCCESS) 
			UpnpActionRequest_strcpy_ErrStr(par_event,  "Internal Error");
	}
	// make Result not NULL
	{
		ret_value = UpnpAddToActionResponse
			(&ActionResult
			,this->_pName->getValue()
			,_pOwnerService->getServiceType()
			,NULL
			,NULL);
		if (ret_value != UPNP_E_SUCCESS) 
			UpnpActionRequest_strcpy_ErrStr(par_event,  "Internal Error");
	}
	
	UpnpActionRequest_set_ActionResult(par_event, ActionResult);
	UpnpActionRequest_set_ErrCode(par_event, ret_value);
	return 0;
}

char* CUpnpAction::IXML_get_first_document_item
	(IXML_Document *doc
	,const char *item)
{
	IXML_NodeList				*node_list = NULL;
	IXML_Node				*tmp_node = NULL;
	IXML_Node				*text_node = NULL;
	char					*ret_val = "";

	node_list = ixmlDocument_getElementsByTagName(doc, (char *)item);
	if (!node_list) {
		printf("ixmlDocument_getElementsByTagName(%p, %s) = NULL\n",
			doc, item);
		goto upnp_helper_get_first_document_item_exit;
	}

	tmp_node = ixmlNodeList_item(node_list, 0);
	if (!tmp_node) {
		printf("ixmlNodeList_item(%p) = NULL\n", node_list);
		goto upnp_helper_get_first_document_item_exit;
	}

	text_node = ixmlNode_getFirstChild(tmp_node);
	if (!text_node) {
		printf("ixmlNode_getFirstChild(%p) = NULL\n", tmp_node);
		goto upnp_helper_get_first_document_item_exit;
	}

	ret_val = strdup(ixmlNode_getNodeValue(text_node));
	if (!ret_val) {
		printf("ixmlNode_getNodeValue(%p) = %s\n",
			text_node, ixmlNode_getNodeValue(text_node));
		goto upnp_helper_get_first_document_item_exit;
	}

upnp_helper_get_first_document_item_exit:
	if (node_list)
		ixmlNodeList_free(node_list);

	return ret_val;
}


/***************************************************************
CUpnpActionUpdate
*/
CUpnpActionUpdate::CUpnpActionUpdate
	(CUpnpService* pService
	) : inherited("update", pService)	
{

}

CUpnpActionUpdate::~CUpnpActionUpdate()
{

}

void CUpnpActionUpdate::execution()
{
	//int num = this->getOwnerService()->getDoc()->GetStateNum();
	//for(int  i = 0; i < num; i++)
	{
		 this->getOwnerService()->EventPropertyUpdate();
	}
}

/***************************************************************
CUpnpServiceDoc <actionList> xmlns="urn:schemas-upnp-org:service-1-0"
*/
CUpnpServiceDoc::CUpnpServiceDoc
	(CUpnpDeviceDoc* pDeviceDoc
	,int maxActionNum
	,int maxStateNum
	): inherited("scpd", NULL , 3, 1)
	,_pDeviceDoc(pDeviceDoc)
{
	this->AddSubNode(_pDeviceDoc->getVersionNode());
	_pActionList = new CUpnpDescriptionNode("actionList", NULL, maxActionNum, 0, this);
	_pStateTable = new CUpnpDescriptionNode("serviceStateTable", NULL, maxStateNum, 0, this);

	this->AddAttribute("xmlns", "urn:schemas-upnp-org:service-1-0");
}

CUpnpServiceDoc::~CUpnpServiceDoc()
{
	delete _pActionList;
	delete _pStateTable;
}

void CUpnpServiceDoc::GenerateXML(char* buffer, int length, int level)
{
	if(length < 1024)
		return;
	
	sprintf(buffer, "<?xml version=\"1.0\"?>\n");
	int len = strlen(buffer);
	inherited::GenerateXML(buffer +len, length - len, 0);
}

void CUpnpServiceDoc::AddAction(CUpnpAction* pAction)
{
	_pActionList->AddSubNode(pAction);
}

void CUpnpServiceDoc::AddState(CUpnpState* pState)
{
	_pStateTable->AddSubNode(pState);
}

CUpnpAction* CUpnpServiceDoc::GetAction(const char* actionName)
{
	int i = 0;
	for(i = 0; i < GetActionNum(); i++)
	{
		if(GetAction(i)->checkName((char*)actionName))
			break;
	}
	if(i < GetActionNum())
		return GetAction(i);
	else return NULL;
}


void CUpnpServiceDoc::printfDoc(char* buffer, int length)
{
	memset(buffer, 0, length);
	this->GenerateXML(buffer, length, 0);
	printf("--printDoc:\n%s\n", buffer);
}

void CUpnpServiceDoc::writeToDocument(char* path, char* tmp, int length)
{
	CFile *file = new CFile(path, CFile::FILE_WRITE);
	memset(tmp, 0, length);
	this->GenerateXML(tmp, length, 0);
	file->write((BYTE*)tmp, strlen(tmp));	
	delete file;
}


bool  CUpnpServiceDoc::testSetState(char* name, char* state)
{
	for(int i = 0; i < _pStateTable->GetSubNodeNumber(); i++)
	{
		CUpnpState* pState = (CUpnpState*)_pStateTable->GetSubNode(i);
		if(strcmp(pState->GetNameNode()->getValue(), name) == 0)
		{
			pState->SetCurrentState(state);
			return true;
		}
	}
	return false;
}

CUpnpState* CUpnpServiceDoc::GetState(char* name)
{
	for(int i = 0; i < _pStateTable->GetSubNodeNumber(); i++)
	{
		CUpnpState* pState = (CUpnpState*)_pStateTable->GetSubNode(i);
		if(strcmp(pState->GetNameNode()->getValue(), name) == 0)
		{
			return pState;
		}
	}
	return NULL;
}

void CUpnpServiceDoc::GenerateCodeFiles
	(char* path
	, char* buffer
	, int bufferlen
	, CFile* ih
	, CFile* im
	,  CFile* iffh
	,  CFile* iffm)
{	
	for(int i = 0; i < _pStateTable->GetSubNodeNumber(); i++)
	{
		//memset(path, 0, 256);
		//sprintf(path, "%sstate_%s_",path, ((CUpnpState*)_pStateTable->GetSubNode(i)));
		//CFile file = CFile()	
		//if( ((CUpnpState*)_pStateTable->GetSubNode(i))->isEventState() )
		 ((CUpnpState*)_pStateTable->GetSubNode(i))->GenerateCodeFiles(path
																,buffer
																,bufferlen
																,ih
																,im
																,iffh
																,iffm);
		
	}
}

void CUpnpServiceDoc::GenerateCodeFiles2
	(char* path
	, char* buffer
	, int bufferlen
	,  CFile* iffh
	,  CFile* iffm)
{
	for(int i = 0; i < _pStateTable->GetSubNodeNumber(); i++)
	{
		 ((CUpnpState*)_pStateTable->GetSubNode(i))->GenerateCodeFiles2(path
																,buffer
																,bufferlen
																,iffh
																,iffm);
		
	}

}




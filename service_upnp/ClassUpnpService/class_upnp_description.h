/*****************************************************************************
 * class_upnp_description.h
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - 2012, linnsong.
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

#ifndef __H_class_upnp_description__
#define __H_class_upnp_description__
#include "class_upnp_thread.h"

class CUpnpDeviceDoc;
class CUpnpAction;
class CUpnpState;
class CUpnpDevice;
class CUpnpActionUpdate;



/***************************************************************
CNodeAttribute
*/
class CNodeAttribute
{
public:
	char* name;
	char* value;
	int getStringLength();
};

/***************************************************************
CUpnpDescriptionNode
*/
class CUpnpDescriptionNode
{
public:
	CUpnpDescriptionNode(const char*nodeTag
								,const char* nodeValue
								,int maxSubNodeNum
								,int maxAttriNum);
	~CUpnpDescriptionNode();

	CUpnpDescriptionNode(const char*nodeTag
								,const char* nodeValue
								,int maxSubNodeNum
								, int maxAttriNum
								,CUpnpDescriptionNode* parent
								);
	
	virtual void GenerateXML(char* buffer, int length, int level);
	void AddSubNode(CUpnpDescriptionNode* pNode);
	int GetSubNodeNumber(){return _addedNodeNum;};
	CUpnpDescriptionNode* GetSubNode(int i){return _ppUpnpNodeList[i];};

	const char* getName(){return _tag;};
	virtual void SetValue(char* value);
	const char* getValue(){if(!_bNumberValue){return _value;}else{return NULL;}};
	
	void AddAttribute(const char* name, const char* value);
	void SetNumber(double value);
	
protected:
	CUpnpDescriptionNode		**_ppUpnpNodeList;
	int							_maxNodNum;
	int							_addedNodeNum;
	const char					*_tag;
	const char					*_value;
	CNodeAttribute				*_pAttributes;
	int							_maxAttriNum;
	int							_addedAttriNum;
	bool						_bNumberValue;
	double						_floatNumber;	
};

/***************************************************************
CUpnpServiceDoc
*/
class CUpnpServiceDoc : public CUpnpDescriptionNode
{
typedef CUpnpDescriptionNode inherited;
public:
	CUpnpServiceDoc(CUpnpDeviceDoc* pDeviceDoc, int maxActionNum, int maxStateNum);
	~CUpnpServiceDoc();
	virtual void GenerateXML(char* buffer, int length, int level);

	void AddAction(CUpnpAction* pAction);
	void AddState(CUpnpState* pState);

	int GetActionNum(){return _pActionList->GetSubNodeNumber();};
	CUpnpAction* GetAction(int index){return (CUpnpAction*)_pActionList->GetSubNode(index);};
	CUpnpAction* GetAction(const char* actionName);

	int GetStateNum(){return _pStateTable->GetSubNodeNumber();};
	CUpnpState* GetState(int index){return (CUpnpState*)_pStateTable->GetSubNode(index);};
	CUpnpState* GetState(char* name);
		
	void printfDoc(char* buffer, int length);
	void writeToDocument(char* path, char* tmp, int length);

	void GenerateCodeFiles(char* path, char* buffer, int bufferlen, CFile* itemsHFile, CFile* itemsMFile,  CFile* itemFactHFile,  CFile* itemFactMFile);
	void GenerateCodeFiles2	(char* path
								, char* buffer
								, int bufferlen
								,  CFile* iffh
								,  CFile* iffm);
	bool testSetState(char* name, char* state);
	
protected:
	CUpnpDeviceDoc* _pDeviceDoc;
	CUpnpDescriptionNode *_pActionList;
	CUpnpDescriptionNode *_pStateTable;
};

/***************************************************************
CUpnpService
*/
class CUpnpService  : public CUpnpDescriptionNode
{
public:
	CUpnpService(const char* ShortName
					,const char* serviceType
					,const char* serviceId
					,const char* SCPDURL
					,const char* controlURL
					,const char* eventSubURL
					,CUpnpDevice* pDevice
					,int maxStateNum
					,int masActionNum
					);
	~CUpnpService();

	CUpnpServiceDoc* getDoc(){return _pServiceDoc;};
	const char* getServiceID(){return _serviceId->getValue();};

	int UpnpActionProcess(UpnpActionRequest *par_event);
	//int UpnpGetVar();
	//int UpnpSubscribe();
	char* getServiceType();
	int getEventStates(char** name, char**value, int num);
	const char* getDocPath(){return _SCPDURL->getValue();};
	CUpnpDevice* getDevice(){return _pDevice;};
	const char* getSName(){return _pShortName;};

	// for update system properties.
	void EventPropertyUpdate();
private:

	CUpnpDescriptionNode* _serviceType;
	CUpnpDescriptionNode* _serviceId;
	CUpnpDescriptionNode* _SCPDURL;
	CUpnpDescriptionNode* _controlURL;
	CUpnpDescriptionNode* _eventSubURL;

	CUpnpServiceDoc* 		_pServiceDoc;
	CUpnpDevice*			_pDevice;

	CUpnpActionUpdate*		_pUpdateAction;
	const char*				_pShortName;
};

/***************************************************************
CUpnpIcon
*/
class CUpnpIcon : public CUpnpDescriptionNode
{

};


/***************************************************************
CUpnpState
*/
typedef enum StateVariableType
{
	StateVariableType_freestring = 0,
	StateVariableType_limitedstring,
	StateVariableType_number,
	StateVariableType_dateTime,			
}StateVariableType; 
class CUpnpState : public CUpnpDescriptionNode
{
typedef CUpnpDescriptionNode inherited;
public:
	CUpnpState(const char* name
					,StateVariableType type
					,bool event
					,bool broadcast
					,CUpnpService* pService);
	~CUpnpState();

	CUpnpDescriptionNode* GetNameNode(){return _pName;};
	CUpnpDescriptionNode* GetRelatedNode(){return _pRelatedNod;};
	
	virtual char* getCurrentState() = 0;
	virtual void SetCurrentState(const char* wishValue) = 0;
	virtual void setWishAsCurrent() = 0;
	virtual void setWishState(const char* wishValue) = 0;
	virtual char* getWishState() = 0;
	
	bool isEventState(){return _event;};
	int sendUpnpEvent();
	//int GetSaveKey(char* tmp, int len);
	int loadProperty(char* def = "");
	int saveProperty();
	virtual void Update(bool bSendEvent);
	//code tool
	virtual void GenerateCodeFiles(char* path
							, char* buffer
							, int bufferlen
							, CFile* ih
							, CFile* im
							,  CFile* iffh
							,  CFile* iffm) = 0;
	virtual void GenerateCodeFiles2(char* path
							, char* buffer
							, int bufferlen
							,  CFile* iffh
							,  CFile* iffm) = 0;
	
	
protected:
	StateVariableType		_type;
	CUpnpDescriptionNode* _pName;
	CUpnpDescriptionNode* _pDataType;
	CUpnpDescriptionNode* _pDefaultValue;	
	CUpnpDescriptionNode* _pRelatedNod;
	CMutex*				  _pMutex;

	bool					_event;
	bool					_broadCast;

	CUpnpService* 		_pService;
};
/***************************************************************
CUpnpStateVariable
*/
#define MAX_STRING_STATE_CHAR_LENGTH 128
class CUpnpNumberState : public CUpnpState
{
typedef CUpnpState inherited;
public:
	CUpnpNumberState(const char* name
							,bool limited
							,double maxValue
							,double minValue
							,double step
							,bool event
							,bool broadcast
							,CUpnpService* pService);
	~CUpnpNumberState();

	void SetState(double value);
	double getState();
	double getWish();

	void StepIncrease();
	void StepDecrease();

	virtual void setWishState(const char* wishValue);
	virtual char* getCurrentState();
	virtual void SetCurrentState(const char* value);
	virtual void setWishAsCurrent();
	virtual char* getWishState();

	//code tool
	virtual void GenerateCodeFiles(char* path
							, char* buffer
							, int bufferlen
							, CFile* ih
							, CFile* im
							,  CFile* iffh
							,  CFile* iffm);
	virtual void GenerateCodeFiles2(char* path
							, char* buffer
							, int bufferlen
							,  CFile* iffh
							,  CFile* iffm);
	
protected:
	
	bool				_limited;
	double			_numValue;
	double			_maxValue;
	double			_minValue;
	double			_step;
	double			_currentValue;
	double			_wishValue;
	
	CUpnpDescriptionNode* _pMaxValue;
	CUpnpDescriptionNode* _pMinValue;
	CUpnpDescriptionNode* _pStep;
	CUpnpDescriptionNode* _pAllowedValueRange;
	char					_tmp[MAX_STRING_STATE_CHAR_LENGTH];
};


/***************************************************************
CUpnpStringFreeState
*/

class CUpnpStringFreeState : public CUpnpState
{
typedef CUpnpState inherited;
public:	
	CUpnpStringFreeState(const char* name
						  ,bool event
						  ,bool broadcast
						  ,CUpnpService* pService);
	~CUpnpStringFreeState();


	void SetState(char* value);
	//char* GetState();
	virtual void setWishState(const char* wishValue);
	virtual char* getCurrentState();
	virtual void SetCurrentState(const char* value);
	virtual void setWishAsCurrent();
	virtual char* getWishState();

	//code tool
	virtual void GenerateCodeFiles(char* path
							, char* buffer
							, int bufferlen
							, CFile* ih
							, CFile* im
							,  CFile* iffh
							,  CFile* iffm);
	virtual void GenerateCodeFiles2(char* path
							, char* buffer
							, int bufferlen
							,  CFile* iffh
							,  CFile* iffm);
	
protected:

	//bool					_event;
	//bool					_broadCast;
	char*				_currentValue;
	char*				_wishValue;
	
};

/***************************************************************
CUpnpStringLimitedState
*/
class CUpnpStringLimitedState : public CUpnpState
{
typedef CUpnpState inherited;
public:	
	CUpnpStringLimitedState(const char* name
						  ,bool event
						  ,bool broadcast
						  ,CUpnpService* pService);
	~CUpnpStringLimitedState();

	void SetAllowedList(int allowedNum ,char** allowedValueList);

	void SetState(char* value);
	//char* GetCurrentState();
	int GetCurrentIndex(){return _currentValueIndex;};

	void SetState(int listIndex);
	void setWishState(int index){_wishValueIndex = index;};

	virtual void setWishState(const char* wishValue);
	virtual char* getCurrentState();
	virtual void SetCurrentState(const char* value);
	virtual void setWishAsCurrent();
	virtual char* getWishState();

	//code tool
	virtual void GenerateCodeFiles(char* path
							, char* buffer
							, int bufferlen
							, CFile* ih
							, CFile* im
							,  CFile* iffh
							,  CFile* iffm);
	virtual void GenerateCodeFiles2(char* path
							, char* buffer
							, int bufferlen
							,  CFile* iffh
							,  CFile* iffm);
	
protected:
	int				_allowedValueNum;
	int				_wishValueIndex;
	int				_currentValueIndex;

	CUpnpDescriptionNode** _ppAllowedValueList;
	CUpnpDescriptionNode* _pAllowedList;
};


/***************************************************************
CUpnpDevice
*/
#define MAX_STATES_IN_SERVICE 20
class CUpnpDevice : public CUpnpDescriptionNode , public CUpnpEventHandler
{
public:
	CUpnpDevice(const char* UDN
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
					//,CUpnpDescriptionNode* doc	
					);
	~CUpnpDevice();

	CUpnpDeviceDoc* getDoc(){return _pDoc;};

	void AddService(CUpnpService* pService);
	void AddIcon(CUpnpIcon* picon);
	int GetServiceNum(){return _pServiceList->GetSubNodeNumber();};
	
	CUpnpService* getService(int index){return (CUpnpService*)_pServiceList->GetSubNode(index);};

	virtual int upnp_action_request_handler(UpnpActionRequest *par_event);
	virtual int upnp_getvar_request_handler(UpnpStateVarRequest *pvr_event);
	virtual int upnp_subscription_request_handler(UpnpSubscriptionRequest *psr_event);
	virtual int upnp_event_handler(Upnp_EventType EventType, const void *Event);

	const char* getUDN(){return _pUDN->getValue();};
	void testSetState(char* name, char* state);
	//char* get
private:
	CUpnpDescriptionNode* _pUDN;
	CUpnpDescriptionNode* _pdeviceType;
	CUpnpDescriptionNode* _pfriendlyName;
	CUpnpDescriptionNode* _pmanufacturer;
	CUpnpDescriptionNode* _pmanufacturerURL;
	CUpnpDescriptionNode* _pmodelDescription;
	CUpnpDescriptionNode* _pmodelName;
	CUpnpDescriptionNode* _pmodelNumber;
	CUpnpDescriptionNode* _pmodelURL;
	CUpnpDescriptionNode* _pSerialNumber;	
	CUpnpDescriptionNode* _pIconList;	
	CUpnpDescriptionNode* _pServiceList;
	CUpnpDescriptionNode* _pPresentationURL;

	CUpnpDeviceDoc* _pDoc;

	UpnpDevice_Handle 	_upnpDevice_handle;
};

/***************************************************************
CUpnpDeviceDoc
*/
class CUpnpDeviceDoc :  public CUpnpDescriptionNode
{
typedef CUpnpDescriptionNode inherited;
public:
	CUpnpDeviceDoc(const char* versionMajor
						,const char* versionMinor);
	~CUpnpDeviceDoc();
	virtual void GenerateXML(char* buffer, int length, int level);

	CUpnpDescriptionNode* getVersionNode(){return _pVersion;};

	void printDoc(char* tmp, int length);
	void writeToDocument(char* path, char* tmp, int length);
	
private:
	CUpnpDescriptionNode* _pVersion;
	CUpnpDescriptionNode* _pMajor;
	CUpnpDescriptionNode* _pMinor;
};

/***************************************************************
CUpnpAction
			
*/
class CUpnpAction :  public CUpnpDescriptionNode
{
typedef CUpnpDescriptionNode inherited;
public:
	CUpnpAction(const char* name
					,CUpnpService* svc
					,int inArguNum
					,CUpnpState** inList
					,int outArguNum
					,CUpnpState** outList);
	~CUpnpAction();

	CUpnpAction(const char* name ,CUpnpService* svc);
	void SetArguList(int inArguNum
					,CUpnpState** inList
					,int outArguNum
					,CUpnpState** outList);
	virtual void execution() = 0;
	bool checkName(char* name);

	int action(UpnpActionRequest *par_event);
	CUpnpService* getOwnerService(){return _pOwnerService;};
	
protected:
	static CUpnpDescriptionNode directionIn;
	static CUpnpDescriptionNode directionOut;
	
	CUpnpDescriptionNode* _pName;
	CUpnpDescriptionNode* _pArgulist;
	CUpnpDescriptionNode** _ppArgus;
	
	CUpnpState			**_ppInStates;
	CUpnpState			**_ppOutStates;

	CUpnpService*		_pOwnerService;
	int 					_inNum;
	int 					_outNum;

	char* IXML_get_first_document_item(IXML_Document *doc ,const char *item);
};

/***************************************************************
CUpnpActionUpdate
*/
class CUpnpActionUpdate : public CUpnpAction
{
	typedef CUpnpAction inherited;
public:
	CUpnpActionUpdate(CUpnpService* pService);
	~CUpnpActionUpdate();
	virtual void execution();
private:
	
};

#endif

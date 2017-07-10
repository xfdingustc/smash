/*****************************************************************************
 * sd_general_description.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1975 - , linnsong.
 *
 *
 *****************************************************************************/
#ifndef __H_sd_general_description__
#define __H_sd_general_description__

//#include "GobalMacro.h"
#include "ClinuxThread.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include "class_TCPService.h"

#define envelop_root_element_name "ccev"
#define envelop_attr_type_name "tp"
#define envelop_attr_time_name "st"
//const static char* cmd_attr_para1 = "id";
#define cmd_element_name "cmd"
#define cmd_attr_action "act"
#define cmd_attr_para1 "p1"
#define cmd_attr_para2 "p2"
#define cmd_attr_sn "sn"
#define cmd_attr_id "id"

#define in_envelope_max_cmd_num 10
#define MaxCMDQueueDeapth 60 // in case some time cost commands are called

enum EnvelopeType
{
    EnvelopeType_cmd = 0,
    EnvelopeType_offline_cmd,
    EnvelopeType_state,
};

class SCMD_Domain;
class Property;

/***************************************************************
StringCMD
*/
class StringCMD
{
public:
    StringCMD
    (char* name = NULL
     , char* para1 = NULL
     , char* para2 = NULL
    );
    virtual ~StringCMD();

    void copyName(char* name);
    void copyPara1(char* para1);
    void copyPara2(char* para2);
    void copyFromID(char* from);
    void setSequence(char* seq);

    unsigned int getSeq(){return _sequence;};
    char* getName(){return _name;};
    const char* getPara1(){return _para1;};
    const char* getPara2(){return _para2;};
    char* getFromID(){return _from;};

    void setLock(){_bLock = true;};
    bool isLock(){return _bLock;};

    void setSendBack(SessionResponseSendBack* bk) { _sendBack = bk; _sessionService = bk->pService; }
    SessionResponseSendBack* getResponsSendBack() { return _sendBack; }
    int SendBack(char* data, int len) { return _sessionService->sendSessionBack(_sendBack, data, len); }

private:
    unsigned int _sequence;
    char *_name;
    char *_para1;
    char *_para2;
    bool _bOut;
    bool _bLock;
    char *_from;
    SessionResponseSendBack* _sendBack;
    TCPSessionService* _sessionService;
};

class EnumedStringCMD : public StringCMD
{
public:
    typedef StringCMD inherited;
    EnumedStringCMD  // FOR Reciever
    (SCMD_Domain* pDomain
     , int cmd
    );

    EnumedStringCMD // FOR Sendor
    (int domain
     , int cmd
     ,char* para1
     ,char* para2
    );
    virtual ~EnumedStringCMD();

    int getDomain();
    int getCMD();
    bool isCmd(int cmd){return cmd == _cmd;};
    //bool isEnumedCMD();
    virtual int execute(EnumedStringCMD* p);
    SCMD_Domain* getDomainObj(){return _pDomain;};
    void Send(EnumedStringCMD* tricker);

private:
    char         _cmdBuffer[64];
    int          _cmd;
    SCMD_Domain* _pDomain;
};


/***************************************************************
StringEnvelope
*/
class StringEnvelope
{
public:
    StringEnvelope(StringCMD** cmds,int num);
    StringEnvelope(char* buf ,int length);
    ~StringEnvelope();
    static bool isEnvelope(char* buf, int len);
    void print();
    bool isNotEmpty();
    int getNum(){return _num;};
    StringCMD* GetCmd(int i){return _cmds[i];};
    char* getBuffer(){return _buffer;};
    int getBufferLen(){return _bufferLen;};

private:
    StringCMD*        _cmds[in_envelope_max_cmd_num];
    int               _num;
    unsigned int      _sendtime; //UTC time when this was send out.
    bool              _bOut;
    int               _type;
    char*             _buffer;
    int               _bufferLen;

private:
    void ParseCmd(xmlNode* root_element);
    void SendOut();
};

class StringResponse : public StringCMD
{
public:
    StringResponse
    (StringCMD* cmd // for get cmd and sequence num,
     ,char* result
    );
    ~StringResponse();

private:

};

/***************************************************************
StringCMDProcessor1
*/
class StringCMDProcessor1 : public CThread
{
public:
    StringCMDProcessor1();
    ~StringCMDProcessor1();

    int ProecessEnvelope(StringEnvelope* pEv,  SessionResponseSendBack* sendback);
    int ProcessCmd();
    virtual void Stop();
    //void setID(char* id){_id = id;};
    //char* getID(){return _id;};
    //virtual int OnDisconnect(char* name) = 0;

protected:
    virtual void main(void * p);
    virtual int inProcessCmd(StringCMD* cmd) = 0;
    int removeCmdsInQfor(SessionResponseSendBack* sendback);

    UINT32      _currentControlID;
    StringCMD*   _cmdQueue[MaxCMDQueueDeapth];
    int          _head;
    int          _num;
    CMutex*      _lock;
    CSemaphore*  _pSem;
    CSemaphore*  _pStopSem;
    //char*        _id;

private:
    StringCMD* PopProcessCmd();
    void PostProcessCmd(StringCMD* cmd);
    int appendCMD(StringCMD* cmd);
    bool  _bRun;
};

class BroadCaster
{
public:
    BroadCaster() {}
    virtual ~BroadCaster() {}
    virtual void BroadCast(StringEnvelope* ev) = 0;

private:

};

class CameraResponder
{
public:
    CameraResponder() {}
    virtual ~CameraResponder() {}
    virtual int OnCmd(EnumedStringCMD* cmd) = 0;
    //virtual void OnState(int stateName, int StateValue) = 0;
    virtual int OnConnect() = 0;
    virtual int OnDisconnect() = 0;

private:

};


class SCMD_Domain
{
public:
    SCMD_Domain(int domain, int cmdNum);
    ~SCMD_Domain();

    int getDomain(){return _domain;};
    EnumedStringCMD* searchCMD(int cmd);
    //void register(EnumedStringCMD* cmd);
    void Register(EnumedStringCMD*);
    void Event(int cmdID);
    void SetBroadCaster(BroadCaster *b){_pBroadCaster = b;};
    BroadCaster* getBroadCaster(){return _pBroadCaster;};
    void BroadCast(StringEnvelope* ev);

    void SetCameraResponder(CameraResponder *r){_pResponder = r;};
    CameraResponder* getResponder(){return _pResponder;};
    Property* getProperty(int index){return _pProperties[index];};

protected:
    Property**  _pProperties;
    int         _iPropNum;

private:
    int                 _domain;
    EnumedStringCMD**   _ppRegistedCmds;
    int                 _CmdsListLenght;
    int                 _addedNum;
    BroadCaster*        _pBroadCaster;
    CameraResponder*    _pResponder;
};

class Domain_EvnetGenerator
{
public:
    Domain_EvnetGenerator(SCMD_Domain *domain)
    {
        _pDomain = domain;
    }
    ~Domain_EvnetGenerator() {}
    void Event(int i) { _pDomain->Event(i); }
    SCMD_Domain* GetDomainObj() {return _pDomain; }

private:
    SCMD_Domain* _pDomain;
};

class Property
{
public:
    Property(const char* propName
        , int OptionNumber
        //, unsigned long long avalible
    );
    ~Property();
    void save(int value);
    int load();
    void setString(int index, const char* value)
    {
        _values[index] = value;
        _avaliable = _avaliable | (0x01<<index);
    }
    unsigned long long GetAvalible() {return _avaliable; }

protected:
    const char*         _propername;
    const char**        _values;
    int                 _valueNumber;
    unsigned long long  _avaliable;
    int                 _currentValue;
};

#define SCMD_CMD_CLASS(name,index) \
    class ccam_cmd_##name : public EnumedStringCMD \
    { \
        public: \
            typedef EnumedStringCMD inherited; \
            ccam_cmd_##name(SCMD_Domain* domain):inherited(domain, index) \
            { \
            } \
            virtual ~ccam_cmd_##name() {} \
            \
            virtual int execute(EnumedStringCMD* p); \
        private: \
    };

#define SCMD_CMD_EXECUTE(name) \
    int ccam_cmd_##name::execute(EnumedStringCMD* p)

#define SCMD_CMD_NEW(name, domainObj) \
    new ccam_cmd_##name(domainObj)

#define SCMD_DOMIAN_CLASS(name) \
    class ccam_domain_##name : public SCMD_Domain \
    { \
    public: \
        typedef SCMD_Domain inherited; \
        ccam_domain_##name(); \
        ~ccam_domain_##name(); \
    };

#define SCMD_DOMIAN_CONSTRUCTOR(name,domain,num) \
    ccam_domain_##name::ccam_domain_##name \
    ():inherited(domain, num)

#define SCMD_DOMIAN_DISTRUCTOR(name) \
    ccam_domain_##name::~ccam_domain_##name()

#define SCMD_DOMAIN_NEW(domainName)\
    new ccam_domain_##domainName()


#endif


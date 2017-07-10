
#ifndef __UPNP_CLASS_UPGRADER_H__
#define __UPNP_CLASS_UPGRADER_H__

#include "ClinuxThread.h"
#include "GobalMacro.h"

#define MAX_UPGRADER_CLIENT_NUM     1
#define UPGRADE_PORT                10097
#define RECV_BUFFER_SIZE            1024 * 256

#define TMP_UPGRADE_FILENAME        "upgrade.tmp"
#define UPGRADE_FILENAME            "upgrade.tsf"

class TSUpgradeCB
{
public:
    TSUpgradeCB() {}
    ~TSUpgradeCB() {}
    virtual void onUpgradeResult(int ok) = 0;//-1 failed, 0. done, 1 rdy
};

//-----------------------------------------------------------------------
//
//  CTSUpgrader
//
//-----------------------------------------------------------------------
class CTSUpgrader : public CThread
{
public:
    static CTSUpgrader* getInstance(const char* md5, TSUpgradeCB* cb);
    void releaseInstance();

protected:
    virtual void main();

private:
    typedef enum{
        UpgradeStatus_Idle = 0,
        UpgradeStatus_Rdy,
        UpgradeStatus_Ing,
        UpgradeStatus_Done,
        UpgradeStatus_Err,
        UpgradeStatus_Num
    } EUpgradeStatus;

    CTSUpgrader(int lisenPort , const char* md5, TSUpgradeCB* cb);
    ~CTSUpgrader();
    int createServer(int port);
    EUpgradeStatus getState();
    int doGetBuffer(char* buf, int len);
    int doCheckRecvDone();

    static CTSUpgrader *pInstance;

    int mLisenPort;
    char* pBuffer;
    char pMD5[64];
    char pTmpFile[128];
    char pFile[128];
    EUpgradeStatus mState;
    int mServerFd;
    int mUpgradeSock;
    TSUpgradeCB *pCB;
    FILE* fileFd;
};

#endif //__UPNP_CLASS_UPGRADER_H__



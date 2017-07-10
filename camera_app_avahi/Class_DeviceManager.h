
#ifndef __UPNP_CLASS_BTMANAGER_H__
#define __UPNP_CLASS_BTMANAGER_H__

#include "ClinuxThread.h"
#include "GobalMacro.h"

#include "agbox.h"
#include "agbt.h"
//#include "agbt_util.h"
//#include "agbt_hci.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <unistd.h>
#include "CKeyEventProcessThread.h"

class CameraCallback;

typedef enum{
    BTStatus_OFF = 0,
    BTStatus_ON,
    BTStatus_Busy,
    BTStatus_Wait,
    BTStatus_Num,
}EBTStatus;

typedef enum{
    BTScanStatus_IDLE = 0,
    BTScanStatus_Scaning,
    BTScanStatus_GotList,
    BTScanStatus_Num,
}EBTScanStatus;

typedef enum {
    BTErr_OK,
    BTErr_TryLater,
    BTErr_Err,
}EBTErr;

typedef enum {
    BTType_OBD = 0,
    BTType_HID,
}EBTType;

class TSBTManagerCB
{
public:
    TSBTManagerCB(){};
    ~TSBTManagerCB(){};
    virtual void onBTEnabled(EBTErr err) = 0;
    virtual void onBTDisabled(EBTErr err) = 0;
    virtual void onBTScanDone(EBTErr err, const char *list) = 0;
    virtual void onBTOBDBinded(EBTErr err) = 0;
    virtual void onBTOBDUnBinded(EBTErr err) = 0;
    virtual void onBTHIDBinded(EBTErr err) = 0;
    virtual void onBTHIDUnBinded(EBTErr err) = 0;
    virtual void sendStorageState() = 0;
    virtual void sendClientsInfo() = 0;
    virtual void onWifiScanDone(const char *list) = 0;
    virtual void onHotspotConnected(bool bConnected) = 0;
};

//-----------------------------------------------------------------------
//
//  CTSDeviceManager
//
//-----------------------------------------------------------------------
class CTSDeviceManager : public CThread
{
public:
    static void Create(CameraCallback* pr);
    static CTSDeviceManager* getInstance();
    static void releaseInstance();

    EBTStatus getBTStatus() { return mBTStatus; };
    int doEnableBT(bool enable);
    EBTStatus getDevStatus(EBTType type);
    int getHostNum();
    int getHostInfor(int index, char** name, char** mac);
    int doScan();
    int doBind(const char* mac, EBTType type);
    int doUnBind(EBTType type);

    int onBTStatus(int isOn);
    int onOBDStatus(int isOn);
    int onHIDStatus(int isOn);

    void broadcastStorageState();
    void broadcastClientsChanged();

    int SetWifiMode(int mode, const char* ssid = NULL);
    int getWifiMode();
    void ScanNetworks();
    bool ConnectHost(char *ssid);
    int GetWifiCltSignal();

    void AddBTManagerCB(TSBTManagerCB* cb) { pCB = cb; }
    void SetCameraCallback(CameraCallback* cb) { _pCameraCallback = cb; }

private:
    CTSDeviceManager();
    virtual ~CTSDeviceManager();

    void doGetBTStatus();
    void doGetOBDStatus();
    void doGetHIDStatus();
    int findToBind();
    void getBTList(char** list);
    bool isBusy();

protected:
    virtual void main(void * p);
    virtual void Stop();
    enum DevMngCmd {
        BTCmdNull = 0,
        BTCmdEnable,
        BTCmdDisable,
        BTCmdScan,
        BTCmdBind,
        BTCmdUnBind,
        DeviceStorageState,
        DeviceClientsInfo,
        WifiCmdScan,
        WifiCmdConnect,
        WifiGetCltSignal,
    };
    struct doBindDev {
        char mac[64];
        EBTType type;
    };

private:
    void _scanWifiHotSpots();
    bool _connectHotSpot();
    bool _getWifiCltSigal();

    static CTSDeviceManager *pInstance;

    bool    bRun;
    DevMngCmd   mCmd;
    doBindDev   mCurDev;
    CSemaphore  *pCmdSem;
    CMutex      *pMutex;

    TSBTManagerCB   *pCB;
    agbt_scan_list_s    mBTScanList;
    EBTStatus       mBTStatus;
    EBTStatus       mOBDStatus;
    EBTStatus       mHIDStatus;
    EBTScanStatus   mScanStatus;

    bool mFormatResult;

    CameraCallback    *_pCameraCallback;
    char              *_pWifiList;
    char              *_pBTList;

    char  _ssid[128];
    int   _last_wifi_signal;
};

#endif //__UPNP_CLASS_BTMANAGER_H__



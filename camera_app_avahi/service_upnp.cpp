#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <dirent.h>
#include <arpa/inet.h> //struct in_addr
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <math.h>

#include "ClinuxThread.h"
#include "upnp_sdcc.h"
#include "class_gsensor_thread.h"

#ifdef AVAHI_ENABLE
#include "class_avahi_thread.h"
#endif

#ifdef support_motorControl
#include "MotorController.h"
#endif

#ifdef ENABLE_LED
#include "LedIndicator.h"
#endif

#include "AudioPlayer.h"

#include "fast_Camera.h"
#include "agbox.h"
#include "aglog.h"
#include "GobalMacro.h"

#ifdef SUPPORT_YKCAM
#include "yk_cameraservice.h"
#endif

#define CORE_DUMP_UPNP

bool bReleaseVersion = false;

int agclkd_open_rtc(int flags)
{
    int ret_val;

    ret_val = open("/dev/rtc", flags);
    if (ret_val >= 0) {
        goto agclkd_open_rtc_exit;
    }
    ret_val = open("/dev/rtc0", flags);
    if (ret_val >= 0) {
        goto agclkd_open_rtc_exit;
    }
    ret_val = open("/dev/misc/rtc", flags);
    if (ret_val >= 0) {
        goto agclkd_open_rtc_exit;
    }

agclkd_open_rtc_exit:
    return ret_val;
}

int agclkd_set_rtc(time_t utc)
{
    int ret_val;
    int rtc_fd;
    struct tm sys_tm;
    struct tm *psys_tm;
    struct tm loctime;
    char tm_buf[256];

    rtc_fd = agclkd_open_rtc(O_WRONLY);
    if (rtc_fd < 0) {
        ret_val = rtc_fd;
        //AGLOG_ERR("%s: agclkd_open_rtc() = %d\n",__func__, ret_val);
        goto agclkd_set_rtc_exit;
    }
    psys_tm = gmtime_r(&utc, &sys_tm);
    if (psys_tm != &sys_tm) {
        ret_val = -1;
        goto agclkd_set_rtc_close;
    }
    ret_val = ioctl(rtc_fd, RTC_SET_TIME, &sys_tm);
    if (ret_val < 0) {
        //AGLOG_ERR("%s: RTC_SET_TIME(%ld) = %d\n",	__func__, utc, ret_val);
        goto agclkd_set_rtc_close;
    }
    localtime_r(&utc, &loctime);
    strftime(tm_buf, sizeof(tm_buf), "%a %b %e %H:%M:%S %Z %Y", &loctime);
    //AGLOG_NOTICE("%s(%ld)\t%s\n", __func__, utc, tm_buf);

agclkd_set_rtc_close:
    close(rtc_fd);
agclkd_set_rtc_exit:
    return ret_val;
}

class cameraPropertyEventHandler : public CPropertyEventHandler
{
public:
    cameraPropertyEventHandler(ui::CFastCamera *camera)
        : _CurrentIP(0) { _pCameraUI = camera; }
    virtual ~cameraPropertyEventHandler(){}
    virtual void EventPropertyUpdate(changed_property_type state) {
        if (property_type_storage == state) {
            char tmp[256] = {0x00};
            agcmd_property_get(SotrageStatusPropertyName, tmp, "");
            CAMERALOG("temp.mmcblk0p1.status = %s", tmp);
            if (strcasecmp(tmp, "mount_ok") == 0) {
                IPC_AVF_Client::getObject()->onTFReady(true);
            } else {
                IPC_AVF_Client::getObject()->onTFReady(false);
            }
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_storage);
            }
        } else if (property_type_usb_storage == state) {
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_usb_storage);
            }
        } else if (property_type_wifi == state) {
            CAMERALOG("wifi changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_Wifi);
            }
#ifdef SUPPORT_YKCAM
            char ip[32];
            if (_checkIpStatus(ip, 32) != 0) {
                if (YKCameraService::getInstance()) {
                    YKCameraService::getInstance()->start(ip);
                }
            }
#endif
        } else if (property_type_language == state) {
            CAMERALOG("language changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_language);
            }
        } else if (property_type_audio == state) {
            CAMERALOG("audio status changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_audio);
            }
        } else if ((property_type_Bluetooth == state)
            || (property_type_hid == state)
            || (property_type_obd == state))
        {
            //CAMERALOG("bluetooth status changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_bluetooth);
            }
        } else if (property_type_power == state) {
            //CAMERALOG("power status changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_power);
            }
        } else if (property_type_resolution == state) {
            CAMERALOG("resolution status changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_resolution);
            }
        } else if (property_type_rec_mode == state) {
            CAMERALOG("rec mode status changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_rec_mode);
            }
        } else if (property_type_ptl_interval == state) {
            CAMERALOG("ptl interval status changed");
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_ptl_interval);
            }
        } else if (property_type_settings == state) {
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_settings);
            }
        } else if (property_type_rotation == state) {
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_rotation);
            }
        } else if (property_type_rotation_mode == state) {
            if (_pCameraUI) {
                _pCameraUI->OnPropertyChange(NULL, PropertyChangeType_rotation_mode);
            }
        }
    }

private:
    int _checkIpStatus(char *ip, int len)
    {
        char wifi_prop[AGCMD_PROPERTY_SIZE];
        char ip_prop_name[AGCMD_PROPERTY_SIZE];
        char prebuild_prop[AGCMD_PROPERTY_SIZE];

        memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get(wifiStatusPropertyName, wifi_prop, "NA");
        if (strcasecmp(wifi_prop, "on") == 0) {
            // wifi is on, continue
        } else {
            return 0;
        }

        memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get(wifiModePropertyName, wifi_prop, "NA");
        if (strcasecmp(wifi_prop, "AP") == 0) {
            memset(prebuild_prop, 0x00, AGCMD_PROPERTY_SIZE);
            agcmd_property_get("prebuild.board.if_ap", prebuild_prop, "NA");

            memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
            agcmd_property_get("temp.earlyboot.if_ap", wifi_prop, prebuild_prop);
        } else {
            memset(prebuild_prop, 0x00, AGCMD_PROPERTY_SIZE);
            agcmd_property_get("prebuild.board.if_wlan", prebuild_prop, "NA");

            memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
            agcmd_property_get("temp.earlyboot.if_wlan", wifi_prop, prebuild_prop);
        }

        snprintf(ip_prop_name, AGCMD_PROPERTY_SIZE, "temp.%s.ip", wifi_prop);
        memset(wifi_prop, 0x00, AGCMD_PROPERTY_SIZE);
        agcmd_property_get(ip_prop_name, wifi_prop, "NA");

        UINT32 rt = inet_addr(wifi_prop);
        CAMERALOG("%s = %s, ip = %d", ip_prop_name, wifi_prop, rt);
        if ((rt != (UINT32)-1) && (rt != 0)) {
            if (ip) {
                snprintf(ip, len, "%s", wifi_prop);
            }
            if(_CurrentIP != (int)rt) {
                _CurrentIP = rt;
                return 2;
            } else {
                return 1;
            }
        }

        // IP is not ready.
        return 0;
    }
    ui::CFastCamera *_pCameraUI;
    int             _CurrentIP;
};

class UCCFactory
{
public:
    UCCFactory();
    ~UCCFactory();

    static UCCFactory* Create();
    static void Destroy();

    void NoBatteryTime();
	
private:
    static UCCFactory *_pInstance;

    CUpnpCloudCamera *_pCamera;

    ui::CFastCamera *_pFC;

    cameraPropertyEventHandler *_pPropHandler;
};


UCCFactory* UCCFactory::_pInstance = NULL;

void UCCFactory::NoBatteryTime()
{
    time_t t;
    time(&t);
    struct tm tt2016;
    memset(&tt2016, 0x00, sizeof(tt2016));
    tt2016.tm_year = 2016 - 1900;
    tt2016.tm_mon = 0;
    tt2016.tm_mday = 1;
    tt2016.tm_hour = 0;
    tt2016.tm_min = 0;
    tt2016.tm_sec = 1;
    tt2016.tm_wday = 2;
    time_t t2016 = mktime(&tt2016);
    if (difftime(t, t2016) < 0) {
        struct timeval tt;
        tt.tv_sec = t2016;
        tt.tv_usec = 0;
        struct timezone tz;
        tz.tz_minuteswest = 0;
        tz.tz_dsttime = 0;
        settimeofday(&tt, &tz);
        agclkd_set_rtc(t2016);

#if defined(PLATFORM22A)
        char tmp[256];
        memset(tmp, 0, sizeof(tmp));
        snprintf(tmp, 256, "%sGMT%+d", timezongDir, 4);
        int rt = 0;
        rt = remove(timezongLink);
        if (rt < 0) {
            CAMERALOG("remove(timezongLink) failed!");
        }
        rt = symlink(tmp, timezongLink);
        if (rt < 0) {
            CAMERALOG("symlink(tmp, timezongLink) failed!");
        }
#endif
    }
}


UCCFactory::UCCFactory()
{
    // use system log:
    aglog_init("upnp_service",
        (AGLOG_MODE_STD | AGLOG_MODE_SYSLOG),
        AGLOG_LEVEL_NOTICE);

#ifdef SET_DEFAULTTIME
    NoBatteryTime();
#endif
    CTimerThread::Create();
    _pCamera = new CUpnpCloudCamera();
    _pFC = new ui::CFastCamera(_pCamera);
    _pFC->Start();

    _pCamera->setCameraCallback(_pFC); // TODO: find a better way to add the callback

    // Create device manager.
    CTSDeviceManager::Create(_pFC);

    CPropsEventThread::Create();
    _pPropHandler = new cameraPropertyEventHandler(_pFC);
    CPropsEventThread::getInstance()->AddPEH(_pPropHandler);

    // Fake an event to trigger an detect on boot up, so that to get initial status
    _pPropHandler->EventPropertyUpdate(property_type_wifi);
    _pPropHandler->EventPropertyUpdate(property_type_storage);
    _pPropHandler->EventPropertyUpdate(property_type_rotation);

#ifdef ENABLE_LED
    CLedIndicator::Create();
    CPropsEventThread::getInstance()->AddPEH(CLedIndicator::getInstance());
#endif

#ifdef GSensorTestThread
    CGsensorThread::create();
#endif

#ifdef AVAHI_ENABLE
    CAvahiServerThread::create();
#endif

#ifdef support_motorControl
    MotorControlService::Create();
#endif

#ifdef support_obd
    COBDServiceThread::create();
#endif

    // lastly, check whether any external device is available
    _pFC->CheckStatusOnBootup();
}

UCCFactory::~UCCFactory()
{
    CPropsEventThread::Destroy();

#ifdef support_obd
    COBDServiceThread::destroy();
#endif

#ifdef support_motorControl
    MotorControlService::Destroy();
#endif

#ifdef AVAHI_ENABLE
    CAvahiServerThread::destroy();
#endif

#ifdef GSensorTestThread
    CGsensorThread::destroy();
#endif

    delete _pPropHandler;

    _pFC->Stop();
    delete _pFC;

    CTimerThread::Destroy();
    EventProcessThread::destroyEPTObject();
    CTSDeviceManager::releaseInstance();

    delete _pCamera;

#ifdef ENABLE_LOUDSPEAKER
    CAudioPlayer::releaseInstance();
#endif

    CAMERALOG("---delete factory finished");
}

UCCFactory* UCCFactory::Create()
{
    if (_pInstance == NULL) {
        _pInstance = new UCCFactory();
    }
    return _pInstance;
}

void UCCFactory::Destroy()
{
    CAMERALOG("UCCFactory::Destroy");
    if (_pInstance != NULL) {
        delete _pInstance;
    }
}

static CSemaphore *pSystemSem;
static char systemCMD[256];

static void signal_handler_SIGTERM(int sig)
{
    CAMERALOG("----terminate signal got");
    memset(systemCMD, 0, sizeof(systemCMD));
    snprintf(systemCMD, 256, "terminate");
    pSystemSem->Give();
}

static void signal_segment_fault(int sig) {
    void *array[15];
    size_t size;
    char **strings;
    size_t i;

    size = backtrace(array, 15);
    strings = backtrace_symbols(array, size);

    CAMERALOG("Obtained %zd stack frames.", size);

    for (i = 0; i < size; i++) {
        CAMERALOG("%2d %s", i, strings[i]);
    }

    free (strings);

#if 0
    memset(systemCMD, 0, sizeof(systemCMD));
    snprintf(systemCMD, 256, "terminate");
    pSystemSem->Give();
#else
    exit(0);
#endif
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        CAMERALOG("%d %s", i, argv[i]);
        if (strcmp(argv[i], "-release") == 0) {
            bReleaseVersion = true;
        }
    }
#if defined(CORE_DUMP_UPNP)
{
    int ret_val = 0;
    struct rlimit limit;

    limit.rlim_cur = RLIM_INFINITY;
    limit.rlim_max = RLIM_INFINITY;
    ret_val = setrlimit(RLIMIT_CORE, &limit);
    if (ret_val) {
        CAMERALOG("setrlimit(RLIMIT_CORE) = %d", ret_val);
    }
    ret_val = getrlimit(RLIMIT_CORE, &limit);
    if (ret_val) {
        CAMERALOG("getrlimit(RLIMIT_CORE) = %d", ret_val);
    } else {
        CAMERALOG("getrlimit(0x%08X:0x%08X)",
            (unsigned int)limit.rlim_cur,
            (unsigned int)limit.rlim_max);
    }
}
#endif

    UCCFactory* fact = UCCFactory::Create();
    if (fact == NULL) {
        CAMERALOG("Create camera app failed!");
        return 0;
    }

    bool bPowerOff = false;
    bool bReboot = false;

    pSystemSem = new CSemaphore(0,1, "service upnp main sem");

    signal(SIGHUP, signal_handler_SIGTERM);
    signal(SIGINT, signal_handler_SIGTERM);
#if !defined(CORE_DUMP_UPNP)
    signal(SIGSEGV, signal_segment_fault);
    signal(SIGFPE, signal_segment_fault);
#endif

    while (1) {
        pSystemSem->Take(-1);
        if (strcmp(systemCMD, "poweroff") == 0) {
            bPowerOff = true;
            break;
        } else if(strcmp(systemCMD, "terminate") == 0) {
            //bPowerOff = true;
            break;
        } else if(strcmp(systemCMD, "reboot") == 0) {
            bReboot = true;
            break;
        }
    }

#ifndef USE_AVF
    sleep(3);
#endif
    if (bPowerOff) {
        const char *tmp_args[8];

        CAMERALOG("Call \"agsh poweroff\" to power off now!");
        tmp_args[0] = "agsh";
        tmp_args[1] = "poweroff";
        tmp_args[2] = NULL;
        agbox_sh(2, (const char *const *)tmp_args);
    } else if(bReboot) {
        const char *tmp_args[8];
        CAMERALOG("Call \"agsh reboot\" to reboot now!");
        tmp_args[0] = "agsh";
        tmp_args[1] = "reboot";
        tmp_args[2] = NULL;
        agbox_sh(2, (const char *const *)tmp_args);
    } else {
        delete pSystemSem;
        CAMERALOG("--destroy UCCFactory");
        //UCCFactory::Destroy();
        //CAMERALOG("--upnp exit");
    }
    return 0;
}


void DestroyUpnpService(bool bReboot)
{
    CAMERALOG("--- destroy Upnp service");
    //UCCFactory::Destroy();

    memset(systemCMD, 0, sizeof(systemCMD));
    if (bReboot) {
        snprintf(systemCMD, 256, "reboot");
    } else {
        snprintf(systemCMD, 256, "poweroff");
    }
    pSystemSem->Give();
}

void CameraDelayms(int time_ms)
{
    if (time_ms <= 0) {
        return;
    }

    timeval _tt;
    _tt.tv_sec = time_ms / 1000;
    _tt.tv_usec = (time_ms % 1000) * 1000;
    select(0, NULL, NULL, NULL, &_tt);
}

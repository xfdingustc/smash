
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <agbox.h>
#include "GobalMacro.h"
#include "LedIndicator.h"
#include "ClinuxThread.h"
#include "class_ipc_rec.h"


static void indicator_green_on() {
    // turn on green
    FILE *pfGreen = fopen("/sys/class/leds/green/trigger", "rb+");
    if (pfGreen) {
        const char *cmd = "default-on";
        fseek(pfGreen, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfGreen);
        fflush(pfGreen);
        fclose(pfGreen);
        pfGreen = NULL;
    }
}

static void indicator_green_off() {
    // turn off green
    FILE *pfGreen = fopen("/sys/class/leds/green/trigger", "rb+");
    if (pfGreen) {
        const char *cmd = "none";
        fseek(pfGreen, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfGreen);
        fflush(pfGreen);
        fclose(pfGreen);
        pfGreen = NULL;
    }
}

static void indicator_green_twinkle(int speed /* 0: fast; 1: normal; 2: low */) {
    // turn on green
    FILE *pfGreen = fopen("/sys/class/leds/green/trigger", "rb+");
    if (pfGreen) {
        const char *cmd = "timer";
        fseek(pfGreen, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfGreen);
        fflush(pfGreen);
        fclose(pfGreen);
        pfGreen = NULL;
    }

    FILE *pfDelay_off = fopen("/sys/class/leds/green/delay_off", "w+");
    if (pfDelay_off) {
        const char *delay = "500";
        if (speed == 1) {
            delay = "1000";
        } else if (speed == 2) {
            delay = "2000";
        }
        fwrite(delay, strlen(delay), 1, pfDelay_off);
        fflush(pfDelay_off);
        fclose(pfDelay_off);
        pfDelay_off = NULL;
    }

    FILE *pfDelay_on = fopen("/sys/class/leds/green/delay_on", "w+");
    if (pfDelay_on) {
        const char *on = "500";
        if (speed == 1) {
            on = "1000";
        } else if (speed == 2) {
            on = "2000";
        }
        fwrite(on, strlen(on), 1, pfDelay_on);
        fflush(pfDelay_on);
        fclose(pfDelay_on);
        pfDelay_on = NULL;
    }
}

static void indicator_green_oneshot(int times) {
    // turn on green oneshot
    FILE *pfGreen = fopen("/sys/class/leds/green/trigger", "rb+");
    if (pfGreen) {
        const char *cmd = "oneshot";
        fseek(pfGreen, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfGreen);
        fflush(pfGreen);
        fclose(pfGreen);
        pfGreen = NULL;
    }

    // shot
    FILE *pfShot = fopen("/sys/class/leds/green/shot", "w");
    if (pfShot) {
        char cmd[8] = "";
        snprintf(cmd, 8, "%d", times);
        fseek(pfShot, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfShot);
        fflush(pfShot);
        fclose(pfShot);
        pfShot = NULL;
    }
}

static void indicator_red_on() {
    // turn on red
    FILE *pfRed = fopen("/sys/class/leds/red/trigger", "rb+");
    if (pfRed) {
        const char *cmd = "default-on";
        fseek(pfRed, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfRed);
        fflush(pfRed);
        fclose(pfRed);
        pfRed = NULL;
    }
}

static void indicator_red_off() {
    // turn off red
    FILE *pfRed = fopen("/sys/class/leds/red/trigger", "rb+");
    if (pfRed) {
        const char *cmd = "none";
        fseek(pfRed, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfRed);
        fflush(pfRed);
        fclose(pfRed);
        pfRed = NULL;
    }
}

static void indicator_red_twinkle(int speed /* 0: fast; 1: normal; 2: low */) {
    // turn on red
    FILE *pfRed = fopen("/sys/class/leds/red/trigger", "rb+");
    if (pfRed) {
        const char *cmd = "timer";
        fseek(pfRed, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfRed);
        fflush(pfRed);
        fclose(pfRed);
        pfRed = NULL;
    }

    FILE *pfDelay_off = fopen("/sys/class/leds/red/delay_off", "w+");
    if (pfDelay_off) {
        const char *delay = "500";
        if (speed == 1) {
            delay = "1000";
        } else if (speed == 2) {
            delay = "2000";
        }
        fwrite(delay, strlen(delay), 1, pfDelay_off);
        fflush(pfDelay_off);
        fclose(pfDelay_off);
        pfDelay_off = NULL;
    }

    FILE *pfDelay_on = fopen("/sys/class/leds/red/delay_on", "w+");
    if (pfDelay_on) {
        const char *on = "500";
        if (speed == 1) {
            on = "1000";
        } else if (speed == 2) {
            on = "2000";
        }
        fwrite(on, strlen(on), 1, pfDelay_on);
        fflush(pfDelay_on);
        fclose(pfDelay_on);
        pfDelay_on = NULL;
    }
}

static void indicator_red_oneshot(int times) {
    // turn on red oneshot
    FILE *pfRed = fopen("/sys/class/leds/red/trigger", "rb+");
    if (pfRed) {
        const char *cmd = "oneshot";
        fseek(pfRed, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfRed);
        fflush(pfRed);
        fclose(pfRed);
        pfRed = NULL;
    }

    // shot
    FILE *pfShot = fopen("/sys/class/leds/red/shot", "w");
    if (pfShot) {
        char cmd[8] = "";
        snprintf(cmd, 8, "%d", times);
        fseek(pfShot, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfShot);
        fflush(pfShot);
        fclose(pfShot);
        pfShot = NULL;
    }
}

static void indicator_blue_on() {
    // turn on blue
    FILE *pfBlue = fopen("/sys/class/leds/blue/trigger", "rb+");
    if (pfBlue) {
        const char *cmd = "default-on";
        fseek(pfBlue, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfBlue);
        fflush(pfBlue);
        fclose(pfBlue);
        pfBlue = NULL;
    }
}

static void indicator_blue_off() {
    // turn off blue
    FILE *pfBlue = fopen("/sys/class/leds/blue/trigger", "rb+");
    if (pfBlue) {
        const char *cmd = "none";
        fseek(pfBlue, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfBlue);
        fflush(pfBlue);
        fclose(pfBlue);
        pfBlue = NULL;
    }
}

static void indicator_blue_twinkle(int speed /* 0: fast; 1: normal; 2: low */) {
    // turn on blue
    FILE *pfBlue = fopen("/sys/class/leds/blue/trigger", "rb+");
    if (pfBlue) {
        const char *cmd = "timer";
        fseek(pfBlue, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfBlue);
        fflush(pfBlue);
        fclose(pfBlue);
        pfBlue = NULL;
    }

    FILE *pfDelay_off = fopen("/sys/class/leds/blue/delay_off", "w+");
    if (pfDelay_off) {
        const char *delay = "500";
        if (speed == 1) {
            delay = "1000";
        } else if (speed == 2) {
            delay = "2000";
        }
        fwrite(delay, strlen(delay), 1, pfDelay_off);
        fflush(pfDelay_off);
        fclose(pfDelay_off);
        pfDelay_off = NULL;
    }

    FILE *pfDelay_on = fopen("/sys/class/leds/blue/delay_on", "w+");
    if (pfDelay_on) {
        const char *on = "500";
        if (speed == 1) {
            on = "1000";
        } else if (speed == 2) {
            on = "2000";
        }
        fwrite(on, strlen(on), 1, pfDelay_on);
        fflush(pfDelay_on);
        fclose(pfDelay_on);
        pfDelay_on = NULL;
    }
}

static void indicator_blue_oneshot(int times) {
    // turn on blue oneshot
    FILE *pfBlue = fopen("/sys/class/leds/blue/trigger", "rb+");
    if (pfBlue) {
        const char *cmd = "oneshot";
        fseek(pfBlue, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfBlue);
        fflush(pfBlue);
        fclose(pfBlue);
        pfBlue = NULL;
    }

    // shot
    FILE *pfShot = fopen("/sys/class/leds/blue/shot", "w");
    if (pfShot) {
        char cmd[8] = "";
        snprintf(cmd, 8, "%d", times);
        fseek(pfShot, 0, SEEK_SET);
        fwrite(cmd, strlen(cmd), 1, pfShot);
        fflush(pfShot);
        fclose(pfShot);
        pfShot = NULL;
    }
}


//-------------- CLedIndicator --------------
const static char* indicatorTimer = "indicator Timer";
CLedIndicator* CLedIndicator::pInstance = NULL;

void CLedIndicator::Create() {
    if (pInstance == NULL) {
        pInstance = new CLedIndicator();
    }
}

void CLedIndicator::releaseInstance() {
    // TODO:
}

CLedIndicator* CLedIndicator::getInstance() {
    if (pInstance == NULL) {
        pInstance = new CLedIndicator();
    }
    return pInstance;
}

void CLedIndicator::IndicatorTimer(void *para) {
    CLedIndicator *indicator = (CLedIndicator *)para;
    if (indicator) {
        indicator->pMutex->Take();
        indicator->_updateLedDisplay();
        indicator->pMutex->Give();
    }
}

CLedIndicator::CLedIndicator()
    : _bRecording(false)
    , _bLCDOn(true)
{
    pMutex = new CMutex("CLedIndicator");
    if (CTimerThread::GetInstance()) {
        CTimerThread::GetInstance()->ApplyTimer
            (60/* 60=6s */, CLedIndicator::IndicatorTimer, this, true, indicatorTimer);
    }
}

CLedIndicator::~CLedIndicator()
{
    delete pMutex;
    CAMERALOG("destroyed");
}

void CLedIndicator::_updateLedDisplay() {
    // get battery status
    char tmp[AGCMD_PROPERTY_SIZE]= {0x00};;
    agcmd_property_get(propPowerSupplyBCapacity, tmp, "0");
    int batteryV = strtoul(tmp, NULL, 0);
    agcmd_property_get(powerstateName, tmp, "Discharging");
    bool bIncharge = !!(strcmp(tmp, "Charging") == 0);

    // get TF status
    agcmd_property_get(SotrageStatusPropertyName, tmp, "");
    bool bNoTF = false;
    bool bTFFull = false;
    if((strcmp(tmp, "mount_fail") == 0) || (strcmp(tmp, "NA") == 0)) {
        bNoTF = true;
    } else if (strcmp(tmp, "mount_ok") == 0) {
        if (IPC_AVF_Client::getObject()) {
            int vdbSpaceState = IPC_AVF_Client::getObject()->GetStorageState();
            if (vdbSpaceState == 1) {
                bTFFull = true;
            }
        }
    }

    // update red LED display
    if (bNoTF || bTFFull) {
        indicator_red_twinkle(1);
    } else if (!bIncharge && (batteryV < 15)) {
        indicator_red_twinkle(2);
    } else if (bIncharge && (batteryV < 100)) {
        indicator_red_on();
    } else {
        indicator_red_off();
    }

    // update blue RED display
    if (_bRecording) {
        indicator_blue_twinkle(0);
    } else if (_bLCDOn) {
        indicator_blue_off();
    } else if (!_bLCDOn) {
        indicator_blue_on();
    }
}

void CLedIndicator::EventPropertyUpdate(changed_property_type state) {
    if ((property_type_storage == state) || (property_type_power == state)) {
        pMutex->Take();
        _updateLedDisplay();
        pMutex->Give();
    }
}

void CLedIndicator::recStatusChanged(bool recording) {
    pMutex->Take();
    _bRecording = recording;
    _updateLedDisplay();
    pMutex->Give();
}

void CLedIndicator::blueOneshot() {
    pMutex->Take();
    indicator_blue_oneshot(1);
    pMutex->Give();
}


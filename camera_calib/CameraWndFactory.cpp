

#include <stdlib.h>
#include <unistd.h>

#include "linux.h"
#include "Window.h"
#include "WindowManager.h"
#include "CalibUI.h"
#include "CameraWndFactory.h"

namespace ui {

Window* CameraWndFactory::createWindow(const char *name) {
    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    // For test purpose
    if (strcmp(name, WND_BEGIN) == 0) {
        return new CalibBeginWnd(WND_BEGIN, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_TOUCH) == 0) {
        return new TouchTestWnd(WND_TOUCH, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_BATTERY) == 0) {
        return new BatteryTestWnd(WND_BATTERY, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_SDCARD) == 0) {
        return new SDCardTestWnd(WND_SDCARD, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_Audio) == 0) {
        return new AudioTestWnd(WND_Audio, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_Wifi) == 0) {
        return new WifiTestWnd(WND_Wifi, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_Bluetooth) == 0) {
        return new BluetoothTestWnd(WND_Bluetooth, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_IIO) == 0) {
        return new IIOTestWnd(WND_IIO, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WND_GPS) == 0) {
        return new GPSTestWnd(WND_GPS, Size(width, height), Point(0, 0));
    }

    return NULL;
}

};


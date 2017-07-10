

#include <stdlib.h>
#include <unistd.h>

#include "linux.h"
#include "CameraAppl.h"
#include "Window.h"
#include "WindowManager.h"
#include "WindowRegistration.h"
#include "CCameraCircularUI.h"
#include "DebugMenus.h"
#include "FastFurious.h"
#include "LapTimer.h"
#include "Dialogues.h"
#include "Bluetooth.h"
#include "WiFi.h"
#include "CameraWndFactory.h"

namespace ui {

Window* CameraWndFactory::createWindow(const char *name) {
    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);

    Window *wnd = NULL;

    if (strcmp(name, WINDOW_poweroff) == 0) {
        wnd = new PowerOffWnd(_cp, WINDOW_poweroff, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_usb_storage) == 0) {
        wnd = new USBStorageWnd(_cp, WINDOW_usb_storage, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_carmountunplug) == 0) {
        wnd = new CarMountUnplugWnd(_cp, WINDOW_carmountunplug, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_spacefull) == 0) {
        wnd = new SpaceFullWnd(_cp, WINDOW_spacefull, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_cardmissing) == 0) {
        wnd = new CardMissingWnd(_cp, WINDOW_cardmissing, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_diskerror) == 0) {
        wnd = new WriteDiskErrorWnd(_cp, WINDOW_diskerror, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_UnknownFormat) == 0) {
        wnd = new UnknowSDFormatWnd(_cp, WINDOW_UnknownFormat, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_highlight) == 0) {
        wnd = new HighlightWnd(_cp, WINDOW_highlight, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_highlight_failed) == 0) {
        wnd = new HighlightFailedWnd(_cp, WINDOW_highlight_failed, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_obd_connected) == 0) {
        wnd = new OBDConnectedWnd(_cp, WINDOW_obd_connected, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_obd_disconnected) == 0) {
        wnd = new OBDDisconnectedWnd(_cp, WINDOW_obd_disconnected, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_hid_connected) == 0) {
        wnd = new HIDConnectedWnd(_cp, WINDOW_hid_connected, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_hid_disconnected) == 0) {
        wnd = new HIDDisconnectedWnd(_cp, WINDOW_hid_disconnected, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_OBD_not_binded) == 0) {
        wnd = new OBDNotBindedWnd(_cp, WINDOW_OBD_not_binded, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_HID_not_binded) == 0) {
        wnd = new HIDNotBindedWnd(_cp, WINDOW_HID_not_binded, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_overheat) == 0) {
        wnd = new OverheatWnd(_cp, WINDOW_overheat, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wrong_wifi_pwd) == 0) {
        wnd = new WrongWifiPwdWnd(_cp, WINDOW_wrong_wifi_pwd, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_add_wifi_success) == 0) {
        wnd = new WifiAddSuccessWnd(_cp, WINDOW_add_wifi_success, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_loop_space_warning) == 0) {
        wnd = new LoopSpaceWarningWnd(_cp, WINDOW_loop_space_warning,
            Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_space_warning_guide) == 0) {
        wnd = new LoopSpaceWarningDetailWnd(_cp, WINDOW_space_warning_guide,
            Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_obd_defect) == 0) {
        wnd = new BTDefectWnd(_cp, WINDOW_obd_defect, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_obd_upgrading) == 0) {
        wnd = new OBDUpgradingWnd(_cp, WINDOW_obd_upgrading, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_hid_upgrading) == 0) {
        wnd = new HIDUpgradingWnd(_cp, WINDOW_hid_upgrading, Size(width, height), Point(0, 0));

    } else if (strcmp(name, WINDOW_shotcuts) == 0) {
        wnd = new ViewPagerShotcutsWnd(
            _cp, WINDOW_shotcuts,
            Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_gauges) == 0) {
        wnd = new GaugesWnd(
            _cp, WINDOW_gauges,
            Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_settingsgauges) == 0) {
        wnd = new SettingsGaugesWnd(
            _cp, WINDOW_settingsgauges,
            Size(width, height), Point(0, 0));

    } else if (strcmp(name, WINDOW_videomenu) == 0) {
        wnd = new VideoMenuWnd(_cp, WINDOW_videomenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_videoquality) == 0) {
        wnd = new VideoQualityWnd(_cp, WINDOW_videoquality, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_videoresolution) == 0) {
        wnd = new VideoResolutionWnd(_cp, WINDOW_videoresolution, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_audiomenu) == 0) {
        wnd = new AudioMenuWnd(_cp, WINDOW_audiomenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_spkvolume) == 0) {
        wnd = new SpeakerVolumeMenuWnd(_cp, WINDOW_spkvolume, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_micvolume) == 0) {
        wnd = new MicVolumeMenuWnd(_cp, WINDOW_micvolume, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_displaymenu) == 0) {
        wnd = new DisplayMenuWnd(_cp, WINDOW_displaymenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_brightnessmenu) == 0) {
        wnd = new BrightnessMenuWnd(_cp, WINDOW_brightnessmenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_screensaver_settings) == 0) {
        wnd = new ScreenSaverSettingsWnd(_cp, WINDOW_screensaver_settings, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_displayoffmenu) == 0) {
        wnd = new DisplayOffMenuWnd(_cp, WINDOW_displayoffmenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_rotation_menu) == 0) {
        wnd = new RotationModeWnd(_cp, WINDOW_rotation_menu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_screensaver_style) == 0) {
        wnd = new ScreenSaverStyleWnd(_cp, WINDOW_screensaver_style, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_autooffmenu) == 0) {
        wnd = new AutoOffMenuWnd(_cp, WINDOW_autooffmenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_auto_record_menu) == 0) {
        wnd = new AutoRecordWnd(_cp, WINDOW_auto_record_menu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_storagemenu) == 0) {
        wnd = new StorageMenuWnd(_cp, WINDOW_storagemenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_formatTF) == 0) {
        wnd = new FormatTFWnd(_cp, WINDOW_formatTF, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_advancedmenu) == 0) {
        wnd = new AdvancedMenuWnd(_cp, WINDOW_advancedmenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_versionmenu) == 0) {
        wnd = new VersionMenuWnd(_cp, WINDOW_versionmenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_factory_reset_menu) == 0) {
        wnd = new FactoryResetWnd(_cp, WINDOW_factory_reset_menu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_timemenu) == 0) {
        wnd = new TimeDateMenuWnd(_cp, WINDOW_timemenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_timeformat) == 0) {
        wnd = new TimeFormatMenuWnd(_cp, WINDOW_timeformat, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_dateformat) == 0) {
        wnd = new DateFormatMenuWnd(_cp, WINDOW_dateformat, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wifimenu) == 0) {
        wnd = new WifiMenuWnd(_cp, WINDOW_wifimenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_btmenu) == 0) {
        wnd = new BluetoothMenuWnd(_cp, WINDOW_btmenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_obd_detail) == 0) {
        wnd = new OBDDetailWnd(_cp, WINDOW_obd_detail, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_hid_detail) == 0) {
        wnd = new HIDDetailWnd(_cp, WINDOW_hid_detail, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wifimodemenu) == 0) {
        wnd = new WifiModeMenuWnd(_cp, WINDOW_wifimodemenu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_autohotspot_menu) == 0) {
        wnd = new AutoAPMenuWnd(_cp, WINDOW_autohotspot_menu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wifi_detail) == 0) {
        wnd = new WifiClientWnd(_cp, WINDOW_wifi_detail, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wifi_no_connection) == 0) {
        wnd = new WifiNoConnectWnd(_cp, WINDOW_wifi_no_connection, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_obd_preferences) == 0) {
        wnd = new OBDMenuWnd(_cp, WINDOW_obd_preferences, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_gauges_preferences) == 0) {
        wnd = new OBDGaugesWnd(_cp, WINDOW_gauges_preferences, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_auto_mark) == 0) {
        wnd = new AutoMarkWnd(_cp, WINDOW_auto_mark, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_mark_sensitivity) == 0) {
        wnd = new SensitivityWnd(_cp, WINDOW_mark_sensitivity, Size(width, height), Point(0, 0));

    } else if (strcmp(name, WINDOW_screen_saver) == 0) {
        wnd = new ScreenSaverWnd(_cp, WINDOW_screen_saver, Size(width, height), Point(0, 0));

    // For test purpose:
    } else if (strcmp(name, WINDOW_touch) == 0) {
        wnd =  new TouchTestWnd(_cp, WINDOW_touch, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_ColorTest) == 0) {
        wnd = new ColorPalleteWnd(_cp, WINDOW_ColorTest, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_fpstest) == 0) {
        wnd = new FpsTestWnd(_cp, WINDOW_fpstest, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_debug_menu) == 0) {
        wnd = new DebugMenuWnd(_cp, WINDOW_debug_menu, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wifi_mode_test) == 0) {
        wnd = new WifiModeTestWnd(_cp, WINDOW_wifi_mode_test, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wifi_signal_test) == 0) {
        wnd = new WifiSignalDebugWnd(_cp, WINDOW_wifi_signal_test, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_wav_play) == 0) {
        wnd = new WavPlayWnd(_cp, WINDOW_wav_play, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_gps_test) == 0) {
        wnd = new GPSDebugWnd(_cp, WINDOW_gps_test, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_units_test) == 0) {
        wnd = new UnitsWnd(_cp, WINDOW_units_test, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_obd_vin) == 0) {
        wnd = new OBDVINWnd(_cp, WINDOW_obd_vin, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_option_23976_fps) == 0) {
        wnd = new Opt23976FpsMenuWnd(_cp, WINDOW_option_23976_fps, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_no_write_record) == 0) {
        wnd = new NoWriteRecordMenuWnd(_cp, WINDOW_no_write_record, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_temperature) == 0) {
        wnd = new TemperatureMenuWnd(_cp, WINDOW_temperature, Size(width, height), Point(0, 0));
    // End of test windows

    } else if (strcmp(name, WINDOW_60mph_countdown) == 0) {
        wnd = new CountdownTestWnd(_cp, WINDOW_60mph_countdown, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_60mph_auto) == 0) {
        wnd = new AutoTestWnd(_cp, WINDOW_60mph_auto, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_test_score) == 0) {
        wnd = new TestScoreWnd(_cp, WINDOW_test_score, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_laptimer) == 0) {
        wnd = new LaptimerWnd(_cp, WINDOW_laptimer, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_lap_score) == 0) {
        wnd = new LapScoreWnd(_cp, WINDOW_lap_score, Size(width, height), Point(0, 0));
    } else if (strcmp(name, WINDOW_lap_realtime_score) == 0) {
        wnd = new LapRealtimeScoreWnd(_cp, WINDOW_lap_realtime_score, Size(width, height), Point(0, 0));

    } else if (strcmp(name, WINDOW_IME) == 0) {
        wnd = new IMEWnd(_cp, WINDOW_IME, Size(width, height), Point(0, 0));
    }

    if (wnd) {
        wnd->SetApp(CameraAppl::getInstance());
    }
    return wnd;
}

};


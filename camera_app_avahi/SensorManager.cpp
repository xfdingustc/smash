
#include "SensorManager.h"
#include "GobalMacro.h"

static uint16_t uint8_to_uint16(uint8_t *pdata)
{
    uint16_t tmp_data;

    tmp_data = pdata[0];
    tmp_data <<= 8;
    tmp_data |= pdata[1];

    return tmp_data;
}

SensorManager::SensorManager()
    : _piio_info_orient(NULL)
    , _piio_info_accel(NULL)
    , _piio_pressure_info(NULL)
    , _obd_client(NULL)
    , _b_obd_flag_set(false)
{
    _piio_info_orient = agsys_iio_orientation_open();
    if (_piio_info_orient) {
        CAMERALOG("orientationopen IIO %s successful", _piio_info_orient->name);
    }

    _piio_info_accel = agsys_iio_accel_open();
    if (_piio_info_accel) {
        CAMERALOG("accel open IIO %s successful", _piio_info_accel->name);
    }

    _piio_pressure_info = agsys_iio_pressure_open();
    if (_piio_pressure_info) {
        CAMERALOG("pressure open IIO %s successful", _piio_pressure_info->name);
    }

    _obd_client = agobd_open_client();
    if (_obd_client) {
        CAMERALOG("open obd successful");
    } else {
        CAMERALOG("Failed to open OBD client!");
        return;
    }

    _setOBDFlags();
}

SensorManager::~SensorManager()
{
    agsys_iio_orientation_close(_piio_info_orient);
    agsys_iio_accel_close(_piio_info_accel);
    agsys_iio_pressure_close(_piio_pressure_info);
    agobd_close_client(_obd_client);
    CAMERALOG("%s() destroyed\n", __FUNCTION__);
}

bool SensorManager::_setOBDFlags()
{
    if (!_obd_client) {
        return false;
    }

    int ret_val = agobd_client_get_info(_obd_client);
    if (ret_val) {
        CAMERALOG("agobd_client_get_info() = %d\n", ret_val);
        return false;
    }

    struct agobd_pid_info_s *ppid_info;
    ppid_info = _obd_client->pobd_info->pid_info;

    // speed
    //ppid_info[0x0D].flag |= AGOBDD_PID_FLAG_POLLING;

    // PSI & Boost
    ppid_info[0x0B].flag |= AGOBDD_PID_FLAG_POLLING;

    // RPM
    ppid_info[0x0C].flag |= AGOBDD_PID_FLAG_POLLING;

    // Engine temperature
    ppid_info[0x05].flag |= AGOBDD_PID_FLAG_POLLING;

    // mass air flow rate
    ppid_info[0x10].flag |= AGOBDD_PID_FLAG_POLLING;

    ret_val = agobd_client_set_info_flag(_obd_client);
    if (ret_val) {
        CAMERALOG("agobd_client_set_info_flag() = %d", ret_val);
    } else {
        CAMERALOG("agobd_client_set_info_flag() success");
        _b_obd_flag_set = true;
    }

    return _b_obd_flag_set;
}

bool SensorManager::getOrientation(
    float &euler_heading,
    float &euler_roll,
    float &euler_pitch)
{
    if (!_piio_info_orient) {
        CAMERALOG("orientation sensor is not opened!\n");
        return false;
    }

    struct agsys_iio_orientation_s iio_orientation;
    int ret_val = agsys_iio_orientation_read(_piio_info_orient, &iio_orientation);
    if (ret_val) {
        CAMERALOG("read orientation sensor failed with error %d!\n", ret_val);
        return false;
    }
    euler_heading =
        (iio_orientation.euler_heading * _piio_info_orient->orientation_scale);
    euler_roll =
        (iio_orientation.euler_roll * _piio_info_orient->orientation_scale);
    euler_pitch =
        (iio_orientation.euler_pitch * _piio_info_orient->orientation_scale);

    return true;
}

bool SensorManager::getAccelXYZ(
    float &accel_x,
    float &accel_y,
    float &accel_z)
{
    if (!_piio_info_accel) {
        CAMERALOG("accel sensor is not opened!\n");
        return false;
    }

    struct agsys_iio_accel_xyz_s iio_accel_xyz;
    int ret_val = agsys_iio_accel_read_xyz(_piio_info_accel, &iio_accel_xyz);
    if (ret_val) {
        CAMERALOG("read accel sensor failed with error %d!\n", ret_val);
        return false;
    }
    accel_x = (iio_accel_xyz.accel_x * _piio_info_accel->accel_scale);
    accel_y = (iio_accel_xyz.accel_y * _piio_info_accel->accel_scale);
    accel_z = (iio_accel_xyz.accel_z * _piio_info_accel->accel_scale);

    return true;
}

bool SensorManager::getIIOPressure(float &pressurePa)
{
    if (!_piio_pressure_info) {
        CAMERALOG("IIO pressure sensor is not opened!\n");
        return false;
    }

    struct agsys_iio_pressure_s iio_pressure;
    int ret_val = agsys_iio_pressure_read(_piio_pressure_info, &iio_pressure);
    if (ret_val) {
        CAMERALOG("read pressure sensor failed with error %d!\n", ret_val);
        return false;
    }

    pressurePa = (iio_pressure.pressure * _piio_pressure_info->pressure_scale);
    //CAMERALOG("%s: Pressure[%8.6f Pa]", _piio_pressure_info->name, pressurePa);

    return true;
}

bool SensorManager::getSpeedKmh(int &speed)
{
    if (!_obd_client) {
        CAMERALOG("obd is not opened!\n");
        return false;
    }

    if (!_b_obd_flag_set) {
        _setOBDFlags();
    }
    if (!_b_obd_flag_set) {
        CAMERALOG("OBD flag is not set!");
        return false;
    }

    int retry = 5;
    int ret_val = 0;
    do {
        retry--;
        ret_val = agobd_client_get_info(_obd_client);
        if (ret_val) {
            CAMERALOG("agobd_client_get_info = %d", ret_val);
            continue;
        } else {
            break;
        }
    } while (retry > 0);
    if (ret_val) {
        return false;
    }

    struct agobd_pid_info_s *ppid_info;
    ppid_info = _obd_client->pobd_info->pid_info;
    if ((ppid_info[0x0D].flag & AGOBDD_PID_FLAG_VALID) != AGOBDD_PID_FLAG_VALID)
    {
        CAMERALOG("ppid_info[0x0D].flag = 0x%x, ppid_info[0x0D]->offset = %d",
            ppid_info[0x0D].flag, ppid_info[0x0D].offset);
        return false;
    }

    //CAMERALOG("%s[%d km/h]",
    //    "Vehicle Speed Sensor",
    //    _obd_client->pobd_info->pid_data[ppid_info[0x0D].offset]);

    speed = _obd_client->pobd_info->pid_data[ppid_info[0x0D].offset];

    return true;
}

bool SensorManager::getBoostKpa(int &pressure)
{
    bool got = false;
    int intakePKpa = 0;
    float atmosphericKpa = 0.0;

    got = getOBDIntakePKpa(intakePKpa);
    if (!got) {
        return false;
    }

    got = getBarometricPressureKpa(atmosphericKpa);
    if (!got) {
        float atmosPa = 0.0;
        got = getIIOPressure(atmosPa);
        if (!got) {
            return false;
        }
        atmosphericKpa = atmosPa / 1000;
    }

    pressure = intakePKpa - (int)atmosphericKpa;
    //CAMERALOG("pressure = %dKpa, intakePKpa = %dKpa, atmosphericKpa = %.2f",
    //    pressure, intakePKpa, atmosphericKpa);

    return true;
}

bool SensorManager::getBarometricPressureKpa(float &pressurePa)
{
    if (!_obd_client) {
        CAMERALOG("obd is not opened!\n");
        return false;
    }

    if (!_b_obd_flag_set) {
        _setOBDFlags();
    }
    if (!_b_obd_flag_set) {
        CAMERALOG("OBD flag is not set!");
        return false;
    }

    int ret_val = 0;
    int retry = 5;
    do {
        retry--;
        ret_val = agobd_client_get_info(_obd_client);
        if (ret_val) {
            CAMERALOG("agobd_client_get_info() = %d", ret_val);
            continue;
        } else {
            break;
        }
    } while (retry > 0);
    if (ret_val) {
        return false;
    }

    struct agobd_pid_info_s *ppid_info;
    ppid_info = _obd_client->pobd_info->pid_info;

    if ((ppid_info[0x33].flag & AGOBDD_PID_FLAG_VALID) == AGOBDD_PID_FLAG_VALID) {
        //CAMERALOG("ppid_info[0x33].flag = 0x%x, ppid_info[0x33]->offset = %d, "
        //    "_obd_client->pobd_info->pid_data[ppid_info[0x33].offset] = %d",
        //    ppid_info[0x33].flag, ppid_info[0x33].offset,
        //    _obd_client->pobd_info->pid_data[ppid_info[0x33].offset]);
        pressurePa = _obd_client->pobd_info->pid_data[ppid_info[0x33].offset];
        return true;
    } else {
        return false;
    }
}

bool SensorManager::getOBDIntakePKpa(int &pressure)
{
    if (!_obd_client) {
        CAMERALOG("obd is not opened!\n");
        return false;
    }

    if (!_b_obd_flag_set) {
        _setOBDFlags();
    }
    if (!_b_obd_flag_set) {
        CAMERALOG("OBD flag is not set!");
        return false;
    }

    int ret_val = 0;
    int retry = 5;
    do {
        retry--;
        ret_val = agobd_client_get_info(_obd_client);
        if (ret_val) {
            CAMERALOG("agobd_client_get_info() = %d", ret_val);
            continue;
        } else {
            break;
        }
    } while (retry > 0);
    if (ret_val) {
        return false;
    }

    struct agobd_pid_info_s *ppid_info;
    ppid_info = _obd_client->pobd_info->pid_info;
    if ((ppid_info[0x0B].flag & AGOBDD_PID_FLAG_VALID) != AGOBDD_PID_FLAG_VALID)
    {
        CAMERALOG("ppid_info[0x0B].flag = 0x%x, ppid_info[0x0B]->offset = %d",
            ppid_info[0x0B].flag, ppid_info[0x0B].offset);
        return false;
    }

    int IntakePressure = _obd_client->pobd_info->pid_data[ppid_info[0x0B].offset];
    if (IntakePressure <= 0) {
        //CAMERALOG("######### IntakePressure = %d, which is invalid", IntakePressure);
        return false;
    }

    if ((ppid_info[0x4f].flag & AGOBDD_PID_FLAG_VALID) == AGOBDD_PID_FLAG_VALID)
    {
        int scaling = _obd_client->pobd_info->pid_data[ppid_info[0x4f].offset];
        //CAMERALOG("ppid_info[0x4f].flag = 0x%x, ppid_info[0x4f]->offset = %d, scaling = %d",
        //    ppid_info[0x4f].flag, ppid_info[0x4f].offset, scaling);
        if (scaling > 0) {
            IntakePressure = IntakePressure * scaling * 10 / 255;
            //CAMERALOG("IntakePressure = %d, scaling = %d", IntakePressure, scaling);
        }
    }

    pressure = IntakePressure;
    //CAMERALOG("pressure = %dkpa, IntakePressure = %dkpa",
    //    pressure, IntakePressure);

    if (pressure == 0) {
        return false;
    } else {
        return true;
    }
}

bool SensorManager::getRPM(int &rpm)
{
    if (!_obd_client) {
        CAMERALOG("obd is not opened!\n");
        return false;
    }

    if (!_b_obd_flag_set) {
        _setOBDFlags();
    }
    if (!_b_obd_flag_set) {
        CAMERALOG("OBD flag is not set!");
        return false;
    }

    int ret_val = 0;
    int retry = 5;
    do {
        retry--;
        ret_val = agobd_client_get_info(_obd_client);
        if (ret_val) {
            CAMERALOG("agobd_client_get_info = %d", ret_val);
            continue;
        } else {
            break;
        }
    } while (retry > 0);
    if (ret_val) {
        return false;
    }

    struct agobd_pid_info_s *ppid_info;
    ppid_info = _obd_client->pobd_info->pid_info;
    if ((ppid_info[0x0C].flag & AGOBDD_PID_FLAG_VALID) != AGOBDD_PID_FLAG_VALID)
    {
        CAMERALOG("ppid_info[0x0C].flag = 0x%x, ppid_info[0x0C]->offset = %d",
            ppid_info[0x0C].flag, ppid_info[0x0C].offset);
        return false;
    }

    rpm = uint8_to_uint16(&_obd_client->pobd_info->pid_data[ppid_info[0x0C].offset]);
    rpm >>= 2;

    //CAMERALOG("%s[%d rpm]", "Engine RPM", rpm);

    return true;
}

bool SensorManager::getEngineTemp(int &temperature)
{
    if (!_obd_client) {
        CAMERALOG("obd is not opened!\n");
        return false;
    }

    if (!_b_obd_flag_set) {
        _setOBDFlags();
    }
    if (!_b_obd_flag_set) {
        CAMERALOG("OBD flag is not set!");
        return false;
    }

    int ret_val = 0;
    int retry = 5;
    do {
        retry--;
        ret_val = agobd_client_get_info(_obd_client);
        if (ret_val) {
            CAMERALOG("agobd_client_get_info = %d", ret_val);
            continue;
        } else {
            break;
        }
    } while (retry > 0);
    if (ret_val) {
        return false;
    }

    struct agobd_pid_info_s *ppid_info;
    ppid_info = _obd_client->pobd_info->pid_info;
    if ((ppid_info[0x05].flag & AGOBDD_PID_FLAG_VALID) != AGOBDD_PID_FLAG_VALID)
    {
        CAMERALOG("ppid_info[0x05].flag = 0x%x, ppid_info[0x05]->offset = %d",
            ppid_info[0x05].flag, ppid_info[0x05].offset);
        return false;
    }

    int tmp_data;
    tmp_data = _obd_client->pobd_info->pid_data[ppid_info[0x05].offset];
    tmp_data -= 40;
    temperature = tmp_data;

    //CAMERALOG("%s[%d]",
    //    "Engine Coolant Temperature",
    //    temperature);

    return true;
}

bool SensorManager::getAirFlowRate(int &volume)
{
    if (!_obd_client) {
        CAMERALOG("obd is not opened!\n");
        return false;
    }

    if (!_b_obd_flag_set) {
        _setOBDFlags();
    }
    if (!_b_obd_flag_set) {
        CAMERALOG("OBD flag is not set!");
        return false;
    }

    int ret_val = 0;
    int retry = 5;
    do {
        retry--;
        ret_val = agobd_client_get_info(_obd_client);
        if (ret_val) {
            CAMERALOG("agobd_client_get_info = %d", ret_val);
            continue;
        } else {
            break;
        }
    } while (retry > 0);
    if (ret_val) {
        return false;
    }

    struct agobd_pid_info_s *ppid_info;
    ppid_info = _obd_client->pobd_info->pid_info;
    if ((ppid_info[0x10].flag & AGOBDD_PID_FLAG_VALID) != AGOBDD_PID_FLAG_VALID)
    {
        CAMERALOG("ppid_info[0x10].flag = 0x%x, ppid_info[0x10]->offset = %d",
            ppid_info[0x10].flag, ppid_info[0x10].offset);
        return false;
    }

    float tmp_data;
    tmp_data = uint8_to_uint16(&_obd_client->pobd_info->pid_data[ppid_info[0x10].offset]);
    tmp_data /= 100;

    volume = tmp_data;

    //CAMERALOG("%s[%d grams/sec]",
    //    "Air Flow Rate from Mass Air Flow Sensor",
    //    volume);

    return true;
}


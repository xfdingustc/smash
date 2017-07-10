

#include "agbox.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <getopt.h>
#include <pthread.h>
#include <inttypes.h>

#include <sys/prctl.h>

#include "aglog.h"
#include "agsys_iio.h"
#include "agobd.h"

class SensorManager
{
public:
    SensorManager();
    ~SensorManager();

    bool getOrientation(
        float &euler_heading,
        float &euler_roll,
        float &euler_pitch);

    bool getAccelXYZ(
        float &accel_x,
        float &accel_y,
        float &accel_z);

    bool getIIOPressure(float &pressurePa);
    bool getBarometricPressureKpa(float &pressurePa);

    bool getSpeedKmh(int &speed);

    bool getBoostKpa(int &pressure);

    bool getOBDIntakePKpa(int &pressure);

    bool getRPM(int &rpm);

    bool getAirFlowRate(int &volume);

    bool getEngineTemp(int &temperature);

private:
    bool _setOBDFlags();

    struct agsys_iio_orientation_info_s  *_piio_info_orient;
    struct agsys_iio_accel_info_s        *_piio_info_accel;
    struct agsys_iio_pressure_info_s     *_piio_pressure_info;

    struct agobd_client_info_s *_obd_client;
    bool   _b_obd_flag_set;
};


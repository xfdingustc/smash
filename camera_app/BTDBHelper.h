#ifndef __DB_HELPER_H__
#define __DB_HELPER_H__

#include <stdio.h>
#include <stdlib.h>

#include "ClinuxThread.h"

#include "sqlite3.h"

class BTDBHelper
{
public:
    enum ERROR_CODE {
        ERROR_CODE_None = 0,
        ERROR_CODE_Already_Exists,
        ERROR_CODE_Not_Exists,
        ERROR_CODE_DB_Error,
    };

    enum DEVICE_TYPE {
        DEVICE_TYPE_Unknown = 0,
        DEVICE_TYPE_RC,
        DEVICE_TYPE_OBD,
        DEVICE_TYPE_Car_Mount,
    };

    static BTDBHelper* getBTDBHelper();

    bool isBTDevExists(int type, const char *mac);
    bool isCarMountExists(const char *sn);
    bool addBTDevice(int type,
        const char *mac,
        const char *origin_name,
        const char *new_name,
        const char *car_mount,
        long used_time);
    bool addCarMount(const char *sn,
        const char *mac_last_rc, long rc_used_time,
        const char *mac_last_obd, long obd_used_time);


    bool updateBTDevice(int type,
        const char *mac,
        const char *origin_name,
        const char *new_name,
        const char *car_mount,
        long used_time);

    bool updateCarMount(const char *sn,
        const char *mac_last_rc, long rc_used_time,
        const char *mac_last_obd, long obd_used_time);

    // If not exist, insert one:
    bool updateLastUsedDev(int type, const char *mac_last_used, long used_time);


    bool deleteBTDev(int type, const char *mac);
    bool rmBTDevFromMounts(int type, const char *mac);
    bool deleteCarMounts(); // Can only happen during factory reset?
    bool deleteAllRecords();

    char* getBTDevice(int type, const char *mac);
    char* getAllBTDevs(int type);
    char* getAllBTDevsMapMount(const char *sn, int type);
    char* getCarMount(const char *sn);
    char* getLastBTDevUsed(int type); // may be on a mount, may be not


private:
    BTDBHelper();
    ~BTDBHelper();
    static BTDBHelper *_pInstance;

private:
    enum SQL_OPERATION
    {
        SQL_OPERATION_None = 0,
        SQL_OPERATION_Check_Exist,
        SQL_OPERATION_Insert,
        SQL_OPERATION_Update,
        SQL_OPERATION_Delete,
    };

    static int DBCallback(void *NotUsed, int argc, char **argv, char **azColName);

    bool _connectDB();
    bool _closeDB();
    bool _executeSQL(const char *sql);


    CMutex          *_pMutex;

    sqlite3         *_dbHandle;
    SQL_OPERATION   _cur_op;

    bool            _item_exists;
    char            *_json;
};

#endif

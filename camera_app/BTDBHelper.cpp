
#include <time.h>

#include "GobalMacro.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "BTDBHelper.h"

using namespace rapidjson;

#define DB_NANE "/castle/camera_app_bt.data"

#define TABLE_RC_DEVICES   ("CREATE TABLE IF NOT EXISTS  RC_DEVICES(" \
                            "MAC          TEXT PRIMARY KEY  NOT NULL," \
                            "ORIGIN_NAME  TEXT              NOT NULL," \
                            "NEW_NAME     TEXT              NOT NULL," \
                            "MOUNT_SN     TEXT              NOT NULL," \
                            "USED_TIME    INT               NOT NULL);")

#define TABLE_OBD_DEVICES  ("CREATE TABLE IF NOT EXISTS  OBD_DEVICES(" \
                            "MAC          TEXT PRIMARY KEY  NOT NULL," \
                            "ORIGIN_NAME  TEXT              NOT NULL," \
                            "NEW_NAME     TEXT              NOT NULL," \
                            "MOUNT_SN     TEXT              NOT NULL," \
                            "USED_TIME    INT               NOT NULL);")

#define TABLE_CAR_MOUNTS   ("CREATE TABLE IF NOT EXISTS  CAR_MOUNTS(" \
                            "SN             TEXT PRIMARY KEY  NOT NULL," \
                            "RC_MAC         TEXT              NOT NULL," \
                            "RC_USED_TIME   INT               NOT NULL," \
                            "OBD_MAC        TEXT              NOT NULL," \
                            "OBD_USED_TIME  INT               NOT NULL);")

#define TABLE_LAST_DEVICES ("CREATE TABLE IF NOT EXISTS  LAST_DEVICES(" \
                            "TYPE       INT PRIMARY KEY  NOT NULL," \
                            "MAC        TEXT             NOT NULL," \
                            "USED_TIME  INT              NOT NULL);")

BTDBHelper* BTDBHelper::_pInstance = NULL;

BTDBHelper* BTDBHelper::getBTDBHelper()
{
    if (!_pInstance) {
        _pInstance = new BTDBHelper();
    }
    return _pInstance;
}

/*
typedef int (*sqlite3_callback)(
void*,    Data provided in the 4th argument of sqlite3_exec()
int,      The number of columns in row
char**,   An array of strings representing fields in the row
char**    An array of strings representing column names
);
*/
int BTDBHelper::DBCallback(void *userdata, int argc, char **argv, char **azColName)
{
    BTDBHelper *db = (BTDBHelper *)userdata;
    if (!db) {
        CAMERALOG("user data is null");
        return 0;
    }

    /*CAMERALOG("argc = %d", argc);

    char str[128];
    int len = 0;
    for(int i=0; i < argc; i++){
        len += snprintf(str + len, sizeof(str) - len, "%s\t|\t", azColName[i]);
    }
    CAMERALOG("%s", str);

    len = 0;
    for(int i=0; i < argc; i++){
        len += snprintf(str + len, sizeof(str) - len, "%s\t|\t", argv[i] ? argv[i] : "NULL");
    }
    CAMERALOG("%s", str);*/

    switch (db->_cur_op) {
        case SQL_OPERATION_Check_Exist:
            CAMERALOG("SQL_OPERATION_Check_Exist");
            db->_item_exists = true;
            break;
        default:
            break;
    }

    return 0;
}

BTDBHelper::BTDBHelper()
  : _dbHandle(NULL)
  , _cur_op(SQL_OPERATION_None)
  , _json(NULL)
{
    _pMutex = new CMutex("bt db mutex");

    if (_connectDB())
    {
        _executeSQL(TABLE_RC_DEVICES);
        _executeSQL(TABLE_OBD_DEVICES);
        _executeSQL(TABLE_CAR_MOUNTS);
        _executeSQL(TABLE_LAST_DEVICES);
    }
}

BTDBHelper::~BTDBHelper()
{
    delete _pMutex;

    if (_dbHandle) {
        _closeDB();
    }
}

bool BTDBHelper::_connectDB()
{
    CAMERALOG();

    int ret = sqlite3_open(DB_NANE, &_dbHandle);
    if( ret != SQLITE_OK){
        CAMERALOG("Error(%d) open database: %s", ret, sqlite3_errmsg(_dbHandle));
        return false;
    }

    //CAMERALOG("opened database successfully");
    return true;
}

bool BTDBHelper::_closeDB()
{
    CAMERALOG();

    int ret = sqlite3_close(_dbHandle);
    if ( ret == SQLITE_BUSY ){
        return false;
    }

    //CAMERALOG("database is closed");

    _dbHandle = NULL;

    return true;
}

bool BTDBHelper::_executeSQL(const char *sql)
{
    CAMERALOG("sql = %s", sql);

    if (!sql) {
        return false;
    }

    char *zErrMsg = NULL;

    /* Execute SQL statement */
    int ret = sqlite3_exec(_dbHandle, sql, DBCallback, this, &zErrMsg);
    if( ret != SQLITE_OK ){
        CAMERALOG("Error SQL: %s", zErrMsg);
        sqlite3_free(zErrMsg);

        return false;
    }

    //CAMERALOG("SQL executed successfully\n");
    return true;
}

bool BTDBHelper::isBTDevExists(int type, const char *mac)
{
    AutoLock lock(_pMutex);

    if (!mac) {
        return false;
    }

    const char *bt_type = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_DEVICES";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_DEVICES";
    } else {
        CAMERALOG("Not a bt device!");
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from %s where MAC = \"%s\";",
        bt_type, mac);
    _cur_op = SQL_OPERATION_Check_Exist;
    _item_exists = false;
    bool ret = _executeSQL(sql);
    if (ret) {
        ret = _item_exists;
    }

    _item_exists = false;
    return ret;
}

bool BTDBHelper::isCarMountExists(const char *sn)
{
    AutoLock lock(_pMutex);

    if (!sn) {
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from CAR_MOUNTS where SN = \"%s\";",
        sn);

    _cur_op = SQL_OPERATION_Check_Exist;
    _item_exists = false;
    bool ret = _executeSQL(sql);
    if (ret) {
        ret = _item_exists;
    }

    _item_exists = false;
    return ret;
}

bool BTDBHelper::addBTDevice(int type,
    const char *mac,
    const char *origin_name,
    const char *new_name,
    const char *car_mount,
    long used_time)
{
    AutoLock lock(_pMutex);

    if (!mac || !origin_name || !new_name || !car_mount) {
        return false;
    }

    const char *bt_type = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_DEVICES";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_DEVICES";
    } else {
        CAMERALOG("Not a bt device!");
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "INSERT INTO %s (MAC, ORIGIN_NAME, NEW_NAME, MOUNT_SN, USED_TIME) "
        "VALUES (\"%s\", \"%s\", \"%s\", \"%s\", %ld); ",
        bt_type, mac, origin_name, new_name, car_mount, used_time);
    _cur_op = SQL_OPERATION_Insert;

    return _executeSQL(sql);
}

bool BTDBHelper::addCarMount(const char *sn,
    const char *mac_last_rc, long rc_used_time,
    const char *mac_last_obd, long obd_used_time)
{
    AutoLock lock(_pMutex);

    if (!sn || !mac_last_rc || !mac_last_obd) {
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "INSERT INTO CAR_MOUNTS (SN, RC_MAC, RC_USED_TIME, OBD_MAC, OBD_USED_TIME) "
        "VALUES (\"%s\", \"%s\", %ld, \"%s\", %ld); ",
        sn, mac_last_rc, rc_used_time, mac_last_obd, obd_used_time);
    _cur_op = SQL_OPERATION_Insert;

    return _executeSQL(sql);
}

bool BTDBHelper::updateBTDevice(int type,
        const char *mac,
        const char *origin_name,
        const char *new_name,
        const char *car_mount,
        long used_time)
{
    AutoLock lock(_pMutex);

    if (!mac || !new_name) {
        return false;
    }

    const char *bt_type = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_DEVICES";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_DEVICES";
    } else {
        CAMERALOG("Not a bt device!");
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE %s set "
        "ORIGIN_NAME = \"%s\", "
        "NEW_NAME = \"%s\", "
        "MOUNT_SN = \"%s\", "
        "USED_TIME = %ld "
        "where MAC= \"%s\";",
        bt_type, origin_name, new_name, car_mount, used_time, mac);
    _cur_op = SQL_OPERATION_Update;

    return _executeSQL(sql);
}

bool BTDBHelper::updateCarMount(const char *sn,
        const char *mac_last_rc, long rc_used_time,
        const char *mac_last_obd, long obd_used_time)
{
    AutoLock lock(_pMutex);

    if (!sn || !mac_last_rc) {
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE CAR_MOUNTS set "
        "RC_MAC = \"%s\", "
        "RC_USED_TIME = %ld, "
        "OBD_MAC = \"%s\", "
        "OBD_USED_TIME = %ld "
        "where SN = \"%s\";",
        mac_last_rc, rc_used_time, mac_last_obd, obd_used_time, sn);
    _cur_op = SQL_OPERATION_Update;

    return _executeSQL(sql);
}

bool BTDBHelper::updateLastUsedDev(int type, const char *mac_last_used, long used_time)
{
    AutoLock lock(_pMutex);

    // If don't exist, insert one into LAST_DEVICES table.
    if (!mac_last_used) {
        return false;
    }

    if ((type != DEVICE_TYPE_RC) && (type != DEVICE_TYPE_OBD)) {
        CAMERALOG("Not a bt device!");
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from LAST_DEVICES where TYPE = %d;",
        type);
    _cur_op = SQL_OPERATION_Check_Exist;
    _item_exists = false;
    bool ret = _executeSQL(sql);
    if (ret) {
        ret = _item_exists;
        if (_item_exists) {
            // update
            snprintf(sql, sizeof(sql),
                "UPDATE LAST_DEVICES set MAC = \"%s\", USED_TIME = %ld where TYPE = %d;",
                mac_last_used, used_time, type);
            _cur_op = SQL_OPERATION_Update;
        } else {
            // insert
            snprintf(sql, sizeof(sql),
                "INSERT INTO LAST_DEVICES (TYPE, MAC, USED_TIME) "
                "VALUES (%d, \"%s\", %ld);",
                type, mac_last_used, used_time);
            _cur_op = SQL_OPERATION_Insert;
        }
        ret = _executeSQL(sql);
    }

    _item_exists = false;
    return ret;
}

bool BTDBHelper::deleteBTDev(int type, const char *mac)
{
    AutoLock lock(_pMutex);

    if (!mac) {
        return false;
    }

    const char *bt_type = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_DEVICES";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_DEVICES";
    } else {
        CAMERALOG("Not a bt device!");
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE from %s where MAC= \"%s\";",
        bt_type, mac);
    _cur_op = SQL_OPERATION_Delete;

    return _executeSQL(sql);
}

bool BTDBHelper::deleteCarMounts()
{
    AutoLock lock(_pMutex);

    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE * from CAR_MOUNTS;");
    _cur_op = SQL_OPERATION_Delete;

    return _executeSQL(sql);
}

bool BTDBHelper::deleteAllRecords()
{
    AutoLock lock(_pMutex);

    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE * from RC_DEVICES;"
        "DELETE * from OBD_DEVICES;"
        "DELETE * from CAR_MOUNTS;"
        "DELETE * from LAST_DEVICES;");
    _cur_op = SQL_OPERATION_Delete;

    return _executeSQL(sql);
}

bool BTDBHelper::rmBTDevFromMounts(int type, const char *mac)
{
    AutoLock lock(_pMutex);

    if (!mac) {
        return false;
    }

    const char *bt_type = NULL;
    const char *used_time = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_MAC";
        used_time = "RC_USED_TIME";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_MAC";
        used_time = "OBD_USED_TIME";
    } else {
        CAMERALOG("Not a bt device!");
        return false;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE CAR_MOUNTS set "
        "%s = \"null\", "
        "%s = 0 "
        "where %s = \"%s\";",
        bt_type, used_time, bt_type, mac);
    _cur_op = SQL_OPERATION_Update;

    return _executeSQL(sql);
}

char* BTDBHelper::getBTDevice(int type, const char *mac)
{
    AutoLock lock(_pMutex);

    const char *bt_type = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_DEVICES";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_DEVICES";
    } else {
        CAMERALOG("Not a bt device!");
        return NULL;
    }

    // reset status:
    if (_json) {
        delete [] _json;
        _json = NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from %s where MAC = \"%s\";",
        bt_type, mac);

    sqlite3_stmt * stmt;
    int result = sqlite3_prepare_v2(_dbHandle, sql, -1, &stmt, NULL);
    if (SQLITE_OK == result) {
        Document d1;
        Document::AllocatorType& allocator = d1.GetAllocator();
        d1.SetObject();

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char * str = (const char *)sqlite3_column_text(stmt, 0);
            //CAMERALOG("%s", str);
            Value jMac(kStringType);
            jMac.SetString(str, allocator);
            d1.AddMember("mac", jMac, allocator);

            str = (const char *)sqlite3_column_text(stmt, 1);
            //CAMERALOG("%s", str);
            Value jOrigin_name(kStringType);
            jOrigin_name.SetString(str, allocator);
            d1.AddMember("origin_name", jOrigin_name, allocator);

            str = (const char *)sqlite3_column_text(stmt, 2);
            //CAMERALOG("%s", str);
            Value jNew_name(kStringType);
            jNew_name.SetString(str, allocator);
            d1.AddMember("new_name", jNew_name, allocator);

            str = (const char *)sqlite3_column_text(stmt, 3);
            //CAMERALOG("%s", str);
            Value jCar_mount_sn(kStringType);
            jCar_mount_sn.SetString(str, allocator);
            d1.AddMember("car_mount_sn", jCar_mount_sn, allocator);

            str = (const char *)sqlite3_column_text(stmt, 4);
            //CAMERALOG("%s", str);
            d1.AddMember("used_time", atoi(str), allocator);

            break;
        }

        StringBuffer buffer;
        Writer<StringBuffer> write(buffer);
        d1.Accept(write);
        const char *json_str = buffer.GetString();
        //CAMERALOG("strlen %d: %s", strlen(json_str), json_str);

        _json = new char [strlen(json_str) + 1];
        if (_json) {
            memset(_json, 0x00, strlen(json_str) + 1);
            snprintf(_json, strlen(json_str) + 1, json_str);
        }
    }

    sqlite3_finalize(stmt);

    return _json;
}

char* BTDBHelper::getAllBTDevs(int type)
{
    AutoLock lock(_pMutex);

    const char *bt_type = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_DEVICES";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_DEVICES";
    } else {
        CAMERALOG("Not a bt device!");
        return NULL;
    }

    // reset status:
    if (_json) {
        delete [] _json;
        _json = NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from %s ORDER BY USED_TIME DESC;",
        bt_type);

    sqlite3_stmt * stmt;
    int result = sqlite3_prepare_v2(_dbHandle, sql, -1, &stmt, NULL);
    if (SQLITE_OK == result) {
        Document d1;
        Document::AllocatorType& allocator = d1.GetAllocator();
        d1.SetObject();

        Value array(kArrayType);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Value object(kObjectType);

            const char * str = (const char *)sqlite3_column_text(stmt, 0);
            //CAMERALOG("%s", str);
            Value jMac(kStringType);
            jMac.SetString(str, allocator);
            object.AddMember("mac", jMac, allocator);

            str = (const char *)sqlite3_column_text(stmt, 1);
            //CAMERALOG("%s", str);
            Value jOrigin_name(kStringType);
            jOrigin_name.SetString(str, allocator);
            object.AddMember("origin_name", jOrigin_name, allocator);

            str = (const char *)sqlite3_column_text(stmt, 2);
            //CAMERALOG("%s", str);
            Value jNew_name(kStringType);
            jNew_name.SetString(str, allocator);
            object.AddMember("new_name", jNew_name, allocator);

            str = (const char *)sqlite3_column_text(stmt, 3);
            //CAMERALOG("%s", str);
            Value jCar_mount_sn(kStringType);
            jCar_mount_sn.SetString(str, allocator);
            object.AddMember("car_mount_sn", jCar_mount_sn, allocator);

            str = (const char *)sqlite3_column_text(stmt, 4);
            //CAMERALOG("%s", str);
            object.AddMember("used_time", atoi(str), allocator);

            array.PushBack(object, allocator);
        }
        d1.AddMember("bt_devs", array, allocator);

        StringBuffer buffer;
        Writer<StringBuffer> write(buffer);
        d1.Accept(write);
        const char *json_str = buffer.GetString();
        //CAMERALOG("strlen %d: %s", strlen(json_str), json_str);

        _json = new char [strlen(json_str) + 1];
        if (_json) {
            memset(_json, 0x00, strlen(json_str) + 1);
            snprintf(_json, strlen(json_str) + 1, json_str);
        }
    }

    sqlite3_finalize(stmt);

    return _json;
}

char* BTDBHelper::getAllBTDevsMapMount(const char *sn, int type)
{
    AutoLock lock(_pMutex);

    if (!sn) {
        return NULL;
    }

    const char *bt_type = NULL;
    if (type == DEVICE_TYPE_RC) {
        bt_type = "RC_DEVICES";
    } else if (type == DEVICE_TYPE_OBD) {
        bt_type = "OBD_DEVICES";
    } else {
        CAMERALOG("Not a bt device!");
        return NULL;
    }

    // reset status:
    if (_json) {
        delete [] _json;
        _json = NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from %s where MOUNT_SN = \"%s\" ORDER BY USED_TIME DESC;",
        bt_type, sn);

    sqlite3_stmt * stmt;
    int result = sqlite3_prepare_v2(_dbHandle, sql, -1, &stmt, NULL);
    if (SQLITE_OK == result) {
        Document d1;
        Document::AllocatorType& allocator = d1.GetAllocator();
        d1.SetObject();

        Value array(kArrayType);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Value object(kObjectType);

            const char * str = (const char *)sqlite3_column_text(stmt, 0);
            //CAMERALOG("%s", str);
            Value jMac(kStringType);
            jMac.SetString(str, allocator);
            object.AddMember("mac", jMac, allocator);

            str = (const char *)sqlite3_column_text(stmt, 1);
            //CAMERALOG("%s", str);
            Value jOrigin_name(kStringType);
            jOrigin_name.SetString(str, allocator);
            object.AddMember("origin_name", jOrigin_name, allocator);

            str = (const char *)sqlite3_column_text(stmt, 2);
            //CAMERALOG("%s", str);
            Value jNew_name(kStringType);
            jNew_name.SetString(str, allocator);
            object.AddMember("new_name", jNew_name, allocator);

            str = (const char *)sqlite3_column_text(stmt, 3);
            //CAMERALOG("%s", str);
            Value jCar_mount_sn(kStringType);
            jCar_mount_sn.SetString(str, allocator);
            object.AddMember("car_mount_sn", jCar_mount_sn, allocator);

            str = (const char *)sqlite3_column_text(stmt, 4);
            //CAMERALOG("%s", str);
            object.AddMember("used_time", atoi(str), allocator);

            array.PushBack(object, allocator);
        }
        d1.AddMember("bt_devs", array, allocator);

        StringBuffer buffer;
        Writer<StringBuffer> write(buffer);
        d1.Accept(write);
        const char *json_str = buffer.GetString();
        //CAMERALOG("strlen %d: %s", strlen(json_str), json_str);

        _json = new char [strlen(json_str) + 1];
        if (_json) {
            memset(_json, 0x00, strlen(json_str) + 1);
            snprintf(_json, strlen(json_str) + 1, json_str);
        }
    }

    sqlite3_finalize(stmt);

    return _json;
}

char* BTDBHelper::getCarMount(const char *sn)
{
    AutoLock lock(_pMutex);

    if (!sn) {
        return NULL;
    }

    // reset status:
    if (_json) {
        delete [] _json;
        _json = NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from CAR_MOUNTS where SN = \"%s\";",
        sn);

    sqlite3_stmt * stmt;
    int result = sqlite3_prepare_v2(_dbHandle, sql, -1, &stmt, NULL);
    if (SQLITE_OK == result) {
        Document d1;
        Document::AllocatorType& allocator = d1.GetAllocator();
        d1.SetObject();

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char * str = (const char *)sqlite3_column_text(stmt, 0);
            //CAMERALOG("%s", str);
            Value jSN(kStringType);
            jSN.SetString(str, allocator);
            d1.AddMember("sn", jSN, allocator);

            str = (const char *)sqlite3_column_text(stmt, 1);
            //CAMERALOG("%s", str);
            Value jRC_MAC(kStringType);
            jRC_MAC.SetString(str, allocator);
            d1.AddMember("rc_mac", jRC_MAC, allocator);

            str = (const char *)sqlite3_column_text(stmt, 2);
            //CAMERALOG("%s", str);
            d1.AddMember("rc_used_time", atoi(str), allocator);

            str = (const char *)sqlite3_column_text(stmt, 3);
            //CAMERALOG("%s", str);
            Value jOBD_MAC(kStringType);
            jOBD_MAC.SetString(str, allocator);
            d1.AddMember("obd_mac", jOBD_MAC, allocator);

            str = (const char *)sqlite3_column_text(stmt, 4);
            //CAMERALOG("%s", str);
            d1.AddMember("obd_used_time", atoi(str), allocator);
        }

        StringBuffer buffer;
        Writer<StringBuffer> write(buffer);
        d1.Accept(write);
        const char *json_str = buffer.GetString();
        //CAMERALOG("strlen %d: %s", strlen(json_str), json_str);

        _json = new char [strlen(json_str) + 1];
        if (_json) {
            memset(_json, 0x00, strlen(json_str) + 1);
            snprintf(_json, strlen(json_str) + 1, json_str);
        }
    }

    sqlite3_finalize(stmt);

    return _json;
}

char* BTDBHelper::getLastBTDevUsed(int type)
{
    AutoLock lock(_pMutex);

    if ((type != DEVICE_TYPE_RC) && (type != DEVICE_TYPE_OBD)) {
        CAMERALOG("Not a bt device!");
        return NULL;
    }

    // reset status:
    if (_json) {
        delete [] _json;
        _json = NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * from LAST_DEVICES where TYPE = %d;",
        type);

    sqlite3_stmt * stmt;
    int result = sqlite3_prepare_v2(_dbHandle, sql, -1, &stmt, NULL);
    if (SQLITE_OK == result) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char * str = (const char *)sqlite3_column_text(stmt, 1);
            CAMERALOG("%s", str);
            _json = new char [strlen(str) + 1];
            if (_json) {
                memset(_json, 0x00, strlen(str) + 1);
                snprintf(_json, strlen(str) + 1, str);
            }
            break;
        }
    }

    sqlite3_finalize(stmt);

    return _json;
}


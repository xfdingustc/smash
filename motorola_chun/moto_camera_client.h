
#ifndef __MOTO_CAMERA_CLIENT_H_
#define __MOTO_CAMERA_CLIENT_H_	1

#include "elist.h"

typedef struct moto_upload_file_s
{
	char file_path[256];
	unsigned int start_utime;
	unsigned int end_utime;	//only mp4 need

	//location
	double latitude, longitude;
	unsigned int location_utime;

	list_head_t list_node;

	//only use me, do not write for the caller
	char id[40];
	char moto_name[64];
	char file_sha256[65];
	int file_size;

	list_head_t chunk_list;
}moto_upload_file_t;

#define MAX_WIFI_AVAILABLESSID_COUNT	5
#define MAX_WIFI_SSID_SIZE	64
#define MAX_WIFI_BSSID_SIZE 32
typedef struct moto_wifi_info_s
{
	//active connection
	char active_ssid[MAX_WIFI_SSID_SIZE];
	char active_bssid[MAX_WIFI_BSSID_SIZE];
	short active_frequency;
	short active_linkspeed;
	short avtive_signal_strength;

	//availableSSIDs
	char available_ssids[MAX_WIFI_AVAILABLESSID_COUNT][MAX_WIFI_SSID_SIZE];
}moto_wifi_info_t;

#define MAX_POWER_SOURCE_SIZE 16
typedef struct moto_battery_info_s
{
	char charge_percent;
	char charging;	//true or false
	char bfull;		
	char power_source[MAX_POWER_SOURCE_SIZE]; //usb and so on
}moto_battery_info_t;

typedef struct moto_location_s
{
	double latitude;
	double longitude;
	double altitude;
	double accuracy;
	double speed;
}moto_location_t;

#define MAX_USER_ID_SIZE	64
#define MAX_DEVICE_IS_SIZE 	64
typedef struct moto_upload_des_s
{
	char moto_oath_host[128];
	int moto_oath_port;

	char moto_api_host[128];
	int moto_api_port;

	char tls_key[128];
	char tls_crt[128];
	char sig_crt_path[128];
	char sig_key_path[128];

	char user_id[MAX_USER_ID_SIZE];
	char device_id[MAX_DEVICE_IS_SIZE];
	char dev_key[32];

	moto_wifi_info_t wifi_info;
	moto_battery_info_t battery;
	int storage_available;
	moto_location_t location;

	list_head_t file_list;

	//only use me, caller do not care
	char token[2048];
	char incident_id[40];
	char complete_url[1024];
	unsigned int start_utime, end_utime;
	unsigned int location_utime;
}moto_upload_des_t;

enum MOTO_RES_STATE_ID {
	
	MOTO_RES_STATE_OK	= 0,
	MOTO_RES_STATE_GET_TOKEN_FAIL			= -0x01,
	MOTO_RES_STATE_SEND_INIT_REQUEST_FAIL	= -0x02,
	MOTO_RES_STATE_SEND_FILE_FAIL			= -0x03,
	MOTO_RES_STATE_SEND_FINISH_FAIL			= -0x04,
};

//return: 0 success (must query incident later to check whether finish); -1 error(detail return by result); 1 success(do not query)
int moto_camera_upload_file(moto_upload_des_t *des_ptr, int *result);

//return 0 success; -1 error
int moto_camera_query_incident(moto_upload_des_t *des_ptr, int *result);

moto_upload_file_t *moto_camera_find_upload_file_by_id(moto_upload_des_t *des_ptr, const char *id);

#endif //__MOTO_CAMERA_CLIENT_H_


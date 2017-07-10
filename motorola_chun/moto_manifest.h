
#ifndef __MOTO_MANIFEST_H_
#define __MOTO_MANIFEST_H_	1

#include "elist.h"

typedef struct camera_info_s
{
	char user_id[48];
	char device_id[48];
	char incident_id[40];
	unsigned int start_time, end_time;
	double longitude, latitude, altitude;
	double accuracy, speed;
	char mp4_sha256[256], mp4_file_path[256];
	char jpg_sha256[256], jpg_file_path[256];
	int mp4_file_size, jpg_file_size;
}camera_info_t;

typedef struct moto_upload_des_s moto_upload_des_t;

const char *moto_create_manifest_file(camera_info_t *pd, char *json, int *json_size);
const char *moto_create_manifest_file2(moto_upload_des_t*ptr, char *json, int *json_size);
const char *moto_create_jws_protected_header(const char *public_key_path, char *header);

#endif //__MOTO_MANIFEST_H_


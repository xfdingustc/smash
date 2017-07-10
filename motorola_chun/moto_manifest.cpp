
#include "moto_manifest.h"
#include "moto_tool.h"
#include "moto_camera_client.h"

#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "GobalMacro.h"

static const char *moto_generate_file_name(unsigned int utm, char *file_name, char res_type);

const char *moto_create_manifest_file(camera_info_t *pd, char *json, int *json_size)
{
	char cstart_time[128] = {0}, cend_time[128] = {0},group_id[36] = {0},
		 video_id[36] = {0}, video_file_name[256] = {0}, picture_id[36] = {0}, 
		 picture_file_name[256] = {0};

	int left = (*json_size);
	jwt_generate_iso8601_tz_datetime_str(pd->start_time, cstart_time);
	jwt_generate_iso8601_tz_datetime_str(pd->end_time, cend_time);

	moto_generate_file_name(pd->start_time, video_file_name, 0);
	moto_generate_file_name(pd->start_time, picture_file_name, 1);

	int n = snprintf(json, left, "{\n"
			"  \"deviceId\": \"%s\",\n"
			"  \"incident\": {\n"
			"    \"id\": \"%s\",\n"
			"    \"startTime\": \"%s\",\n"
			"    \"endTime\": \"%s\",\n"
			/*"    \"tags\": [\n"
			"      {\n"
			"        \"text\": \"Carjacking\"\n"
			"      }\n"
			"    ],\n"*/
			"    \"groups\": [\n"
			"      {\n"
			"        \"id\": \"%s\",\n"
			"        \"startTime\": \"%s\",\n"
			"        \"endTime\": \"%s\",\n"
			/*"        \"tags\": [\n"
			"          {\n"
			"            \"text\": \"robbery\"\n"
			"          }\n"
			"        ],\n" */
			"        \"videos\": [\n"
			"          {\n"
			"            \"id\": \"%s\",\n"
			"            \"startTime\": \"%s\",\n"
			"            \"endTime\": \"%s\",\n"
			"            \"file\": {\n"
			"              \"name\": \"%s\",\n"
			"              \"size\": %d,\n"
			"              \"hash\": {\n"
			"                \"function\": \"sha256\",\n"
			"                \"value\": \"%s\"\n"
			"              }\n"
			"            },\n"
			"            \"location\": {\n"
			"              \"latitude\": %.6f,\n"
			"              \"longitude\": %.6f,\n"
			"              \"time\": \"%s\"\n"
			"            }\n"
            "          }\n"
			"        ],"
			"        \"pictures\": [\n"
			"          {\n"
			"            \"id\": \"%s\",\n"
			"            \"time\": \"%s\",\n"
			"            \"file\": {\n"
			"              \"name\": \"%s\",\n"
			"              \"size\": %d,\n"
			"              \"hash\": {\n"
			"                \"function\": \"sha256\",\n"
			"                \"value\": \"%s\"\n"
			"              }\n"
			"            },\n"
			"            \"location\": {\n"
			"              \"latitude\": %.6f,\n"
			"              \"longitude\": %.6f,\n"
			"              \"time\": \"%s\"\n"
			"            }\n"
			"          }\n"
			"        ]\n"
			"      }\n"
			"    ],\n"
			"    \"location\": {\n"
			"      \"latitude\": %.6f,\n"
			"      \"longitude\": %.3f,\n"
			"      \"time\": \"%s\"\n"
			"    }\n"
			"  }\n"
			"}", pd->device_id, pd->incident_id, cstart_time, cend_time, comm_generate_guid(group_id), cstart_time, cend_time,
			comm_generate_guid(video_id), cstart_time, cend_time, video_file_name, pd->mp4_file_size, 
			pd->mp4_sha256, pd->latitude, pd->longitude, cstart_time, comm_generate_guid(picture_id),
			cstart_time, picture_file_name, pd->jpg_file_size, pd->jpg_sha256, pd->latitude, pd->longitude,
			cstart_time, pd->latitude, pd->longitude, cstart_time);
	json[n] = '\0';
	(*json_size) = n;

	return json; 
}
const char *moto_create_manifest_file2(moto_upload_des_t*ptr, char *json, int *json_size)
{
	list_head_t *p, *c;
	int left = (*json_size) - 1, n = 0, ilen = 0, i = 0;
	char cstart_time[128] = {0}, cend_time[128] = {0}, loc_time[128] = {0}, group_id[36] = {0}, *pbuf = json;

	jwt_generate_iso8601_tz_datetime_str(ptr->start_utime, cstart_time);
	jwt_generate_iso8601_tz_datetime_str(ptr->end_utime, cend_time);

    CAMERALOG("#### start_utime = %d, end_utime = %d, cstart_time = %s, cend_time = %s",
        ptr->start_utime, ptr->end_utime, cstart_time, cend_time);

	n = snprintf(pbuf, left, "{\"deviceId\": \"%s\","
			"\"incident\": {"
				"\"id\": \"%s\","
				"\"startTime\": \"%s\","
				"\"endTime\": \"%s\","
				"\"groups\": ["
					"{"
						"\"id\": \"%s\","
						"\"startTime\": \"%s\","
						"\"endTime\": \"%s\",", ptr->device_id,
			comm_generate_guid(ptr->incident_id), cstart_time, cend_time,
			comm_generate_guid(group_id), cstart_time, cend_time);
	ilen += n; left -= n; pbuf += n;

	n = snprintf(pbuf, left, "\"videos\":[");
	ilen += n; left -= n; pbuf += n;

	//mp4
	i = 0;
	list_for_each_safe(p, c, &ptr->file_list)
	{
		if(p == c) break;
		moto_upload_file_t *pf = list_entry(p, moto_upload_file_t, list_node);
		if(pf && strstr(pf->file_path, ".mp4"))
		{
			memset(cstart_time, 0x00, sizeof(cstart_time));
			memset(cend_time, 0x00, sizeof(cend_time));
			memset(pf->id, 0x00, sizeof(pf->id));
			memset(loc_time, 0x00, sizeof(loc_time));
			memset(pf->moto_name, 0x00, sizeof(pf->moto_name));
			memset(pf->file_sha256, 0x00, sizeof(pf->file_sha256));
			comm_generate_guid(pf->id);
			jwt_generate_iso8601_tz_datetime_str(pf->start_utime, cstart_time);
			jwt_generate_iso8601_tz_datetime_str(pf->end_utime, cend_time);
			jwt_generate_iso8601_tz_datetime_str(pf->location_utime, loc_time);
			moto_generate_file_name(pf->start_utime, pf->moto_name, 0);
			pf->file_size = comm_get_file_size(pf->file_path);
			jws_sha256_file(pf->file_path, pf->file_sha256);

			if(0 < i) 
			{
				strcpy(pbuf, ","); pbuf += 1; left -= 1; ilen += 1;
			}

			n = snprintf(pbuf, left, "{\"id\": \"%s\", \"startTime\": \"%s\","
					"\"endTime\": \"%s\", \"file\": {\"name\": \"%s\","
					"\"size\": %d, \"hash\": {\"function\": \"sha256\", \"value\": \"%s\"}"
					"}, \"location\": {\"latitude\": %.6f, \"longitude\": %.6f, \"time\": \"%s\"}}",
					pf->id, cstart_time, cend_time, pf->moto_name, pf->file_size, pf->file_sha256,
					pf->latitude, pf->longitude, loc_time);

			ilen += n; left -= n; pbuf += n; ++i;
		}
	}
	n = snprintf(pbuf, left, "],");
	ilen += n; left -= n; pbuf += n;

	//picture
	n = snprintf(pbuf, left, "\"pictures\":[");
	ilen += n; left -= n; pbuf += n;
	i = 0;
	list_for_each_safe(p, c, &ptr->file_list)
	{
		if(p == c) break;
		moto_upload_file_t *pf = list_entry(p, moto_upload_file_t, list_node);
		if(pf && strstr(pf->file_path, ".jpg"))
		{
			memset(cstart_time, 0x00, sizeof(cstart_time));
			memset(cend_time, 0x00, sizeof(cend_time));
			memset(pf->id, 0x00, sizeof(pf->id));
			memset(loc_time, 0x00, sizeof(loc_time));
			memset(pf->moto_name, 0x00, sizeof(pf->moto_name));
			memset(pf->file_sha256, 0x00, sizeof(pf->file_sha256));
			comm_generate_guid(pf->id);
			jwt_generate_iso8601_tz_datetime_str(pf->start_utime, cstart_time);
			jwt_generate_iso8601_tz_datetime_str(pf->location_utime, loc_time);
			moto_generate_file_name(pf->start_utime, pf->moto_name, 1);
			pf->file_size = comm_get_file_size(pf->file_path);
			jws_sha256_file(pf->file_path, pf->file_sha256);

			if(0 < i) 
			{
				strcpy(pbuf, ","); pbuf += 1; left -= 1; ilen += 1;
			}

			n = snprintf(pbuf, left, "{\"id\": \"%s\", \"time\": \"%s\","
					"\"file\": {\"name\": \"%s\", \"size\": %d, \"hash\": "
					"{ \"function\": \"sha256\", \"value\": \"%s\"}},"
					"\"location\": { \"latitude\": %.6f, \"longitude\": %.6f, \"time\": \"%s\"}}",
					pf->id, cstart_time, pf->moto_name, pf->file_size, pf->file_sha256,
					pf->latitude, pf->longitude, loc_time);

			ilen += n; left -= n; pbuf += n; ++i;
		}
	}
	n = snprintf(pbuf, left, "]}],");
	ilen += n; left -= n; pbuf += n;

	//location
	memset(loc_time, 0x00, sizeof(loc_time));
	jwt_generate_iso8601_tz_datetime_str(ptr->location_utime, loc_time);

	n = snprintf(pbuf, left, "\"location\": {\"latitude\": %.6f, \"longitude\": %.6f, \"time\": \"%s\"}}}",
			ptr->location.latitude, ptr->location.longitude, loc_time);

	ilen += n; left -= n; pbuf += n;
	(*json_size) = ilen;
	return json;
}
const char *moto_create_jws_protected_header(const char *public_key_path, char *header)
{
	FILE *fp = fopen(public_key_path, "rb");
	if(!fp)
	{
		printf("%s[%d]: fopen %s fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, public_key_path, errno, strerror(errno));
		return "";
	}
	strcpy(header, "{\n  \"alg\": \"RS256\",\n  \"x5c\": [\n\"");
	char *pbuf = header + strlen(header);
	char tmp_buf[1024] = {0};
	while(fgets(tmp_buf,1024, fp))
	{
		if(tmp_buf[0] != '-') 
		{
			int ilen = (int)strlen(tmp_buf);
			if(tmp_buf[ilen-1] == '\n') {tmp_buf[ilen-1] = '\0'; ilen -= 1;}
			strcpy(pbuf, tmp_buf); pbuf += strlen(tmp_buf);
		}
		memset(tmp_buf, 0x00, strlen(tmp_buf));
	}
	fclose(fp);
	strcpy(pbuf, "\"\n  ]\n}");
	return header;
}
static const char *moto_generate_file_name(unsigned int utm, char *file_name, char res_type)
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);

	long tv_usec = 0;
	if(tv.tv_sec == utm) tv_usec = tv.tv_usec/1000;

	char suffix[16] = {0}, prefix[16] = {0};
	switch(res_type)
	{
		case 0: strcpy(suffix, "mp4"); strcpy(prefix, "VID"); break;
		case 1: strcpy(suffix, "jpg"); strcpy(prefix, "IMG"); break;
		case 2: strcpy(suffix, "ts"); strcpy(prefix, "VID"); break;
		default: strcpy(suffix, "mp4"); strcpy(prefix, "VID"); break;
	}

	time_t  cur_time = utm;
	struct tm curr = *gmtime(&cur_time);
	if (curr.tm_year > 50)
	{
		sprintf(file_name, "%s_%04d%02d%02d_%02d%02d%02d.%03d.%s", prefix,curr.tm_year+1900, curr.tm_mon+1,
			curr.tm_mday, curr.tm_hour, curr.tm_min, curr.tm_sec,(int)tv_usec, suffix);
	}
	else
	{
		sprintf(file_name, "%s_%04d%02d%02d_%02d%02d%02d.%03d.%s", prefix,curr.tm_year+2000, curr.tm_mon+1,
			curr.tm_mday, curr.tm_hour, curr.tm_min, curr.tm_sec,(int)tv_usec, suffix);
	}
	return file_name;
}

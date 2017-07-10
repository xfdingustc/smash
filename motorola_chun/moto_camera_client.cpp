
#include "moto_camera_client.h"
#include "moto_upload.h"

#include <stdio.h>
#include <stddef.h>

static void moto_print_upload_description(moto_upload_des_t *ptr);

static int moto_query_auto_token(moto_upload_des_t *ptr);

//return: 0 success; -1 error(detail return by result) 
int moto_camera_upload_file(moto_upload_des_t *des_ptr, int *result)
{
	moto_print_upload_description(des_ptr);

	//1 query auth token
	int ret = moto_query_auto_token(des_ptr);
	if(0 != ret)
	{
		(*result) = MOTO_RES_STATE_GET_TOKEN_FAIL; 
		return -1;
	}
	//2 upload video
	return moto_camera_upload_request(des_ptr, result);
}
//return 0 success; -1 error
int moto_camera_query_incident(moto_upload_des_t *des_ptr, int *result)
{
	char json[4096] = {0};
	int json_size = (int)sizeof(json) - 1;
	int ret = moto_camera_query_incident(des_ptr->moto_api_host, des_ptr->moto_api_port, 
			des_ptr->tls_crt, des_ptr->tls_key, des_ptr->incident_id, des_ptr->token, json, json_size);

	if(0 != ret)
	{
		printf("%s[%d]: moto_camera_query_incident fail, ret = %d\r\n", __FUNCTION__, __LINE__, ret);
	}
	else
	{
		printf("%s[%d]: query json [%s]\r\n", __FUNCTION__, __LINE__, json);
	}
	return 0;
}
moto_upload_file_t *moto_camera_find_upload_file_by_id(moto_upload_des_t *des_ptr, const char *id)
{
	list_head_t *p, *c;
	list_for_each_safe(p, c, &des_ptr->file_list)
	{
		if(p == c) break;
		moto_upload_file_t *pf = list_entry(p, moto_upload_file_t, list_node);
		if(pf && (strlen(pf->id) == strlen(id)) && (!strcmp(pf->id, id)))
			return pf;
	}
	return NULL;
}

static void moto_print_upload_description(moto_upload_des_t *ptr)
{
	list_head_t *p, *c;
	printf("moto_oath_server [%s : %d], moto_api_server [%s : %d]\r\ntls_key [%s],"
			"tls_crt [%s], sig_crt_path [%s], sig_key_path [%s]\r\nuser_id [%s], device_id [%s],"
			"dev_key [%s]\r\n", ptr->moto_oath_host, ptr->moto_oath_port, ptr->moto_api_host, 
			ptr->moto_api_port, ptr->tls_key, ptr->tls_crt, ptr->sig_crt_path, ptr->sig_key_path,
			ptr->user_id, ptr->device_id, ptr->dev_key);

	//wifi
	printf("active: ssid [%s], bssid [%s], frequency [%d], linkspeed [%d], signal_strength [%d];\r\n"
			"available_ssids: %s\r\n", ptr->wifi_info.active_ssid, ptr->wifi_info.active_bssid,
			ptr->wifi_info.active_frequency, ptr->wifi_info.active_linkspeed,
			ptr->wifi_info.avtive_signal_strength, ptr->wifi_info.available_ssids[0]);

	//battery
	printf("battery: charge_percent %d, charging [%s], full [%s], power_source [%s]\r\n",
			ptr->battery.charge_percent, ptr->battery.charging?"true":"false",
			ptr->battery.bfull?"true":"false", ptr->battery.power_source);

	printf("storage_available %d, location: {latitude %f, longitude %f, altitude %f, accuracy %f, speed %f}\r\n",
			ptr->storage_available, ptr->location.latitude, ptr->location.longitude, ptr->location.altitude,
			ptr->location.accuracy, ptr->location.speed);

	list_for_each_safe(p, c, &ptr->file_list)
	{
		if(p == c) break;
		moto_upload_file_t *pf = list_entry(p, moto_upload_file_t, list_node);	
		if(pf)
		{
			printf("file: %s\r\n", pf->file_path);
		}
	}
}
static int moto_query_auto_token(moto_upload_des_t *ptr)
{
	int token_size = (int)sizeof(ptr->token) -1;
	int ret = moto_camera_query_oauth_token(ptr->moto_oath_host, ptr->moto_oath_port, ptr->tls_crt, ptr->tls_key, ptr->token, token_size);

	if(0 != ret)
	{
		printf("%s[%d]: moto_camera_query_oauth_token fail, ret = %d\r\n", __FUNCTION__, __LINE__, ret);
	}
	else
	{
		printf("%s[%d]: query token [%s]\r\n", __FUNCTION__, __LINE__, ptr->token);
	}

	return ret;
}

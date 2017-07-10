#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <time.h>
#include "moto_camera_client.h"

#include "moto_upload.h"
#include "moto_tool.h"

static void test_upload2();

int main(int argc, char **argv)
{
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	SSL_load_error_strings();

	test_upload2();
	return 0;
}
static void test_upload2()
{
	moto_upload_des_t upload_des;
	memset(&upload_des, 0x00, sizeof(upload_des));
	INIT_LIST_HEAD(&upload_des.file_list);

	strcpy(upload_des.moto_oath_host, "idmmaster.imw.motorolasolutions.com");
	upload_des.moto_oath_port = 9032;
	strcpy(upload_des.moto_api_host, "api.master.commandcentral.com");
	upload_des.moto_api_port = 443;


	strcpy(upload_des.tls_key, "./ssl/431WAY2765_tls_key.key");
	strcpy(upload_des.tls_crt, "./ssl/431WAY2765_tls.crt");
	strcpy(upload_des.sig_crt_path, "./ssl/431WAY2765_sign.crt");
	strcpy(upload_des.sig_key_path, "./ssl/431WAY2765_sign_key.key");

	strcpy(upload_des.user_id, "xutao.wang@waylens.com");
	strcpy(upload_des.device_id, "431WAY2765");
	strcpy(upload_des.dev_key, "MSISiDevice");

	//wifi
	strcpy(upload_des.wifi_info.active_ssid, "Transee22");
	strcpy(upload_des.wifi_info.active_bssid, "02197545");
	upload_des.wifi_info.active_frequency = 5805;
	upload_des.wifi_info.active_linkspeed = 54;
	upload_des.wifi_info.avtive_signal_strength = 35;
	strcpy(upload_des.wifi_info.available_ssids[0], "Transee22");

	//battery
	upload_des.battery.charge_percent = 75;
	upload_des.battery.charging = 1;
	upload_des.battery.bfull = 0;
	strcpy(upload_des.battery.power_source, "usb");

	upload_des.storage_available = 27812513;

	upload_des.location.latitude = 38.651;
	upload_des.location.longitude = 104.076;
	upload_des.location.altitude = 102.7;
	upload_des.location.accuracy = 2.8;
	upload_des.location.speed = 9.8;

	//mp4
	upload_des.start_utime = time(NULL);
	upload_des.end_utime = upload_des.start_utime + 30;
	upload_des.location_utime = upload_des.start_utime;
	moto_upload_file_t *pfile = (moto_upload_file_t*)malloc(sizeof(moto_upload_file_t));
	if(pfile)
	{
		memset(pfile, 0x00, sizeof(*pfile));
		strcpy(pfile->file_path, "./392_Youtube.mp4");
		pfile->start_utime = upload_des.start_utime;
		pfile->end_utime = upload_des.end_utime;

		pfile->latitude = upload_des.location.latitude;
		pfile->longitude = upload_des.location.longitude;
		pfile->location_utime = upload_des.start_utime;
		INIT_LIST_HEAD(&pfile->list_node);
		INIT_LIST_HEAD(&pfile->chunk_list);
		list_add_tail(&pfile->list_node, &upload_des.file_list);
	}
	//jpg
	pfile = (moto_upload_file_t*)malloc(sizeof(moto_upload_file_t));
	if(pfile)
	{
		memset(pfile, 0x00, sizeof(*pfile));
		strcpy(pfile->file_path, "./00000.jpg");
		pfile->start_utime = upload_des.start_utime;
		pfile->end_utime = upload_des.end_utime;

		pfile->latitude = upload_des.location.latitude;
		pfile->longitude = upload_des.location.longitude;
		pfile->location_utime = upload_des.start_utime;
		INIT_LIST_HEAD(&pfile->list_node);
		INIT_LIST_HEAD(&pfile->chunk_list);
		list_add_tail(&pfile->list_node, &upload_des.file_list);
	}

	int result = 0;
	moto_camera_upload_file(&upload_des, &result);

}

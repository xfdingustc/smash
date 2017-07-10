
#ifndef __MOTO_UPLOAD_H_
#define __MOTO_UPLOAD_H_ 1

#include "elist.h"
#include "moto_manifest.h"

typedef struct chunk_header_item_s
{
	char key[128];
	char value[128];
	list_head_t list_node;
}chunk_header_item_t;

typedef struct chunk_info_s
{
	int chunk_start;
	int length;
	char url[1024];

	list_head_t list_node;
	list_head_t header_list;
}chunk_info_t;

/*
typedef struct file_info_s
{
	char id[40];
	char name[64];
	int size;

	list_head_t chunk_list;
}file_info_t;

typedef struct upload_detail_s
{
	file_info_t mp4_file;
	file_info_t pic_file;

	char complete_url[1024];
}upload_detail_t;
*/

typedef struct moto_upload_des_s moto_upload_des_t;

int moto_camera_query_oauth_token(const char *host, int port, const char *tls_crt, const char *tls_key, char *token, int token_size);
int moto_camera_query_incident(const char *host, int port, const char *tls_crt, const char *tls_key, 
		const char *incident_id,const char *token, char *json, int json_size);
int moto_camera_upload_request(moto_upload_des_t *pc, int *result);

void moto_camera_free_upload_detail(moto_upload_des_t *pd);

#endif //__MOTO_UPLOAD_H_

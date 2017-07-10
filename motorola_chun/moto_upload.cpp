
#include "moto_upload.h"
#include "moto_tool.h"
#include "cJSON.h"
#include "moto_camera_client.h"

#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netdb.h>

static void moto_camera_free_file_info(moto_upload_file_t *pf);

static char *moto_camera_domain_resolution(const char *domain, char *ip);
static int moto_camera_create_tcp_conn(char *ip, unsigned short port, char *err_msg);
static SSL * moto_camera_bind_conn_to_ssl(int des_fd, SSL_CTX *ctx, const char *key, const char *crt);
static int moto_camera_recv_http_header(SSL *ssl, char *buf, int buf_size);
static int moto_camera_recv_packet(SSL *ssl, char *buf, int buf_size);
static int moto_camera_send_packet(SSL *ssl, const char *buf, int buf_size);


static int moto_camera_comm_exec_query_op(const char *host, int port, const char *tls_crt, const char *tls_key, 
		const char *content, char *json, int json_size);
static int moto_camera_exec_upload_file(const char *url, const char *tls_crt, const char *tls_key,
		  const char *ext_header, const char *file_path, int start, int length);

static const char *moto_camera_upload_init_body(moto_upload_des_t *pd, const char *crt, const char *privkey, char *json, int json_size);
static int moto_camera_upload_init_request(moto_upload_des_t *pc);
static int moto_camera_parse_init_response(moto_upload_des_t *pd, cJSON *json_ptr);
static int moto_camera_send_file(moto_upload_file_t *pfile, const char *tls_crt, const char *tls_key);
static int moto_camera_send_finish_upload(const char *url, const char *tls_crt, const char *tls_key, const char *jwt, const char *dev_key);

static void moto_camera_print_init_response(moto_upload_des_t *pd);

int moto_camera_query_oauth_token(const char *host, int port, const char *tls_crt, const char *tls_key, char *token, int token_size)
{
	const char *kAccessTokenTag = "access_token=";
	char header[1024] ={0}, body[256] = {0}, http_buf[4096] = {0};
	int http_buf_size = (int)sizeof(http_buf) - 1;

	sprintf(body, "response_type=token&scope=msi_vault.upload&client_id=fusion");

	sprintf(header, "POST /as/authorization.oauth2 HTTP/1.1\r\nHost: %s:%d\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s",
				host, port, (int)strlen(body), body);

	int ret = moto_camera_comm_exec_query_op(host, port, tls_crt, tls_key, header, http_buf, http_buf_size);
	if(0 == ret)
	{
		char *p = strstr(http_buf, kAccessTokenTag);
		if(p)
		{
			char *q = NULL;
			p += strlen(kAccessTokenTag);
			q = strstr(p, "&");
			memcpy(token, p, q - p);
		}
	}
	return ret;
}
int moto_camera_query_incident(const char *host, int port, const char *tls_crt, const char *tls_key, 
		const char *incident_id,const char *token, char *json, int json_size)
{
	char content[4096] = {0};
	sprintf(content, "GET /api/vault-upload/v1.0/incidents/%s HTTP/1.1\r\n"
		"Host: %s:%d\r\nAuthorization: Bearer %s\r\nmsi-license: MSISiDevice\r\n"
		"Content-Length: 0\r\n\r\n", incident_id, host, port, token);
	return moto_camera_comm_exec_query_op(host, port, tls_crt, tls_key, content, json, json_size);
}
int moto_camera_upload_request(moto_upload_des_t *pc, int *result)
{
	int ret = 0;
	list_head_t *p, *c;

	do
	{
		//1 firs send init request
		ret = moto_camera_upload_init_request(pc);
		if(0 != ret)
		{
			printf("%s[%d]: moto_camera_upload_init_request fail\r\n", __FUNCTION__, __LINE__);
			(*result) = MOTO_RES_STATE_SEND_INIT_REQUEST_FAIL;
			break;
		}
		moto_camera_print_init_response(pc);

		//2 send file
		list_for_each_safe(p, c, &pc->file_list)
		{
			if(p == c) break;
			moto_upload_file_t *pf = list_entry(p, moto_upload_file_t, list_node);
			ret = moto_camera_send_file(pf, pc->tls_crt, pc->tls_key);
			if(0 != ret)
			{
				printf("%s[%d]: send [%s] is fail\r\n", __FUNCTION__, __LINE__, pf->file_path);
				break;
			}
		}

		if(0 != ret)
		{
			(*result) = MOTO_RES_STATE_SEND_FILE_FAIL;
			printf("%s[%d]: send file is fail\r\n", __FUNCTION__, __LINE__);
			break;
		}

		//4 send finish
		ret = moto_camera_send_finish_upload(pc->complete_url, pc->tls_crt, pc->tls_key, pc->token, pc->dev_key);
		if(0 > ret)
		{
			(*result) = MOTO_RES_STATE_SEND_FINISH_FAIL;
			break;
		}
		if(0 == ret)
		{
			printf("%s[%d]: just process for motorola sever, then try query status later\r\n", __FUNCTION__, __LINE__);
		}
		else
		{
			printf("%s[%d]: upload finish\r\n", __FUNCTION__, __LINE__);
			ret = 1;
		}
	}while(0);

	moto_camera_free_upload_detail(pc);
	return ret;
}

void moto_camera_free_upload_detail(moto_upload_des_t *pd)
{
	if(pd)
	{
		list_head_t *p, *c;
		list_for_each_safe(p, c, &pd->file_list)
		{
			if(p == c) break;
			moto_upload_file_t *pf = list_entry(p, moto_upload_file_t, list_node);
			if(pf)
			{
				moto_camera_free_file_info(pf);
				INIT_LIST_HEAD(&pf->chunk_list);
			}
		}
	}
}

static void moto_camera_free_file_info(moto_upload_file_t *pf)
{
	list_head_t *p, *c;
	list_for_each_safe(p, c, &pf->chunk_list)
	{
		if(p == c) break;
		list_del(p);
		INIT_LIST_HEAD(p);

		chunk_info_t *pchunk = list_entry(p, chunk_info_t, list_node);
		if(pchunk)
		{
			list_head_t *p1, *c1;
			list_for_each_safe(p1, c1, &pchunk->header_list)
			{
				if(p1 == c1) break;
				list_del(p1);
				INIT_LIST_HEAD(p1);
				chunk_header_item_t *pitem = list_entry(p1, chunk_header_item_t, list_node);
				if(pitem) 
				{
					free(pitem);
				}
			}
			free(pchunk);
		}
	}
}

static char *moto_camera_domain_resolution(const char *domain, char *ip)
{
	struct hostent *h=NULL;
	struct sockaddr_in addr; 
	if(NULL == (h=gethostbyname(domain)))
		return ip;
	
	addr.sin_addr.s_addr = (*(unsigned int *)h->h_addr);
	strcpy(ip, inet_ntoa(addr.sin_addr));
	return ip;
}

static int moto_camera_create_tcp_conn(char *ip, unsigned short port, char *err_msg)
{
	int des_fd = 0;
	struct sockaddr_in service_addr = {0};
	do
	{
		des_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		if(-1 == des_fd)
		{
			sprintf(err_msg, "%s[%d]: socket failed, errno %d, %s\r\n",__FUNCTION__,__LINE__,
				errno, strerror(errno));
			break;
		}
		
		service_addr.sin_family=AF_INET;
		service_addr.sin_addr.s_addr=inet_addr(ip);
		service_addr.sin_port=htons(port);
		if(-1 == connect(des_fd, (struct sockaddr*)(&service_addr),sizeof(struct sockaddr_in)))
		{
			sprintf(err_msg, "%s[%d]: connect [%s|%d] failed, errno %d, %s\r\n",__FUNCTION__,
				__LINE__, ip, port,  errno, strerror(errno));
			close(des_fd);
			break;
		}
		
		return des_fd;
	}while(0);
	return -1;
}
static SSL * moto_camera_bind_conn_to_ssl(int des_fd, SSL_CTX *ctx, const char *key, const char *crt)
{
	if(0 >= des_fd || !ctx) return NULL;
	if(!SSL_CTX_load_verify_locations(ctx, "/etc/ssl/certs/ca-bundle.trust.crt", NULL))
	{
		if(! SSL_CTX_load_verify_locations(ctx, NULL, "/etc/ssl/certs"))
		{
			printf("%s[%d]: SSL_CTX_load_verify_locations from fail\r\n", __FUNCTION__, __LINE__);
			return NULL;
		}
	}

	if (SSL_CTX_use_certificate_file(ctx, crt, SSL_FILETYPE_PEM) != 1)
	{
		printf("%s[%d]: SSL_CTX_use_certificate_file [%s] fail\r\n", __FUNCTION__, __LINE__, crt);
		return NULL;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1)
	{
		printf("%s[%d]: SSL_CTX_use_PrivateKey_file [%s] fail\r\n", __FUNCTION__, __LINE__, key);
		return NULL;
	}

	SSL *ssl = SSL_new(ctx);
	if(!ssl)
	{
		printf("%s[%d]: SSL_new fail \r\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	SSL_set_fd(ssl, des_fd);
	if(0 >= SSL_connect(ssl))
	{
		printf("%s[%d]: SSL_connect fail\r\n", __FUNCTION__, __LINE__);
		SSL_free(ssl); ssl = NULL;
		return NULL;
	}
	int result = SSL_get_verify_result(ssl);
	if(X509_V_OK != result)
	{
		printf("%s[%d]: SSL_get_verify_result fail, result = %d\r\n", __FUNCTION__, __LINE__, result);
		SSL_free(ssl); ssl = NULL;
	}

	return ssl;
}
static int moto_camera_recv_http_header(SSL *ssl, char *buf, int buf_size)
{
	const char *kContentLength[] = {"Content-Length", "content-length", NULL};
	char *pbuf = buf, *p = NULL;
	int left = buf_size, ilen = 0, n = 0, i = 0;
	while(0 < left)
	{
		n = SSL_read(ssl, pbuf, 1);
		if(1 != n)
		{
			printf("%s[%d]: recv fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, errno, strerror(errno));
			return -1;
		}
		ilen += n;
		
		if(((*pbuf) == '\n') && ilen > 4 && (*(pbuf-1)) == '\r' && (*(pbuf - 2)) == '\n' && (*(pbuf-3)) == '\r')
		{
			for(i=0; kContentLength[i]; ++i)
			{
				p = strstr(buf, kContentLength[i]);
				if(p) 
				{
					p += strlen(kContentLength[i]);
					while((*p) == ':' || (*p) == ' ') ++p;
					return atoi(p);
				}
			}
			return -1;
		}
		pbuf += n; left -= n;
	}
	return -1;
}
static int moto_camera_recv_packet(SSL *ssl, char *buf, int buf_size)
{
	char *pbuf = buf;
	int left = buf_size, n = 0;
	while(0 < left)
	{
		n = SSL_read(ssl, pbuf, left);
		if(0 >= n)
		{
			printf("%s[%d]: send fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, errno, strerror(errno));
			return -1;
		}
		pbuf += n; left -= n;
	}
	return (0 == left ? 0:-1);
}
static int moto_camera_send_packet(SSL *ssl, const char *buf, int buf_size)
{
	const char *pbuf = buf;
	int left = buf_size, n = 0;
	while(0 < left)
	{
		n = SSL_write(ssl, pbuf, left);
		if(0 >= n) return -1;
		pbuf += n; left -= n;
	}
	
	return (0 == left ? 0 : -1);
}


static int moto_camera_comm_exec_query_op(const char *host, int port, const char *tls_crt, const char *tls_key, 
		const char *content, char *json, int json_size)
{
	char ip[64] = {0}, err_msg[256] = {0};
	int des_fd = -1, ret = -1, body_size = 0;
	SSL_CTX * ctx = NULL;
	SSL *ssl = NULL;
	do
	{
		ctx = SSL_CTX_new(SSLv23_client_method());
		if(!ctx)
		{
			printf("%s[%d]: SSL_CTX_new fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, errno, strerror(errno));
			break;
		}

		moto_camera_domain_resolution(host, ip);
		des_fd = moto_camera_create_tcp_conn(ip, port, err_msg);
		if(0 >= des_fd)
		{
			printf("%s[%d]: %s\r\n", __FUNCTION__, __LINE__, err_msg);
			break;
		}
		if(!(ssl = moto_camera_bind_conn_to_ssl(des_fd, ctx, tls_key, tls_crt))) break;

		//1 send 
		if(0 != moto_camera_send_packet(ssl, content, (int)strlen(content)))
		{
			printf("%s[%d]: send [%s] fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, content, errno, strerror(errno));
			break;
		}
		printf("%s[%d]: send [%s] success, wait to recv:\r\n", __FUNCTION__, __LINE__, content);

		// recv http header;
		memset(json, 0x00, json_size);
		body_size = moto_camera_recv_http_header(ssl, json, json_size - 1);

		printf("%s[%d]: recv http_header [%s], body_size = %d\r\n", __FUNCTION__, __LINE__, json, body_size);
		if(0 < body_size)
		{
			if(0 != moto_camera_recv_packet(ssl, json, body_size))
			{
				printf("%s[%d]: recv body fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, errno, strerror(errno));
				break;
			}
			json[body_size] = '\0';
			printf("%s[%d]: recv body [%s]\r\n", __FUNCTION__, __LINE__, json);
		}

		ret = 0;
	}while(0);

	if(ssl)
	{
		SSL_shutdown(ssl);
		SSL_free(ssl); ssl = NULL;
	}

	if(0 < des_fd) close(des_fd);
	if(ctx) SSL_CTX_free(ctx);
	return ret;
}
static int moto_camera_exec_upload_file(const char *url, const char *tls_crt, const char *tls_key,
		  const char *ext_header, const char *file_path, int start, int length)
{
	char host[128] = {0}, ip[64] = {0}, err_msg[256] = {0}, http_buf[4096] = {0};
	short port = 0, http_buf_size = (int)sizeof(http_buf);
	int des_fd = -1, ret = -1, n = 0, send_length = 0, readlen = 0, is_err = 0;
	SSL_CTX * ctx = NULL;
	SSL *ssl = NULL;
	FILE *rfp = NULL;

	moto_parse_domain_form_url(url, host, &port, NULL);
	do
	{
		ctx = SSL_CTX_new(SSLv23_client_method());
		if(!ctx)
		{
			printf("%s[%d]: SSL_CTX_new fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, errno, strerror(errno));
			break;
		}

		moto_camera_domain_resolution(host, ip);
		des_fd = moto_camera_create_tcp_conn(ip, port, err_msg);
		if(0 >= des_fd)
		{
			printf("%s[%d]: %s\r\n", __FUNCTION__, __LINE__, err_msg);
			break;
		}
		if(!(ssl = moto_camera_bind_conn_to_ssl(des_fd, ctx, tls_key, tls_crt))) break;

		//1 send header
		n = snprintf(http_buf, sizeof(http_buf) - 1, "PUT %s HTTP/1.1\r\nHost: %s:%d\r\n"
				"%sContent-Length: %d\r\n\r\n", url, host, port, ext_header, length);
		if(0 != moto_camera_send_packet(ssl, http_buf, n))
		{
			printf("%s[%d]: send [%s] fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, http_buf, errno, strerror(errno));
			break;
		}

		printf("%s[%d]: send header [%s] success, then beign to send body\r\n", __FUNCTION__, __LINE__, http_buf);

		rfp = fopen(file_path, "rb");
		if(!rfp)
		{
			printf("%s[%d]: fopen %s fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, file_path, errno, strerror(errno));
			break;
		}
		fseek(rfp, start, SEEK_SET);
		is_err = 0;
		send_length = 0;
		while(send_length < length)
		{
			readlen = length - send_length;
			if(readlen > http_buf_size) readlen = http_buf_size;
			if(0 >= readlen) break;

			n = fread(http_buf, 1, readlen, rfp);
			if(0 < n)
			{
				if(0 != moto_camera_send_packet(ssl, http_buf, n))
				{
					is_err = 1;
					printf("%s[%d]: moto_camera_send_packet fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, errno, strerror(errno));
					break;
				}
				send_length += n;
			}
		}
		if(is_err) break;

		// recv http header;
		memset(http_buf, 0x00, sizeof(http_buf));
		n = moto_camera_recv_http_header(ssl, http_buf, sizeof(http_buf) - 1);
		printf("%s[%d]: recv http_header [%s]\r\n", __FUNCTION__, __LINE__, http_buf);

		if(strstr(http_buf, "200") || strstr(http_buf, "201"))
		{
			printf("%s[%d]: upload file [%s] success\r\n", __FUNCTION__, __LINE__, file_path);
			ret = 0;
		}
	}while(0);

	if(rfp) fclose(rfp);

	if(ssl)
	{
		SSL_shutdown(ssl);
		SSL_free(ssl); ssl = NULL;
	}

	if(0 < des_fd) close(des_fd);
	if(ctx) SSL_CTX_free(ctx);
	return ret;
}
static const char *moto_camera_upload_init_body(moto_upload_des_t *pd, const char *crt, const char *privkey, char *json, int json_size)
{
	char header[2048] = {0}, header_enc[2048] = {0}, payload[8192] = {0}, payload_enc[8192] = {0},
		 signature[1024] = {0}, tmp_buf[20480] = {0}, *pjson = json;
	int header_enc_size = (int)sizeof(header_enc), payload_size = (int)sizeof(payload),
		payload_enc_size = (int)sizeof(payload_enc), signature_size = (int)sizeof(signature),
		json_left = json_size, json_len = 0, i = 0, j = 0;

	//1 protect header
	moto_create_jws_protected_header(crt, header);
	jwt_b64_encode(header, (int)strlen(header), header_enc, &header_enc_size);

//	printf("%s[%d]: header [%s]\r\n jwt_b63:\r\n[%s]\r\n", __FUNCTION__, __LINE__, header, header_enc);

	//2 manifest
	moto_create_manifest_file2(pd, payload, &payload_size);
	jwt_b64_encode(payload, (int)strlen(payload), payload_enc, &payload_enc_size);

	printf("%s[%d]: payload [%s]\r\n jwt_b63:\r\n[%s]\r\n", __FUNCTION__, __LINE__, payload, payload_enc);

	//3 signature
	int n = snprintf(tmp_buf, sizeof(tmp_buf) -1, "%s.%s", header_enc, payload_enc);
	jws_rs_sha256(privkey, tmp_buf, n, signature, signature_size);

	//printf("%s[%d]: signature [%s] (%d)\r\n\r\n", __FUNCTION__, __LINE__, signature, (int)strlen(signature));

	n = snprintf(pjson, json_left, "{\"deviceStatus\": {"
			"\"attachedRadio\": {\"id\": \"12345\"},");

	json_left -= n; json_len += n; pjson += n;
	//wifi
	n = snprintf(pjson, json_left, "\"wifi\": {\"activeConnection\": {"
			"\"ssid\": \"%s\", \"bssid\": \"%s\", \"frequency\": %d, "
			"\"linkSpeed\": %d, \"signalStrength\": %d}, \"availableSSIDs\": [",
			pd->wifi_info.active_ssid, pd->wifi_info.active_bssid,
			pd->wifi_info.active_frequency, pd->wifi_info.active_linkspeed,
			pd->wifi_info.avtive_signal_strength);
	json_left -= n; json_len += n; pjson += n;
	for(i = 0; i < MAX_WIFI_AVAILABLESSID_COUNT; ++i)
	{
		if(0 >= strlen(pd->wifi_info.available_ssids[i])) break;
		if(0 < j)
		{
			strcpy(pjson, ","); pjson += 1; json_len += 1; json_left -= 1;
		}
		n = snprintf(pjson, json_left, "\"%s\"", pd->wifi_info.available_ssids[i]);
		json_left -= n; json_len += n; pjson += n;
	}
	n = snprintf(pjson, json_left, "]},");
	json_left -= n; json_len += n; pjson += n;

	//battery
	n = snprintf(pjson, json_left, "\"battery\": {\"chargePercent\": %d,"
			" \"charging\": %s, \"full\": %s, \"powerSource\": \"%s\"}, "
			"\"storage\": {\"bytesAvailable\": %d},", pd->battery.charge_percent,
			pd->battery.charging?"true":"false", pd->battery.bfull?"true":"false",
			pd->battery.power_source, pd->storage_available);
	json_left -= n; json_len += n; pjson += n;

	//location
	n = snprintf(pjson, json_left, "\"location\": {\"latitude\": %.6f, "
			"\"longitude\": %.6f, \"altitude\": %.6f, \"accuracy\": %.6f, "
			"\"speed\": %.3f}}, \"user\": {\"username\": \"%s\"},",
			pd->location.latitude, pd->location.longitude, pd->location.altitude,
			pd->location.accuracy, pd->location.speed, pd->user_id);
	json_left -= n; json_len += n; pjson += n;

	//manifest
	n = snprintf(pjson, json_left, "\"manifest\": {"
			"\"protected\": \"%s\", "
			"\"payload\": \"%s\", "
			"\"signature\": \"%s\"}}", 
			header_enc, payload_enc, signature);
	json_left -= n; json_len += n; pjson += n;

	/*
	n = snprintf(json, json_size - 1, "{\n"
			"  \"deviceStatus\": {\n"
			"    \"attachedRadio\": {\n"
			"      \"id\": \"12345\"\n"
			"    },\n"
			"    \"wifi\": {\n"
			"      \"activeConnection\": {\n"
			"        \"ssid\": \"Transee22\",\n"
			"        \"bssid\": \"02197545\",\n"
			"        \"frequency\": 5805,\n"
			"        \"linkSpeed\": 54,\n"
			"        \"signalStrength\": 35\n"
			"      },\n"
			"      \"availableSSIDs\": [\n"
			"        \"Transee22\"\n"
			"      ]\n"
			"    },\n"
			"    \"battery\": {\n"
			"      \"chargePercent\": 75,\n"
			"      \"charging\": true,\n"
			"      \"full\": false,\n"
			"      \"powerSource\": \"usb\"\n"
			"    },\n"
			"    \"storage\": {\n"
			"      \"bytesAvailable\": 2781251320\n"
			"    },\n"
			"    \"location\": {\n"
			"      \"latitude\": %.6f,\n"
			"      \"longitude\": %.5f,\n"
			"      \"altitude\": %.5f,\n"
			"      \"accuracy\": %.3f,\n"
			"      \"speed\": 9.8\n"
   			"    }\n"
			"  },\n"
			"  \"user\": {\n"
			"    \"username\": \"%s\"\n"
			"  },\n"
			"  \"manifest\": {\n"
			"    \"protected\":\n\"%s\",\n"
			"    \"payload\":\n\"%s\",\n"
			"    \"signature\":\n\"%s\"\n"
			"  }\n"
			"}", pd->latitude, pd->longitude, pd->altitude, pd->accuracy, 
			pd->user_id, header_enc, payload_enc, signature);
			*/

	return json;
}

static int moto_camera_upload_init_request(moto_upload_des_t *pc)
{
	char body[10240] = {0}, fun_name[128] = {0}, http_buf[20480] = {0};
	int body_size = (int)sizeof(body), ret = 0, http_buf_size = (int)sizeof(http_buf), n = 0;
	moto_camera_upload_init_body(pc, pc->tls_crt, pc->tls_key, body, body_size);

	sprintf(fun_name, "/api/vault-upload/v1.0/incidents/%s", pc->incident_id);

	n = snprintf(http_buf, http_buf_size - 1, "PUT %s HTTP/1.1\r\nHost: %s:%d\r\n"
			"Content-Type: application/json;charset=utf-8\r\nContent-Length: %d\r\n"
			"msi-license: %s\r\nAuthorization: Bearer %s\r\n\r\n%s", fun_name, pc->moto_api_host,
			pc->moto_api_port, (int)strlen(body), pc->dev_key, pc->token, body);

	ret = moto_camera_comm_exec_query_op(pc->moto_api_host, pc->moto_api_port, pc->tls_crt, pc->tls_key, 
			http_buf, body, body_size);
	
	if(0 == ret)
	{
		printf("%s[%d]: response [%s]\r\n", __FUNCTION__, __LINE__, body);
		cJSON *json_ptr = cJSON_Parse(body);
		if(!json_ptr)
		{
			ret = -1;
		}
		else
		{
			moto_camera_parse_init_response(pc, json_ptr);
			cJSON_Delete(json_ptr); json_ptr = NULL;
		}
	}

	return ret;
}
static int moto_camera_parse_init_response(moto_upload_des_t *pd, cJSON *json_ptr)
{
	cJSON *pfiles = cJSON_GetObjectItem(json_ptr, "files");
	cJSON *pcompleturl = cJSON_GetObjectItem(json_ptr, "completionUrl");
	int ret = -1, i = 0;
	do
	{
		if(!pfiles || !pcompleturl || !pcompleturl->valuestring) break;

		memset(pd->complete_url, 0x00, sizeof(pd->complete_url));
		strcpy(pd->complete_url, pcompleturl->valuestring);

		int arr_size = cJSON_GetArraySize(pfiles);//数组大小
		cJSON *file_item_ptr = pfiles->child;//子对象
		for(i = 0; i < arr_size && file_item_ptr; ++i)
		{
			cJSON *pfile = cJSON_GetObjectItem(file_item_ptr, "file");
			cJSON *pdetail = cJSON_GetObjectItem(file_item_ptr, "destination");
			if(pfile && pdetail)
			{
				cJSON *pfid = cJSON_GetObjectItem(pfile, "id");
				cJSON *pfname = cJSON_GetObjectItem(pfile, "name");
				cJSON *pfsize = cJSON_GetObjectItem(pfile, "size");
				cJSON *pchunks = cJSON_GetObjectItem(pdetail, "chunks");
				cJSON *chunk_ptr = pchunks?pchunks->child:NULL;
				int chunk_size = pchunks?cJSON_GetArraySize(pchunks):0, j = 0;

				moto_upload_file_t *pfi = NULL;
				if(pfid && pfid->valuestring && pfname && pfname->valuestring && pfsize)
				{
					pfi = moto_camera_find_upload_file_by_id(pd, pfid->valuestring);
					if(pfi)
					{
						/*
						memset(pfi->id, 0x00, sizeof(pfi->id));
						strcpy(pfi->id, pfid->valuestring);
						memset(pfi->name, 0x00, sizeof(pfi->name));
						strcpy(pfi->name, pfname->valuestring);
						pfi->size = pfsize->valueint;
						*/
						INIT_LIST_HEAD(&pfi->chunk_list);
					}
				}
				if(pfi && chunk_ptr && 0 < chunk_size)
				{
					for(j = 0; j < chunk_size && chunk_ptr; ++j)
					{
						cJSON *pchunkstart = cJSON_GetObjectItem(chunk_ptr, "chunkStart");
						cJSON *pchunklength = cJSON_GetObjectItem(chunk_ptr, "length");
						cJSON *pchunkurl = cJSON_GetObjectItem(chunk_ptr, "uri");
						cJSON *pchunkheaders = cJSON_GetObjectItem(chunk_ptr, "headers");
						cJSON *chunk_header_ptr = pchunkheaders?pchunkheaders->child:NULL;
						int chunk_header_size = pchunkheaders?cJSON_GetArraySize(pchunkheaders):0,  k= 0;

						chunk_info_t *pchunk_info = NULL;
						if(pchunkstart && pchunklength && pchunkurl && pchunkurl->valuestring)
						{
							pchunk_info = (chunk_info_t*)malloc(sizeof(chunk_info_t));
							if(pchunk_info)
							{
								memset(pchunk_info, 0x00, sizeof(pchunk_info));
								INIT_LIST_HEAD(&pchunk_info->list_node);
								INIT_LIST_HEAD(&pchunk_info->header_list);
								pchunk_info->chunk_start = pchunkstart->valueint;
								pchunk_info->length = pchunklength->valueint;
								strcpy(pchunk_info->url, pchunkurl->valuestring);

								for(k = 0; k < chunk_header_size && chunk_header_ptr; ++k)
								{
									cJSON *pchunkheaderkey = cJSON_GetObjectItem(chunk_header_ptr, "key");
									cJSON *pchunkheadervalue = cJSON_GetObjectItem(chunk_header_ptr, "value");
									if(pchunkheaderkey && pchunkheaderkey->valuestring && pchunkheadervalue && pchunkheadervalue->valuestring)
									{
										chunk_header_item_t *chunk_header_item_ptr = (chunk_header_item_t*)malloc(sizeof(chunk_header_item_t));
										if(chunk_header_item_ptr)
										{
											memset(chunk_header_item_ptr, 0x00, sizeof(*chunk_header_item_ptr));
											INIT_LIST_HEAD(&chunk_header_item_ptr->list_node);
											strcpy(chunk_header_item_ptr->key, pchunkheaderkey->valuestring);
											strcpy(chunk_header_item_ptr->value, pchunkheadervalue->valuestring);
											list_add_tail(&chunk_header_item_ptr->list_node, &pchunk_info->header_list);
										}
									}
									chunk_header_ptr = chunk_header_ptr->next;
								}
								list_add_tail(&pchunk_info->list_node, &pfi->chunk_list);
							}
						}
						chunk_ptr = chunk_ptr->next;
					}
				}
			}
			file_item_ptr = file_item_ptr->next;
		}
		ret = 0;
	}while(0);
	return ret;
}
static const char *moto_camera_generate_chunk_ext_header(chunk_info_t *pchunk, char *ext_header)
{
	char *pbuf = ext_header;
	list_head_t *p, *c;
	list_for_each_safe(p, c, &pchunk->header_list)
	{
		if(p == c) break;
		chunk_header_item_t *pitem = list_entry(p, chunk_header_item_t, list_node);
		if(pitem)
		{
			sprintf(pbuf, "%s: %s\r\n", pitem->key, pitem->value);
			pbuf += strlen(pbuf);
		}
	}
	return ext_header;
}

static int moto_camera_send_file(moto_upload_file_t *pfile, const char *tls_crt, const char *tls_key)
{
	list_head_t *p, *c;
	int ret = 0;
	list_for_each_safe(p, c, &pfile->chunk_list)
	{
		if(p == c) break;
		chunk_info_t *pchunk = list_entry(p, chunk_info_t, list_node);
		if(pchunk)
		{
			char ext_header[256] = {0};
			moto_camera_generate_chunk_ext_header(pchunk, ext_header);
			ret = moto_camera_exec_upload_file(pchunk->url, tls_crt, tls_key, 
					ext_header, pfile->file_path, pchunk->chunk_start, pchunk->length);
			memset(ext_header, 0x00, sizeof(ext_header));
			if(0!=ret) break;
		}
	}

	return ret;
}
static int moto_camera_send_finish_upload(const char *url, const char *tls_crt, const char *tls_key, const char *jwt, const char *dev_key)
{
	char host[128] = {0}, ip[64] = {0}, content[2048] = {0}, uri[1024] = {0}, http_buf[2048] = {0};
	short port = 0, ret = 0, http_buf_size = (int)sizeof(http_buf);
	moto_parse_domain_form_url(url, host, &port, uri);

	sprintf(content, "POST %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: 0\r\nmsi-license: %s\r\nAuthorization: Bearer %s\r\n\r\n", 
		uri, host, port, dev_key, jwt);
	ret = moto_camera_comm_exec_query_op(host, port, tls_crt, tls_key, content, http_buf, http_buf_size);
	if(0 == ret)
	{
		printf("%s[%d]: json [%s]\r\n", __FUNCTION__, __LINE__, http_buf);
		cJSON *json_ptr = cJSON_Parse(http_buf);
		if(!json_ptr)
		{
			printf("%s[%d]: invalid json\r\n", __FUNCTION__, __LINE__);
		}
		else
		{
			cJSON *pverified = cJSON_GetObjectItem(json_ptr, "verified");
			ret = pverified?pverified->valueint:0;
			cJSON_Delete(json_ptr);
		}
	}
	return ret;
}	
static void moto_camera_print_file_info(moto_upload_file_t *pfile)
{
	printf("\r\nfile: id [%s], name [%s], size [%d]\r\n", pfile->id, pfile->moto_name, pfile->file_size);
	list_head_t *p, *c;
	list_for_each_safe(p, c, &pfile->chunk_list)
	{
		if(p == c) break;
		chunk_info_t *pchunk = list_entry(p, chunk_info_t, list_node);
		if(pchunk)
		{
			printf("chunk [start = %d, length = %d, url = %s]\r\nheaders: \r\n", pchunk->chunk_start, pchunk->length,
					pchunk->url);

			list_head_t *p1, *c1;
			list_for_each_safe(p1, c1, &pchunk->header_list)
			{
				if(p1 == c1) break;
				chunk_header_item_t *pitem = list_entry(p1, chunk_header_item_t, list_node);
				if(pitem)
				{
					printf("====key: %s, value: %s\r\n", pitem->key, pitem->value);
				}
			}
		}
	}
}
static void moto_camera_print_init_response(moto_upload_des_t *pd)
{
	list_head_t *p, *c;
	list_for_each_safe(p, c, &pd->file_list)
	{
		if(p == c) break;
		moto_upload_file_t *pf = list_entry(p, moto_upload_file_t, list_node);
		if(pf)
			moto_camera_print_file_info(pf);
	}

	printf("completionUrl: %s\r\n", pd->complete_url);
}


#ifndef __MOTO_TOOL_H_
#define __MOTO_TOOL_H_	1

#include <openssl/rsa.h>

const char *b64_decode(char *b64src, int b64src_size, char *clrdst, int *clrdst_size);
const char *b64_encode(char *clrstr, int clrstr_size, char *b64dst, int *b64dst_size);
const char *jwt_b64_encode(char *clrstr, int clrstr_size, char *b64dst, int *b64dst_size);
const char *jwt_b64_decode(char *clrstr, int clrstr_size, char *b64dst, int *b64dst_size);

const char *jws_hmac_sha256(const char *key, int keylen, const char *msg, int msglen, char *hmac, int hmac_size);
const char *jws_rs_sha256(const char *key_path, const char *msg, int msglen, char *rs256, int rs256_size);

const char *jws_sha256(char *buf, int buf_size, char *outbuf);
const char *jws_sha256_file(const char *file_path, char *outbuf);

const char *url_encode(const char* str, char* result);

const char *jwt_generate_iso8601_tz_datetime_str(unsigned int utm, char *info);

const char *comm_generate_guid(char *guid);
unsigned long comm_get_file_size(const char *file_name);

RSA *get_rsa_from_privekey_file(const char *rsaprivKeyPath);

int moto_parse_domain_form_url(const char *url, char *host, short *port, char *uri);

#endif //__MOTO_TOOL_H_


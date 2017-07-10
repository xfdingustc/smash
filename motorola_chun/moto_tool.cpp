
#include "moto_tool.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <openssl/hmac.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <ctype.h>
#include <uuid/uuid.h>


/* ---- Base64 Encoding/Decoding Table --- */
static const char kB64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char kPad = '=';
static const char kNp = (const char)-1;
#define kNp ((const char)-1)
static const char table64vals[] =
{
	62, kNp, kNp, kNp, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, kNp, kNp, kNp, kNp, kNp,
	kNp, kNp,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25, kNp, kNp, kNp, kNp, kNp, kNp, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

//RSA *get_rsa_from_privekey_file(const char *rsaprivKeyPath);

inline char table64(unsigned char c)
{
	return ( c < 43 || c > 122 ) ? kNp : table64vals[c-43];
}

const char *b64_decode(char *b64src, int b64src_size, char *clrdst, int *clrdst_size)
{
	char c, d;
	char *pbuf = clrdst, *pend = clrdst + (*clrdst_size);
	int length = b64src_size, i = 0, j = 0;
	for(i = 0; i < length; ++i)
	{
		c = table64(b64src[i]);
		++i;
		d = table64(b64src[i]);
		c = (char)((c << 2) | ((d >> 4) & 0x3));
		(*pbuf) = c; ++pbuf; ++j;

		if(pbuf >= pend) return NULL;

		if(++i < length)
		{
			c = b64src[i];
			if(kPad == c)
				break;
		
			c = table64(b64src[i]);
			d = (char)(((d << 4) & 0xf0) | ((c >> 2) & 0xf));
			(*pbuf) = d; ++pbuf; ++j;
			if(pbuf >= pend) return NULL;
		}
		if(++i < length)
		{
			d = b64src[i];
			if(kPad == d) break;
			d = table64(b64src[i]);
			c = (char)(((c << 6) & 0xc0) | d);
			(*pbuf) = c; ++pbuf; ++j;
			if(pbuf >= pend) return NULL;
		}
	}
	if(clrdst_size) (*clrdst_size) = j;

	return clrdst;
}

const char *b64_encode(char *clrstr, int clrstr_size, char *b64dst, int *b64dst_size)
{
	char c;
	char *pbuf = b64dst, *pend = b64dst + (*b64dst_size);
	int length = clrstr_size, i = 0, j = 0;
	for(i = 0; i < length; ++i)
	{
		c = (char)((clrstr[i] >> 2) & 0x3f);
		(*pbuf) = kB64Chars[(int)c]; ++pbuf; ++j;
		if(pbuf >= pend) return NULL;

		c = (char)((clrstr[i] << 4) & 0x3f);
		if(++i < length)
			c = (char)(c|(char)((clrstr[i] >> 4) & 0x0f));
		(*pbuf) = kB64Chars[(int)c]; ++pbuf; ++j;
		if(pbuf >= pend) return NULL;

		if(i < length)
		{
			c = (char)((clrstr[i] << 2) & 0x3c);
			if(++i < length)
			c = (char)(c | (char)((clrstr[i] >> 6) & 0x03));
			(*pbuf) = kB64Chars[(int)c]; ++pbuf; ++j;
			if(pbuf >= pend) return NULL;
		}
		else
		{
			++i;
			(*pbuf) = kPad; ++pbuf; ++j;
			if(pbuf >= pend) return NULL;
		}
		if(i < length)
		{
			c = (char)(clrstr[i] & 0x3f);
			(*pbuf) = kB64Chars[(int)c]; ++pbuf; ++j;
			if(pbuf >= pend) return NULL;
		}
		else
		{
			(*pbuf) = kPad; ++pbuf; ++j;
			if(pbuf >= pend) return NULL;
		}
	}
	if(b64dst_size) (*b64dst_size) = j;

	return b64dst;
}
const char *jwt_b64_encode(char *clrstr, int clrstr_size, char *b64dst, int *b64dst_size)
{
	int length = 0, i = 0, j = -1;
	if(!b64_encode(clrstr, clrstr_size, b64dst, b64dst_size)) return NULL;

	length = (*b64dst_size);
	for(i = 0; i < length; ++i)
	{
		if(b64dst[i] == '+') b64dst[i] = '-';
		else if(b64dst[i] == '/') b64dst[i] = '_';
		else if((b64dst[i] == kPad) && -1 == j)
		{
			j = i;
		}
	}

	//cut =
	while(0 < length)
	{
		int left = 0;
		if(-1 == j) break;
		left = length - j - 1;
		memmove(b64dst+j, b64dst + j + 1, left); --length;
		for(i = j; i < length; ++i)
		{
			if(b64dst[i] == '=') 
			{
				j = i; break;
			}
		}
		if(i >= length) break;
	}
	b64dst[length] = '\0';
	return b64dst;
}
const char *jwt_b64_decode(char *clrstr, int clrstr_size, char *b64dst, int *b64dst_size)
{
	int length = clrstr_size, i = 0;
	for(i = 0; i < length; ++i)
	{
		if('-' == clrstr[i]) clrstr[i] = '+';
		else if('_' == clrstr[i]) clrstr[i] = '/';
	}
	switch(length % 4)
	{
	case 0: break;
	case 2: clrstr[length++] = '='; clrstr[length++] = '='; clrstr[length]='\0';break;
	case 3: clrstr[length++] = '='; clrstr[length]='\0';break;
	default: 
			{
				printf("%s[%d]: Illegal base64url string!\r\n", __FUNCTION__, __LINE__);
				return NULL;
			}
	}
	return b64_decode(clrstr, length, b64dst, b64dst_size);
}
const char *jws_hmac_sha256(const char *key, int keylen, const char *msg, int msglen, char *hmac, int hmac_size)
{
	unsigned char hash[32];
	HMAC_CTX hmac_ctx;
	HMAC_CTX_init(&hmac_ctx);
	HMAC_Init_ex(&hmac_ctx, (unsigned char*)key, keylen, EVP_sha256(), NULL);
	if(0 < msglen) HMAC_Update(&hmac_ctx, (unsigned char*)msg, msglen);
	unsigned int len = 32;
	HMAC_Final(&hmac_ctx, hash, &len);
	HMAC_CTX_cleanup(&hmac_ctx);

	//base 64url
	if(!jwt_b64_encode((char*)hash, 32, hmac, &hmac_size))
		return NULL;
	hmac[hmac_size] = '\0';
	return hmac;
}
const char *jws_rs_sha256(const char *key_path, const char *msg, int msglen, char *rs256, int rs256_size)
{
	unsigned char hash[1024];
	unsigned int len = sizeof(hash), signLen;

	EVP_PKEY *evpKey = EVP_PKEY_new();
	RSA *rsa = get_rsa_from_privekey_file(key_path);
	EVP_MD_CTX* ctx = EVP_MD_CTX_create();

	int is_suc = 0;
	if(evpKey && rsa && ctx)
	{
		EVP_PKEY_set1_RSA(evpKey, rsa);
		EVP_SignInit_ex(ctx, EVP_sha256(), 0);
		//EVP_SignInit_ex(ctx, EVP_sha384(), 0);
		EVP_SignUpdate(ctx, msg, msglen);
		EVP_SignFinal(ctx, hash, &len,evpKey);
		signLen=EVP_PKEY_size(evpKey);
		//base 64url
		if(jwt_b64_encode((char*)hash, len, rs256, &rs256_size))
		{
			rs256[rs256_size] = '\0'; is_suc = 1;
		}
	}
	if(rsa) RSA_free(rsa);
	if(evpKey) EVP_PKEY_free(evpKey);
	if(ctx) EVP_MD_CTX_destroy(ctx);
	if(is_suc) return rs256;
	return NULL;
}
const char *jws_sha256(char *buf, int buf_size, char *outbuf)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	int i = 0;
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, (unsigned char*)buf, buf_size);
	SHA256_Final(hash, &sha256);
	OPENSSL_cleanse(&sha256, sizeof(sha256));

	for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sprintf(outbuf + (i * 2), "%02x", hash[i]);
	}
	return outbuf;
}
const char *jws_sha256_file(const char *file_path, char *outbuf)
{
	const int kTmpBufSize = 4096;
	int i = 0;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	char tmp_buf[kTmpBufSize];
	FILE *fp = fopen(file_path, "rb");
	if(!fp)
	{
		printf("%s[%d]: fopen %s fail, errno %d, %s\r\n", __FUNCTION__, __LINE__, file_path, errno, strerror(errno));
		return NULL;
	}
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	while(1)
	{
		int n = fread(tmp_buf, 1, kTmpBufSize, fp);
		if(0 < n)
		{
			SHA256_Update(&sha256, (unsigned char*)tmp_buf, n);
		}
		if(n < kTmpBufSize) break;
	}
	fclose(fp);
	SHA256_Final(hash, &sha256);
	OPENSSL_cleanse(&sha256, sizeof(sha256));
	for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sprintf(outbuf + (i * 2), "%02x", hash[i]);
	}
	return outbuf;
}
const char *url_encode(const char* str, char* result)
{
	static char hex[] = "0123456789ABCDEF";
	char *pbuf = result;
	unsigned char c;
	int length = (int)strlen(str), i = 0;
	for (i = 0; i < length; ++i)
	{
		c = (unsigned char)str[i];
		if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			(*pbuf) = c; ++pbuf;
		}
		else if(c == ' ')
		{
			(*pbuf) = '+'; ++pbuf;
		}
		else 
		{
			(*pbuf) = '%'; ++pbuf;
			(*pbuf) = hex[c / 16]; ++pbuf;
			(*pbuf) = hex[c % 16]; ++pbuf;
		}
	}
	return result;
}
const char *jwt_generate_iso8601_tz_datetime_str(unsigned int utm, char *info)
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);

	long tv_usec = 0;
	if(tv.tv_sec == utm) tv_usec = tv.tv_usec/1000;

	time_t  cur_time = utm;
	struct tm curr = *gmtime(&cur_time);
	int tz_minuteswest = abs(tz.tz_minuteswest);
	if (curr.tm_year > 50)
	{
		sprintf(info, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%s%02d%02d", curr.tm_year+1900, curr.tm_mon+1,
			curr.tm_mday, curr.tm_hour, curr.tm_min, curr.tm_sec,(int)tv_usec,tz.tz_minuteswest >0 ? "-":"+", 
			tz_minuteswest/60, tz_minuteswest%60);
	}
	else
	{
		sprintf(info, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%s%02d%02d", curr.tm_year+2000, curr.tm_mon+1,
			curr.tm_mday, curr.tm_hour, curr.tm_min, curr.tm_sec,(int)tv_usec,tz.tz_minuteswest>0 ? "-":"+", 
			tz_minuteswest/60, tz_minuteswest%60);
	}
	return info;
}
static void comm_to_lower(char *info)
{
	int count = (int)strlen(info), i = 0;
	for(i = 0; i < count; ++i)
	{
		if((info[i]>='A')&&(info[i]<='Z'))
		info[i]+=32;
	}
}
const char *comm_generate_guid(char *guid)
{
	uuid_t uu;
	uuid_generate(uu);
	uuid_unparse(uu, guid);

	comm_to_lower(guid);
	
	return guid;
}
unsigned long comm_get_file_size(const char *file_name)
{
	unsigned long filesize = -1;
	struct stat statbuff;
	if(stat(file_name, &statbuff) < 0)
	{
		return filesize;
	}
	else
	{
		filesize = statbuff.st_size;
	}
	return filesize;
}
int moto_parse_domain_form_url(const char *url, char *host, short *port, char *uri)
{
	const char *kUrlPrefix = "https://";
	const char *p = strstr(url, kUrlPrefix), *q = NULL, *pend = url+strlen(url);
	if(!p || p != url) return -1;

	p = url + strlen(kUrlPrefix);
	q = strstr(p, "/");
	if(!q || q >= pend) return -1;
	memcpy(host, p, q - p);
	if(uri)
	{
		strcpy(uri, q);
	}

	(*port) = 443;
	p = strstr(host, ":");
	if(p)
	{
		(*(char*)p) = '\0';
		(*port) = atoi(p+1);
	}
	return 0;
}
/*
static int pass_cb( char *buf, int size, int rwflag, void *u )
{
	int len;
	char tmp[1024];
	printf( "Enter pass phrase for '%s': ", (char*)u );
	scanf( "%s", tmp );
	len = strlen( tmp );
		 
	if (len <= 0) return 0;
	if ( len > size ) len = size;
	memset( buf, '\0', size );
	memcpy( buf, tmp, len );
	return len;
}
*/

RSA *get_rsa_from_privekey_file(const char *rsaprivKeyPath)
{
	FILE* fp = fopen(rsaprivKeyPath, "rb");
	if ( fp == 0 ) 
	{
		printf("Couldn't open RSA priv key: '%s'. %s\n", rsaprivKeyPath, strerror(errno));
		return NULL;
	}
	//RSA *rsa = (RSA*)PEM_read_RSAPrivateKey(fp, 0, pass_cb, (char*)rsaprivKeyPath);
	RSA *rsa = RSA_new();
	if(!PEM_read_RSAPrivateKey(fp, &rsa, 0, 0))
	{
		if(rsa) 
		{
			RSA_free(rsa);
			rsa = NULL;
		}
	}
	fclose(fp);
	return rsa;
}

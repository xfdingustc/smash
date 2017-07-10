#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <openssl/md5.h>
#include <openssl/aes.h>

#include "cfutils.h"

static char HEX_LOWER_CODES[] = "0123456789abcdef";
static char HEX_UPPER_CODES[] = "0123456789ABCDEF";

int cf_encode_hex(unsigned char *input, unsigned int len, unsigned char *out, unsigned int *out_len, bool is_upper)
{
    if (*out_len < len*2) {
        return -1;
    }

    char *hex_codes_ = (is_upper ? HEX_UPPER_CODES : HEX_LOWER_CODES);
    int j = 0;
    unsigned int tmp_num = 0;
    for(unsigned int i=0; i<len; i++) {
        tmp_num  = input[i];
        out[j++] = hex_codes_[(tmp_num & 0xf0) >> 4];
        out[j++] = hex_codes_[tmp_num & 0x0f];
    }
    *out_len = j;
    return j;
}

int cf_decode_hex(unsigned char *input,unsigned int input_len,unsigned char *out,unsigned int *out_len)
{
    if (input_len/2 > (*out_len)) {
        return -1;
    }

    int j = 0;
    unsigned int tmp_num=0;
    for(unsigned int i=0; i<input_len; ++i) {
        tmp_num = 0;
        do {
            if (input[i] >= '0' && input[i] <= '9') {
                tmp_num += input[i]-'0';
            } else if (input[i] >= 'a' && input[i] <= 'f') {
                tmp_num += input[i]-'a'+10;
            } else if (input[i]>='A' && input[i]<='Z') {
                tmp_num += input[i]-'A'+10;
            } else {
                *out_len = 0;
                return -2;
            }

            if (i%2 == 0) {
                tmp_num <<= 4;
                i++;
            } else {
                out[j++] = tmp_num;
                break;
            }
        } while (i%2 == 1);
    }//for
    *out_len = j;

    return j;
}

//md5
int cf_encode_md5(unsigned char *input, unsigned int input_len, unsigned char *out)
{
    int ret = -1;
    do {
        MD5_CTX ctx;
        if(1 != MD5_Init(&ctx))
            break;
        if(1 != MD5_Update(&ctx, input, input_len))
            break;
        if(1 != MD5_Final(out, &ctx))
            break;
        ret = 0;
    }while(0);
    return ret;
}

//aes
int cf_encode_aes(
    unsigned char *input, unsigned int input_len,
    unsigned char *out, unsigned int out_len,
    unsigned char*key, int key_len/*=16*/,
    SL_CRYPTO_MODE_t mode/*=SL_CRYPTO_MODE_ECB*/,
    SL_CRYPTO_BLOCK_SIZE_t block_size/* = 16*/)
{
    unsigned char aes_key[512] = {0};
    if(key_len > (int)sizeof(aes_key)) {
        //printf("AES Key size error\n");
        return -1;
    }
    memcpy(aes_key, key, key_len);

    AES_KEY encrypt_key;
    AES_set_encrypt_key(aes_key, 8*block_size, &encrypt_key);

    unsigned int endblock_len	= input_len % block_size;
    unsigned int need_len		= endblock_len ? (input_len-endblock_len+block_size+1) : (input_len+1);
    if (out_len < need_len)
        return -1;

    unsigned char *input_pos = (unsigned char *)input;
    unsigned char *out_pos   = (out + 1);
    unsigned int block_count = input_len/block_size;
    for (unsigned int i=0; i<block_count; ++i) {
        AES_encrypt(input_pos, out_pos, &encrypt_key);
        input_pos	+= block_size;
        out_pos		+= block_size;
    }
    if (endblock_len > 0) {
        char endblock[SL_CRYPTO_BLOCK_SIZE_32] = {0};
        memset(endblock, 0x00, SL_CRYPTO_BLOCK_SIZE_32);
        memcpy(endblock, input_pos, endblock_len);
        out[0] = block_size-endblock_len;
        AES_encrypt((unsigned char *)endblock, out_pos, &encrypt_key);
    } else {
        out[0] = 0;
    }
    return need_len;
}

int cf_decode_aes(
    unsigned char *input, unsigned int input_len,
    unsigned char *out, unsigned int out_len,
    unsigned char*key, int key_len/*=16*/,
    SL_CRYPTO_MODE_t mode/*=SL_CRYPTO_MODE_ECB*/,
    SL_CRYPTO_BLOCK_SIZE_t block_size/* = 16*/)
{
    if (out_len < input_len)
        return -1;

    unsigned char aes_key[512] = {0};
    if(key_len > (int)sizeof(aes_key))
        return -1;
    memcpy(aes_key, key, key_len);

    AES_KEY decrypt_key;
    AES_set_decrypt_key(key, 8*block_size, &decrypt_key);

    unsigned char *input_pos = (unsigned char *)(input + 1);
    unsigned char *out_pos = out;
    unsigned int block_count = input_len/block_size;
    for (unsigned int i=0; i<block_count; ++i) {
        AES_decrypt(input_pos, out_pos, &decrypt_key);
        input_pos	+= block_size;
        out_pos		+= block_size;
    }
    return (input_len-input[0]-1);
}


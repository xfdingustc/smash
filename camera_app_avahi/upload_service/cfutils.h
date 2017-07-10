#ifndef CFUTILS_H
#define CFUTILS_H


typedef enum SL_CRYPTO_MODE_s {
    SL_CRYPTO_MODE_ECB	= 0,
    SL_CRYPTO_MODE_CBC,
    SL_CRYPTO_MODE_CFB,
    SL_CRYPTO_MODE_OFB,
    SL_CRYPTO_MODE_CTR,
    SL_CRYPTO_MODE_IGE,
    SL_CRYPTO_MODE_DEFAULT  = SL_CRYPTO_MODE_ECB
}SL_CRYPTO_MODE_t;

typedef enum SL_CRYPTO_BLOCK_SIZE_s {
    SL_CRYPTO_BLOCK_SIZE_8 = 8,
    SL_CRYPTO_BLOCK_SIZE_16 = 16,
    SL_CRYPTO_BLOCK_SIZE_24 = 24,
    SL_CRYPTO_BLOCK_SIZE_32 = 32,
    SL_CRYPTO_BLOCK_SIZE_DEFAULT = SL_CRYPTO_BLOCK_SIZE_16
}SL_CRYPTO_BLOCK_SIZE_t;

// 16 hex
extern int cf_encode_hex(unsigned char *input, unsigned int len, unsigned char *out, unsigned int *out_len, bool is_upper);
extern int cf_decode_hex(unsigned char *input, unsigned int input_len, unsigned char *out, unsigned int *out_len);
//md5
extern int cf_encode_md5(unsigned char *input, unsigned int input_len, unsigned char *out);
//aes
extern int cf_encode_aes(unsigned char *input, unsigned int input_len, unsigned char *out, unsigned int out_len, unsigned char*key,
        int key_len=16,SL_CRYPTO_MODE_t mode=SL_CRYPTO_MODE_ECB, SL_CRYPTO_BLOCK_SIZE_t block_size = SL_CRYPTO_BLOCK_SIZE_16);
extern int cf_decode_aes(unsigned char *input, unsigned int input_len, unsigned char *out, unsigned int out_len, unsigned char*key,
        int key_len=16, SL_CRYPTO_MODE_t mode=SL_CRYPTO_MODE_ECB, SL_CRYPTO_BLOCK_SIZE_t block_size = SL_CRYPTO_BLOCK_SIZE_16);

#endif // CFUTILS_H

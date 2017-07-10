/*
 * Copyright (C) 2011, 2012 Citrix Systems
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __APP_LIB__
#define __APP_LIB__

//#include <event2/event.h>

#include <openssl/ssl.h>

#include "ns_turn_ioaddr.h"

#ifdef __cplusplus
extern "C" {
#endif

//////////// Common defines ///////////////////////////

#define PEER_DEFAULT_PORT (3480)

#define DTLS_MAX_RECV_TIMEOUT (5)
#define DTLS_MAX_CONNECT_TIMEOUT (5)

#define UR_CLIENT_SOCK_BUF_SIZE (65536)
#define UR_SERVER_SOCK_BUF_SIZE (UR_CLIENT_SOCK_BUF_SIZE*2)

//////////////////////////////////////////

#define EVENT_DEL(ev) if(ev) { event_del(ev); event_free(ev); ev=NULL; }

//////////////////////////////////////////

#define ioa_socket_raw int

///////////////////////// Sockets ///////////////////////////////

void read_spare_buffer(int fd);

int set_sock_buf_size(int fd, int sz);
int socket_set_reusable(int fd);
int sock_bind_to_device(int fd, const unsigned char* ifname);

int addr_connect(int fd, const ioa_addr* addr, int *out_errno);
int addr_bind(int fd, const ioa_addr* addr);
int addr_get_from_sock(int fd, ioa_addr *addr);

int handle_socket_error(void);

/////////////////////// SYS /////////////////////

void set_system_parameters(int max_resources);

///////////////////////// MTU //////////////////////////

#define MAX_MTU (1500 - 20 - 8)
#define MIN_MTU (576 - 20 - 8)
#define SOSO_MTU (1300)

#define MTU_STEP (68)

int set_socket_df(int fd, int family, int value);
int set_mtu_df(SSL* ssl, int fd, int family, int mtu, int df_value, int verbose);
int decrease_mtu(SSL* ssl, int mtu, int verbose);
int get_socket_mtu(int fd, int family, int verbose);

////////////////// Misc utils /////////////////////////

char *skip_blanks(char* s);

////////////////// File search ////////////////////////

char* find_config_file(const char *config_file, int print_file_name);
void set_execdir(void);

////////////////// Base64 /////////////////////////////

char *base64_encode(const unsigned char *data,
                    size_t input_length,
                    size_t *output_length);

void build_base64_decoding_table(void);

unsigned char *base64_decode(const char *data,
                             size_t input_length,
                             size_t *output_length);

//////////////////// HMAC ///////////////////////////

int calculate_hmac(u08bits *buf, size_t len, const void *key, int key_len, u08bits *hmac, unsigned int *hmac_len);

///////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif //__APP_LIB__

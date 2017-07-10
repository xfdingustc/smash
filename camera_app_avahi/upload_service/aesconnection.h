#ifndef AESCONNECTION_H
#define AESCONNECTION_H

#include <getopt.h>
#include <fcntl.h>

#include <sys/types.h>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "cfutils.h"
#include "crs_protocol.h"

class BasicTCPConnection
{
public:
    BasicTCPConnection();
    ~BasicTCPConnection();

    bool Connect(char* ip, unsigned short port);
    void Disconnect();
    int Send(char* buffer, int len);
    int Recv(char *buf, int& rlen);
    bool isConnected() {return bConnected;};
    int getSockFd() {return socketFd;};

private:
    int		socketFd;
    bool	bConnected;
};

class AESConnection : public BasicTCPConnection
{
public:
    AESConnection(const char* key);
    ~AESConnection();

    int Send(char* buffer, int len, bool open = false);
    int Recv(char *buf, int& rlen);
    int Decode(void* in, int ilen, void* out, int olen);
    void UpdateKey(const char* key);

char* GetHash(char *id, int* buf_size)
{
    unsigned char hash_value[33];
    sprintf(id, "%s%s", id, pAESKey);
    *buf_size = strlen(id);
    cf_encode_md5((unsigned char*)id, *buf_size, hash_value);
    /*
    for (int i = 0; i < strlen((const char*)hash_value); ++i) {
        qDebug("%x\t", (unsigned char)hash_value[i]);
    }
    qDebug("\n");
    */
    *buf_size = 33;
    cf_encode_hex(hash_value, 16, (unsigned char*)id, (unsigned int*)buf_size, false);
    return (char*)id;
}
private:
    char* pAESKey;
};

#endif // AESCONNECTION_H

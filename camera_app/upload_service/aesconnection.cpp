#include <getopt.h>
#include <fcntl.h>

#include <errno.h>
#include <cstring>
#include "aesconnection.h"

#ifdef Q_OS_WIN
#include <ws2tcpip.h>
#endif

// BasicTCPConnection
BasicTCPConnection::BasicTCPConnection():socketFd(-1), bConnected(false)
{
}
BasicTCPConnection::~BasicTCPConnection()
{
    Disconnect();
}

bool BasicTCPConnection::Connect(char* ip, unsigned short port)
{
    struct sockaddr_in addr;
    memset(&addr,0 ,sizeof(addr));
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#ifdef Q_OS_WIN
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN+1];

    ZeroMemory(&ss, sizeof(ss));
    /* stupid non-const API */
    strncpy (src_copy, ip, INET6_ADDRSTRLEN+1);
    src_copy[INET6_ADDRSTRLEN] = 0;

    if (WSAStringToAddressA(src_copy, AF_INET, NULL, (struct sockaddr *)&ss, &size) == 0) {
        addr.sin_addr = ((struct sockaddr_in *)&ss)->sin_addr;
    } else {
        return false;
    }
#else
    if (0 == inet_pton(AF_INET, ip, &addr.sin_addr)) {
        return false;
    }
#endif
    if(socketFd > 0) {
        int ret = 0;
        if((ret = ::connect(socketFd, (sockaddr*)&addr, sizeof(addr))) < 0) {
#ifdef WIN32_OS
            closesocket(socketFd);
#else
            ::close(socketFd);
#endif
            bConnected = false;
            socketFd= -1;
        } else {
            bConnected = true;
        }
    } else {
        bConnected = false;
    }
    return bConnected;
}

void BasicTCPConnection::Disconnect()
{
    if (socketFd > 0) {
#ifdef WIN32_OS
        closesocket(socketFd);
#else
        ::close(socketFd);
#endif
        socketFd= -1;
    }
    bConnected = false;
}
int BasicTCPConnection::Send(char* buffer, int len)
{
    if (!bConnected){
        return -1;
    }
    int ret = 0;
    int left = len;
    char *pbuf = buffer;
    while(0 < left) {
        errno = 0;
#ifdef Q_OS_WIN
        ret = ::send(socketFd, pbuf, left, 0);
#elif defined Q_OS_OSX
        ret = ::send(socketFd, pbuf, left, SO_NOSIGPIPE);
#else
        ret = ::send(socketFd, pbuf, left, MSG_DONTWAIT | MSG_NOSIGNAL);
#endif
        //ret = ::send(socketFd, pbuf, left, MSG_NOSIGNAL);
        if(ret < 0) {
            if (errno != 0) {
                if (errno == EAGAIN) {
                    usleep(5000);
                    ret = 0;
                }
                else
                    return errno;
            }
        }
        pbuf += ret, left -= ret;
    }
    //qDebug("%s[%d]: BasicTCPConnection::Send, ret = %d %d\r\n", __FUNCTION__, __LINE__, ret, errno);
    return 0;
}
int BasicTCPConnection::Recv(char *buf, int& rlen)
{
    char *pbuf = buf;
    int left = 2;

    while (left > 0) {
        int n = ::recv(socketFd, pbuf, left, 0);
        if(0 >= n) return -1;
        pbuf += n; left -= n;
    }
    rlen = ntohs(*(short*)buf);
    left = rlen - 2;
    while(left > 0) {
        int n = ::recv(socketFd, pbuf, left, 0);
        if(0 >= n) return -1;
        pbuf += n; left -= n;
    }
    return left ? -2 : 0;
}

//AESConnection
AESConnection::AESConnection( const char* key)
    : BasicTCPConnection()
    , pAESKey(NULL)
{
    if (key != NULL) {
        int keysize = strlen(key);
        pAESKey = (char*)malloc(((keysize/32)+1)*32);
        strcpy(pAESKey, key);
    }
}
AESConnection::~AESConnection()
{
    if (pAESKey != NULL) {
        free(pAESKey);
        pAESKey = NULL;
    }
}

void AESConnection::UpdateKey(const char* key)
{
    if (pAESKey) {
        if (strcmp(pAESKey, key) == 0) {
            return;
        }
        free(pAESKey);
    }
    int keysize = strlen(key);
    pAESKey = (char*)malloc(((keysize/32)+1)*32);
    strcpy(pAESKey, key);
    return;
}

int AESConnection::Send(char* buffer, int len, bool open)
{
    char out_buf[8192] = {0};
    char *poutbuf;
    int out_len = (int)sizeof(out_buf);
    int ilen = 0;
    if(open) {
        poutbuf = buffer;
        ilen = len;
    } else {
        poutbuf = out_buf;
        ilen = cf_encode_aes((unsigned char*)buffer, len, (unsigned char*)out_buf, out_len, (unsigned char*)pAESKey);
    }

    EncodeCommHead encode_head;
    memset(&encode_head, 0x00, sizeof(encode_head));
    encode_head.encode_type = open ? ENCODE_TYPE_OPEN : ENCODE_TYPE_AES;
    char tmp_buf[8192] = {0};
    int tmp_buf_size = (int)sizeof(tmp_buf),  rlen = 0;
    int ret = EncodePkgEncodeMessage(&encode_head, poutbuf, ilen,
        tmp_buf, tmp_buf_size, &rlen);
    if(0 != ret) {
        return -1;
    }
    //qDebug("%s[%d]: send %d bytes:\r\n", __FUNCTION__, __LINE__, rlen);
    return BasicTCPConnection::Send(tmp_buf, rlen);
}

int AESConnection::Recv(char *buf, int &rlen)
{
    int ret = 0;
    char tmpbuf[8192];
    int len = (int)sizeof(tmpbuf);
    ret = BasicTCPConnection::Recv(tmpbuf, len);
    if(ret < 0 || 0 >= len){
        return -1;
    }

    EncodeCommHead *ph = (EncodeCommHead*)tmpbuf;
    char *pbuf = tmpbuf + sizeof(*ph);
    int ilen = (int)(ntohs(ph->size) - sizeof(*ph));

    ret = cf_decode_aes((unsigned char*)pbuf, ilen,
                    (unsigned char*)buf, len,
                    (unsigned char*)pAESKey);
    rlen = ret;
    return ret <0 ? ret : 0;
}


#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <getopt.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/md5.h>

#include "Class_Upgrader.h"

CTSUpgrader* CTSUpgrader::pInstance = NULL;

CTSUpgrader::CTSUpgrader(int lisenPort, const char* md5, TSUpgradeCB* cb)
    : CThread("CTSUpgrader", CThread::NORMAL, 0, NULL)
    , mLisenPort(lisenPort)
    , pCB(cb)
{
    pBuffer = new char[RECV_BUFFER_SIZE];
    strcpy(pMD5, md5);
    mServerFd = -1;
    mUpgradeSock = -1;
    fileFd = NULL;
    mState = UpgradeStatus_Idle;
    snprintf(pTmpFile, 128, "%s%s", POOL_PATH, TMP_UPGRADE_FILENAME);
    snprintf(pFile, 128, "%s%s", POOL_PATH, UPGRADE_FILENAME);
}

CTSUpgrader::~CTSUpgrader()
{
    if (mServerFd > 0) {
        ::close(mServerFd);
    }
    delete[] pBuffer;
}

CTSUpgrader *CTSUpgrader::getInstance(const char* md5, TSUpgradeCB* cb)
{
    if (CTSUpgrader::pInstance == NULL) {
        CTSUpgrader::pInstance = new CTSUpgrader(UPGRADE_PORT, md5, cb);
        CTSUpgrader::pInstance->Go();
    } else {
        if ( CTSUpgrader::pInstance->getState() == UpgradeStatus_Rdy) {
            if (cb) {
                cb->onUpgradeResult(1);
            }
        }
    }
    return CTSUpgrader::pInstance;
}

void CTSUpgrader::releaseInstance()
{
    if (CTSUpgrader::pInstance != NULL) {
        CTSUpgrader::pInstance->Stop();
        delete CTSUpgrader::pInstance;
        CTSUpgrader::pInstance = NULL;
    }
}

int CTSUpgrader::createServer(int port)
{
    struct sockaddr_in server_addr;
    const char *func = NULL;
    int socket_fd;

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        CAMERALOG("socket error %d", socket_fd);
        return -1;
    } else {
        CAMERALOG("connect %d, get Socket: %d.\n", port, socket_fd);
    }

    int flag = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        func = "setsockopt";
        goto Fail;
    }

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        func = "bind";
        goto Fail;
    }

    if (listen(socket_fd, MAX_UPGRADER_CLIENT_NUM) < 0) {
        func = "listen";
        goto Fail;
    }

    mState = UpgradeStatus_Rdy;
    return socket_fd;

Fail:
    mState = UpgradeStatus_Err;
    CAMERALOG("%s error %d", func, errno);
    ::close(socket_fd);
    return -1;
}

CTSUpgrader::EUpgradeStatus CTSUpgrader::getState()
{
    return mState;
}

int CTSUpgrader::doGetBuffer(char* buf, int len)
{
    if (fileFd == NULL) {
        if (::access(pTmpFile, F_OK) >= 0) {
            ::unlink(pTmpFile);
        }
        if ((fileFd = fopen(pTmpFile,"w")) == NULL) {
            return -1;
        }
    }
    if (fwrite(buf, 1, len, fileFd) < (size_t)len) {
        return -1;
    }
    return 0;
}
int CTSUpgrader::doCheckRecvDone()
{
    if (fileFd) {
        fclose(fileFd);
    }
    if (::access(pFile, F_OK) >= 0) {
        ::unlink(pFile);
    }

    FILE *fp = NULL;
    unsigned char buf[1024 * 128] = "";
    unsigned char md5[128] = "";
    char md5str[128] = "";
    memset(md5, 0, 128);
    memset(md5str, 0, 128);

    if ((fp = fopen(pTmpFile,"r")) == NULL) {
        return -1;
    } else {
        MD5_CTX ctx;
        MD5_Init(&ctx);
        fseek(fp, 0, SEEK_END);
        size_t filesize = ftell(fp);
        size_t readBytes = 0;
        fseek(fp, 0, SEEK_SET);
        CAMERALOG("md5 read file, size:%d", (int)filesize);
        while (readBytes < filesize) {
            fseek(fp, readBytes, SEEK_SET);
            size_t len = fread(buf, 1, 1024 * 128, fp);
            if (len < 0) {
                CAMERALOG("md5 read error, len: %d, read:%d", len, readBytes);
                break;
            }
            MD5_Update(&ctx, buf, len);
            readBytes += len;
            //CAMERALOG("md5 read len: %d, offset:%d", len, (int)ftell(fp));
        }
        fclose(fp);
        MD5_Final(md5, &ctx);
        int index = 0;
        for (int i = 0; i < MD5_LBLOCK; i++) {
            index += snprintf(md5str + index, 128 - index, "%02x", md5[i]);
        }
        CAMERALOG("md5: %s, should be: %s", md5str, pMD5);
    }

    if (strcmp(pMD5, (const char*)md5str) == 0) {
        CAMERALOG("check md5 success, tell APP to do upgrading");
        ::rename(pTmpFile, pFile);
        if (pCB) {
            pCB->onUpgradeResult(0);
        }
    } else {
        if (pCB) {
            pCB->onUpgradeResult(-1);
        }
    }
    return 0;
}

void CTSUpgrader::main(void * p)
{
    mServerFd = createServer(mLisenPort);
    if (mServerFd < 0) {
        if (pCB) {
            pCB->onUpgradeResult(-1);
        }
        return;
    } else {
        if (pCB) {
            pCB->onUpgradeResult(1);
        }
    }

    fd_set readfds;
    int Maxfd;
    int newFd;
    int buffersize = 0;
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    memset(&addr,0 ,sizeof(addr));
    bzero(pBuffer, RECV_BUFFER_SIZE);
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(mServerFd, &readfds);
        Maxfd = mServerFd + 1;
        if (mUpgradeSock > 0) {
            FD_SET(mUpgradeSock, &readfds);
            Maxfd = (mUpgradeSock > (Maxfd - 1))? (mUpgradeSock + 1) : Maxfd;
        }
        //CAMERALOG("RDY to wait connection/data for upgrading:%d (%d, %d)", Maxfd, mUpgradeSock, mServerFd);
        if (::select(Maxfd, &readfds, NULL, NULL, NULL)) {
            if (FD_ISSET(mServerFd, &readfds)) {
                newFd = accept(mServerFd, (sockaddr*)&addr, (socklen_t*)&addr_len);
                if ((newFd)&& (mUpgradeSock < 0)) {
                    CAMERALOG("---new connect : %d", newFd);
                    mUpgradeSock = newFd;
                    FD_SET(newFd, &readfds);
                } else if(newFd > 0) {
                    close(newFd);
                    CAMERALOG("too much upgrade connection");
                }
            } else {
                if (FD_ISSET(mUpgradeSock, &readfds)) {
                    int len = (int)::recv(mUpgradeSock,
                        pBuffer  + buffersize,
                        RECV_BUFFER_SIZE - buffersize,
                        MSG_WAITALL | MSG_NOSIGNAL);
                    if (len > 0) {
                        buffersize += len;
                        if (buffersize < RECV_BUFFER_SIZE) {
                            continue;
                        }
                    } else {
                        CAMERALOG("upgrade socket closed fd : %d", mUpgradeSock);
                        close(mUpgradeSock);
                        FD_CLR(mUpgradeSock,&readfds);
                        mUpgradeSock = -1;
                    }
                    {
                        if (buffersize > 0) {
                            if (this->doGetBuffer(pBuffer, buffersize) != 0) {
                                if (pCB) {
                                    pCB->onUpgradeResult(-1);
                                }
                                close(mUpgradeSock);
                                FD_CLR(mUpgradeSock,&readfds);
                                mUpgradeSock = -1;
                            } else {
                                static int size = 0;
                                size += buffersize;
                                CAMERALOG("get size: %d", size);
                                buffersize = 0;
                            }
                        }
                    }
                    if (mUpgradeSock == -1) {
                        this->doCheckRecvDone();
                    }
                }
            }
        }
    }
}



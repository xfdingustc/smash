
#ifndef __CLASS_VDBUPLOADER_H__
#define __CLASS_VDBUPLOADER_H__

#include <curl/curl.h>
#include <curl/easy.h>

#include "vdb_cmd.h"
#include "aesconnection.h"
#include "uploadlister.h"

class VdbUploader : public AESConnection
{
public:
    VdbUploader();
    ~VdbUploader();

    void setListenser(UploadListener *listener) { _pListener = listener; }
    void uploadMarkedClip();

private:

    struct clip_info {
        uint8_t uuid[40];
        vdb_clip_info_ex_s clip_ex;
        avf_stream_attr_t stream_attr[2];
    };

    int _openVDBCMDPort();
    void _closeVDBCMDPort(int sock);

    static size_t write_data(void *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t headerfunc(void *ptr, size_t size, size_t nmemb, void *userdata);
    int getMarkedClip();
    int getClipUrl();
    int getContentLength(const char *url);
    int downloadData(const char *url, int offset = 0, int length = 0);

    int sendThumnail();

    int createLoginJson();
    static size_t login_write_func(void *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t login_read_func(void *ptr, size_t size, size_t nmemb, void *userdata);
    int login();

    int createApplicationJson();
    static size_t application_write_func(void *ptr, size_t size, size_t nmemb, void *userdata);
    int applyForUpload();

    char* GetHash(char *id, int* buf_size);
    int handshake();
    int momentDesc();
    int startUpload(int type);
    int stopUpload(int type);
    int sendData(uint32 type, char * buf, int size, const char * guid_str);

    enum UPLOAD_STATE {
        STATE_IDLE = 0,
        STATE_GOT_CLIP,
        STATE_GOT_URL,
        STATE_LOGININ,
        STATE_APPLY_SUCCESS,
        STATE_HANDSHAKE_SUCCESS,
        STATE_SENT_DESCRIPTION,
    };

    UploadListener *_pListener;

    CURL  *_curlhandle;

    long  _contentLength;
    int   _bytesDownloaded;

    int        _socket_VDB_CMD;
    clip_info  _clipinfo;
    char       _clipUrl[128];

    char  _username[32];
    char  _password[32];
    char  _loginJson[32];

    char  _userID[34];
    char  _token[512];

    char     _applicationJson[32];
    int64_t  _momentsID;
    char     _serverIP[128];
    int      _serverPort;
    char     _privateKey[1024];
    int      _socketServer;
    int      _bytesUploaded;
    int      mCryptoType_;   //0: open 1: aes all;
    uint32   mUploadSeqNum_;
    bool     bUploading_;

    int   _state;
};

#endif

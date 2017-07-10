
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>

#include <openssl/sha.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "class_ipc_rec.h"


#include "comm_protocol.h"
#include "crs_protocol.h"

#include "vdbupload.h"

using namespace rapidjson;

#if 1

#define SERVICE_LOGIN_URL   "http://ws.waylens.com:9000/api/users/signin"
#define SERVICE_UPLOAD_URL  "http://ws.waylens.com:9000/api/moments"
#define SERVICE_QUERY_UPLOAD_URL    "http://ws.waylens.com:9000/api/clips/check_uploaded"

#else

#define SERVICE_LOGIN_URL   "http://192.168.20.97:9000/api/users/signin"
#define SERVICE_UPLOAD_URL  "http://192.168.20.97:9000/api/moments"
#define SERVICE_QUERY_UPLOAD_URL    "http://192.168.20.97:9000/api/clips/check_uploaded"

#endif

VdbUploader::VdbUploader()
    : AESConnection(NULL)
    , _pListener(NULL)
    , _socket_VDB_CMD(-1)
    , mCryptoType_(1)
    , mUploadSeqNum_(0)
    , bUploading_(false)
{
    memset(_clipUrl, 0x00, 128);
    memset(_serverIP, 0x00, 32);

    curl_global_init(CURL_GLOBAL_ALL);  
}

VdbUploader::~VdbUploader()
{
    curl_global_cleanup();
}

int VdbUploader::_openVDBCMDPort() {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    //addr.sin_addr.s_addr = inet_addr(_ip);
    addr.sin_port = htons(VDB_CMD_PORT);

    _socket_VDB_CMD = socket(AF_INET, SOCK_STREAM, 0);
    //CAMERALOG("_socket_VDB_CMD = %d", _socket_VDB_CMD);
    int ret = connect(_socket_VDB_CMD, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        CAMERALOG("connect error, ret = %d", ret);
        return -1;
    }

    int bytes_received = 0;

    vdb_ack_t ack;
    ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        CAMERALOG("recv error, ret = %d", ret);
        return -1;
    }
    bytes_received += ret;

    vdb_ack_GetVersionInfo_s version_ack;
    ret = recv(_socket_VDB_CMD, &version_ack, sizeof(version_ack), 0);
    bytes_received += ret;
    //CAMERALOG("version.major = %d, version.minor = %d",
    //    version_ack.major, version_ack.minor);

    int remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    if (remaining > 0) {
        char ackBuffer[160] = {0x00};
        ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
    }

    return _socket_VDB_CMD;
}

void VdbUploader::_closeVDBCMDPort(int sock) {
    if (sock > 0) {
        close(sock);
        sock = -1;
    }
}

int VdbUploader::createLoginJson()
{
    strcpy(_username, "cxiang");
    strcpy(_password, "Transee0415");

    Document d1;
    d1.SetObject();
    Document::AllocatorType& allocator = d1.GetAllocator();

    Value username(kStringType);
    username.SetString(_username, allocator);
    d1.AddMember("userName", username, allocator);

    Value password(kStringType);
    password.SetString(_password, allocator);
    d1.AddMember("password", password, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> write(buffer);
    d1.Accept(write);
    const char *str = buffer.GetString();
    memset(_loginJson, 0x00, 32);
    snprintf(_loginJson, 32, "%s", str);
    return strlen(_loginJson);
}

size_t VdbUploader::login_write_func(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    VdbUploader *loader = (VdbUploader *)userdata;
    if (!loader) {
        CAMERALOG("no user data, size = %d", size);
        return size * nmemb;
    }

    char *header = (char *)ptr;
    //CAMERALOG("header = %s", header);

    Document d;
    d.Parse(header);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        return size * nmemb;
    }

    bool found_id = false, found_token = false;
    if (d.HasMember("user")) {
        const Value &user = d["user"];
        if (user.HasMember("userID")) {
            const Value &userID = user["userID"];
            memset(loader->_userID, 0x00, 34);
            strcpy(loader->_userID, userID.GetString());
            //CAMERALOG("userID = %s", loader->_userID);
            found_id = true;
        }
    }
    if (d.HasMember("token")) {
        const Value &token = d["token"];
        memset(loader->_token, 0x00, 512);
        strcpy(loader->_token, token.GetString());
        //CAMERALOG("len = %d, _token = %s", strlen(loader->_token), loader->_token);
        found_token = true;
    }

    if (found_id && found_token) {
        loader->_state = STATE_LOGININ;
    }
    return size * nmemb;
}

size_t VdbUploader::login_read_func(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    VdbUploader *loader = (VdbUploader *)userdata;
    if (!loader) {
        CAMERALOG("no user data, size = %d", size);
        return size * nmemb;
    }

    return size * nmemb;
}

int VdbUploader::login()
{
    int len = createLoginJson();
    if (len <= 0) {
        return 0;
    }

    _curlhandle = curl_easy_init();
    curl_easy_setopt(_curlhandle, CURLOPT_URL, SERVICE_LOGIN_URL);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_WRITEFUNCTION, login_write_func);
    curl_easy_setopt(_curlhandle, CURLOPT_WRITEDATA, this);
    //curl_easy_setopt(_curlhandle, CURLOPT_READFUNCTION, login_read_func);
    //curl_easy_setopt(_curlhandle, CURLOPT_READDATA, this);

    struct curl_slist *list = NULL;
    list = curl_slist_append(list, "user-Agent:Waylens Uploader v0.1");
    list = curl_slist_append(list, "X-Custom-User-Agent:Waylens Uploader v0.1");
    list = curl_slist_append(list, "Content-Type:application/json;charset=UTF-8");
    //char con_len[32] = {0x00};
    //snprintf(con_len, 32, "Content-Length:%d", len);
    //list = curl_slist_append(list, con_len);
    curl_easy_setopt(_curlhandle, CURLOPT_HTTPHEADER, list);

    // 设置要POST的JSON数据
    //CAMERALOG("_loginJson = %s", _loginJson);
    curl_easy_setopt(_curlhandle, CURLOPT_POSTFIELDS, _loginJson); 

    CURLcode res = curl_easy_perform(_curlhandle);
    if (res != 0) {
        CAMERALOG("curl_easy_perform failed with error %d", res);
    }
    curl_easy_cleanup(_curlhandle);

    return 0;
}

int VdbUploader::createApplicationJson()
{
    Document d1;
    d1.SetObject();
    Document::AllocatorType& allocator = d1.GetAllocator();

    Value title(kStringType);
    title.SetString("uploaded from Waylens camera", allocator);
    d1.AddMember("title", title, allocator);

    d1.AddMember("desc", "uploaded from Waylens camera", allocator);
    d1.AddMember("accessLevel", "PUBLIC", allocator);
    d1.AddMember("audioType", 1, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> write(buffer);
    d1.Accept(write);
    const char *str = buffer.GetString();
    memset(_applicationJson, 0x00, 32);
    snprintf(_applicationJson, 32, "%s", str);

    return strlen(_applicationJson);
}

size_t VdbUploader::application_write_func(
    void *ptr, size_t size, size_t nmemb, void *userdata)
{
    VdbUploader *loader = (VdbUploader *)userdata;
    if (!loader) {
        CAMERALOG("no user data, size = %d", size);
        return size * nmemb;
    }

    char *header = (char *)ptr;
    //CAMERALOG("header = %s", header);

    Document d;
    d.Parse(header);
    if(!d.IsObject()) {
        CAMERALOG("Didn't get a JSON object!");
        return size * nmemb;
    }

    bool found = true;
    if (d.HasMember("momentID")) {
        const Value &momentID = d["momentID"];
        loader->_momentsID = momentID.GetInt64();
        CAMERALOG("_momentsID = %lld", loader->_momentsID);
    } else {
        found = false;
    }
    if (d.HasMember("uploadServer")) {
        const Value &uploadServer = d["uploadServer"];
        if (uploadServer.HasMember("ip")) {
            const Value &ip = uploadServer["ip"];
            memset(loader->_serverIP, 0x00, 128);
            strcpy(loader->_serverIP, ip.GetString());
            CAMERALOG("_serverIP = %s", loader->_serverIP);
        } else {
            found = false;
        }
        if (uploadServer.HasMember("port")) {
            const Value &port = uploadServer["port"];
            loader->_serverPort = port.GetInt();
            CAMERALOG("_serverPort = %d", loader->_serverPort);
        } else {
            found = false;
        }
        if (uploadServer.HasMember("privateKey")) {
            const Value &privateKey = uploadServer["privateKey"];
            memset(loader->_privateKey, 0x00, 1024);
            strcpy(loader->_privateKey, privateKey.GetString());
            //CAMERALOG("len = %d, _privateKey = %s",
            //    strlen(loader->_privateKey), loader->_privateKey);
            loader->UpdateKey(loader->_privateKey);
        } else {
            found = false;
        }
    } else {
        found = false;
    }

    if (found) {
        loader->_state = STATE_APPLY_SUCCESS;
    }
    return size * nmemb;
}

int VdbUploader::applyForUpload()
{
    int len = createApplicationJson();
    if (len <= 0) {
        return 0;
    }

    _curlhandle = curl_easy_init();
    curl_easy_setopt(_curlhandle, CURLOPT_URL, SERVICE_UPLOAD_URL);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_WRITEFUNCTION, application_write_func);
    curl_easy_setopt(_curlhandle, CURLOPT_WRITEDATA, this);

    struct curl_slist *list = NULL;
    list = curl_slist_append(list, "user-Agent:Waylens Uploader v0.1");
    list = curl_slist_append(list, "X-Custom-User-Agent:Waylens Uploader v0.1");
    list = curl_slist_append(list, "Content-Type:application/json;charset=UTF-8");
    char auth[512] = {0x00};
    snprintf(auth, 512, "X-Auth-Token:%s", _token);
    //CAMERALOG("auth = %s", auth);
    list = curl_slist_append(list, auth);
    curl_easy_setopt(_curlhandle, CURLOPT_HTTPHEADER, list);

    // 设置要POST的JSON数据
    //CAMERALOG("_applicationJson = %s", _applicationJson);
    curl_easy_setopt(_curlhandle, CURLOPT_POSTFIELDS, _applicationJson); 

    CURLcode res = curl_easy_perform(_curlhandle);
    if (res != 0) {
        CAMERALOG("curl_easy_perform failed with error %d", res);
    }
    curl_easy_cleanup(_curlhandle);

    return 0;
}

char* VdbUploader::GetHash(char *id, int* buf_size)
{
    unsigned char hash_value[33];
    sprintf(id, "%s%s", id, _privateKey);
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

int VdbUploader::handshake()
{
    WaylensCommHead cmm_head  = {0, CRS_C2S_LOGIN, WAYLENS_VERSION};
    CrsClientLogin req;
    memset(&req, 0x00, sizeof(req));
    sprintf(req.jid, "%s/%s", _userID, WAYLENS_RESOURCE_TYPE_PC);
    req.device_type = DEVICE_VIDIT;
    req.moment_id = _momentsID;
    char hash_value[1024] = {0};
    snprintf(hash_value, 1024, "%s%lld%d", req.jid, _momentsID, req.device_type);
    //CAMERALOG("hash value: %s", hash_value);
    int len = (int)strlen(hash_value);
    char* hash_str = GetHash(hash_value, &len);
    //CAMERALOG("hash string: %s\n", hash_str);

    memcpy(req.hash_value, hash_str, len);
    char tmp_buf[512] = {0};
    int tmp_buf_size = (int)sizeof(tmp_buf), rlen = 0;
    int ret = EncodePkgCrsClientLogin(&cmm_head, &req, tmp_buf, tmp_buf_size, &rlen);
    if(0 != ret) {
        CAMERALOG("EncodePkgCrsClientLogin failed, ret = %d", ret);
        return -1;
    }

    if (Send(tmp_buf, rlen) < 0){
        CAMERALOG("Send error");
        return -1;
    }

    rlen = 0;
    char buf[8192] = {0};
    Recv(buf, rlen);
    if(0 >= rlen) {
        CAMERALOG("recv login ack error, %d", rlen);
        return rlen;
    }

    memset(&cmm_head, 0x00, sizeof(cmm_head));
    CrsCommResponse rsp;
    memset(&rsp, 0x00, sizeof(rsp));
    ret = DecodePkgCrsCommResponse(buf, rlen, &cmm_head, &rsp);
    if(0 != ret) {
        CAMERALOG("%s DecodePkgCrsCommResponse failed, ret = %d", rsp.jid_ext, ret);
        return ret;
    }

    CAMERALOG("hand shake success");
    _state = STATE_HANDSHAKE_SUCCESS;

    return ret;
}

int VdbUploader::momentDesc()
{
    WaylensCommHead cmm_head  = {0, CRS_C2S_UPLOAD_MOMENT_DESC, WAYLENS_VERSION};
    CrsMomentDescription req;

    memset(&req, 0x00, sizeof(req));
    sprintf(req.jid, "%s/%s", _userID, WAYLENS_RESOURCE_TYPE_PC);
    req.moment_id = _momentsID;

    // thumnail description
    req.fragments[0].data_type = VIDIT_THUMBNAIL_JPG;
    sprintf(req.fragments[0].guid, "%lld", _momentsID);

    // clip description
    strcpy(req.fragments[1].guid, (char *)_clipinfo.uuid);

    time_t tim = _clipinfo.clip_ex.inherited.clip_date;
    struct tm* clip_tm = gmtime(&tim);
    sprintf(req.fragments[1].clip_capture_time, "%04d-%02d-%02d %02d:%02d:%02d",
        1900 + clip_tm->tm_year, 1 + clip_tm->tm_mon, clip_tm->tm_mday,
        clip_tm->tm_hour, clip_tm->tm_min, clip_tm->tm_sec);
    //CAMERALOG("###########req.fragments[1].clip_capture_time = %s",
    //    req.fragments[1].clip_capture_time);

    uint64 start_time = _clipinfo.clip_ex.inherited.clip_start_time_ms_lo;
    start_time += (uint64)(_clipinfo.clip_ex.inherited.clip_start_time_ms_hi) << 32;
    req.fragments[1].begin_time = start_time;
    req.fragments[1].offset = 0;
    req.fragments[1].duration = _clipinfo.clip_ex.inherited.clip_duration_ms;

    //CAMERALOG("resolution %d x %d",
    //    _clipinfo.stream_attr[1].video_width, _clipinfo.stream_attr[1].video_height);
    req.fragments[1].resolution = _clipinfo.stream_attr[1].video_width;
    req.fragments[1].resolution <<= 16;
    req.fragments[1].resolution += _clipinfo.stream_attr[1].video_height;

    req.fragments[1].data_type =
        VIDIT_VIDEO_DATA_LOW | VIDIT_RAW_GPS | VIDIT_RAW_OBD | VIDIT_RAW_ACC;

    char tmp_buf[512] = {0};
    int tmp_buf_size = (int)sizeof(tmp_buf), rlen = 0;
    int ret = EncodePkgCrsMomentDescription(&cmm_head, &req, tmp_buf, tmp_buf_size, &rlen);
    if(0 != ret) {
        CAMERALOG("EncodePkgCrsMomentDescription failed, ret = %d", ret);
        return -1;
    }

    if (Send(tmp_buf, rlen) < 0){
        CAMERALOG("Send error");
        return -1;
    }

    rlen = 0;
    char buf[8192] = {0};
    Recv(buf,rlen);
    if(0 >= rlen) {
        CAMERALOG("recv login ack error, %d", rlen);
        return rlen;
    }

    memset(&cmm_head, 0x00, sizeof(cmm_head));
    CrsMomentDescription rsp;
    memset(&rsp, 0x00, sizeof(rsp));
    ret = DecodePkgCrsMomentDescription(buf, rlen, &cmm_head, &rsp);
    if(0 != ret) {
        CAMERALOG("%s DecodePkgCrsMomentDescription failed, ret = %d", rsp.jid, ret);
        return ret;
    }

    CAMERALOG("send moment description success");
    _state = STATE_SENT_DESCRIPTION;

    return ret;
}

// type:
//   0: thumnail
//   1: clip data
int VdbUploader::startUpload(int type)
{
    WaylensCommHead cmm_head  = {0, CRS_C2S_START_UPLOAD, WAYLENS_VERSION};
    CrsClientStartUpload req;
    memset(&req, 0x00, sizeof(req));

    if (type == 0) {
        sprintf(req.jid_ext, "%s/%s/%lld",
            _userID, WAYLENS_RESOURCE_TYPE_PC, _momentsID);
        req.data_type = VIDIT_THUMBNAIL_JPG;
        req.moment_id = _momentsID;
        req.begin_time = 0;
        req.offset = 0;
        req.duration = 0;
    } else if (type == 1) {
        sprintf(req.jid_ext, "%s/%s/%s",
            _userID, WAYLENS_RESOURCE_TYPE_PC, (char *)_clipinfo.uuid);

        uint32 data_type =
            VIDIT_VIDEO_DATA_LOW | VIDIT_RAW_GPS | VIDIT_RAW_OBD | VIDIT_RAW_ACC;
        req.data_type = data_type;

        uint64 start_time = _clipinfo.clip_ex.inherited.clip_start_time_ms_lo;
        start_time += (uint64)(_clipinfo.clip_ex.inherited.clip_start_time_ms_hi) << 32;
        req.moment_id = _momentsID;
        req.begin_time = start_time;
        req.offset = 0;
        req.duration = _clipinfo.clip_ex.inherited.clip_duration_ms;
    } else {
        CAMERALOG("bad data type %d", type);
        return -1;
    }

    char tmp_buf[512] = {0};
    int tmp_buf_size = (int)sizeof(tmp_buf), rlen = 0;
    int ret = EncodePkgCrsClientStartUpload(&cmm_head, &req, tmp_buf, tmp_buf_size, &rlen);
    if(0 != ret)
    {
        CAMERALOG("EncodePkgCrsClientStartUpload failed, ret = %d", ret);
        return -1;
    }

    if (Send(tmp_buf, rlen)<0){
        CAMERALOG("Send error");
        return -1;
    }

    rlen = 0;
    char buf[8192] = {0};
    Recv(buf, rlen);
    if(0 >= rlen) {
        CAMERALOG("recv startupload ack error, %d", rlen);
        return -1;
    }

    CrsCommResponse comm_rsp;
    memset(&comm_rsp, 0x00, sizeof(comm_rsp));
    if(0 != DecodePkgCrsCommResponse(buf,rlen, &cmm_head, &comm_rsp))
    {
        CAMERALOG("DecodePkgCrsCommResponse failed");
        return -1;
    }

    if (comm_rsp.ret == 0) {
        bUploading_ = true;
    }
    if (type == 0) {
        CAMERALOG("start upload thumnail ack: result = %d", comm_rsp.ret);
    } else if (type == 1) {
        CAMERALOG("start upload clip ack: result = %d", comm_rsp.ret);
    }
    return comm_rsp.ret;
}

// type:
//   0: thumnail
//   1: clip data
int VdbUploader::stopUpload(int type)
{
    if (!bUploading_){
        CAMERALOG("not in uploading state");
        return 0;
    }

    WaylensCommHead cmm_head  = {0, CRS_C2S_STOP_UPLOAD, WAYLENS_VERSION};
    CrsClientStopUpload req;
    memset(&req, 0x00, sizeof(req));
    if (type == 0) {
        sprintf(req.jid_ext, "%s/%s/%lld",
            _userID, WAYLENS_RESOURCE_TYPE_PC, _momentsID);
    } else if (type == 1) {
        sprintf(req.jid_ext, "%s/%s/%s",
            _userID, WAYLENS_RESOURCE_TYPE_PC, (char *)_clipinfo.uuid);
    } else {
        CAMERALOG("bad data type %d", type);
        return -1;
    }

    char tmp_buf[512] = {0};
    int tmp_buf_size = 512, rlen = 0;
    int ret = EncodePkgCrsClientStopUpload(&cmm_head, &req, tmp_buf, tmp_buf_size, &rlen);
    if (0 != ret) {
        CAMERALOG("EncodePkgCrsClientStopUpload failed, ret = %d", ret);
        return -1;
    }

    ret = Send(tmp_buf, rlen);
    if (ret < 0) {
        CAMERALOG("Send error");
        return -1;
    }

    rlen = 0;
    char rspBuf[8192] = {0};
    Recv(rspBuf,rlen);
    if(0 >= rlen) {
        CAMERALOG("recv stop ack error, %d", rlen);
        return -1;
    }

    CrsCommResponse comm_rsp;
    memset(&comm_rsp, 0x00, sizeof(comm_rsp));
    if(0 != DecodePkgCrsCommResponse(rspBuf,rlen, &cmm_head, &comm_rsp))
    {
        CAMERALOG("DecodePkgCrsCommResponse failed");
        return -1;
    }

    if (comm_rsp.ret != 2) {
        CAMERALOG("error stop upload, comm_rsp.ret = %d", comm_rsp.ret);
        ret = -2;
    } else {
        ret = 0;
    }

    if (type == 0) {
        CAMERALOG("upload thumnail stopped");
    } else if (type == 1) {
        CAMERALOG("upload clip stopped");
    }
    bUploading_ = false;
    return ret;
}

int VdbUploader::sendData(
    uint32 type, char * buf, int size, const char * guid_str)
{
    if (!bUploading_) {
        CAMERALOG("Uploading is not started!");
        return -1;
    }

    char tmp_buf[CAM_TRAN_BLOCK_SIZE * 2] = {0};
    int tmp_buf_size = CAM_TRAN_BLOCK_SIZE * 2, rlen = 0;
    int ret = 0;

    WaylensCommHead cmm_head  = {0, CRS_UPLOADING_DATA, WAYLENS_VERSION};
    CrsClientTranData req;

    int offset = 0;
    bool open = false;
    if (mCryptoType_ > 0) {
        open = false;
    } else {
        open = true;
    }

    memset(&req, 0x00, sizeof(req));
    sprintf(req.jid_ext, "%s/%s/%s", _userID, WAYLENS_RESOURCE_TYPE_PC, guid_str);
    req.moment_id = _momentsID;
    req.data_type = type;
    req.block_num = 0;

    do {
        if (size - offset >= CAM_TRAN_BLOCK_SIZE) {
            req.length = CAM_TRAN_BLOCK_SIZE;
        } else {
            req.length = size - offset;
        }
        memset(tmp_buf, 0, sizeof(tmp_buf));
        memcpy(req.buf, buf + offset, req.length);
        offset += req.length;
        req.seq_num = mUploadSeqNum_;
        mUploadSeqNum_++;

        SHA_CTX stx;
        SHA1_Init(&stx);
        SHA1_Update(&stx, req.buf, req.length);
        unsigned char outmd[20];//注意这里的字符个数为20
        SHA1_Final(outmd, &stx);
        memcpy(req.block_sha1, outmd, 20);

        ret = EncodePkgCrsClientTranData(&cmm_head, &req, tmp_buf, tmp_buf_size, &rlen);
        if(0 != ret) {
            CAMERALOG("EncodePkgCrsClientTranData failed, ret = %d", ret);
            ret = -1;
            break;
        }
        ret = Send(tmp_buf, rlen, open);
        if (ret < 0){
            CAMERALOG("Send error %d", ret);
            break;
        }
    } while (offset < size);

    return ret;
}

int VdbUploader::getMarkedClip()
{
    int _socket_VDB_CMD = _openVDBCMDPort();
    if (_socket_VDB_CMD <= 0) {
        CAMERALOG("Bad socket %d", _socket_VDB_CMD);
        return -1;
    }

    int ret_code = 0;
    char cmdBuffer[160] = {0x00};
    char ackBuffer[160] = {0x00};

    // first of all: get all marked clips
    vdb_cmd_GetClipSetInfoEx_t clipinfo_cmd;
    clipinfo_cmd.inherited.header.cmd_code = VDB_CMD_GetClipSetInfoEx;
    clipinfo_cmd.inherited.header.cmd_flags = 0;
    clipinfo_cmd.inherited.header.cmd_tag = 0;
    clipinfo_cmd.inherited.header.user1 = 0;
    clipinfo_cmd.inherited.header.user2 = 0;
    clipinfo_cmd.inherited.clip_type = CLIP_TYPE_MARKED;
    clipinfo_cmd.flags = GET_CLIP_EXTRA;
    int sent = send(_socket_VDB_CMD, &clipinfo_cmd, sizeof(clipinfo_cmd), MSG_NOSIGNAL);
    if (sent < 0) {
        CAMERALOG("send() failed with error %d", sent);
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(clipinfo_cmd), MSG_NOSIGNAL);

    int bytes_received = 0;

    vdb_ack_t ack;
    memset(&ack, 0x00, sizeof(ack));
    int ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        CAMERALOG("Failed to receive data with error %d", ack.ret_code);
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    bytes_received += ret;

    vdb_ack_GetClipSetInfoEx_s clipset_ack;
    ret = recv(_socket_VDB_CMD, &clipset_ack, sizeof(clipset_ack), 0);
    bytes_received += ret;
    CAMERALOG("clipset_ack.clip_type = %d, ack.total_clips = %d",
        clipset_ack.clip_type, clipset_ack.total_clips);

    if (clipset_ack.total_clips <= 0) {
        CAMERALOG("No clip is found!");
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }

    vdb_clip_info_t clip;
    ret = recv(_socket_VDB_CMD, &clip, sizeof(vdb_clip_info_t), 0);
    bytes_received += ret;
    /*CAMERALOG("ret = %d, sizeof(vdb_clip_info_t) = %d, "
        "clip.clip_id = %d, clip.clip_date = %d, clip.clip_duration_ms = %d, "
        "clip.clip_start_time_ms_lo = %d, clip.clip_start_time_ms_hi = %d, "
        "clip.num_streams = %d, clip.flags = 0x%x",
        ret, sizeof(vdb_clip_info_t),
        clip.clip_id, clip.clip_date, clip.clip_duration_ms,
        clip.clip_start_time_ms_lo, clip.clip_start_time_ms_hi,
        clip.num_streams, clip.flags);*/
    time_t tim = clip.clip_date;
    struct tm* clip_tm = gmtime(&tim);
    char cap_time[64] = {0x00};
    snprintf(cap_time, 64, "%04d-%02d-%02d %02d:%02d:%02d",
        1900 + clip_tm->tm_year, 1 + clip_tm->tm_mon, clip_tm->tm_mday,
        clip_tm->tm_hour, clip_tm->tm_min, clip_tm->tm_sec);
    CAMERALOG("cap_time: %s", cap_time);

    _clipinfo.clip_ex.inherited.clip_id = clip.clip_id;
    _clipinfo.clip_ex.inherited.clip_date = clip.clip_date;
    _clipinfo.clip_ex.inherited.clip_duration_ms = clip.clip_duration_ms;
    _clipinfo.clip_ex.inherited.clip_start_time_ms_lo = clip.clip_start_time_ms_lo;
    _clipinfo.clip_ex.inherited.clip_start_time_ms_hi = clip.clip_start_time_ms_hi;
    _clipinfo.clip_ex.inherited.num_streams = clip.num_streams;
    _clipinfo.clip_ex.inherited.flags = clip.flags;
    for (int j = 0; j < clip.num_streams; j++) {
        ret = recv(_socket_VDB_CMD, &_clipinfo.stream_attr[j], sizeof(avf_stream_attr_t), 0);
        bytes_received += ret;
        /*CAMERALOG("ret = %d, sizeof(avf_stream_attr_t) = %d, "
            "clips[i].stream_attr[j].video_width = %d, "
            "clips[i].stream_attr[j].video_height = %d, "
            "clips[i].stream_attr[j].video_framerate = %d",
            ret, sizeof(avf_stream_attr_t),
            _clipinfo.stream_attr[j].video_width, _clipinfo.stream_attr[j].video_height,
            _clipinfo.stream_attr[j].video_framerate);*/
    }
    ret = recv(_socket_VDB_CMD, &(_clipinfo.clip_ex.clip_type), sizeof(_clipinfo.clip_ex.clip_type), 0);
    bytes_received += ret;
    ret = recv(_socket_VDB_CMD, &(_clipinfo.clip_ex.extra_size), sizeof(_clipinfo.clip_ex.extra_size), 0);
    bytes_received += ret;
    //CAMERALOG("_clipinfo.clip_ex.inherited.flags = 0x%x, extra_size = %d",
    //    _clipinfo.clip_ex.inherited.flags, _clipinfo.clip_ex.extra_size);
    if (_clipinfo.clip_ex.inherited.flags & GET_CLIP_EXTRA) {
        memset(_clipinfo.uuid, 0x00, 40);
        ret = recv(_socket_VDB_CMD, _clipinfo.uuid, 36, 0);
        uint32_t ref_clip_date;	// same with clip_date
        int32_t gmtoff;
        uint32_t real_clip_id;
        bytes_received += ret;
        ret = recv(_socket_VDB_CMD, &ref_clip_date, sizeof(ref_clip_date), 0);
        bytes_received += ret;
        ret = recv(_socket_VDB_CMD, &gmtoff, sizeof(gmtoff), 0);
        bytes_received += ret;
        ret = recv(_socket_VDB_CMD, &real_clip_id, sizeof(real_clip_id), 0);
        bytes_received += ret;
    }

    int remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    //CAMERALOG("bytes_received = %d, ack.extra_bytes = %d, sizeof(ack) = %d, remaining = %d",
    //    bytes_received, ack.extra_bytes, sizeof(ack), remaining);
    if (remaining > 0) {
        ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
        //CAMERALOG("ret = %d\n\n", ret);
    }
    bytes_received = 0;

    _closeVDBCMDPort(_socket_VDB_CMD);
    return ret_code;
}

int VdbUploader::getClipUrl()
{
    int _socket_VDB_CMD = _openVDBCMDPort();
    if (_socket_VDB_CMD <= 0) {
        CAMERALOG("Bad socket %d", _socket_VDB_CMD);
        return -1;
    }

    int ret_code = 0;
    char cmdBuffer[160] = {0x00};
    char ackBuffer[160] = {0x00};

    vdb_cmd_GetUploadUrl_t upload_url_cmd;
    upload_url_cmd.header.cmd_code = VDB_CMD_GetUploadUrl;
    upload_url_cmd.header.cmd_flags = 0;
    upload_url_cmd.header.cmd_tag = 0;
    upload_url_cmd.header.user1 = 0;
    upload_url_cmd.header.user2 = 0;
    upload_url_cmd.b_playlist = false;
    upload_url_cmd.clip_type = CLIP_TYPE_MARKED;
    upload_url_cmd.clip_id = _clipinfo.clip_ex.inherited.clip_id;
    upload_url_cmd.clip_time_ms_lo = _clipinfo.clip_ex.inherited.clip_start_time_ms_lo;
    upload_url_cmd.clip_time_ms_hi = _clipinfo.clip_ex.inherited.clip_start_time_ms_hi;
    upload_url_cmd.length_ms = _clipinfo.clip_ex.inherited.clip_duration_ms;
    upload_url_cmd.upload_opt = UPLOAD_GET_V1 | UPLOAD_GET_GPS | UPLOAD_GET_OBD | UPLOAD_GET_ACC;
    upload_url_cmd.reserved1 = 0;
    upload_url_cmd.reserved2 = 0;
    int sent = send(_socket_VDB_CMD, &upload_url_cmd, sizeof(upload_url_cmd), MSG_NOSIGNAL);
    if (sent < 0) {
        CAMERALOG("send() failed with error %d", sent);
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(upload_url_cmd), MSG_NOSIGNAL);

    int bytes_received = 0;
    int remaining = 0;

    vdb_ack_t ack;
    memset(&ack, 0x00, sizeof(ack));
    int ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        CAMERALOG("ack.ret_code = %d", ack.ret_code);
        ret_code = -1;
        goto EXIT;
    }

    bytes_received += ret;

    vdb_ack_GetUploadUrl_t upload_url_ack;
    ret = recv(_socket_VDB_CMD, &upload_url_ack, sizeof(upload_url_ack), 0);
    bytes_received += ret;
    /*CAMERALOG("ret = %d, upload_url_ack.clip_id = %d, "
        "upload_url_ack.url_size = %d, "
        "upload_url_ack.length_ms = %d",
        ret, upload_url_ack.clip_id,
        upload_url_ack.url_size,
        upload_url_ack.length_ms);*/

    memset(_clipUrl, 0x00, 128);
    ret = recv(_socket_VDB_CMD, _clipUrl, upload_url_ack.url_size, 0);
    bytes_received += ret;
    //CAMERALOG("url = %s", _clipUrl);

    remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    //CAMERALOG("bytes_received = %d, ack.extra_bytes = %d, sizeof(ack) = %d, remaining = %d",
    //    bytes_received, ack.extra_bytes, sizeof(ack), remaining);
    if (remaining > 0) {
        ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
        //CAMERALOG("got remaining, ret = %d", ret);
    }
    bytes_received = 0;

EXIT:
    _closeVDBCMDPort(_socket_VDB_CMD);
    return ret_code;
}

int VdbUploader::getContentLength(const char *url)
{
    _contentLength = 0;
    _curlhandle = curl_easy_init();
    curl_easy_setopt(_curlhandle, CURLOPT_URL, url);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_HEADERFUNCTION, headerfunc);
    curl_easy_setopt(_curlhandle, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(_curlhandle, CURLOPT_NOBODY, 1L);
    CURLcode res = curl_easy_perform(_curlhandle);
    if (res != 0) {
        CAMERALOG("curl_easy_perform failed with error %d", res);
    }
    curl_easy_cleanup(_curlhandle);
    CAMERALOG("got clip _contentLength = %ld", _contentLength);
    return _contentLength;
}

int VdbUploader::downloadData(const char *url, int offset, int length)
{
    _curlhandle = curl_easy_init();
    curl_easy_setopt(_curlhandle, CURLOPT_URL, url);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(_curlhandle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(_curlhandle, CURLOPT_WRITEDATA, this);
    if ((length > 0) && (offset >= 0)) {
        char range[32] = {0x00};
        snprintf(range, 32, "%d-%d", offset, offset + length - 1);
        CAMERALOG("range = %s", range);
        curl_easy_setopt(_curlhandle, CURLOPT_RANGE, range);
    }

    _bytesDownloaded = 0;
    CURLcode res = curl_easy_perform(_curlhandle);
    curl_easy_cleanup(_curlhandle);
    if (res != 0) {
        CAMERALOG("curl_easy_perform failed with error %d", res);
        return res;
    }
    CAMERALOG("finish download, _bytesDownloaded = %d", _bytesDownloaded);
    return _bytesDownloaded;
}

size_t VdbUploader::write_data(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    VdbUploader *loader = (VdbUploader *)userdata;
    if (!loader) {
        CAMERALOG("no user data, size = %d", size);
        return -1;
    }

    if (!loader->bUploading_) {
        CAMERALOG("Uploading is not started!");
        return -1;
    }

    loader->_bytesDownloaded += size * nmemb;
    //CAMERALOG("size = %d, nmemb = %d _bytesDownloaded = %d",
    //    size, nmemb, loader->_bytesDownloaded);

    int tosend = size * nmemb;
    int ret = 0;
    uint32 data_type =
        VIDIT_VIDEO_DATA_LOW | VIDIT_RAW_GPS | VIDIT_RAW_OBD | VIDIT_RAW_ACC;
    ret = loader->sendData(data_type,
        (char *)ptr, tosend,
        (const char *)loader->_clipinfo.uuid);
    if (ret < 0){
        CAMERALOG("Send error, ret = %d", ret);
        return -1;
    }
    loader->_bytesUploaded += tosend;

    int percentage = loader->_bytesUploaded * 100 / loader->_contentLength;
    if (loader->_pListener) {
        //CAMERALOG("on progress %d%%", percentage);
        loader->_pListener->onProgress(percentage);
    }

    return size * nmemb;
}

size_t VdbUploader::headerfunc(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    VdbUploader *loader = (VdbUploader *)userdata;
    if (!loader) {
        CAMERALOG("no user data, size = %d", size);
        return size * nmemb;
    }

    char *header = (char *)ptr;
    //CAMERALOG("header = %s", header);

    int r = 0;
    long len = 0;
    r = sscanf(header, "Content-Length: %ld\n", &len);
    if (r) {
        //CAMERALOG("content len = %ld", len);
        loader->_contentLength = len;
    }
    return size * nmemb;
}

int VdbUploader::sendThumnail()
{
    int _socket_VDB_CMD = _openVDBCMDPort();
    if (_socket_VDB_CMD <= 0) {
        CAMERALOG("Bad socket %d", _socket_VDB_CMD);
        return -1;
    }

    char cmdBuffer[160] = {0x00};
    char ackBuffer[160] = {0x00};

    vdb_cmd_GetIndexPicture_t picture_cmd;
    picture_cmd.header.cmd_code = VDB_CMD_GetIndexPicture;
    picture_cmd.header.cmd_flags = 0;
    picture_cmd.header.cmd_tag = 0;
    picture_cmd.header.user1 = 0;
    picture_cmd.header.user2 = 0;
    picture_cmd.clip_type = CLIP_TYPE_MARKED;
    picture_cmd.clip_id = _clipinfo.clip_ex.inherited.clip_id;
    picture_cmd.clip_time_ms_lo = _clipinfo.clip_ex.inherited.clip_start_time_ms_lo;
    picture_cmd.clip_time_ms_hi = _clipinfo.clip_ex.inherited.clip_start_time_ms_hi;
    //CAMERALOG("picture_cmd.clip_id = %d, request time clip_time_ms_lo = %d, clip_time_ms_hi = %d",
    //    picture_cmd.clip_id, picture_cmd.clip_time_ms_lo, picture_cmd.clip_time_ms_hi);
    int sent = send(_socket_VDB_CMD, &picture_cmd, sizeof(picture_cmd), MSG_NOSIGNAL);
    if (sent < 0) {
        CAMERALOG("send() failed with error %d", sent);
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    sent = send(_socket_VDB_CMD, cmdBuffer, 160 - sizeof(picture_cmd), MSG_NOSIGNAL);

    int bytes_received = 0;

    vdb_ack_t ack;
    memset(&ack, 0x00, sizeof(ack));
    int ret = recv(_socket_VDB_CMD, &ack, sizeof(ack), 0);
    if (ack.ret_code != 0) {
        CAMERALOG("VDB_CMD_GetIndexPicture failed with error %d", ack.ret_code);
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }
    bytes_received += ret;

    vdb_ack_GetIndexPicture_s picture_ack;
    ret = recv(_socket_VDB_CMD, &picture_ack, sizeof(picture_ack), 0);
    bytes_received += ret;

#if 0
    CAMERALOG("#### picture_ack.clip_id = %d, duration = %d, clip_type = %d, "
        "requested_time_ms_lo = %d, requested_time_ms_hi = %d, "
        "while real time clip_time_ms_lo = %d, clip_time_ms_hi = %d",
        picture_ack.clip_id, picture_ack.duration, picture_ack.clip_type,
        picture_ack.requested_time_ms_lo, picture_ack.requested_time_ms_hi,
        picture_ack.clip_time_ms_lo, picture_ack.clip_time_ms_hi);

    time_t tim = picture_ack.clip_date;
    struct tm* clip_tm = gmtime(&tim);
    CAMERALOG("#### clip date: %s", asctime(clip_tm));
#endif

    //CAMERALOG("#### picture size = %d", picture_ack.picture_size);
    if (picture_ack.picture_size <= 0) {
        CAMERALOG("incorrect picture_size");
        _closeVDBCMDPort(_socket_VDB_CMD);
        return -1;
    }

    if (picture_ack.picture_size > 0) {
        uint8_t *data = new uint8_t [picture_ack.picture_size];
        memset(data, 0x00, picture_ack.picture_size);
        uint32_t pic_received = 0;
        while (pic_received < picture_ack.picture_size) {
            ret = recv(_socket_VDB_CMD,
                data + pic_received,
                picture_ack.picture_size - pic_received,
                MSG_NOSIGNAL);
            if (ret < 0) {
                delete [] data;
                CAMERALOG("recv failed with error %d", ret);
                _closeVDBCMDPort(_socket_VDB_CMD);
                return -1;
            }
            pic_received += ret;
            bytes_received += ret;
        }

#if 0
        // dump the thumnail
        FILE *fp = fopen("/tmp/mmcblk0p1/thumnail.jpg", "wb+");
        if (fp) {
            CAMERALOG("###### dump thumnail ######");
            fwrite(data, picture_ack.picture_size, 1, fp);
            fflush(fp);
            fclose(fp);
        }
#endif
        char moment_guid[32] = {0x00};
        snprintf(moment_guid, 32, "%lld", _momentsID);
        ret = sendData(VIDIT_THUMBNAIL_JPG,
            (char *)data, picture_ack.picture_size,
            (const char *)moment_guid);
        delete [] data;
        if (ret < 0){
            CAMERALOG("Send error, ret = %d", ret);
            _closeVDBCMDPort(_socket_VDB_CMD);
            return -1;
        }
    }

    int remaining = 128 + sizeof(ack) + ack.extra_bytes - bytes_received;
    if (remaining > 0) {
        ret = recv(_socket_VDB_CMD, ackBuffer, remaining, 0);
        CAMERALOG("ret = %d, remaining = %d, ack.extra_bytes = %d",
            ret, remaining, ack.extra_bytes);
    }

    _closeVDBCMDPort(_socket_VDB_CMD);
    return 0;
}

void VdbUploader::uploadMarkedClip()
{
    int ret = 0;

    _state = STATE_IDLE;
    ret = getMarkedClip();
    if (ret != 0) {
        return;
    }

    _state = STATE_GOT_CLIP;
    ret = getClipUrl();
    if (ret != 0) {
        return;
    }

    CAMERALOG("_clipUrl = %s", _clipUrl);
    _state = STATE_GOT_URL;
    const char *url = _clipUrl;
    ret = getContentLength(url);
    if (ret == 0) {
        return;
    }

    if (_pListener) {
        _pListener->onState(UploadListener::STATE_LOGIN);
    }

    ret = login();
    if (_state != STATE_LOGININ) {
        CAMERALOG("login failed");
        goto EXIT;
    }

    ret = applyForUpload();
    if (_state != STATE_APPLY_SUCCESS) {
        CAMERALOG("apply for upload failed");
        goto EXIT;
    }

    CAMERALOG("connect server %s:%d", _serverIP, _serverPort);
    if (Connect(_serverIP, (unsigned short)_serverPort) == false) {
        CAMERALOG("connect server(%s) port(%d) failed", _serverIP, _serverPort);
        goto EXIT;
    }

    ret = handshake();
    if (_state != STATE_HANDSHAKE_SUCCESS) {
        CAMERALOG("hand shakes failed");
        goto EXIT;
    }

    ret = momentDesc();
    if (_state != STATE_SENT_DESCRIPTION) {
        CAMERALOG("sent moment description failed");
        goto EXIT;
    }

    ret = startUpload(0);
    if (ret != 0) {
        CAMERALOG("start upload failed");
        goto EXIT;
    }

    ret = sendThumnail();
    if (ret < 0) {
        CAMERALOG("send thumnail failed");
        goto EXIT;
    }

    ret = stopUpload(0);
    if (ret != 0) {
        CAMERALOG("upload thumnail failed");
        goto EXIT;
    }

    ret = startUpload(1);
    if (ret != 0) {
        CAMERALOG("start upload failed");
        goto EXIT;
    }

    _bytesUploaded = 0;
    _bytesDownloaded = 0;
    mUploadSeqNum_ = 0;
    downloadData(url);
    CAMERALOG("finish sending data, "
        "_bytesDownloaded = %d, _bytesUploaded = %d, _contentLength = %ld",
        _bytesDownloaded, _bytesUploaded, _contentLength);

    ret = stopUpload(1);
    if (ret != 0) {
        CAMERALOG("upload clip data failed");
        goto EXIT;
    }

    if (_pListener) {
        _pListener->onState(UploadListener::STATE_SUCCESS);
    }

    return;

EXIT:
    if (_pListener) {
        _pListener->onState(UploadListener::STATE_FAILED);
    }
    return;
}


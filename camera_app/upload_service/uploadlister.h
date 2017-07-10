
#ifndef __CLASS_UploadListener_H__
#define __CLASS_UploadListener_H__

class UploadListener
{
public:
    enum UPLOAD_STATE {
        STATE_IDLE = 0,
        STATE_LOGIN,
        STATE_UPLOADING,
        STATE_SUCCESS,
        STATE_FAILED,
    };

    virtual ~UploadListener() {}

    virtual void onState(int state) = 0;
    virtual void onProgress(int percentage) = 0;
};

#endif

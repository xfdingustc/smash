

#ifndef __LED_INDICATOR_H__
#define __LED_INDICATOR_H__

#include "Class_PropsEventThread.h"

class CLedIndicator  : public CPropertyEventHandler
{
public:
    enum SoundEffect {
        SE_Button = 0,
        SE_SingleShot,
        SE_ContinousShot,
        SE_Num,
    };

    static void Create();
    static CLedIndicator* getInstance();
    static void releaseInstance();

    static void IndicatorTimer(void*);

    virtual void EventPropertyUpdate(changed_property_type state);

    void recStatusChanged(bool recording);
    void blueOneshot();

private:
    CLedIndicator();
    virtual ~CLedIndicator();

    void _updateLedDisplay();

    static CLedIndicator *pInstance;

    CMutex      *pMutex;
    bool        _bRecording;
    bool        _bLCDOn; // during screen saver, LCD is off
};

#endif

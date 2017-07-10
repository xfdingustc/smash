

#ifndef __IN_Animation_H__
#define __IN_Animation_H__

#include "Window.h"

namespace ui {

typedef void (*DRAW_FUNC_PTR)();

class Animation : public CThread {
public:
    Animation(int width, int height, DRAW_FUNC_PTR pDrawHandler);

    virtual ~Animation();

    void startAnimation(
        Window *toShow,
        Window *toHide,
        int durationMs,
        int frames,
        int animType = Anim_Right2LeftEnter);

    virtual void main();

protected:
    CSemaphore     *_pWaitEvent;
    CMutex         *_pLock;

    int            _viewportWidth;
    int            _viewportHeight;

    int            _animType;
    int            _animDurationMs;
    int            _animFrames;
    Window         *_wndToShow;
    Window         *_wndToHide;
    DRAW_FUNC_PTR  _pDrawHandler;
};

};

#endif

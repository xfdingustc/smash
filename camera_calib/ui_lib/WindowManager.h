/*******************************************************************************
**       Canvas.h
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Use of Transee Design code is governed by terms and conditions
**      stated in the accompanying licensing statement.
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a  8- 9-2011 linn song CREATE AND MODIFY
**
*******************************************************************************/

#ifndef __IN_WindowManager_h__
#define __IN_WindowManager_h__


#include <linux/fb.h>
#include <sys/ioctl.h>

#include "linux.h"
#include "Window.h"
#include "WindowFactory.h"

namespace ui {

#define maxWndNum 20

class Animation;

class WindowManager
{
public:
    static WindowManager *getInstance() { return _pInstance; }
    static void Create(WindowFactory *factory);
    static void Destroy();

    int AddWnd(Window* pWnd);
    bool OnEvent(CEvent* event);

    Window* GetCurrentWnd();

    void onWndUpdate();

    void StartWnd(const char *name, int animType);
    void CloseWnd(Window *pWnd, int animType);

    bool getWindowInfo(int &width, int &height, int &bitdepth)
    {
        if (_fb > 0) {
            width = _vinfo.xres;
            height = _vinfo.yres;
            bitdepth = _vinfo.bits_per_pixel;
            return true;
        } else {
            return false;
        }
    }

    void onFinishAnim();

private:
    static void FB565DrawHandler();

    WindowManager(WindowFactory *factory);
    ~WindowManager();

    bool OpenFb();

    bool compose();

    static WindowManager *_pInstance;

    WindowFactory *_wndFactory;

    Window  *_ppWndList[maxWndNum];
    int     _wndNum;
    Window  *_ppVisibleList[maxWndNum];
    int     _visibleNum;

    CMutex  *_lock;

    static int    _fb;
    static char   *_fbm;
    static struct fb_var_screeninfo _vinfo;

    UINT32 _screensize;
    bool   _bDualFB;
    char   *_framebuffer;
    int    _postCounter;
    struct fb_fix_screeninfo _finfo;

    Animation *_pAnimation;
    bool      _bInAnim;
};

};

#endif

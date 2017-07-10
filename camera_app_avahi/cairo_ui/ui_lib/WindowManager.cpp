

#include "ClinuxThread.h"
#include "WindowManager.h"
#include "Animation.h"
#include "App.h"

namespace ui {

WindowManager* WindowManager::_pInstance = NULL;
int WindowManager::_fb = -1;
char* WindowManager::_fbm = 0;
fb_var_screeninfo WindowManager::_vinfo;

void WindowManager::Create(WindowFactory *factory)
{
    if (_pInstance == NULL) {
        _pInstance = new WindowManager(factory);
    }
}

void WindowManager::Destroy()
{
    if (_pInstance != NULL) {
        CAMERALOG("delete ui WindowManager 0");
        delete _pInstance;
    }
}

void WindowManager::FB565DrawHandler()
{
    _pInstance->_lock->Take();

    // TODO: compose all visible windows
    _pInstance->compose();

    // notice FB to update here
    ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
    //CAMERALOG("--fb infor after pan : \n\tYoffset:%d\n\tXoffset:%d",
    //    _vinfo.yoffset, _vinfo.xoffset);

    _pInstance->_postCounter++;
    if (_pInstance->_bDualFB) {
        if (_pInstance->_postCounter % 2) {
            if (_vinfo.xres_virtual / _vinfo.xres >= 2) {
                _vinfo.xoffset = _vinfo.xres;
            } else if (_vinfo.yres_virtual / _vinfo.yres >= 2) {
                _vinfo.yoffset = _vinfo.yres;
            }
            _pInstance->_framebuffer = _fbm + _pInstance->_screensize;
        } else {
            if (_vinfo.xres_virtual / _vinfo.xres >= 2) {
                _vinfo.xoffset = 0;
            } else if (_vinfo.yres_virtual / _vinfo.yres >= 2) {
                _vinfo.yoffset = 0;
            }
            _pInstance->_framebuffer = _pInstance->_fbm;
        }
    }

    _pInstance->_lock->Give();
}

WindowManager::WindowManager(WindowFactory *factory)
    : _wndFactory(factory)
    , _wndNum(0)
    , _visibleNum(0)
    , _bDualFB(false)
    , _framebuffer(NULL)
    , _postCounter(0)
    , _bInAnim(false)
{
    _lock = new CMutex("WindowManager mutex");

    OpenFb();

    _pAnimation = new Animation(_vinfo.xres, _vinfo.yres, FB565DrawHandler);
    _pAnimation->Go();
}

WindowManager::~WindowManager()
{
    CAMERALOG("--~WindowManager 1");
    for (int i = 0; i < _wndNum; i++) {
        if (_ppWndList[i] != 0) {
            CAMERALOG("-- delete wnd: %s", _ppWndList[i]->getWndName());
            delete _ppWndList[i];
        }
    }

    _pAnimation->Stop();
    delete _pAnimation;
    delete _wndFactory;
    delete _lock;

    munmap(_fbm, _screensize);
    if (_fb > 0) {
        close(_fb);
    }

    CAMERALOG("--~WindowManager destroyed");
}

bool WindowManager::compose()
{
    if (_visibleNum < 1) {
        CAMERALOG("No window is visible");
        return true;
    }

    for (int i = 0; i < _visibleNum; i++) {
        if (_ppVisibleList[i]->isWindowVisible()) {
            Window *wnd = _ppVisibleList[i];
            Size wndSize = wnd->getWindowSize();
            Point leftTop;
            Size visibleSize;
            wnd->getVisibleArea(leftTop, visibleSize);

            wnd->GetCanvas()->LockCanvas();

            char *src = NULL;
            src = (char *)wnd->GetCanvas()->GetBuffer();
            int bytesDepth = _vinfo.bits_per_pixel / 8;

            Point vLeftTopFB;
            Point vLeftTopWnd;
            // identify left top of visible area in window,
            // and left top of destination in framebuffer
            if (visibleSize.width == _vinfo.xres) {
                vLeftTopFB.x = 0;
                vLeftTopWnd.x = leftTop.x;
            } else {
                if (leftTop.x < 0) {
                    vLeftTopFB.x = 0;
                    vLeftTopWnd.x = -1 * leftTop.x;
                } else {
                    vLeftTopFB.x = leftTop.x;
                    vLeftTopWnd.x = 0;
                }
            }

            if (visibleSize.height == _vinfo.yres) {
                vLeftTopFB.y = 0;
                vLeftTopWnd.y = leftTop.y;
            } else {
                if (leftTop.y < 0) {
                    vLeftTopFB.y = 0;
                    vLeftTopWnd.y = -1 * leftTop.y;
                } else {
                    vLeftTopFB.y = leftTop.y;
                    vLeftTopWnd.y = 0;
                }
            }

            for (unsigned int i = 0; i < visibleSize.height; i++) {
                char *dest = _framebuffer +
                    ((i + vLeftTopFB.y) * _vinfo.xres + vLeftTopFB.x) * bytesDepth;
                char *source = src +
                    ((i + vLeftTopWnd.y) * wndSize.width + vLeftTopWnd.x )* bytesDepth;
                memcpy(dest, source, visibleSize.width * bytesDepth);
            }

            wnd->GetCanvas()->UnlockCanvas();
        }
    }

    return true;
}

bool WindowManager::OpenFb()
{
    _fb = open("/dev/fb0", O_RDWR);
    if (!_fb) {
        CAMERALOG("Error: cannot open framebuffer device.");
    }

    if (_fb > 0) {
        // Get fixed screen information
        if (ioctl(_fb, FBIOGET_FSCREENINFO, &_finfo)) {
            CAMERALOG("Error reading fixed information.");
        }

        if (ioctl(_fb, FBIOGET_VSCREENINFO, &_vinfo)) {
            CAMERALOG("Error reading variable information.");
        }

        _screensize = _vinfo.xres * _vinfo.yres * _vinfo.bits_per_pixel / 8;
        _bDualFB = (_vinfo.xres_virtual / _vinfo.xres >= 2)
            || (_vinfo.yres_virtual / _vinfo.yres >= 2);

        CAMERALOG("open fb success: xres %d, yres %d, "
            "xres_virtual %d, yres_virtual %d, _screensize %d",
            _vinfo.xres, _vinfo.yres,
            _vinfo.xres_virtual, _vinfo.yres_virtual, _screensize);

        _fbm = (char *)mmap(0, _finfo.smem_len,
            PROT_READ | PROT_WRITE, MAP_SHARED, _fb, 0);

        memset(_fbm, 0x00, _screensize);
        ioctl(_fb, FBIOPAN_DISPLAY, &_vinfo);
        _framebuffer = _fbm;

        return true;
    } else {
        CAMERALOG("open framebuffer device failed %d.", _fb);
        return false;
    }
}

int WindowManager::AddWnd(Window* pWnd)
{
    if (_wndNum < maxWndNum) {
        _ppWndList[_wndNum] = pWnd;
        _wndNum++;
    } else {
        CAMERALOG("-- too much wnd over maxWndNum(%d) defined", maxWndNum);
    }
    return _wndNum;
}

int WindowManager::RemoveWnd(Window* pWnd)
{
    _lock->Take();

    int idx = 0;
    for (idx = 0; idx < _wndNum; idx++) {
        if (_ppWndList[idx] == pWnd) {
            break;
        }
    }
    for (int i = idx; i < _wndNum - 1; i++) {
        _ppWndList[i] = _ppWndList[i + 1];
    }
    if (idx < _wndNum) {
        _ppWndList[_wndNum - 1] = NULL;
        _wndNum--;
    }

    _lock->Give();

    return _wndNum;
}

void WindowManager::StartWnd(const char *name, int animType)
{
    _lock->Take();

    Window *pWnd = NULL;
    Window *toHide = NULL;
    int i = 0;

    // firstly, search whether the window was created, if not, create it
    for (i = 0; i < _wndNum; i++) {
        if (strcmp(name, _ppWndList[i]->getWndName()) == 0) {
            pWnd = _ppWndList[i];
            break;
        }
    }
    if (pWnd == NULL) {
        pWnd = _wndFactory->createWindow(name);
        if (pWnd == NULL) {
            CAMERALOG("Create window %s failed", name);
            _lock->Give();
            return;
        }
    }

    // secondly, push it into visible list, and adjust order if possible
    i = 0;
    for (i = 0; i < _visibleNum; i++) {
        if (strcmp(name, _ppVisibleList[i]->getWndName()) == 0) {
            break;
        }
    }
    if (i == _visibleNum) {
        // not in visible list
        if ((_visibleNum > 0) && (_ppVisibleList[_visibleNum - 1] != NULL)) {
            toHide = _ppVisibleList[_visibleNum - 1];
        }
        _ppVisibleList[_visibleNum] = pWnd;
        _visibleNum++;
    } else {
        if (_visibleNum > 0) {
            // already in visible list
            toHide = _ppVisibleList[_visibleNum - 1];
            for (int j = i; j < _visibleNum - 1; j++) {
                _ppVisibleList[j] = _ppVisibleList[j + 1];
            }
        }
        _ppVisibleList[_visibleNum - 1] = pWnd;
    }

    _lock->Give();

    if (toHide && (toHide != pWnd)) {
        _bInAnim = true;
        pWnd->onStart();
        toHide->onPause();
        _pAnimation->startAnimation(pWnd, toHide, 160, 16, animType);
    } else {
        // It is the very first window, show it immediately.
        pWnd->onStart();
        pWnd->onResume();
    }
}

void WindowManager::CloseWnd(Window *pWnd, int animType)
{
    if ((_visibleNum == 0) || (_ppVisibleList[_visibleNum - 1] != pWnd)) {
        CAMERALOG("Status Error when try to destroy window %s!!", pWnd->getWndName());
        if (_visibleNum > 0) {
            for (int i = _visibleNum - 1; i >= 0; i--) {
                CAMERALOG("  ##stack visible windows: %s", _ppVisibleList[i]->getWndName());
            }
        } else {
            CAMERALOG("  ##no visible window when try to close %s", pWnd->getWndName());
        }
        return;
    }

    _lock->Take();

    Window *toShow = NULL;
    if (_visibleNum >= 2) {
        toShow = _ppVisibleList[_visibleNum - 2];
    }

    _lock->Give();

    pWnd->onPause();

    if (toShow) {
        _bInAnim = true;
        toShow->onStart();
        _pAnimation->startAnimation(toShow, pWnd, 160, 16, animType);
    }
}

Window* WindowManager::GetCurrentWnd() {
    if (_visibleNum == 0) {
        return NULL;
    } else {
        return _ppVisibleList[_visibleNum - 1];
    }
}

bool WindowManager::OnEvent(CEvent* event)
{
    bool rt = true;
    Window *wnd = NULL;

    _lock->Take();
    wnd = GetCurrentWnd();
    _lock->Give();

    if (wnd != NULL) {
        if (!_bInAnim) { // <-- to avoid race condition between animation thread and app thread
            // TODO: when switch, should draw cavas
            rt = wnd->processEvent(event);
        }
    }

    return rt;
}

void WindowManager::onFinishAnim()
{
    Window *wndToHide = NULL;
    Window *wndToShow = NULL;
    if (_visibleNum >= 1) {
        wndToHide = _ppVisibleList[_visibleNum - 1];
        // if after animation, the fist window in stack is invisible, remove it
        if (!_ppVisibleList[_visibleNum - 1]->isWindowVisible()) {
            _ppVisibleList[_visibleNum - 1]->onStop();
            _ppVisibleList[_visibleNum - 1] = NULL;
            _visibleNum--;
        }
        if (_visibleNum >= 1) {
            wndToShow = _ppVisibleList[_visibleNum - 1];
        }
    }
    if (wndToHide && wndToHide->isToDelete()) {
        delete wndToHide;
    }

    if (wndToShow) {
        wndToShow->onResume();
    }

    // reset flag is the last step, or animation thread and app thread meets race condition
    _bInAnim = false;
}

void WindowManager::onWndUpdate() {
    //if (!_bInAnim) {
        FB565DrawHandler();
    //} else {
        //CAMERALOG("in animation, ignore the update");
    //}
}

};


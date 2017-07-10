

#include "ClinuxThread.h"
#include "Widgets.h"
#include "WindowManager.h"
#include "Window.h"

namespace ui {

void Window::TimerCB(void *para)
{
    Window *wnd = (Window *)para;
    if (wnd) {
        wnd->onTimerEvent();
    }
}

Window::Window(const char *name, Size wndSize, Point leftTop)
    : _pMainObject(NULL)
    , _pParentApp(NULL)
    , _name(name)
    , _pCanvas(NULL)
    , _wndSize(wndSize)
    , _bVisible(false)
    , _toDelete(false)
{
    if (WindowManager::getInstance()) {
        int width = 0, height = 0, bitdepth = 0;
        WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
        _pCanvas = new CairoCanvas(wndSize.width, wndSize.height, bitdepth);
        _leftTop = leftTop;
        if (wndSize.width > width) {
            _visibleSize.width = width;
        } else {
            _visibleSize.width = wndSize.width;
        }
        if (wndSize.height > height) {
            _visibleSize.height = height;
        } else {
            _visibleSize.height = wndSize.height;
        }
        WindowManager::getInstance()->AddWnd(this);
    }
}

Window::~Window()
{
    delete _pCanvas;
    WindowManager::getInstance()->RemoveWnd(this);
}

bool Window::processEvent(CEvent* event)
{
    bool rt = true;

    // Firstly let widgets consume the event
    if (_pMainObject) {
        rt = _pMainObject->OnEvent(event);
    }

    // Secondly if event is not consumed, let window process it
    if(rt) {
        rt = OnEvent(event);
    }

    return rt;
}

void Window::onStart()
{
    _bVisible = true;
    if (this->GetMainObject()) {
        this->GetMainObject()->Draw(false);
    }
}

void Window::onResume()
{
}

void Window::onPause()
{
}

void Window::onStop()
{
}

void Window::StartWnd(const char *name, int animType)
{
    WindowManager::getInstance()->StartWnd(name, animType);
}

void Window::Close(int animType)
{
    _toDelete = true;
    WindowManager::getInstance()->CloseWnd(this, animType);
}

void Window::onSurfaceChanged() {
    WindowManager::getInstance()->onWndUpdate();
}

void Window::setVisibleArea(const Point& leftTop, const Size& size)
{
    _leftTop.x = leftTop.x;
    _leftTop.y = leftTop.y;
    _visibleSize.width = size.width;
    _visibleSize.height = size.height;

    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    if ((_leftTop.x + width <= 0) || (_leftTop.x >= width)
        || (_leftTop.y + height <= 0) || (_leftTop.y >= height))
    {
        //CAMERALOG("%s() line %d window %s become invisble",
        //    getWndName());
        _bVisible = false;
        _leftTop.x = 0;
        _leftTop.y = 0;
    } else {
        _bVisible = true;
    }
}

void Window::getVisibleArea(Point &leftTop, Size &size) {
    leftTop.x = _leftTop.x;
    leftTop.y = _leftTop.y;
    size.width = _visibleSize.width;
    size.height = _visibleSize.height;
}

bool Window::startTimer(int t_ms, bool bLoop)
{
    if (CTimerThread::GetInstance() != NULL) {
        CTimerThread::GetInstance()->ApplyTimer(
            t_ms, Window::TimerCB, this, bLoop, getWndName());
    }
    return true;
}

bool Window::cancelTimer()
{
    if (CTimerThread::GetInstance() != NULL) {
        CTimerThread::GetInstance()->CancelTimer(getWndName());
    }
    return true;
}

};


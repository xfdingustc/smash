

#include "linux.h"
#include "ClinuxThread.h"

#include "Animation.h"
#include "WindowManager.h"

namespace ui {

Animation::Animation(int width, int height, DRAW_FUNC_PTR pDrawHandler)
    : CThread("anim thread", CThread::NORMAL , 0, NULL)
    , _viewportWidth(width)
    , _viewportHeight(height)
    , _animType(Anim_Right2LeftEnter)
    , _pDrawHandler(pDrawHandler)
{
    _pWaitEvent = new CSemaphore(0, 1, "anim sem");
    _pLock = new CMutex("anim mutex");
}

Animation::~Animation()
{
    delete _pLock;
    delete _pWaitEvent;
    printf("%s() destroyed\n", __FUNCTION__);
}

void Animation::startAnimation(
    Window *toShow,
    Window *toHide,
    int durationMs,
    int frames,
    int animType)
{
    _wndToShow = toShow;
    _wndToHide = toHide;
    _animDurationMs = durationMs;
    _animFrames = frames;
    if ((animType >= Anim_Null) && (animType < Anim_Num)) {
        _animType = animType;
    }

    _pWaitEvent->Give();
}

void Animation::main(void *)
{
    while (true) {
        _pWaitEvent->Take(-1);
        if (_animType == Anim_Null) {
            // Just let window switch animation happen in this window.
            _wndToShow->setVisibleArea(
                Point(0, 0),
                Size(_viewportWidth, _viewportHeight));
            _wndToHide->setVisibleArea(
                Point(-1 * _viewportWidth, 0),
                Size(0, 0));
            _pDrawHandler();
            WindowManager::getInstance()->onFinishAnim();
        } else {
            for (int i = 1; i <= _animFrames; i++) {
                timeval _tt;
                _tt.tv_sec = 0;
                _tt.tv_usec = _animDurationMs / _animFrames  * 1000;
                select(0, NULL, NULL, NULL, &_tt);
                switch (_animType) {
                    case Anim_Right2LeftEnter:
                        // enter from right to left
                        _wndToShow->setVisibleArea(
                            Point(_viewportWidth - _viewportWidth / _animFrames * i, 0),
                            Size(_viewportWidth / _animFrames * i, _viewportHeight));
                        _wndToHide->setVisibleArea(
                            Point(-1 * _viewportWidth / _animFrames * i, 0),
                            Size(_viewportWidth - _viewportWidth / _animFrames * i, _viewportHeight));
                        break;
                    case Anim_Left2RightEnter:
                        // enter from left to right
                        _wndToShow->setVisibleArea(
                            Point(-1 * _viewportWidth + _viewportWidth / _animFrames * i, 0),
                            Size(_viewportWidth / _animFrames * i, _viewportHeight));
                        _wndToHide->setVisibleArea(
                            Point(_viewportWidth / _animFrames * i, 0),
                            Size(_viewportWidth - _viewportWidth / _animFrames * i, _viewportHeight));
                        break;
                    case Anim_Top2BottomEnter:
                        // enter from top to bottom
                        _wndToShow->setVisibleArea(
                            Point(0, -1 * _viewportHeight + _viewportHeight / _animFrames * i),
                            Size(_viewportWidth, _viewportHeight / _animFrames * i));
                        _wndToHide->setVisibleArea(
                            Point(0, _viewportHeight / _animFrames * i),
                            Size(_viewportWidth, _viewportHeight - _viewportHeight / _animFrames * i));
                        break;
                    case Anim_Bottom2TopEnter:
                        // enter from bottom to top
                        _wndToShow->setVisibleArea(
                            Point(0, _viewportHeight - _viewportHeight / _animFrames * i),
                            Size(_viewportWidth, _viewportHeight / _animFrames * i));
                        _wndToHide->setVisibleArea(
                            Point(0, -1 * _viewportHeight / _animFrames * i),
                            Size(_viewportWidth, _viewportHeight - _viewportHeight / _animFrames * i));
                        break;
                    case Anim_Bottom2TopExit:
                        // from bottom to top exit
                        _wndToShow->setVisibleArea(
                            Point(0, 0),
                            Size(_viewportWidth, _viewportHeight));
                        _wndToHide->setVisibleArea(
                            Point(0, -1 * _viewportHeight / _animFrames * i),
                            Size(_viewportWidth, _viewportHeight - _viewportHeight / _animFrames * i));
                        break;
                    case Anim_Top2BottomExit:
                        // from top to bottom exit
                        _wndToShow->setVisibleArea(
                            Point(0, 0),
                            Size(_viewportWidth, _viewportHeight));
                        _wndToHide->setVisibleArea(
                            Point(0, _viewportHeight / _animFrames * i),
                            Size(_viewportWidth, _viewportHeight - _viewportHeight / _animFrames * i));
                        break;
                }
                _pDrawHandler();
            }
            WindowManager::getInstance()->onFinishAnim();
        }
    }
}

};

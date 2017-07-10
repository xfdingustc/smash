
#ifndef __IN_CameraWndFactory_H__
#define __IN_CameraWndFactory_H__

#include "Window.h"
#include "WindowFactory.h"

class CCameraProperties;

namespace ui {

class CameraWndFactory : public WindowFactory {
public:
    CameraWndFactory(CCameraProperties *cp) : _cp(cp) {}
    virtual ~CameraWndFactory() {}

    virtual Window* createWindow(const char *name);

private:
    CCameraProperties *_cp;
};

};

#endif



#ifndef __IN_WindowFactory_H__
#define __IN_WindowFactory_H__

namespace ui {

class Window;

class WindowFactory {
public:
    WindowFactory() {}
    virtual ~WindowFactory() {}

    virtual Window* createWindow(const char *name) = 0;
};

};

#endif

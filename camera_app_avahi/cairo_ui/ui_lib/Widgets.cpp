

#include <math.h>
#include <sys/prctl.h>

#include "ClinuxThread.h"
#include "linux.h"
#include "CEvent.h"
#include "CairoCanvas.h"

#include "Widgets.h"
#include "Window.h"
#include "WindowManager.h"

#include "AudioPlayer.h"

namespace ui {

Control::Control
    (
        Container* pParent,
        const Point& leftTop,
        const Size& CSize
    ):_pParent(pParent)
    ,_leftTop(leftTop)
    ,_absPos(leftTop)
    ,_size(CSize)
    ,_property(0)
{
    if (_pParent != NULL) {
        _pParent->AddControl(this);
    }
}

Control::~Control()
{
    if (_pParent != NULL) {
        _pParent->RemoveControl(this);
    }
}

Point& Control::GetAbsPos()
{
    if (_pParent != NULL) {
        _absPos.Set(_pParent->GetAbsPos().x + _leftTop.x , _pParent->GetAbsPos().y + _leftTop.y);
    } else {
        _absPos.Set(_leftTop.x, _leftTop.y);
    }
    return _absPos;
}

Window* Control::GetOwner()
{
    if (_pParent != NULL) {
        return _pParent->GetOwner();
    } else {
        return NULL;
    }
}

CairoCanvas* Control::GetCanvas()
{
    if (_pParent != NULL) {
        return _pParent->/*GetOwner()->*/GetCanvas();
    } else {
        return NULL;
    }
}

void Control::Draw(bool bPost)
{
}

void Control::Clear()
{
    // TODO:
}

bool Control::OnEvent(CEvent *event)
{
    bool rt = true;
    if (event->_type == eventType_touch) {
        //rt = isTouchOn(event->_paload, event->_paload1);
    }
    return rt;
}

bool Control::isTouchOn(int x, int y)
{
    Point point = this->GetAbsPos();
    //CAMERALOG("isTouchOn, x : %d, posi.x : %d, y : %d, posi.y : %d ", x, point.x, y, point.y);
    if ((x >=  point.x) && ( x < point.x + _size.width)
        &&(y >= point.y) && (y < point.y + _size.height))
    {
        return true;
    } else {
        return false;
    }
}


Container::Container
    (
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& CSize,
        UINT16 maxChildren
    ) : Control(pParent, leftTop, CSize)
    ,_pOwner(pOwner)
    ,_pControlList(NULL)
    ,_number(maxChildren)
    ,_count(0)
{
    _pControlList = new Control* [_number];
    for (int i = 0; i < _number; i++) {
        _pControlList[i] = NULL;
    }
}

Container::~Container()
{
    int numToRelease = _count;
    for (int i = 0; i < numToRelease; i++) {
        delete _pControlList[0];
    }
    delete[] _pControlList;
}

Window* Container::GetOwner()
{
    if (GetParent() != NULL) {
        return GetParent()->GetOwner();
    } else {
        return _pOwner;
    }
}

CairoCanvas* Container::GetCanvas()
{
    if (GetParent() != NULL) {
        return GetParent()->GetCanvas();
    } else {
        return _pOwner->GetCanvas();
    }
}

bool Container::AddControl(Control* pControl)
{
    if (_count < _number) {
        *(_pControlList + _count) = pControl;
        _count++;
        return true;
    } else {
        return false;
    }
}

bool Container::RemoveControl(Control* pControl)
{
    int id = 0;
    for (int i = 0; i < _count; i++) {
        if (_pControlList[i] == pControl) {
            id = i;
            break;
        }
    }

    if (id < _count) {
        for (int i = id; i < _count - 1; i++) {
            *(_pControlList + i) = *(_pControlList + i + 1);
        }
        _pControlList[_count] = NULL;
        _count--;
        return true;
    } else {
        return false;
    }
}

Control* Container::GetControl(UINT16 index)
{
    if (index < _count) {
        return _pControlList[index];
    } else {
        return NULL;
    }
}

void Container::Draw(bool bPost)
{
    if (bPost) {
        if (this->GetParent()) {
            this->GetParent()->Draw(true);
        } else {
            this->GetOwner()->onSurfaceChanged();
        }
    } else {
        for (int i = 0; i < GetChildrenCount(); i++) {
            if ((GetControl(i) != 0) && (!GetControl(i)->IsHiden())) {
                GetControl(i)->Draw(false);
            }
        }
    }
}

bool Container::OnEvent(CEvent *event)
{
    bool ret = true;

    for (int i = 0; i < GetChildrenCount(); i ++) {
        if ((GetControl(i) != 0) && (!GetControl(i)->IsHiden())) {
            ret = GetControl(i)->OnEvent(event);
            if (!ret) {
                break;
            }
        }
    }

    return ret;
}


Panel::~Panel()
{
}

CairoCanvas* Panel::GetCanvas()
{
    CairoCanvas* pCav = NULL;
    if (_pCanvas) {
        pCav = _pCanvas;
    } else {
        pCav = this->GetOwner()->GetCanvas();
    }
    return pCav;
}

void Panel::Draw(bool bPost)
{
    CairoCanvas* pCav = GetCanvas();
    if (!pCav) {
        return;
    }
    /*CAMERALOG("%s() line %d, Panel::Draw, _bgColor = 0x%x, bPost = %d",
            _bgColor, bPost);*/

    pCav->LockCanvas();

    pCav->DrawRect(
        GetRelativePos(),
        pCav->GetSize(),
        _bgColor,
        0);

    for (int i = 0; i < GetChildrenCount(); i++) {
        if ((GetControl(i) != 0) && (!GetControl(i)->IsHiden())) {
            GetControl(i)->Draw(false);
        }
    }

    pCav->UnlockCanvas();

    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}

bool Panel::OnEvent(CEvent *event)
{
    bool rt = inherited::OnEvent(event);
    if (rt) {
        if (event->_type == eventType_key) {
        }
    }

    if (!rt) {
        //CAMERALOG("%s() line %d, ##########", __FUNCTION__, __LINE__);
        //this->GetOwner()->onSurfaceChanged();
    }
    return rt;
}


UIPoint::UIPoint(Container* pParent, const Point& leftTop, const Size& CSize)
    : inherited(pParent, leftTop, CSize)
{
}

UIPoint::~UIPoint()
{
}

void UIPoint::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetOwner()->GetCanvas();
    pCav->DrawPoint(_point, _color);
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}


UILine::UILine(Container* pParent, const Point& leftTop, const Size& CSize)
    : inherited(pParent, leftTop, CSize)
    , _bDash(false)
{
}

UILine::~UILine()
{
}

void UILine::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    Point tplft = GetAbsPos();
    if (_bDash) {
        pCav->DrawDashLine(
            tplft,
            Point((_end.x - _start.x) + tplft.x, (_end.y - _start.y) + tplft.y),
            _width, _color);
    } else {
        pCav->DrawLine(
            tplft,
            Point((_end.x - _start.x) + tplft.x, (_end.y - _start.y) + tplft.y),
            _width, _color);
    }
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}

UIRectangle::UIRectangle(
    Container* pParent,
    const Point& leftTop,
    const Size& CSize,
    COLOR color)
  : inherited(pParent, leftTop, CSize)
  , _cornerRaidus(0)
{
    _start = leftTop;
    _end.x = leftTop.x + CSize.width;
    _end.y = leftTop.y + CSize.height;
    _color = color;
}

UIRectangle::~UIRectangle()
{
}

void UIRectangle::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    Point tplft = GetAbsPos();
    if (_cornerRaidus > 0) {
        pCav->DrawRect(Point(tplft.x, tplft.y),
            Size((_end.x - _start.x), (_end.y - _start.y)),
            _color,
            _cornerRaidus);
    } else {
        pCav->DrawRect(Point(tplft.x, tplft.y),
            Point(tplft.x + (_end.x - _start.x), tplft.y + (_end.y - _start.y)),
            _color,
            0);
    }
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}


TranslucentRect::TranslucentRect(
    Container* pParent,
    const Point& leftTop,
    const Size& CSize)
  : inherited(pParent, leftTop, CSize)
  , _color1(0x00)
  , _color2(0x00)
  , _alpha1(1.0)
  , _alpha2(1.0)
  , _bHorizontal(true)
{
}

TranslucentRect::~TranslucentRect()
{
}

void TranslucentRect::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetOwner()->GetCanvas();
    Point tplft = GetAbsPos();
    pCav->DrawRectGradient(tplft, GetSize(),
        _color1, _alpha1,
        _color2, _alpha2
        );
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}


Circle::Circle
    (
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 borderWidth
        , UINT16 color
    )
    : inherited(pParent,
        Point(center.x, center.y),
        Size(radius * 2, radius * 2))
    , _center(center)
    , _radius(radius)
    , _borderWidth(borderWidth)
    , _color(color)
{
}

Circle::~Circle()
{
}

void Circle::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    Point tplft = GetAbsPos();
    pCav->DrawCircle(tplft, _radius, _borderWidth, _color);
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}


Arc::Arc
    (
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 borderWidth
        , UINT16 color
    )
    : inherited(pParent,
        Point(center.x, center.y),
        Size(radius * 2, radius * 2))
    , _center(center)
    , _radius(radius)
    , _borderWidth(borderWidth)
    , _color(color)
    , _start_angle(0.0)
    , _end_angle(0.0)
{
}

Arc::~Arc()
{
}

void Arc::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    Point tplft = GetAbsPos();
    pCav->DrawArc(tplft, _radius, _borderWidth, _color, _start_angle, _end_angle);
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}


StaticText::StaticText(Container* pParent, const Point& leftTop, const Size& CSize)
    : inherited(pParent, leftTop, CSize)
    , _pText(NULL)
    , _fonttype(FONT_ROBOTOMONO_Medium)
    , _line_height(0)
    , _alpha(1.0)
{
}

StaticText::~StaticText()
{
}

void StaticText::Draw(bool bPost) {
    //CairoCanvas* pCav = this->GetOwner()->GetCanvas();
    CairoCanvas* pCav = this->GetCanvas();
    if (_pText && (strlen((char *)_pText) > 0)) {
        pCav->DrawText((const char *)_pText, (FONT)_fonttype, _txtSize, _txtColor,
            GetAbsPos(), GetSize(), _hAlign, _vAlign, _alpha, _line_height);
    }
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}


Round::Round
    (
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 color
    )
    : inherited(pParent,
        Point(center.x, center.y),
        Size(radius * 2, radius * 2))
    , _center(center)
    , _radius(radius)
    , _color(color)
    , _alpha(1.0)
{

}

Round::~Round()
{

}

void Round::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    Point tplft = GetAbsPos();
    pCav->DrawRound(tplft, _radius, _color, _alpha);
    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}


Fan::Fan(
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 fan_color
    )
    : inherited(pParent,
        Point(center.x, center.y),
        Size(radius * 2, radius * 2))
    , _center(center)
    , _radius(radius)
    , _fan_color(fan_color)
    , _start_angle(0.0)
    , _end_angle(0.0)
{

}

Fan::~Fan()
{

}

void Fan::setAngles(float start_angle, float end_angle) {
    _start_angle = start_angle;
    _end_angle = end_angle;
}

void Fan::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    if (pCav) {
        Point tplft = GetAbsPos();
        pCav->DrawFan(tplft, _radius, _start_angle, _end_angle, _fan_color);
        if (bPost) {
            this->GetOwner()->onSurfaceChanged();
        }
    }
}


FanRing::FanRing(
    Container* pParent
    , const Point& center
    , UINT16 radius_outer
    , UINT16 radius_inner
    , UINT16 fan_color
    , UINT16 bg_color
    )
    : inherited(pParent,
        Point(center.x - radius_outer, center.y - radius_outer),
        Size(radius_outer * 2, radius_outer * 2))
    , _center(center)
    , _radius_outer(radius_outer)
    , _radius_inner(radius_inner)
    , _fan_color(fan_color)
    , _bg_color(bg_color)
    , _start_angle(0.0)
    , _end_angle(0.0)
{

}

FanRing::~FanRing()
{

}

void FanRing::setAngles(float start_angle, float end_angle) {
    _start_angle = start_angle;
    _end_angle = end_angle;
}

void FanRing::setRadius(UINT16 radius_outer, UINT16 radius_inner)
{
    _radius_outer = radius_outer;
    _radius_inner = radius_inner;
}

void FanRing::setColor(UINT16 fan_color, UINT16 bg_color)
{
    _fan_color = fan_color;
    _bg_color = bg_color;
}

void FanRing::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    if (pCav) {
        //pCav->DrawRing(_center, _radius_outer, _radius_inner, _bg_color);
        pCav->DrawFanRing(
            _center,
            _radius_outer,
            _radius_inner,
            _start_angle,
            _end_angle,
            _fan_color);
        if (bPost) {
            this->GetOwner()->onSurfaceChanged();
        }
    }
}


RoundCap::RoundCap(
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 color
    )
    : inherited(pParent,
        Point(center.x, center.y),
        Size(radius * 2, radius * 2))
    , _center(center)
    , _radius(radius)
    , _color(color)
    , _angle(0.0)
    , _ratio(1.0)
{
}

RoundCap::~RoundCap()
{
}

void RoundCap::setAngle(float angle) {
    _angle = ANGLE(angle);
}

void RoundCap::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    if (pCav) {
        float theta = asin(1 - _ratio);
        float angle1 = ANGLE(360) - theta + _angle;
        float angle2 = ANGLE(180) + theta + _angle;
        Point tplft = GetAbsPos();
        pCav->DrawCap(tplft, _radius, angle1, angle2, _color);
        if (bPost) {
            this->GetOwner()->onSurfaceChanged();
        }
    }
}

DrawingBoard::DrawingBoard(
    Container* pParent,
    const Point& leftTop,
    const Size& CSize,
    UINT16 color)
  : inherited(pParent, leftTop, CSize)
  , _brushColor(color)
{
}

DrawingBoard::~DrawingBoard()
{
}

void DrawingBoard::Draw(bool bPost)
{
    // TODO:
}

void DrawingBoard::drawPoint(const Point& point)
{
    CairoCanvas* pCav = this->GetOwner()->GetCanvas();
    pCav->DrawPoint(point, _brushColor);
    this->GetOwner()->onSurfaceChanged();
}

void DrawingBoard::clearBoard()
{
    CairoCanvas* pCav = this->GetOwner()->GetCanvas();
    pCav->DrawRect(
        GetRelativePos(),
        GetSize(),
        0x1863);
    this->GetOwner()->onSurfaceChanged();
}

Button::Button(
    Container* pParent,
    const Point& leftTop, const Size& CSize,
    UINT16 colorNormal, UINT16 colorPressed)
  : inherited(pParent, leftTop, CSize)
  , _listener(NULL)
  , _bPressed(false)
  , _btnColorNormal(colorNormal)
  , _btnColorPressed(colorPressed)
  , _bWithBorder(false)
  , _btnColorBorder(0x0000)
  , _btnWidthBorder(1)
  , _pText(NULL)
  , _fonttype(FONT_ROBOTOMONO_Medium)
  , _line_height(0)
  , _hAlign(CENTER)
  , _vAlign(MIDDLE)
  , _padding(4)
  , _cornerRadius(5)
  , _alpha(1.0)
{
}

Button::~Button()
{
}

void Button::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    Point tplft = GetAbsPos();
    if (_bPressed) {
        pCav->DrawRect(
            tplft,
            GetSize(),
            _btnColorPressed,
            _cornerRadius,
            _alpha);
    } else {
        pCav->DrawRect(
            tplft,
            GetSize(),
            _btnColorNormal,
            _cornerRadius,
            _alpha);
    }
    // if hightlight border, draw it separately
    if (_bWithBorder) {
        pCav->DrawRectStroke(
            tplft,
            GetSize(),
            _btnColorBorder,
            _btnWidthBorder,
            _cornerRadius,
            _alpha);
    }
    if (_pText) {
        pCav->DrawText(
            (const char *)_pText, (FONT)_fonttype, _txtSize, _txtColor,
            tplft, GetSize(),
            _hAlign, _vAlign, 1.0, _line_height);
    }

    if (bPost) {
        this->GetParent()->Draw(true);
    }
}

bool Button::OnEvent(CEvent *event) {
    bool b = true;

    if (event->_type == eventType_touch)
    {
        switch (event->_event) {
            case TouchEvent_SingleClick:
                {
                    int x = event->_paload;
                    int y = event->_paload1;

                    Point pos = GetRelativePos();
                    Size size = GetSize();
                    /*CAMERALOG("TouchEvent_SingleClick point(%d, %d) is "
                        "clicked, button area: (%d, %d) -> (%d, %d)",
                        x, y,
                        pos.x, pos.y,
                        pos.x +size.width, pos.y + size.height);*/
                    if ((x >= pos.x) && (x <= pos.x + size.width)
                        && (y >= pos.y) && (y <= pos.y + size.height))
                    {
                        _bPressed = true;
                        Draw(true);
                        usleep(10 * 1000);
                        _bPressed = false;
                        Draw(true);

                        CAudioPlayer::getInstance()->playSoundEffect(
                            CAudioPlayer::SE_Click);

                        if (_listener) {
                            _listener->onClick(this);
                        }
                        b = false;
                    }
                }
                break;
#if 0
            case TouchEvent_OnPressed:
                {
                    int x = event->_paload;
                    int y = event->_paload1;

                    Point pos = GetRelativePos();
                    Size size = GetSize();
                    /*CAMERALOG("TouchEvent_OnPressed point(%d, %d) is "
                        "clicked, button area: (%d, %d) -> (%d, %d)\n",
                        x, y,
                        pos.x, pos.y,
                        pos.x +size.width, pos.y + size.height);*/
                    if ((x >= pos.x) && (x <= pos.x + size.width)
                        && (y >= pos.y) && (y <= pos.y + size.height))
                    {
                        //CAMERALOG("%s() line %d, pressed", __FUNCTION__, __LINE__);
                        _bPressed = true;
                        Draw(true);

                        b = false;
                    }
                }
                break;
            case TouchEvent_OnReleased:
                {
                    if (_bPressed) {
                        int x = event->_paload;
                        int y = event->_paload1;

                        Point pos = GetRelativePos();
                        Size size = GetSize();
                        /*CAMERALOG("TouchEvent_OnReleased point(%d, %d) is "
                            "clicked, button area: (%d, %d) -> (%d, %d)\n",
                            x, y,
                            pos.x, pos.y,
                            pos.x +size.width, pos.y + size.height);*/
                        if ((x >= pos.x) && (x <= pos.x + size.width)
                            && (y >= pos.y) && (y <= pos.y + size.height))
                        {
                            //CAMERALOG("%s() line %d, released", __FUNCTION__, __LINE__);
                            _bPressed = false;
                            Draw(true);
                        }

                        b = false;
                    }
                }
                break;
            case TouchEvent_coordinate:
                {
                    if (_bPressed) {
                        int x = event->_paload;
                        int y = event->_paload1;

                        Point pos = GetRelativePos();
                        Size size = GetSize();

                        if ((x < pos.x) || (x > pos.x + size.width)
                            || (y < pos.y) || (y > pos.y + size.height))
                        {
                            _bPressed = false;
                            Draw(true);
                        }

                        b = false;
                    }
                }
                break;
#endif
        }
    }

    return b;
}

PngImage::PngImage(Container* pParent, const Point& leftTop, const Size& CSize, char *path)
    : inherited(pParent, leftTop, CSize)
    , _imagePath(path)
{
}

PngImage::~PngImage()
{
}

void PngImage::Draw(bool bPost)
{
    CairoCanvas* pCav = this->GetCanvas();
    pCav->DrawPng(
        _imagePath,
        GetRelativePos(),
        GetSize()
        );
}

BmpImage::BmpImage(
        Container* pParent,
        const Point& leftTop,
        const Size& size,
        const char *path,
        const char *bmpName
    )
    : inherited(pParent, leftTop, size)
    , _pBmp(NULL)
{
    snprintf(_dirName, 128, "%s", path);
    snprintf(_fileName, 128, "%s", bmpName);
    _pBmp = new CBmp(path, bmpName);
}
BmpImage::~BmpImage()
{
    delete _pBmp;
}

void BmpImage::setSource(const char *path, const char *bmpName) {
    if ((strcmp(_dirName, path) == 0)
        && (strcmp(_fileName, bmpName) == 0))
    {
        return;
    }

    if (_pBmp) {
        delete _pBmp;
    }
    snprintf(_dirName, 128, "%s", path);
    snprintf(_fileName, 128, "%s", bmpName);
    _pBmp = new CBmp(path, bmpName);
}

void BmpImage::Draw(bool bHilight)
{
    Point tplft = GetAbsPos();
    Point posi;
    Size size = this->GetSize();
    CairoCanvas* pCav = this->GetCanvas();
    Size bmpSize = Size(_pBmp->getWidth(), _pBmp->getHeight());
    posi.x = tplft.x + (size.width - bmpSize.width) / 2;
    posi.y = tplft.y + (size.height - bmpSize.height) /2 ;
    pCav->DrawBmp(posi, bmpSize, _pBmp);
}


ImageButton::ImageButton(
        Container* pParent,
        const Point& leftTop,
        const Size& size,
        const char *path,
        const char *bmpNormal,
        const char *bmpPressed
    )
    : inherited(pParent, leftTop, size)
    , _pBmpNormal(NULL)
    , _pBmpPressed(NULL)
    , _bPressed(false)
{
    snprintf(_dirName, 128, "%s", path);
    snprintf(_fileNormal, 128, "%s", bmpNormal);
    snprintf(_filePressed, 128, "%s", bmpPressed);

    _pBmpNormal = new CBmp(path, bmpNormal);
    _pBmpPressed = new CBmp(path, bmpPressed);
}

ImageButton::~ImageButton()
{
    delete _pBmpNormal;
    delete _pBmpPressed;
}

void ImageButton::setSource(const char *path,
    const char *bmpNormal,
    const char *bmpPressed)
{
    if (strcmp(_fileNormal, bmpPressed) != 0)
    {
        if (_pBmpNormal) {
            delete _pBmpNormal;
        }
        snprintf(_dirName, 128, "%s", path);
        snprintf(_fileNormal, 128, "%s", bmpNormal);
        _pBmpNormal = new CBmp(path, _fileNormal);
    }

    if (strcmp(_filePressed, bmpPressed) != 0)
    {
        if (_pBmpPressed) {
            delete _pBmpPressed;
        }
        snprintf(_dirName, 128, "%s", path);
        snprintf(_filePressed, 128, "%s", bmpPressed);
        _pBmpPressed = new CBmp(path, _filePressed);
    }
}

void ImageButton::Draw(bool bPost)
{
    CBmp *bmp = _pBmpNormal;
    if (_bPressed) {
        bmp = _pBmpPressed;
    }

    Point tplft = GetAbsPos();
    Point posi;
    Size size = this->GetSize();
    CairoCanvas* pCav = this->GetCanvas();
    Size bmpSize = Size(bmp->getWidth(), bmp->getHeight());
    posi.x = tplft.x + (size.width - bmpSize.width) / 2;
    posi.y = tplft.y + (size.height - bmpSize.height) /2 ;
    pCav->DrawBmp(posi, bmpSize, bmp);

    if (bPost) {
        this->GetParent()->Draw(true);
    }
}

bool ImageButton::OnEvent(CEvent *event)
{
    bool b = true;

    if (event->_type == eventType_touch)
    {
        switch (event->_event) {
            case TouchEvent_SingleClick:
                {
                    int x = event->_paload;
                    int y = event->_paload1;

                    Point pos = GetRelativePos();
                    Size size = GetSize();
                    /*CAMERALOG("TouchEvent_SingleClick point(%d, %d) is "
                        "clicked, button area: (%d, %d) -> (%d, %d)\n",
                        x, y,
                        pos.x, pos.y,
                        pos.x +size.width, pos.y + size.height);*/
                    if ((x >= pos.x) && (x <= pos.x + size.width)
                        && (y >= pos.y) && (y <= pos.y + size.height))
                    {
                        _bPressed = true;
                        Draw(true);
                        usleep(10 * 1000);
                        _bPressed = false;
                        Draw(true);

                        CAudioPlayer::getInstance()->playSoundEffect(
                            CAudioPlayer::SE_Click);

                        if (_listener) {
                            _listener->onClick(this);
                        }
                        b = false;
                    }
                }
                break;
#if 0
            case TouchEvent_OnPressed:
                {
                    int x = event->_paload;
                    int y = event->_paload1;

                    Point pos = GetRelativePos();
                    Size size = GetSize();
                    /*CAMERALOG("TouchEvent_OnPressed point(%d, %d) is "
                        "clicked, button area: (%d, %d) -> (%d, %d)\n",
                        x, y,
                        pos.x, pos.y,
                        pos.x +size.width, pos.y + size.height);*/
                    if ((x >= pos.x) && (x <= pos.x + size.width)
                        && (y >= pos.y) && (y <= pos.y + size.height))
                    {
                        //CAMERALOG("%s() line %d, pressed", __FUNCTION__, __LINE__);
                        _bPressed = true;
                        Draw(true);

                        b = false;
                    }
                }
                break;
            case TouchEvent_OnReleased:
                {
                    if (_bPressed) {
                        int x = event->_paload;
                        int y = event->_paload1;

                        Point pos = GetRelativePos();
                        Size size = GetSize();
                        /*CAMERALOG("TouchEvent_OnReleased point(%d, %d) is "
                            "clicked, button area: (%d, %d) -> (%d, %d)\n",
                            x, y,
                            pos.x, pos.y,
                            pos.x +size.width, pos.y + size.height);*/
                        if ((x >= pos.x) && (x <= pos.x + size.width)
                            && (y >= pos.y) && (y <= pos.y + size.height))
                        {
                            //CAMERALOG("%s() line %d, released", __FUNCTION__, __LINE__);
                            _bPressed = false;
                            Draw(true);
                        }

                        b = false;
                    }
                }
                break;
            case TouchEvent_coordinate:
                {
                    if (_bPressed) {
                        int x = event->_paload;
                        int y = event->_paload1;

                        Point pos = GetRelativePos();
                        Size size = GetSize();

                        if ((x < pos.x) || (x > pos.x + size.width)
                            || (y < pos.y) || (y > pos.y + size.height))
                        {
                            _bPressed = false;
                            Draw(true);
                        }

                        b = false;
                    }
                }
                break;
#endif
        }
    }

    return b;
}


TabIndicator::TabIndicator(
        Container* pParent,
        const Point& leftTop,
        const Size& size,
        int num,
        int hightlight_idx,
        const char *path,
        const char *bmpActive,
        const char *bmpInactive
    )
    : inherited(pParent, leftTop, size)
    , _radius(4)
    , _colorActive(0x9CF3)
    , _colorInactive(0x3187)
    , _pBmpActive(NULL)
    , _pBmpInactive(NULL)
    , _num(num)
    , _indexHighlight(hightlight_idx)
{
    _pBmpActive = new CBmp(path, bmpActive);
    _pBmpInactive = new CBmp(path, bmpInactive);
}

TabIndicator::TabIndicator(
        Container* pParent,
        const Point& leftTop,
        const Size& size,
        int num,
        int hightlight_idx,
        int radius,
        COLOR colorActive,
        COLOR colorInactive)
    : inherited(pParent, leftTop, size)
    , _radius(radius)
    , _colorActive(colorActive)
    , _colorInactive(colorInactive)
    , _pBmpActive(NULL)
    , _pBmpInactive(NULL)
    , _num(num)
    , _indexHighlight(hightlight_idx)
{
}

TabIndicator::~TabIndicator()
{
    delete _pBmpActive;
    delete _pBmpInactive;
}

void TabIndicator::Draw(bool bHilight)
{
    Point tplft = GetAbsPos();
    Size size = this->GetSize();

    UINT16 firstX, firstY;
    firstY = tplft.y + size.height / 2;
    if (_pBmpInactive != NULL) {
        firstX = tplft.x + size.width / 2
            - (_pBmpInactive->getWidth() * _num + 8 * (_num - 1)) / 2;
    } else {
        firstX = tplft.x + size.width / 2 - (_num * _radius * 2 - _radius);
    }

    CairoCanvas* pCav = this->GetCanvas();
    for (int i = 0; i < _num; i++) {
        if ((_pBmpActive != NULL) && (_pBmpInactive != NULL)) {
            if (i == _indexHighlight) {
                Point tplft(firstX + i * (_pBmpInactive->getWidth() + 8), firstY);
                Size bmpSize(_pBmpActive->getWidth(), _pBmpActive->getHeight());
                pCav->DrawBmp(tplft, bmpSize, _pBmpActive);
            } else {
                Point tplft(firstX + i * (_pBmpInactive->getWidth() + 8), firstY);
                Size bmpSize(_pBmpInactive->getWidth(), _pBmpInactive->getHeight());
                pCav->DrawBmp(tplft, bmpSize, _pBmpInactive);
            }
        } else {
            if (i == _indexHighlight) {
                pCav->DrawRound(
                    Point(firstX + i * _radius * 4, firstY),
                    _radius,
                    _colorActive);
            } else {
                pCav->DrawRound(
                    Point(firstX + i * _radius * 4, firstY),
                    _radius,
                    _colorInactive);
            }
        }
    }
}

QrCodeControl::QrCodeControl(
        Container* pParent
        ,const Point& leftTop
        ,const Size& size
    )
    : inherited(pParent, leftTop, size)
    , _qrcode(NULL)
    , _bmpBuf(NULL)
    , _pBmp(NULL)
{
    memset(_string, 0x00, 64);
}

QrCodeControl::~QrCodeControl()
{
    QRcode_free(_qrcode);
    if (_pBmp != 0) {
        delete _pBmp;
    }
}

void QrCodeControl::createQrCodeBmp()
{
    QRecLevel level = QR_ECLEVEL_L;
    _qrcode = QRcode_encodeString8bit(_string, 0, level);
    _code_width = _qrcode->width + 2;
    //CAMERALOG("--qr code size: %d\n", _code_width);
    int m, n;
    float f;
    Size size = this->GetSize();
    if ((_code_width < size.width)
        && (_code_width < size.height)
        && (size.height == size.width))
    {
        _bmpBuf = new COLOR [size.width * size.height];
        //CAMERALOG("------fill qr buf size : %d", size.width * size.height* sizeof(COLOR));
        memset((char*)_bmpBuf, 0xff, size.width * size.height* sizeof(COLOR));
        m = size.width / _code_width;
        n = size.width % _code_width;
        f = (float)n / _code_width;
        int bits;
        int rows;
        //CAMERALOG("------fill qr code buffer : %d %d %f", m, n, f);
        int pp;
        for (int i = 1, k = 0; i < _code_width - 1; i++) {
            if (((i-1) * f - 1) >= k) {
                rows = m + 1;
            } else {
                rows = m;
            }
            for (int j = 1, l = 0; j < _code_width - 1; j++) {
                if (((j -1 ) * f - 1) > l) {
                    bits = m + 1;
                } else {
                    bits = m;
                }
                if ((_qrcode->data[(i-1)*(_code_width - 2) + (j - 1)] & 0x01)) {
                    for (int s = 0; s < rows; s++) {
                        pp = (size.height - 1 - (i* m + k + s)) * size.width + j * m + l;
                        memset((char*)&(_bmpBuf[pp]), 0 , sizeof(COLOR)*bits);
                    }
                }
                if (bits == m + 1) {
                    l++;
                }
                //else
                //CAMERALOG("1");
            }
            //CAMERALOG("");
            if (rows == m + 1) {
                k++;
            }
        }
        _pBmp = new CBmp(GetSize().width,
            GetSize().height,
            sizeof(COLOR) * 8,
            GetSize().width * sizeof(COLOR),
            (BYTE*)_bmpBuf);
    } else {
        CAMERALOG("--too small qrcode control size set to fill the code\n");
    }
}

void QrCodeControl::setString(char *string)
{
    if (strcmp(_string, string) != 0)
    {
        QRcode_free(_qrcode);
        if (_pBmp != 0) {
            delete _pBmp;
        }

        snprintf(_string, 128, "%s", string);
        createQrCodeBmp();
    }
}

void QrCodeControl::Draw(bool bHilight)
{
    if (_bmpBuf != 0) {
        Point tplft = GetAbsPos();
        Size size = this->GetSize();
        CairoCanvas* pCav = this->GetCanvas();
        pCav->DrawBmp(tplft, size, _pBmp);
    }
}

RadioGroup::RadioGroup(Container *pParent, const Point& leftTop, const Size& size)
    : inherited(pParent, leftTop, size)
    , _listener(NULL)
    , _names(NULL)
    , _count(0)
    , _rButton(16)
    , _clrButton(0xFFFF)
    , _bdwidth(4)
    , _rChecked(10)
    , _clrChecked(0x07E0)
    , _interval(18)
    , _txtSize(36)
    , _txtColor(0xFFFF)
{
}

RadioGroup::~RadioGroup()
{
}

void RadioGroup::Draw(bool bPost)
{
    Point tplft = GetAbsPos();
    Size size = this->GetSize();
    CairoCanvas* pCav = this->GetCanvas();
    for (UINT16 i = 0; i < _count; i++) {
        UINT16 top_y = tplft.y + (_interval + _rButton * 2) * i;
        pCav->DrawCircle(
            Point(tplft.x + _rButton, top_y + _rButton),
            _rButton,
            _bdwidth,
            _clrButton);
        if (i == _idxChecked) {
            pCav->DrawRound(
                Point(tplft.x + _rButton, top_y + _rButton),
                _rChecked,
                _clrChecked);
        }
        pCav->DrawText(
            (const char *)_names[i], FONT_ROBOTOMONO_Medium, _txtSize, _txtColor,
            Point(tplft.x + _interval + _rButton * 2, top_y),
            Size(size.width - _interval - _rButton * 2, _rButton * 2),
            LEFT, MIDDLE);
    }

    if (bPost) {
        this->GetParent()->Draw(true);
    }
}

bool RadioGroup::OnEvent(CEvent *event)
{
    bool b = true;

    if (event->_type == eventType_touch)
    {
        switch (event->_event) {
            case TouchEvent_SingleClick:
                {
                    int x = event->_paload;
                    int y = event->_paload1;

                    Point pos = GetRelativePos();
                    Size size = GetSize();
                    /*CAMERALOG("TouchEvent_SingleClick point(%d, %d) is "
                        "clicked, button area: (%d, %d) -> (%d, %d)\n",
                        x, y,
                        pos.x, pos.y,
                        pos.x +size.width, pos.y + size.height);*/
                    if ((x >= pos.x) && (x <= pos.x + size.width)
                        && (y >= pos.y) && (y <= pos.y + size.height))
                    {
                        UINT16 item_height = _interval + _rButton * 2;
                        UINT16 rel_y = y - pos.y;
                        int index = rel_y / item_height;
                        if (index < _count) {
                            _idxChecked = index;
                            Draw(true);
                            if (_listener) {
                                _listener->onCheckedChanged(this, _idxChecked);
                            }
                        }
                        b = false;
                    }
                }
                break;
            default:
                break;
        }
    }

    return b;
}


Pickers::Pickers(
        Container *pParent
        ,const Point& leftTop
        ,const Size& viewportSize
        ,const Size& canvasSize)
    : inherited(pParent, leftTop, viewportSize)
    , _listener(NULL)
    , _pPickerCanvas(NULL)
    , _names(NULL)
    , _count(0)
    , _idxChecked(-1)
    , _colorBG(0x0000)
    , _colorNormal(0xFFE0)
    , _colorHighligt(0xF81F)
    , _fontSize(28)
    , _fontType(FONT_ROBOTOMONO_Medium)
    , _itemHeight(60)
    , _padding(2)
    , _bTriggered(false)
    , _moveBase(0)
{
    _viewPortLft.x = 0;
    _viewPortLft.y = 0;
    _viewPortSize = viewportSize;

    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    _pPickerCanvas = new CairoCanvas(
        canvasSize.width, canvasSize.height,
        bitdepth);
}

Pickers::~Pickers()
{
    delete _pPickerCanvas;
}

// viewport is relative to Picker, not relative to its window
void Pickers::setViewPort(const Point& leftTop)
{
    _viewPortLft = leftTop;
    if (_viewPortLft.y < 0) {
        _viewPortLft.y = 0;
    } else if (_viewPortLft.y + _viewPortSize.height > _pPickerCanvas->GetSize().height) {
        _viewPortLft.y = _pPickerCanvas->GetSize().height - _viewPortSize.height;
    }
    //CAMERALOG("#### _viewPortLft(%d, %d)", _viewPortLft.x, _viewPortLft.y);
}

void Pickers::Draw(bool bPost)
{
    // firstly, draw items to its canvas
    Size size = _pPickerCanvas->GetSize();
    _pPickerCanvas->FillAll(_colorBG);

    // sencodly, draw items
    int avail = size.height / _itemHeight;
    for (int i = 0; i < _count && i < avail; i++) {
        if (_idxChecked == i) {
            _pPickerCanvas->DrawRect(
                Point(0, i * (_itemHeight + _padding)),
                Size(size.width, _itemHeight),
                _colorHighligt,
                _itemHeight / 2);
            _pPickerCanvas->DrawText(_names[i], (FONT)_fontType, _fontSize, _colorNormal,
                Point(0, i * (_itemHeight + _padding)), Size(size.width, _itemHeight),
                CENTER, MIDDLE);
        } else {
            _pPickerCanvas->DrawText(_names[i], (FONT)_fontType, _fontSize, _colorHighligt,
                Point(0, i * (_itemHeight + _padding)), Size(size.width, _itemHeight),
                CENTER, MIDDLE);
        }
    }

    // thirdly, draw shadow if needed
    if (_viewPortLft.y > 0) {
        _pPickerCanvas->DrawRectGradient(
            Point(0, _viewPortLft.y),
            Size(size.width, _itemHeight / 3),
            _colorBG, 0.5,
            _colorBG, 0.0
            );
    }
    if (_viewPortLft.y + _viewPortSize.height < _pPickerCanvas->GetSize().height) {
        _pPickerCanvas->DrawRectGradient(
            Point(0, _viewPortLft.y + _viewPortSize.height - _itemHeight / 3),
            Size(size.width, _itemHeight / 3),
            _colorBG, 0.0,
            _colorBG, 0.5
            );
    }

    if (this->GetParent()) {
        // lastly, copy to parent canvas
        CairoCanvas* pWndCav = this->GetParent()->GetCanvas();
        Size wndCavSize = pWndCav->GetSize();
        UINT8 bytedepth = pWndCav->GetByteDepth();

        Point lefttop = this->GetAbsPos();
        for (UINT16 i = 0; i < _viewPortSize.height; i++) {
            char *dest = ((char *)pWndCav->GetBuffer())
                + ((i + lefttop.y) * wndCavSize.width + lefttop.x) * bytedepth;
            char *source = ((char *)_pPickerCanvas->GetBuffer())
                + ((i + _viewPortLft.y) * _viewPortSize.width + _viewPortLft.x) * bytedepth;
            memcpy(dest, source, _viewPortSize.width * bytedepth);
        }
    }

    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}

bool Pickers::OnEvent(CEvent *event)
{
    bool b = true;
    if (event->_type == eventType_touch)
    {
        switch (event->_event) {
            case TouchEvent_Move_Began:
                {
                    Point tplft = GetAbsPos();
                    int x = event->_paload;
                    int y = event->_paload1;
                    //CAMERALOG("h[%d - %d], v[%d - %d], x = %d, y = %d",
                    //    tplft.x + pos - bondary, tplft.x + pos + bondary,
                    //    tplft.y - bondary, tplft.y + bondary,
                    //    x, y);
                    if ((x >= tplft.x) && (x <= (tplft.x + _viewPortSize.width))
                        && (y >= tplft.y) && (y <= (tplft.y + _viewPortSize.height)))
                    {
                        //CAMERALOG("#### Triggered");
                        _bTriggered = true;
                        _moveBase = 0;
                    }
                }
                break;
            case TouchEvent_Move_Changed:
                if (_bTriggered) {
                    // the minimum movement should make it change at least by Min_move px
                    const int Min_move = 2;

                    int y_move = event->_paload1;
                    int movement = (y_move > _moveBase)
                        ? (y_move - _moveBase) : (_moveBase - y_move);
                    if (movement > Min_move) {
                        _viewPortLft.y -= y_move - _moveBase;
                        if (_viewPortLft.y < 0)
                        {
                            _viewPortLft.y = 0;
                        } else if (_viewPortLft.y + _viewPortSize.height > _pPickerCanvas->GetSize().height) {
                            _viewPortLft.y = _pPickerCanvas->GetSize().height - _viewPortSize.height;
                        }
                        _moveBase = y_move;
                        Draw(true);
                    }
                }
                break;
            case TouchEvent_Move_Ended:
                //CAMERALOG("#### TouchEvent_Move_Ended");
                if (_bTriggered) {
                    _bTriggered = false;
                    b = false;
                }
                break;
            case TouchEvent_SingleClick:
                {
                    int x = event->_paload;
                    int y = event->_paload1;
                    Point tplft = GetAbsPos();
                    if ((x >= tplft.x) && (x <= (tplft.x + _viewPortSize.width))
                        && (y >= tplft.y) && (y <= (tplft.y + _viewPortSize.height)))
                    {
                        int postionInPicker = (y - tplft.y) + _viewPortLft.y;
                        int index = postionInPicker / (_itemHeight + _padding);
                        _idxChecked = index;
                        CAudioPlayer::getInstance()->playSoundEffect(
                            CAudioPlayer::SE_Click);
                        if (_listener) {
                            _listener->onPickersFocusChanged(this, _idxChecked);
                        }
                        b = false;
                    }
                }
                break;
            default:
                break;
        }
    }

    return b;
}


Scroller::Scroller(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& viewportSize,
        const Size& canvasSize,
        UINT16 maxChildren)
    : inherited(pParent, pOwner, leftTop, viewportSize, maxChildren)
    , _pScrollerCanvas(NULL)
    , _pViewRoot(NULL)
    , _colorBG(0x0000)
    , _bScrollable(true)
    , _bTriggered(false)
    , _moveBase(0)
{
    _viewPortLft.x = 0;
    _viewPortLft.y = 0;
    _viewPortSize = viewportSize;

    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    _pScrollerCanvas = new CairoCanvas(
        canvasSize.width, canvasSize.height,
        bitdepth);

    _pViewRoot = new Panel(NULL, pOwner, Point(0, 0), canvasSize, maxChildren);
    _pViewRoot->setCanvas(_pScrollerCanvas);
    _pViewRoot->setBgColor(_colorBG);
}

Scroller::~Scroller()
{
    delete _pViewRoot;
    delete _pScrollerCanvas;
}

// viewport is relative to Picker, not relative to its window
void Scroller::setViewPort(const Point& leftTop, const Size& size)
{
    _viewPortLft = leftTop;
    _viewPortSize = size;
}

void Scroller::setScrollable(bool scrollable)
{
    _bScrollable = scrollable;
    if (!_bScrollable) {
        _viewPortLft.x = 0;
        _viewPortLft.y = 0;
    }
}

CairoCanvas* Scroller::GetCanvas()
{
    return _pScrollerCanvas;
}

void Scroller::Draw(bool bPost)
{
    if (bPost) {
        this->GetOwner()->GetMainObject()->Draw(true);
        return;
    }

    // firstly, draw items to list's canvas
    _pViewRoot->Draw(false);

    // secondly, draw shadow if needed
    if (_viewPortLft.y > 0) {
        _pScrollerCanvas->DrawRectGradient(
            Point(0, _viewPortLft.y),
            Size(_pScrollerCanvas->GetSize().width, 20),
            _colorBG, 0.5,
            _colorBG, 0.0
            );
    }
    if (_viewPortLft.y + _viewPortSize.height < _pScrollerCanvas->GetSize().height) {
        _pScrollerCanvas->DrawRectGradient(
            Point(0, _viewPortLft.y + _viewPortSize.height - 20),
            Size(_pScrollerCanvas->GetSize().width, 20),
            _colorBG, 0.0,
            _colorBG, 0.5
            );
    }

    if (this->GetParent()) {
        // lastly, copy to parent canvas
        CairoCanvas* pWndCav = this->GetParent()->GetCanvas();
        Size wndCavSize = pWndCav->GetSize();
        UINT8 bytedepth = pWndCav->GetByteDepth();

        Point lefttop = this->GetAbsPos();
        for (UINT16 i = 0; i < _viewPortSize.height; i++) {
            char *dest = ((char *)pWndCav->GetBuffer())
                + ((i + lefttop.y) * wndCavSize.width + lefttop.x) * bytedepth;
            char *source = ((char *)_pScrollerCanvas->GetBuffer())
                + ((i + _viewPortLft.y) * _viewPortSize.width + _viewPortLft.x) * bytedepth;
            memcpy(dest, source, _viewPortSize.width * bytedepth);
        }
    }
}

bool Scroller::OnEvent(CEvent *event)
{
    bool b = true;
    if (event->_type == eventType_touch)
    {
        switch (event->_event) {
            case TouchEvent_Move_Began:
                {
                    Point tplft = GetAbsPos();
                    int x = event->_paload;
                    int y = event->_paload1;
                    //CAMERALOG("h[%d - %d], v[%d - %d], x = %d, y = %d",
                    //    tplft.x, tplft.x + _viewPortSize.width,
                    //    tplft.y, tplft.y + _viewPortSize.height,
                    //    x, y);
                    if (_bScrollable
                        && (x >= tplft.x) && (x <= (tplft.x + _viewPortSize.width))
                        && (y >= tplft.y) && (y <= (tplft.y + _viewPortSize.height)))
                    {
                        //CAMERALOG("#### Triggered");
                        _bTriggered = true;
                        _moveBase = 0;
                    }
                }
                break;
            case TouchEvent_Move_Changed:
                if (_bTriggered) {
                    // the minimum movement should make it change at least by Min_move px
                    const int Min_move = 2;

                    int y_move = event->_paload1;
                    int movement = (y_move > _moveBase)
                        ? (y_move - _moveBase) : (_moveBase - y_move);
                    if (movement > Min_move) {
                        _viewPortLft.y -= y_move - _moveBase;
                        if (_viewPortLft.y < 0)
                        {
                            _viewPortLft.y = 0;
                        } else if (_viewPortLft.y + _viewPortSize.height > _pScrollerCanvas->GetSize().height) {
                            _viewPortLft.y = _pScrollerCanvas->GetSize().height - _viewPortSize.height;
                        }
                        _moveBase = y_move;
                        Draw(true);
                    }
                }
                break;
            case TouchEvent_Move_Ended:
                //CAMERALOG("#### TouchEvent_Move_Ended");
                if (_bTriggered) {
                    _bTriggered = false;
                }
                break;
            case TouchEvent_SingleClick:
                {
                    int x = event->_paload;
                    int y = event->_paload1;
                    Point tplft = GetAbsPos();
                    if ((x >= tplft.x) && (x <= (tplft.x + _viewPortSize.width))
                        && (y >= tplft.y) && (y <= (tplft.y + _viewPortSize.height)))
                    {
                        int postionInScroller = (y - tplft.y) + _viewPortLft.y;
                        CEvent new_event = *event;
                        new_event._paload = x - tplft.x;
                        new_event._paload1 = postionInScroller;
                        _pViewRoot->OnEvent(&new_event);
                        b = false;
                    }
                }
                break;
            default:
                break;
        }
    }

    return b;
}


Slider::Slider(Container *pParent, const Point& leftTop, const Size& size)
    : inherited(pParent, leftTop, size)
    , _pListener(NULL)
    , _bTriggered(false)
    , _colorBg(0x8410)
    , _colorHglt(0xFFFF)
    , _radius(10)
    , _percentage(50)
    , _stepper(5)
{
}

Slider::~Slider()
{
}

void Slider::setValue(int value)
{
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    _percentage = value;
}

void Slider::setStepper(int stepper)
{
    _stepper = stepper;
}

void Slider::Draw(bool bPost)
{
    Point tplft = GetAbsPos();
    Size size = this->GetSize();
    CairoCanvas* pCav = this->GetCanvas();

    pCav->DrawLine(tplft, Point(tplft.x + size.width, tplft.y),
        4, _colorBg);

    UINT16 pos = size.width * _percentage / 100;
    pCav->DrawLine(tplft, Point(tplft.x + pos, tplft.y),
        4, _colorHglt);

    UINT16 x_center = tplft.x + pos;
    if (size.width < _radius * 2) {
        _radius = size.width / 2;
        x_center = tplft.x + _radius;
    } else if (pos < _radius) {
        x_center = tplft.x + _radius;
    } else if (pos > size.width - _radius) {
        x_center = tplft.x + (size.width - _radius);
    }
    pCav->DrawRound(Point(x_center, tplft.y),
        _radius, _colorHglt);

    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}

bool Slider::OnEvent(CEvent *event)
{
    bool b = true;
    if (event->_type == eventType_touch)
    {
        switch (event->_event) {
            case TouchEvent_Move_Began:
                {
                    Point tplft = GetAbsPos();
                    Size size = this->GetSize();
                    UINT16 pos = size.width * _percentage / 100;

                    int x = event->_paload;
                    int y = event->_paload1;
                    const int bondary = 50;
                    //CAMERALOG("h[%d - %d], v[%d - %d], x = %d, y = %d",
                    //    tplft.x + pos - bondary, tplft.x + pos + bondary,
                    //    tplft.y - bondary, tplft.y + bondary,
                    //    x, y);
                    if ((x >= (tplft.x + pos - bondary)) && (x <= (tplft.x + pos + bondary))
                        && (y >= (tplft.y - bondary)) && (y <= (tplft.y + bondary)))
                    {
                        //CAMERALOG("#### Triggered");
                        _bTriggered = true;
                        _moveBase = 0;
                        if (_pListener) {
                            _pListener->onSliderTriggered(this, true);
                        }
                    }
                }
                break;
            case TouchEvent_Move_Changed:
                if (_bTriggered) {
                    Size size = this->GetSize();
                    // the minimum movement should make it change at least by _stepper%
                    const int Min_move = (size.width * _stepper / 100 > 2)
                        ? (size.width * _stepper / 100) : 2;

                    int x_move = event->_paload;
                    int movement = (x_move > _moveBase)
                        ? (x_move - _moveBase) : (_moveBase - x_move);
                    if (movement > Min_move) {
                        _percentage += (x_move - _moveBase) * 100 / size.width;
                        //CAMERALOG("#### x_move = %d, _moveBase = %d, "
                        //    "movement = %d, Min_move = %d, _percentage = %d",
                        //    x_move, _moveBase, movement, Min_move, _percentage);
                        if (_percentage < 0) {
                            _percentage = 0;
                        } else if (_percentage > 100) {
                            _percentage = 100;
                        }
                        if (_pListener) {
                            _pListener->onSliderChanged(this, _percentage);
                        } else {
                            Draw(true);
                        }
                        _moveBase = x_move;
                    }
                }
                break;
            case TouchEvent_Move_Ended:
                //CAMERALOG("#### TouchEvent_Move_Ended");
                if (_bTriggered) {
                    _bTriggered = false;
                    if (_pListener) {
                        _pListener->onSliderTriggered(this, false);
                    }
                    b = false;
                }
                break;
            default:
                break;
        }
    }

    return b;
}


TextListAdapter::TextListAdapter()
    : _pText(NULL)
    , _num(0)
    , _itemSize(240, 50)
    , _colorNormal(0x0000)
    , _colorPressed(0x8410)
    , _txtColor(0xFFFF)
    , _txtSize(24)
    , _fontType(FONT_ROBOTOMONO_Medium)
    , _hAlign(LEFT)
    , _vAlign(MIDDLE)
{
}

TextListAdapter::~TextListAdapter()
{
}

UINT16 TextListAdapter::getCount()
{
    return _num;
}

Control* TextListAdapter::getItem(UINT16 position, Container* pParent, const Size& size)
{
    if (position >= _num) {
        return NULL;
    }

    Button *item = new Button(pParent, Point(0, 0), _itemSize, _colorNormal, _colorPressed);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }
    item->SetText(_pText[position], _txtSize, _txtColor, _fontType);
    item->SetTextAlign(_hAlign, _vAlign);

    return item;
}

Control* TextListAdapter::getItem(UINT16 position, Container* pParent)
{
    if (position >= _num) {
        return NULL;
    }

    Button *item = new Button(pParent, Point(0, 0), _itemSize,
        _colorNormal, _colorPressed);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }
    item->SetText(_pText[position], _txtSize, _txtColor, _fontType);
    item->SetTextAlign(_hAlign, _vAlign);

    return item;
}

void TextListAdapter::setTextList(UINT8 **pText, UINT16 num)
{
    _pText = pText;
    _num = num;
}

void TextListAdapter::setItemStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed,
    UINT16 txtColor, UINT16 txtSize,
    UINT8 hAlign, UINT8 vAlign)
{
    _itemSize = size;
    _colorNormal = colorNormal;
    _colorPressed = colorPressed;
    _txtColor = txtColor;
    _txtSize = txtSize;
    _hAlign = hAlign;
    _vAlign = vAlign;
}

void TextListAdapter::setItemStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed,
    UINT16 txtColor, UINT16 txtSize, UINT16 font,
    UINT8 hAlign, UINT8 vAlign)
{
    _itemSize = size;
    _colorNormal = colorNormal;
    _colorPressed = colorPressed;
    _txtColor = txtColor;
    _txtSize = txtSize;
    _fontType = font;
    _hAlign = hAlign;
    _vAlign = vAlign;
}



TextImageAdapter::TextImageAdapter()
    : _pText(NULL)
    , _num(0)
    , _itemSize(240, 50)
    , _colorNormal(0x0000)
    , _colorPressed(0x8410)
    , _txtColor(0xFFFF)
    , _txtSize(24)
    , _fontType(FONT_ROBOTOMONO_Medium)
    , _hAlign(LEFT)
    , _vAlign(MIDDLE)
    , _imageSize(0, 0)
    , _imagePath(NULL)
    , _imageNormal(NULL)
    , _imagePressed(NULL)
    , _bImageLeft(false)
    , _paddingImage(0)
{
}

TextImageAdapter::~TextImageAdapter()
{
}

UINT16 TextImageAdapter::getCount()
{
    return _num;
}

Control* TextImageAdapter::getItem(UINT16 position, Container* pParent, const Size& size)
{
    if (position >= _num) {
        return NULL;
    }

    Container *item = new Container(pParent, pParent->GetOwner(), Point(0, 0), _itemSize, 5);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }

    Button *button = new Button(item, Point(0, 0), _itemSize, _colorNormal, _colorPressed);
    if (!button) {
        CAMERALOG("Out of Memory!");
    }
    button->SetText(_pText[position], _txtSize, _txtColor, _fontType);
    button->SetTextAlign(_hAlign, _vAlign);

    if ((_imagePath != NULL) && (_imageNormal != NULL) && (_imagePressed != NULL)) {
        ImageButton *icon = new ImageButton(
            item,
            Point(_bImageLeft ? _paddingImage: (_itemSize.width - _imageSize.width - _paddingImage),
                (_itemSize.height - _imageSize.height) / 2),
            _imageSize,
            _imagePath,
            _imageNormal,
            _imagePressed);
        if (!icon) {
            CAMERALOG("Out of Memory!");
        }
    }

    return item;
}

Control* TextImageAdapter::getItem(UINT16 position, Container* pParent)
{
    if (position >= _num) {
        return NULL;
    }

    Container *item = new Container(pParent, pParent->GetOwner(), Point(0, 0), _itemSize, 5);
    if (!item) {
        CAMERALOG("Out of Memory!");
        return NULL;
    }

    Button *button = new Button(item, Point(0, 0), _itemSize,
        _colorNormal, _colorPressed);
    if (!button) {
        CAMERALOG("Out of Memory!");
    }
    button->SetText(_pText[position], _txtSize, _txtColor, _fontType);
    button->SetTextAlign(_hAlign, _vAlign);

    if ((_imagePath != NULL) && (_imageNormal != NULL) && (_imagePressed != NULL)) {
        ImageButton *icon = new ImageButton(
            item,
            Point(_bImageLeft ? _paddingImage: (_itemSize.width - _imageSize.width - _paddingImage),
                (_itemSize.height - _imageSize.height) / 2),
            _imageSize,
            _imagePath,
            _imageNormal,
            _imagePressed);
        if (!icon) {
            CAMERALOG("Out of Memory!");
        }
    }

    return item;
}

void TextImageAdapter::setTextList(UINT8 **pText, UINT16 num)
{
    _pText = pText;
    _num = num;
}

void TextImageAdapter::setTextStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed,
    UINT16 txtColor, UINT16 txtSize, UINT16 font,
    UINT8 hAlign, UINT8 vAlign)
{
    _itemSize = size;
    _colorNormal = colorNormal;
    _colorPressed = colorPressed;
    _txtColor = txtColor;
    _txtSize = txtSize;
    _fontType = font;
    _hAlign = hAlign;
    _vAlign = vAlign;
}

void TextImageAdapter::setImageStyle(
    const Size& size,
    const char *path,
    const char *normal,
    const char *pressed,
    bool bLeft)
{
    _imageSize = size;
    _imagePath = path;
    _imageNormal = normal;
    _imagePressed = pressed;
    _bImageLeft = bLeft;
}


List::List(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren)
    : inherited(pParent, pOwner, leftTop, size, maxChildren)
    , _pOwner(pOwner)
    , _pListener(NULL)
    , _adaptor(NULL)
    , _pViewRoot(NULL)
    , _colorBG(0x0000)
    , _visibleList(NULL)
    , _maxVisible(0)
    , _numVisible(0)
    , _topOffset(0)
    , _posFirstVisible(0)
    , _padding(4)
{

}

List::~List()
{
    for (UINT16 i = 0; i < _maxVisible; i++) {
        delete _visibleList[i];
    }
    delete [] _visibleList;
    delete _pViewRoot;
    delete _pListCanvas;
}

void List::setAdaptor(ListAdaptor *adapter)
{
    if (adapter == NULL) {
        return;
    }
    _adaptor = adapter;
    Size itemSize = _adaptor->getItemSize();
    _itemHeight = itemSize.height;
    Size listSize = GetSize();

    _maxVisible = listSize.height / (itemSize.height + _padding) + 2;
    _visibleList = new Control*[_maxVisible];
    memset(_visibleList, 0x00, sizeof(Control*) * _maxVisible);

    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    _pListCanvas = new CairoCanvas(
        listSize.width,
        (itemSize.height + _padding) * _maxVisible - _padding,
        bitdepth);

    _pViewRoot = new Panel(NULL, _pOwner, Point(0, 0), _pListCanvas->GetSize(), 20);
    _pViewRoot->setCanvas(_pListCanvas);
    _pViewRoot->setBgColor(_colorBG);

    _numVisible = 0;
    _topOffset = 0;
    _posFirstVisible = 0;

    UINT16 list_num = _adaptor->getCount();
    UINT16 num = _maxVisible < list_num ? _maxVisible : list_num;
    for (UINT16 i = 0; i < num; i++) {
        Control* widget = _adaptor->getItem(i, _pViewRoot);
        if (widget) {
            widget->SetRelativePos(Point(0, (itemSize.height + _padding) * i));
            _visibleList[i] = widget;
            _numVisible++;
        }
    }
}

void List::notifyChanged()
{
    // reload data, show items from list head
    for (UINT16 i = 0; i < _numVisible; i++) {
        delete _visibleList[i];
    }
    _numVisible = 0;
    _topOffset = 0;
    _posFirstVisible = 0;

    UINT16 list_num = _adaptor->getCount();
    UINT16 num = _maxVisible < list_num ? _maxVisible : list_num;
    Size itemSize = _adaptor->getItemSize();
    for (UINT16 i = 0; i < num; i++) {
        Control* widget = _adaptor->getItem(i, _pViewRoot);
        if (widget) {
            widget->SetRelativePos(Point(0, (itemSize.height + _padding) * i));
            _visibleList[i] = widget;
            _numVisible++;
        }
    }
}

void List::scroll(int distance)
{
    if (distance > _itemHeight) {
        distance = _itemHeight;
    } else if (distance < -1 * _itemHeight) {
        distance = -1 * _itemHeight;
    }
    _topOffset += distance;

    //CAMERALOG("#### _topOffset = %d, distance = %d", _topOffset, distance);

    if (_topOffset < 0) {
        if ((_itemHeight + _padding) * _numVisible - _padding > this->GetSize().height) {
            // remove item from the top, and add item to bottom
            if (_posFirstVisible + _numVisible < _adaptor->getCount()) {
                if (_topOffset <= (-1) * _itemHeight) {
                    _posFirstVisible++;

                    delete _visibleList[0];
                    _visibleList[0] = NULL;
                    for (UINT16 i = 0; i < _numVisible - 1; i++) {
                        _visibleList[i] = _visibleList[i + 1];
                        _visibleList[i]->SetRelativePos(Point(0, (_itemHeight + _padding) * i));
                    }
                    Control* widget = _adaptor->getItem(_posFirstVisible + _numVisible - 1, _pViewRoot,
                        Size(GetSize().width, _itemHeight));
                    widget->SetRelativePos(Point(0, (_itemHeight + _padding) * (_numVisible - 1)));
                    _visibleList[_numVisible - 1] = widget;

                    _topOffset += _itemHeight;
                    if (_topOffset > 0) {
                        _topOffset = 0;
                    }
                }
            } else {
                if (_topOffset < this->GetSize().height - _pListCanvas->GetSize().height) {
                    _topOffset = this->GetSize().height - _pListCanvas->GetSize().height;
                }
            }
        } else {
            _topOffset = 0;
        }
    } else if (_topOffset > 0) {
        // remove item from the bottom, and add item to top
        if (_posFirstVisible > 0) {
            _posFirstVisible--;

            delete _visibleList[_numVisible - 1];
            _visibleList[_numVisible - 1] = NULL;
            for (UINT16 i = _numVisible - 1; i > 0; i--) {
                _visibleList[i] = _visibleList[i - 1];
                _visibleList[i]->SetRelativePos(Point(0, (_itemHeight + _padding) * i));
            }
            Control* widget = _adaptor->getItem(_posFirstVisible, _pViewRoot,
                Size(GetSize().width, _itemHeight));
            widget->SetRelativePos(Point(0, 0));
            _visibleList[0] = widget;

            _topOffset -= _itemHeight;
        } else {
            _topOffset = 0;
        }
    }

    this->Draw(true);
}

CairoCanvas* List::GetCanvas()
{
    return _pListCanvas;
}

void List::Draw(bool bPost)
{
    if (bPost) {
        if (this->GetParent()) {
            this->GetParent()->Draw(true);
            return;
        }
    }

    // firstly, draw items to list's canvas
    _pListCanvas->FillAll(0x00);
    for (UINT16 i = 0; i < _numVisible; i++) {
        _visibleList[i]->Draw(false);
    }

    // secondly, draw shadow if needed
    Size listSize = GetSize();
    if (_topOffset != 0) {
        _pListCanvas->DrawRectGradient(
            Point(0, 0 - _topOffset),
            Size(listSize.width, 20),
            _colorBG, 0.5,
            _colorBG, 0.0
            );
    }
    if ((_posFirstVisible + _numVisible < _adaptor->getCount()) ||
        ((_posFirstVisible + _numVisible == _adaptor->getCount()) &&
          (_topOffset != this->GetSize().height - _pListCanvas->GetSize().height)))
    {
        _pListCanvas->DrawRectGradient(
            Point(0, (listSize.height - _topOffset) - 20),
            Size(listSize.width, 20),
            _colorBG, 0.0,
            _colorBG, 0.5
            );
    }

    if (this->GetParent()) {
        // lastly, copy to parent canvas
        CairoCanvas* pWndCav = this->GetParent()->GetCanvas();
        Size wndCavSize = pWndCav->GetSize();
        UINT8 bytedepth = pWndCav->GetByteDepth();

        Point lefttop = this->GetAbsPos();
        for (UINT16 i = 0; i < this->GetSize().height; i++) {
            char *dest = ((char *)pWndCav->GetBuffer())
                + ((i + lefttop.y) * wndCavSize.width + lefttop.x) * bytedepth;
            char *source = ((char *)_pListCanvas->GetBuffer())
                + (i - _topOffset) * _pListCanvas->GetSize().width * bytedepth;
            memcpy(dest, source, _pListCanvas->GetSize().width * bytedepth);
        }
    }

    if (bPost) {
        this->GetOwner()->onSurfaceChanged();
    }
}

bool List::OnEvent(CEvent *event)
{
    bool b = true;
    // TODO: make more reasonalble gestures for List
    switch (event->_type)
    {
        case eventType_touch:
            /*if (event->_event == TouchEvent_flick) {
                if ((event->_paload == TouchFlick_Down)) {
                    scroll(_itemHeight / 2);
                    b = false;
                } else if ((event->_paload == TouchFlick_UP)) {
                    scroll(-1 * _itemHeight / 2);
                    b = false;
                }
            } else */if (event->_event == TouchEvent_Move_Began) {
                _moveBase = 0;
                b = false;
            } else if (event->_event == TouchEvent_Move_Changed) {
                int y_move = event->_paload1;
                y_move = y_move / 2; // slow down movement

                //CAMERALOG("#### TouchEvent_Move_Changed, "
                //    "y_move =%d, _moveBase = %d, y_move - _moveBase = %d",
                //    y_move, _moveBase, y_move - _moveBase);
                scroll(y_move - _moveBase);
                _moveBase = y_move;
                b = false;
            } else if (event->_event == TouchEvent_SingleClick) {
                int x = event->_paload;
                int y = event->_paload1;
                Point pos = GetRelativePos();
                Size size = GetSize();
                if ((x >= pos.x) && (x <= pos.x + size.width)
                    && (y >= pos.y) && (y <= pos.y + size.height))
                {
                    //CAMERALOG(" #### y = %d, pos.y = %d, _topOffset = %d, _itemHeight = %d, "
                    //    "_padding = %d, _posFirstVisible = %d\n",
                    //    y, pos.y, _topOffset, _itemHeight, _padding, _posFirstVisible);
                    UINT16 index = (y - pos.y - _topOffset) / (_itemHeight + _padding);
                    index += _posFirstVisible;
                    //CAMERALOG("the %dth item is clicked in list", index);

                    if (index < _adaptor->getCount()) {
                        // Set item pressed
                        CEvent new_event = *event;
                        new_event._event = TouchEvent_SingleClick;
                        new_event._paload = x - pos.x;
                        new_event._paload1 = (index - _posFirstVisible) * (_itemHeight + _padding)
                            + _itemHeight / 2;
                        _visibleList[index - _posFirstVisible]->OnEvent(&new_event);

                        CAudioPlayer::getInstance()->playSoundEffect(
                            CAudioPlayer::SE_Click);

                        if (_pListener) {
                            _pListener->onListClicked(this, index);
                        }
                    }
                }
#if 0
            } else if (event->_event == TouchEvent_OnPressed) {
                int x = event->_paload;
                int y = event->_paload1;
                Point pos = GetRelativePos();
                Size size = GetSize();
                if ((x >= pos.x) && (x <= pos.x + size.width)
                    && (y >= pos.y) && (y <= pos.y + size.height))
                {
                    UINT16 index = (y - _topOffset) / (_itemHeight + _padding);
                    CAMERALOG("the %dth item is pressed in list, change its status",
                        index + _posFirstVisible);
                    CEvent new_event = *event;
                    new_event._paload = x - pos.x;
                    new_event._paload1 = _topOffset + (index - 1) * (_itemHeight + _padding) + _itemHeight / 2;
                    _visibleList[index - 1]->OnEvent(&new_event);
                }
            } else if (event->_event == TouchEvent_OnReleased) {
                int x = event->_paload;
                int y = event->_paload1;
                Point pos = GetRelativePos();
                Size size = GetSize();
                if ((x >= pos.x) && (x <= pos.x + size.width)
                    && (y >= pos.y) && (y <= pos.y + size.height))
                {
                    UINT16 index = (y - _topOffset) / (_itemHeight + _padding);
                    CAMERALOG("the %dth item is released in list, change its status\n",
                        index + _posFirstVisible);
                    CEvent new_event = *event;
                    new_event._paload = x - pos.x;
                    new_event._paload1 = _topOffset + (index - 1) * (_itemHeight + _padding) + _itemHeight / 2;
                    _visibleList[index - 1]->OnEvent(&new_event);
                }
#endif
            }
            break;
        default:
            break;
    }
    return b;
}


void Fragment::Draw(bool bPost)
{
    //CAMERALOG("%s Draw(%d)", getFragName(), bPost);
    if (bPost) {
        if (this->GetParent()) {
            this->GetParent()->Draw(true);
        } else {
            for (int i = 0; i < GetChildrenCount() ; i++) {
                if ((GetControl(i) != 0) && (!GetControl(i)->IsHiden())) {
                    GetControl(i)->Draw(false);
                }
            }
            this->GetOwner()->onSurfaceChanged();
        }
    } else {
        for (int i = 0; i < GetChildrenCount() ; i++) {
            if ((GetControl(i) != 0) && (!GetControl(i)->IsHiden())) {
                GetControl(i)->Draw(false);
            }
        }
    }
}

bool Fragment::processEvent(CEvent *event)
{
    bool b = true;
    if (b) {
        b = inherited::OnEvent(event);
    }

    if (b) {
        b = OnEvent(event);
    }

    return b;
}

void Fragment::TimerCB(void *para)
{
    Fragment *frag = (Fragment *)para;
    if (frag) {
        frag->onTimerEvent();
    }
}

bool Fragment::startTimer(int t_ms, bool bLoop)
{
    if ((_name != NULL) && (CTimerThread::GetInstance() != NULL)) {
        CTimerThread::GetInstance()->ApplyTimer(
            t_ms, Fragment::TimerCB, this, bLoop, _name);
    }
    return true;
}

bool Fragment::cancelTimer()
{
    if ((_name != NULL) && (CTimerThread::GetInstance() != NULL)) {
        CTimerThread::GetInstance()->CancelTimer(_name);
    }
    return true;
}


ViewPager::ViewPager(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren)
    : inherited(pParent, pOwner, leftTop, size, maxChildren)
    , _pListener(NULL)
    , _pViewPagerCanvas(NULL)
    , _leftOffset(0)
    , _indexFocus(-1)
    , _indexNextVisible(0)
    , _pPages(NULL)
    , _numPages(0)
    , _flick(TouchFlick_disabled)
    , _duringAnim(false)
    , _bExit(false)
    , _hThread(0)
    , _pWaitEvent(NULL)
    , _pLock(NULL)
{
    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    _pViewPagerCanvas = new CairoCanvas(
        size.width * 2,
        size.height,
        bitdepth);

    _pWaitEvent = new CSemaphore(0, 1, "viewpager sem");
    _pLock = new CMutex("viewpager mutex");
    pthread_create(&_hThread, NULL, ThreadFunc, (void*)this);
}

ViewPager::~ViewPager()
{
    if (_hThread) {
        _bExit = true;
        _pWaitEvent->Give();
        pthread_join(_hThread, NULL);
        _hThread = 0;
        //CAMERALOG("ViewPager_thread destroyed");
    }

    delete _pViewPagerCanvas;

    delete _pWaitEvent;
    delete _pLock;
}

void ViewPager::setFocus(UINT16 index)
{
    _indexFocus = index;
    _indexNextVisible = index;
    _leftOffset = 0;
}

void ViewPager::adjustOnMoveEnd()
{
    const int time_us = 10 * 1000;
    const int frames = 10;

    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);

    _towardsLeft = false;
    if (_flick == TouchFlick_Left) {
        _towardsLeft = true;
        _leftOffset = 0;
        if (_indexFocus == _numPages - 1) {
            _indexNextVisible = 0;
        } else {
            _indexNextVisible = _indexFocus + 1;
        }
    } else if (_flick == TouchFlick_Right) {
        _towardsLeft = false;
        _leftOffset = width;
        if (_indexFocus == 0) {
            _indexNextVisible = _numPages - 1;
        } else {
            _indexNextVisible = _indexFocus - 1;
        }
    } else {
        return;
    }

    //CAMERALOG("anim: _indexFocus = %d, "
    //    "_indexNextVisible = %d, _numPages = %d",
    //    _indexFocus, _indexNextVisible, _numPages);

    // old Fragment lost focus
    _pPages[_indexFocus]->onPause();

    // draw current and next fragment to viewpager's canvas
    // so that during moving both fragments can display
    this->Draw(true);

    _pLock->Take();
    _duringAnim = true;

    UINT16 steplength = this->GetSize().width / frames;
    UINT16 steps = 0;
    if (!_towardsLeft)
    {
        steps = _leftOffset / steplength +
            ((_leftOffset % steplength > 0) ? 1 : 0);
        for (int i = 1; i <= steps; i++) {
            if (_leftOffset > steplength) {
                _leftOffset = _leftOffset - steplength;
            } else {
                _leftOffset = 0;
            }
            //CAMERALOG("anim towards right, %dth frame: _leftOffset = %d",
            //    i, _leftOffset);
            // no re-draw, just copy to window canvas
            postToDisplay();

            if (_leftOffset > 0) {
                timeval _tt;
                _tt.tv_sec = 0;
                _tt.tv_usec = time_us;
                select(0, NULL, NULL, NULL, &_tt);
            }
        }
    } else {
        steps = width / steplength +
            ((width % steplength > 0) ? 1 : 0);
        for (int i = 1; i <= steps; i++) {
            _leftOffset = _leftOffset + steplength;
            if (_leftOffset > width) {
                _leftOffset = width;
            }

            // no re-draw, just copy to window canvas
            //CAMERALOG("anim towards left, %dth frame: _leftOffset = %d",
            //    i, _leftOffset);
            postToDisplay();

            if (_leftOffset < width - 1) {
                timeval _tt;
                _tt.tv_sec = 0;
                _tt.tv_usec = time_us;
                select(0, NULL, NULL, NULL, &_tt);
            }
        }
    }

    _duringAnim = false;
    _pLock->Give();

    // new Fragment got focus
    _indexFocus = _indexNextVisible;
    _leftOffset = 0;
    if (_pListener) {
        _pListener->onFocusChanged(_indexFocus);
    }
    _pPages[_indexFocus]->onResume();
}

CairoCanvas* ViewPager::GetCanvas()
{
    return _pViewPagerCanvas;
}

// no re-draw, just copy to window canvas
void ViewPager::postToDisplay()
{
    Point lefttop = this->GetAbsPos();
    Size  pagerSize = this->GetSize();

    CairoCanvas* pWndCav = this->GetOwner()->GetCanvas();
    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    int bytesDepth = bitdepth / 8;
    /*CAMERALOG("lefttop(%d, %d), pagerSize(%d, %d), _leftOffset = %d\n",
        lefttop.x, lefttop.y,
        pagerSize.width, pagerSize.height,
        _leftOffset);*/
    for (UINT16 i = 0; i < pagerSize.height; i++) {
        char *dest = ((char *)pWndCav->GetBuffer())
            + ((i + lefttop.y) * width + lefttop.x) * bytesDepth;
        char *source = ((char *)_pViewPagerCanvas->GetBuffer())
            + (i * _pViewPagerCanvas->GetSize().width + _leftOffset) * bytesDepth;
        memcpy(dest, source, pagerSize.width * bytesDepth);
    }

    if (_pListener) {
        // notify to re-draw TabIndicator if any
        _pListener->onViewPortChanged(Point(_leftOffset, 0));
    }

    this->GetOwner()->onSurfaceChanged();
}

void ViewPager::Draw(bool bPost)
{
    if (_numPages <=0 || _indexFocus < 0) {
        return;
    }

    bool canDraw = false;
    _pLock->Take();
    canDraw = !_duringAnim;
    _pLock->Give();
    if (!canDraw) {
        return;
    }

    if (bPost) {
        if (this->GetParent()) {
            this->GetParent()->Draw(true);
        } else {
            this->GetOwner()->onSurfaceChanged();
        }
        return;
    }

    Point lefttop = this->GetAbsPos();
    Size  pagerSize = this->GetSize();
    _pViewPagerCanvas->FillAll(0x00);

    //CAMERALOG("_indexFocus = %d, "
    //    "_indexNextVisible = %d, _leftOffset = %d",
    //    _indexFocus, _indexNextVisible, _leftOffset);

    // firstly, draw items to ViewPager canvas
    if (_indexNextVisible == _indexFocus) {
        if ((_indexFocus == 0) && (_leftOffset != 0)) {
            _pPages[_indexFocus]->SetRelativePos(
                Point(lefttop.x + pagerSize.width, lefttop.y));
        } else {
            _pPages[_indexFocus]->SetRelativePos(lefttop);
        }
        _pPages[_indexFocus]->Draw(false);
    } else if (_numPages == 2) {
        if (_towardsLeft) {
            // towards left
            _pPages[_indexFocus]->SetRelativePos(lefttop);
            _pPages[_indexFocus]->Draw(false);
            _pPages[_indexNextVisible]->SetRelativePos(
                Point(lefttop.x + pagerSize.width, lefttop.y));
            _pPages[_indexNextVisible]->Draw(false);
        } else {
            // towards right
            _pPages[_indexNextVisible]->SetRelativePos(lefttop);
            _pPages[_indexNextVisible]->Draw(false);
            _pPages[_indexFocus]->SetRelativePos(
                Point(lefttop.x + pagerSize.width, lefttop.y));
            _pPages[_indexFocus]->Draw(false);
        }
    } else if (_numPages > 2) {
        if ((_indexNextVisible == 0) && (_indexFocus == _numPages - 1)) {
            // towards left
            _pPages[_indexFocus]->SetRelativePos(lefttop);
            _pPages[_indexFocus]->Draw(false);
            _pPages[_indexNextVisible]->SetRelativePos(
                Point(lefttop.x + pagerSize.width, lefttop.y));
            _pPages[_indexNextVisible]->Draw(false);
        } else if ((_indexFocus == 0) && (_indexNextVisible == _numPages - 1)) {
            // towards right
            _pPages[_indexNextVisible]->SetRelativePos(lefttop);
            _pPages[_indexNextVisible]->Draw(false);
            _pPages[_indexFocus]->SetRelativePos(
                Point(lefttop.x + pagerSize.width, lefttop.y));
            _pPages[_indexFocus]->Draw(false);
        } else if (_indexNextVisible < _indexFocus) {
            _pPages[_indexNextVisible]->SetRelativePos(lefttop);
            _pPages[_indexNextVisible]->Draw(false);
            _pPages[_indexFocus]->SetRelativePos(
                Point(lefttop.x + pagerSize.width, lefttop.y));
            _pPages[_indexFocus]->Draw(false);
        } else if (_indexNextVisible > _indexFocus) {
            _pPages[_indexFocus]->SetRelativePos(lefttop);
            _pPages[_indexFocus]->Draw(false);
            _pPages[_indexNextVisible]->SetRelativePos(
                Point(lefttop.x + pagerSize.width, lefttop.y));
            _pPages[_indexNextVisible]->Draw(false);
        }
    }

    // secondly, copy to window canvas
    CairoCanvas* pWndCav = this->GetOwner()->GetCanvas();
    int width = 0, height = 0, bitdepth = 0;
    WindowManager::getInstance()->getWindowInfo(width, height, bitdepth);
    int bytesDepth = bitdepth / 8;
    for (UINT16 i = 0; i < pagerSize.height; i++) {
        char *dest = ((char *)pWndCav->GetBuffer())
            + ((i + lefttop.y) * width + lefttop.x) * bytesDepth;
        char *source = ((char *)_pViewPagerCanvas->GetBuffer())
            + (i * _pViewPagerCanvas->GetSize().width + _leftOffset) * bytesDepth;
        memcpy(dest, source, pagerSize.width * bytesDepth);
    }
}

bool ViewPager::OnEvent(CEvent *event)
{
    bool b = true;

    // Firstly let fragment process the event
    if ((_pPages != NULL) && !_duringAnim && (_indexFocus >= 0) && (_indexFocus < _numPages)) {
        b = _pPages[_indexFocus]->processEvent(event);
    }

    // Secondly if event is not consumed, process it
    if (b) {
        switch (event->_type)
        {
            case eventType_touch:
                switch (event->_event)
                {
                    case TouchEvent_Move_Ended:
                        if (!_duringAnim) {
                            int flick = event->_paload;
                            _flick = flick;
                            _pWaitEvent->Give();
                        }
                        b = false;
                        break;
                    default:
                        break;
                }
                break;
        }
    }

    return b;
}


// TODO: make a thread-model to make tasks run synchronizely
void ViewPager::main()
{
    while (!_bExit) {
        _pWaitEvent->Take(-1);

        if (_bExit) {
            break;
        } else {
            adjustOnMoveEnd();
            _flick = TouchFlick_disabled;
        }
    }
}

void* ViewPager::ThreadFunc(void* lpParam)
{
    ViewPager *pSelf = (ViewPager *)lpParam;
    if (pSelf) {
        prctl(PR_SET_NAME, "ViewPager_thread");
        pSelf->main();
    }
    return NULL;
}

};


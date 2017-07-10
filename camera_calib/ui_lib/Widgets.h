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

#ifndef __IN_Widgets_h__
#define __IN_Widgets_h__

#include "CairoCanvas.h"
#include "CEvent.h"
#include "Listeners.h"
#include "qrencode.h"

namespace ui{

class Container;
class Window;

class Control
{
public:
    Control(Container* pParent, const Point& leftTop, const Size& CSize);
    virtual ~Control();

    virtual Window* GetOwner();
    virtual CairoCanvas *GetCanvas();
    virtual void Draw(bool bPost);
    virtual void Clear();
    virtual bool OnEvent(CEvent *event);
    virtual bool isTouchOn(int x, int y);

    Container* GetParent() { return _pParent; }
    void SetParent(Container* pParent) { _pParent = pParent; }

    Size& GetSize() { return _size; }
    void SetSize(const Size& size) { _size = size; }

    Point& GetRelativePos() { return _leftTop; }
    void SetRelativePos(const Point& leftTop) { _leftTop = leftTop; }
    Point& GetAbsPos();

    bool IsHiden() { return ((_property & 0x01) != 0); }
    void SetHiden() { _property = _property | 0x01; }
    void SetShow() { _property = _property & 0xfffffffe; }

    bool IsDisabled() { return ((_property & 0x02) != 0); }
    void SetDisabled() { _property = _property | 0x02; }

    H_ALIGN GetHAlign() { return (H_ALIGN)((_property & 0x0C) >> 2); }
    void SetHAlign(H_ALIGN align) { _property = _property | ((UINT32)align << 2); }
    V_ALIGN GetVAlign() { return (V_ALIGN)((_property & 0x30) >> 4); }
    void SetVAlign(V_ALIGN align) { _property = _property | ((UINT32)align << 4); }

private:
    Container   *_pParent;
    Point       _leftTop;
    Point       _absPos;
    Size        _size;
    UINT32      _property;
};

class Container : public Control
{
public:
    typedef Control inherited;
    Container(
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& CSize,
        UINT16 maxChildren);
    virtual ~Container();

    virtual Window* GetOwner();
    virtual CairoCanvas *GetCanvas();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    bool AddControl(Control* pControl);
    bool RemoveControl(Control* pControl);
    UINT16 GetChildrenCount() { return _count; }
    Control* GetControl(UINT16 index);
    void SetOwner(Window* pOwner) { _pOwner = pOwner; }
    int getNumber(){return _number;};

private:
    Window*     _pOwner;
    Control**   _pControlList;
    UINT16      _number;
    UINT16      _count;
};

class Panel : public Container
{
public:
    typedef Container inherited;
    Panel
        (
            Container* pParent
            ,Window* pOwner
            ,const Point& leftTop
            ,const Size& size
            ,UINT16 maxChildren
        )
        : inherited(pParent, pOwner, leftTop, size, maxChildren)
        , _bgColor(0x0000)
    {
    }
    virtual ~Panel();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setBgColor(UINT16 color) { _bgColor = color; }

private:
    UINT16  _bgColor;
};

class UIPoint : public Control
{
public:
    typedef Control inherited;
    UIPoint(Container* pParent, const Point& point, const Size& CSize);
    virtual ~UIPoint();
    virtual void Draw(bool bPost);

    void setPoint(const Point& point, COLOR color) { _point = point; _color = color; }

private:
    Point  _point;
    COLOR  _color;
};

class UILine : public Control
{
public:
    typedef Control inherited;
    UILine(Container* pParent, const Point& point, const Size& CSize);
    virtual ~UILine();
    virtual void Draw(bool bPost);

    void setLine(const Point& start, const Point& end, UINT16 width, COLOR color)
    {
        _start = start;
        _end = end;
        _width = width;
        _color = color;
    }

    void setDash(bool dash) { _bDash = dash; }

private:
    bool    _bDash;
    Point   _start;
    Point   _end;
    UINT16  _width;
    COLOR   _color;
};

class UIRectangle : public Control
{
public:
    typedef Control inherited;
    UIRectangle(Container* pParent, const Point& point, const Size& CSize, COLOR color);
    virtual ~UIRectangle();
    virtual void Draw(bool bPost);

    void setRect(const Point& start, const Point& end, UINT16 width, COLOR color)
    {
        _start = start;
        _end = end;
        _width = width;
        _color = color;
    }

private:
    Point   _start;
    Point   _end;
    UINT16  _width;
    COLOR   _color;
};

class Circle : public Control
{
public:
    typedef Control inherited;
    Circle(
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 borderWidth
        , UINT16 color);
    virtual ~Circle();
    virtual void Draw(bool bPost);

    void setParams(const Point& center, UINT16 radius, UINT16 borderWidth, UINT16 color)
    {
        _center = center;
        _radius = radius;
        _borderWidth = borderWidth;
        _color = color;
    }

private:
    Point   _center;
    UINT16  _radius;
    UINT16  _borderWidth;
    UINT16  _color;
};

class StaticText : public Control
{
public:
    typedef Control inherited;
    StaticText(Container* pParent, const Point& leftTop, const Size& CSize);
    virtual ~StaticText();
    virtual void Draw(bool bPost);

    void SetText(UINT8 *pText) { _pText = pText; }
    UINT8* GetText() { return _pText; }

    void SetStyle(UINT16 size, UINT16 color, UINT16 h_aligh, UINT16 v_align)
    {
        _txtSize = size;
        _txtColor = color;
        _hAlign = h_aligh;
        _vAlign = v_align;
    }

    void SetColor(UINT16 color) { _txtColor = color; }
    void SetColor(UINT16 txt_color, UINT16 bg_color)
    {
        _txtColor = txt_color;
        _bDrawBG = true;
        _bgColor = bg_color;
    }

private:
    UINT8   *_pText;
    UINT16  _txtSize;
    UINT16  _txtColor;

    UINT16  _hAlign;
    UINT16  _vAlign;

    bool    _bDrawBG;
    UINT16  _bgColor;
};

class Round : public Control
{
public:
    typedef Control inherited;
    Round(
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 color);
    virtual ~Round();
    virtual void Draw(bool bPost);

    void setParams(const Point& center, UINT16 radius, UINT16 color)
    {
        _center = center;
        _radius = radius;
        _color = color;
    }

private:
    Point  _center;
    UINT16 _radius;
    UINT16 _color;
};

class Fan : public Control
{
public:
    typedef Control inherited;
    Fan(
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 fan_color);
    virtual ~Fan();
    virtual void Draw(bool bPost);
    void setAngles(float start_angle, float end_angle);
    void setColor(UINT16 color) { _fan_color = color; }

private:
    Point   _center;
    UINT16  _radius;
    UINT16  _fan_color;
    float   _start_angle;
    float   _end_angle;
    bool    _bDirty;
};

class FanRing : public Control
{
public:
    typedef Control inherited;
    FanRing(
        Container* pParent
        , const Point& center
        , UINT16 radius_outer
        , UINT16 radius_inner
        , UINT16 fan_color
        , UINT16 bg_color);
    virtual ~FanRing();
    virtual void Draw(bool bPost);
    void setAngles(float start_angle, float end_angle);

private:
    Point  _center;
    UINT16 _radius_outer;
    UINT16 _radius_inner;
    UINT16 _fan_color;
    UINT16 _bg_color;
    float  _start_angle;
    float  _end_angle;
    bool   _bDirty;
};

class RoundCap : public Control {
public:
    typedef Control inherited;
    RoundCap(
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 color);
    virtual ~RoundCap();
    virtual void Draw(bool bPost);
    void setAngle(float angle);
    void setRatio(float ratio) { _ratio = ratio; }
    void setColor(float color) { _color = color; }

private:
    Point   _center;
    UINT16  _radius;
    UINT16  _color;
    float   _angle;
    float   _ratio;
    bool    _bDirty;
};

class DrawingBoard : public Control {
public:
    typedef Control inherited;
    DrawingBoard(Container* pParent, const Point& leftTop, const Size& CSize, UINT16 color);
    virtual ~DrawingBoard();
    virtual void Draw(bool bPost);

    void drawPoint(const Point& point);
    void clearBoard();

private:
    UINT16  _brushColor;
};

class Button : public Control {
public:
    typedef Control inherited;
    Button(
        Container* pParent,
        const Point &leftTop, const Size &CSize,
        UINT16 colorNormal, UINT16 colorPressed);
    virtual ~Button();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setOnClickListener(OnClickListener *listener) { _listener = listener; }

    void SetText(UINT8 *pText, UINT16 size, UINT16 color)
    {
        _pText = pText;
        _txtSize = size;
        _txtColor = color;
    }
    UINT8* GetText() { return _pText; }
    void SetTextAlign(UINT8 h, UINT8 v) { _hAlign = h; _vAlign = v; }

    void SetStyle(UINT16 cornerRadius, float alpha = 1.0)
    {
        _cornerRadius = cornerRadius;
        _alpha = alpha;
    }

private:
    OnClickListener *_listener;

    bool    _bPressed;
    UINT16  _btnColorNormal;
    UINT16  _btnColorPressed;

    UINT8   *_pText;
    UINT16  _txtSize;
    UINT16  _txtColor;
    UINT8   _hAlign;
    UINT8   _vAlign;

    int     _cornerRadius;
    float   _alpha;
};

class PngImage : public Control {
public:
    typedef Control inherited;
    PngImage(Container* pParent, const Point& leftTop, const Size& CSize, char *path);
    virtual ~PngImage();
    virtual void Draw(bool bPost);

private:
    char *_imagePath;
};

class BmpImage : public Control
{
public:
    typedef Control inherited;
    BmpImage
        (Container* pParent
        ,const Point& leftTop
        ,const Size& size
        ,const char *path
        ,const char *bmpName);
    virtual ~BmpImage();
    virtual void Draw(bool bHilight);
    void setSource(const char *path, const char *bmpName);

private:
    CBmp  *_pBmp;
    char  _dirName[32];
    char  _fileName[32];
};

class ImageButton : public Control
{
public:
    typedef Control inherited;
    ImageButton
        (Container* pParent
        ,const Point& leftTop
        ,const Size& size
        ,const char *path
        ,const char *bmpNormal
        ,const char *bmpPressed);
    virtual ~ImageButton();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setSource(const char *path,
        const char *bmpNormal,
        const char *bmpPressed);
    void setOnClickListener(OnClickListener *listener) { _listener = listener; }

private:
    char  _dirName[32];
    char  _fileNormal[32];
    char  _filePressed[32];
    CBmp  *_pBmpNormal;
    CBmp  *_pBmpPressed;
    bool  _bPressed;
    OnClickListener *_listener;
};


class TabIndicator : public Control
{
public:
    typedef Control inherited;
    TabIndicator
        (Container* pParent
        ,const Point& leftTop
        ,const Size& size
        ,int num
        ,int hightlight_idx
        ,int radius
        ,COLOR colorActive
        ,COLOR colorInactive);
    TabIndicator
        (Container* pParent
        ,const Point& leftTop
        ,const Size& size
        ,int num
        ,int hightlight_idx
        ,const char *path
        ,const char *bmpActive
        ,const char *bmpInactive);
    virtual ~TabIndicator();
    virtual void Draw(bool bHilight);
    void setHighlight(int index) { _indexHighlight = index; }

private:
    UINT16  _radius;
    COLOR   _colorActive;
    COLOR   _colorInactive;

    CBmp  *_pBmpActive;
    CBmp  *_pBmpInactive;

    int   _num;
    int   _indexHighlight;
};

class QrCodeControl : public Control
{
public:
    typedef Control inherited;
    QrCodeControl
        (Container *pParent
        ,const Point& leftTop
        ,const Size& size);
    virtual ~QrCodeControl();
    virtual void Draw(bool bPost);
    void setString(char *string);

private:
    void createQrCodeBmp();

    char    _string[64];
    int     _code_width;
    QRcode  *_qrcode;

    COLOR   *_bmpBuf;
    CBmp    *_pBmp;
};

class RadioGroup : public Control
{
public:
    typedef Control inherited;
    RadioGroup
        (Container *pParent
        ,const Point& leftTop
        ,const Size& size);
    virtual ~RadioGroup();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setItems(char **names, int count) { _names = names; _count = count; }
    void setListener(OnRadioChangedListener *listener) { _listener = listener; }

private:
    OnRadioChangedListener *_listener;

    char     **_names;
    UINT16   _count;
    UINT16   _idxChecked;

    UINT16   _rButton;
    UINT16   _clrButton;
    UINT16   _bdwidth;
    UINT16   _rChecked;
    UINT16   _clrChecked;
    UINT16   _interval;

    UINT16   _txtSize;
    UINT16   _txtColor;
};

class Pickers : public Control
{
public:
    typedef Control inherited;
    Pickers
        (Container *pParent
        ,const Point& leftTop
        ,const Size& size);
    virtual ~Pickers();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setItems(char **names, int count) { _names = names; _count = count; }
    void setIndex(UINT16 index) { _idxChecked = index; }

private:
    char     **_names;
    UINT16   _count;
    UINT16   _idxChecked;

    int      _offset;
    int      _base;
};


class Slider : public Control
{
public:
    typedef Control inherited;
    Slider
        (Container *pParent
        ,const Point& leftTop
        ,const Size& size);
    virtual ~Slider();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setListener(OnSliderChangedListener *listener) { _pListener = listener; }

private:
    OnSliderChangedListener *_pListener;

    bool     _bTriggered;
    int      _moveBase;

    UINT16  _colorBg;
    UINT16  _colorHglt;
    int     _percentage;
};

class DataSetObserver
{
public:
    virtual void notifyChanged() = 0;
};

class ListAdaptor
{
public:
    ListAdaptor() : _pObserver(NULL) {}
    virtual ~ListAdaptor() {}
    virtual UINT16 getCount() = 0;
    virtual Control* getItem(UINT16 position, Container* pParent, const Size& size) = 0;
    virtual Control* getItem(UINT16 position, Container* pParent) = 0;
    virtual Size& getItemSize() = 0;

    void setObserver(DataSetObserver *observer) { _pObserver = observer; }
    void notifyDataSetChanged() { if (_pObserver) { _pObserver->notifyChanged(); } }

protected:
    DataSetObserver *_pObserver;
};

class TextListAdapter : public ListAdaptor
{
public:
    TextListAdapter();
    virtual ~TextListAdapter();
    virtual UINT16 getCount();
    virtual Control* getItem(UINT16 position, Container* pParent, const Size& size);
    virtual Control* getItem(UINT16 position, Container* pParent);
    virtual Size& getItemSize() { return _itemSize; }

    void setTextList(UINT8 **pText, UINT16 num);
    void setItemStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed,
        UINT16 txtColor, UINT16 txtSize,
        UINT8 hAlign, UINT8 vAlign);

private:
    UINT8   **_pText;
    UINT16  _num;

    Size    _itemSize;
    UINT16  _colorNormal;
    UINT16  _colorPressed;
    UINT16  _txtColor;
    UINT16  _txtSize;
    UINT8   _hAlign;
    UINT8   _vAlign;
};

class List : public Container, public DataSetObserver
{
public:
    typedef Container inherited;
    List(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren);
    virtual ~List();

    virtual void notifyChanged();

    virtual CairoCanvas *GetCanvas();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setListener(OnListClickListener *listener) { _pListener = listener; }
    void setAdaptor(ListAdaptor *adapter);

private:
    void scroll(int distance);

    OnListClickListener *_pListener;

    CairoCanvas *_pListCanvas;

    ListAdaptor *_adaptor;

    Control     **_visibleList;
    UINT16      _maxVisible;
    UINT16      _numVisible;
    int         _topOffset;
    UINT16      _posFirstVisible;

    UINT16      _itemHeight;
    UINT16      _padding;

    int      _moveBase;
};

class Fragment : public Container
{
public:
    typedef Container inherited;
    Fragment(const char *name,
        Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren)
      : inherited(pParent, pOwner, leftTop, size, maxChildren)
      , _duringAnim(false)
      , _name(name)
      {}
    virtual ~Fragment() {}

    virtual void Draw(bool bPost);

    virtual void onGotFocus() = 0;
    virtual void onLostFocus() = 0;
    virtual bool OnEvent(CEvent *event) { return true; }
    virtual void onTimerEvent() {}

    bool processEvent(CEvent *event);

    bool startTimer(int t_ms, bool bLoop);
    bool cancelTimer();

    void setAnimStatus(bool in) { _duringAnim = in; }

protected:
    bool       _duringAnim;

private:
    static void TimerCB(void *para);
    const char *_name;
};

class ViewPager : public Container, public CThread
{
public:
    typedef Container inherited;
    ViewPager(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& size,
        UINT16 maxChildren);
    virtual ~ViewPager();
    virtual CairoCanvas *GetCanvas();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    virtual void main(void *);

    void setPages(Fragment **pages, UINT16 num) { _pPages = pages; _numPages = num; }
    void setListener(ViewPagerListener *listener) { _pListener = listener; }
    void setFocus(UINT16 index);
    void lostFocus();

private:
    void move(int distance);
    void adjustOnMoveEnd();

    void postToDisplay();

    CSemaphore     *_pWaitEvent;
    CMutex         *_pLock;

    ViewPagerListener *_pListener;

    CairoCanvas   *_pViewPagerCanvas;
    UINT16        _leftOffset;

    int           _indexFocus;
    UINT16        _indexNextVisible;
    Fragment      **_pPages;
    UINT16        _numPages;

    int           _flick;
    bool          _duringAnim;
    bool          _towardsLeft;
};

};

#endif

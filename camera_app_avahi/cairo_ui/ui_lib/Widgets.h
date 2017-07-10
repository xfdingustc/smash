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

#include <pthread.h>

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
        , _pCanvas(NULL)
        , _bgColor(0x0000)
    {
    }
    virtual ~Panel();
    virtual CairoCanvas *GetCanvas();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setCanvas(CairoCanvas *canvas) { _pCanvas = canvas; }
    void setBgColor(UINT16 color) { _bgColor = color; }

private:
    CairoCanvas *_pCanvas;
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
    Point   _start;
    Point   _end;
    UINT16  _width;
    COLOR   _color;
    bool    _bDash;
};

// TODO: implement it with adding property to UIRectangle
class TranslucentRect : public Control
{
public:
    typedef Control inherited;
    TranslucentRect(Container* pParent, const Point& point, const Size& CSize);
    virtual ~TranslucentRect();
    virtual void Draw(bool bPost);

    void setStyle(UINT16 c1, float a1, UINT16 c2, float a2, bool bhorizontal)
    {
        _color1 = c1;
        _alpha1 = a1;
        _color2 = c2;
        _alpha2 = a2;
        _bHorizontal = bhorizontal;
    }

private:
    COLOR   _color1;
    COLOR   _color2;
    float   _alpha1;
    float   _alpha2;
    bool    _bHorizontal; // TODO: support more directions
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

    void setCornerRadius(UINT16 radius) { _cornerRaidus = radius; }

private:
    Point   _start;
    Point   _end;
    UINT16  _width;
    COLOR   _color;
    UINT16  _cornerRaidus;
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

    void setColor(UINT16 color)
    {
        _color = color;
    }

private:
    Point   _center;
    UINT16  _radius;
    UINT16  _borderWidth;
    UINT16  _color;
};

class Arc : public Control
{
public:
    typedef Control inherited;
    Arc(
        Container* pParent
        , const Point& center
        , UINT16 radius
        , UINT16 borderWidth
        , UINT16 color);
    virtual ~Arc();
    virtual void Draw(bool bPost);

    void setParams(const Point& center, UINT16 radius, UINT16 borderWidth, UINT16 color)
    {
        _center = center;
        _radius = radius;
        _borderWidth = borderWidth;
        _color = color;
    }

    void setColor(UINT16 color)
    {
        _color = color;
    }

    void setAngles(float start_angle, float end_angle)
    {
        _start_angle = start_angle;
        _end_angle = end_angle;
    }

private:
    Point   _center;
    UINT16  _radius;
    UINT16  _borderWidth;
    UINT16  _color;
    float   _start_angle;
    float   _end_angle;
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

    void SetStyle(UINT16 size, UINT16 fonttype, UINT16 color, UINT16 h_aligh, UINT16 v_align)
    {
        _txtSize = size;
        _fonttype = fonttype;
        _txtColor = color;
        _hAlign = h_aligh;
        _vAlign = v_align;
    }

    void SetAlpha(float alpha) { _alpha = alpha; }
    void SetColor(UINT16 color) { _txtColor = color; }
    void SetLineHeight(UINT16 height) { _line_height = height; }
    void SetFont(UINT16 fonttype) { _fonttype = fonttype; }

private:
    UINT8   *_pText;
    UINT16  _fonttype;
    UINT16  _txtSize;
    UINT16  _line_height;
    UINT16  _txtColor;
    UINT16  _hAlign;
    UINT16  _vAlign;
    float   _alpha;
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

    void setAlpha(float alpha) { _alpha = alpha; }

    void setColor(UINT16 color) { _color = color; }

private:
    Point  _center;
    UINT16 _radius;
    UINT16 _color;
    float  _alpha;
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
    void setRadius(UINT16 radius_outer, UINT16 radius_inner);
    void setColor(UINT16 fan_color, UINT16 bg_color);

private:
    Point  _center;
    UINT16 _radius_outer;
    UINT16 _radius_inner;
    UINT16 _fan_color;
    UINT16 _bg_color;
    float  _start_angle;
    float  _end_angle;
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

    void SetText(UINT8 *pText, UINT16 size, UINT16 color,
        UINT16 font = FONT_ROBOTOMONO_Medium,
        UINT16 line_height = 0)
    {
        _pText = pText;
        _txtSize = size;
        _txtColor = color;
        _fonttype = font;
        _line_height = line_height;
    }
    UINT8* GetText() { return _pText; }
    void SetTextAlign(UINT8 h, UINT8 v, UINT8 padding = 4)
    {
        _hAlign = h;
        _vAlign = v;
        _padding = padding;
    }
    void SetColor(UINT16 colorNormal, UINT16 colorPressed)
    {
        _btnColorNormal = colorNormal;
        _btnColorPressed = colorPressed;
    }
    void SetStyle(UINT16 cornerRadius, float alpha = 1.0)
    {
        _cornerRadius = cornerRadius;
        _alpha = alpha;
    }
    void SetBorder(bool with_border, UINT16 border_color, UINT16 border_width) {
        _bWithBorder = with_border;
        _btnColorBorder = border_color;
        _btnWidthBorder = border_width;
    }

private:
    OnClickListener *_listener;

    bool    _bPressed;
    UINT16  _btnColorNormal;
    UINT16  _btnColorPressed;

    bool    _bWithBorder;
    UINT16  _btnColorBorder;
    UINT16  _btnWidthBorder;

    UINT8   *_pText;
    UINT16  _fonttype;
    UINT16  _txtSize;
    UINT16  _line_height;
    UINT16  _txtColor;
    UINT8   _hAlign;
    UINT8   _vAlign;
    UINT8   _padding;
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
    char  _dirName[128];
    char  _fileName[128];
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
    char  _dirName[128];
    char  _fileNormal[128];
    char  _filePressed[128];
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

    char    _string[128];
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
        ,const Size& viewportSize
        ,const Size& canvasSize);
    virtual ~Pickers();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setItems(char **names, int count) { _names = names; _count = count; }
    void setIndex(UINT16 index) { _idxChecked = index; }
    void setViewPort(const Point& leftTop);
    void setStyle(UINT16 cbg, UINT16 cn, UINT16 ch,
        UINT16 fontsize, UINT16 font, UINT16 itemheight, UINT16 padding)
    {
        _colorBG = cbg;
        _colorNormal = cn;
        _colorHighligt = ch;
        _fontSize = fontsize;
        _fontType = font;
        _itemHeight = itemheight;
        _padding = padding;
    }

    void setListener(OnPickersListener *listener) { _listener = listener; }

private:
    OnPickersListener *_listener;

    CairoCanvas *_pPickerCanvas;

    char     **_names;
    UINT16   _count;
    int      _idxChecked;

    UINT16   _colorBG;
    UINT16   _colorNormal;
    UINT16   _colorHighligt;
    UINT16   _fontSize;
    UINT16   _fontType;

    UINT16   _itemHeight;
    UINT16   _padding;

    Point    _viewPortLft;
    Size     _viewPortSize;

    bool     _bTriggered;
    int      _moveBase;
};

class Scroller : public Container
{
public:
    typedef Container inherited;
    Scroller(Container* pParent,
        Window* pOwner,
        const Point& leftTop,
        const Size& viewportSize,
        const Size& canvasSize,
        UINT16 maxChildren);
    virtual ~Scroller();
    virtual CairoCanvas *GetCanvas();
    virtual void Draw(bool bPost);
    virtual bool OnEvent(CEvent *event);

    void setViewPort(const Point& leftTop, const Size& size);
    void setScrollable(bool scrollable);

    Container* getViewRoot() { return _pViewRoot; }

private:
    CairoCanvas *_pScrollerCanvas;

    Panel    *_pViewRoot;

    UINT16   _colorBG;
    Point    _viewPortLft;
    Size     _viewPortSize;

    bool     _bScrollable;
    bool     _bTriggered;
    int      _moveBase;
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

    void setValue(int value);
    void setStepper(int stepper);
    void setColor(UINT16 colorBg, UINT16 colorHglt)
    {
        _colorBg = colorBg;
        _colorHglt = colorHglt;
    }
    void setListener(OnSliderChangedListener *listener) { _pListener = listener; }

private:
    OnSliderChangedListener *_pListener;

    bool     _bTriggered;
    int      _moveBase;

    UINT16  _colorBg;
    UINT16  _colorHglt;
    UINT16  _radius;
    int     _percentage;
    int     _stepper;
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
    void setItemStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed,
        UINT16 txtColor, UINT16 txtSize, UINT16 font,
        UINT8 hAlign, UINT8 vAlign);

private:
    UINT8   **_pText;
    UINT16  _num;

    Size    _itemSize;
    UINT16  _colorNormal;
    UINT16  _colorPressed;
    UINT16  _txtColor;
    UINT16  _txtSize;
    UINT16  _fontType;
    UINT8   _hAlign;
    UINT8   _vAlign;
};

class TextImageAdapter : public ListAdaptor
{
public:
    TextImageAdapter();
    virtual ~TextImageAdapter();
    virtual UINT16 getCount();
    virtual Control* getItem(UINT16 position, Container* pParent, const Size& size);
    virtual Control* getItem(UINT16 position, Container* pParent);
    virtual Size& getItemSize() { return _itemSize; }

    void setTextList(UINT8 **pText, UINT16 num);
    void setTextStyle(const Size& size, UINT16 colorNormal, UINT16 colorPressed,
        UINT16 txtColor, UINT16 txtSize, UINT16 font,
        UINT8 hAlign, UINT8 vAlign);
    void setImageStyle(const Size& size,
        const char *path,
        const char *normal,
        const char *pressed,
        bool bLeft);

    void setImagePadding(UINT8 padding) { _paddingImage = padding; }

private:
    UINT8   **_pText;
    UINT16  _num;

    Size    _itemSize;

    UINT16  _colorNormal;
    UINT16  _colorPressed;
    UINT16  _txtColor;
    UINT16  _txtSize;
    UINT16  _fontType;
    UINT8   _hAlign;
    UINT8   _vAlign;

    Size       _imageSize;
    const char *_imagePath;
    const char *_imageNormal;
    const char *_imagePressed;
    bool       _bImageLeft;
    UINT8      _paddingImage;
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

    Window              *_pOwner;
    OnListClickListener *_pListener;

    CairoCanvas *_pListCanvas;

    ListAdaptor *_adaptor;

    Panel       *_pViewRoot;
    UINT16      _colorBG;

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
      , _name(name)
      {}
    virtual ~Fragment() {}

    virtual void Draw(bool bPost);

    virtual void onResume() = 0;
    virtual void onPause() = 0;
    virtual bool OnEvent(CEvent *event) { return true; }
    virtual void onTimerEvent() {}

    bool processEvent(CEvent *event);

    bool startTimer(int t_ms, bool bLoop);
    bool cancelTimer();

    const char* getFragName() { return _name; }

private:
    static void TimerCB(void *para);
    const char *_name;
};

class ViewPager : public Container
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

    void setPages(Fragment **pages, UINT16 num) { _pPages = pages; _numPages = num; }
    void setListener(ViewPagerListener *listener) { _pListener = listener; }
    void setFocus(UINT16 index);

    Fragment *getCurFragment() { return _pPages[_indexFocus]; }

private:
    static void* ThreadFunc(void* lpParam);
    void main();

    void move(int distance);
    void adjustOnMoveEnd();

    void postToDisplay();

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

    bool          _bExit;
    pthread_t     _hThread;
    CSemaphore    *_pWaitEvent;
    CMutex        *_pLock;
};

};

#endif

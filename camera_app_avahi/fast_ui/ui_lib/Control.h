/*******************************************************************************
**       Control.h
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a  6-28-2011 linnsong CREATE AND MODIFY
**
*******************************************************************************/

#ifndef __INCControl_h
#define __INCControl_h
//#ifdef WIN32

namespace ui{
//#endif

/*******************************************************************************
* <Forward declarations>
*******************************************************************************/ 
class CWnd;
class CControl;
class CStaticText;
class CContainer;
class CPanel;
class CBrush;
class CFont;
class CLanguageItem;
/*******************************************************************************
* <Macros>
*******************************************************************************/

/*******************************************************************************
* <Types>
*******************************************************************************/
/*
0: hiden;
1: disabled;
2,3: hAlign;
4,5: vAlign;
*/
enum H_ALIGN
{
    LEFT = 0,
    CENTER,
    RIGHT,    
};

enum V_ALIGN
{
    TOP = 0,
    MIDDLE,
    BOTTOM,
};

class CControl
{
    
public:
    CControl(CContainer* pParent, CPoint leftTop, CSize CSize);
    virtual ~CControl();

    virtual CPoint GetAbsPos();
    virtual CWnd* GetOwner();
    virtual void Draw(bool bHilight);
	virtual void Clear();
    virtual bool OnEvent(CEvent *event);
	virtual bool isTouchOn(int x, int y);	

    CContainer* GetParent() { return _pParent; }
    void SetParent(CContainer* pParent) { _pParent = pParent; }
    CSize GetSize() { return _size; }
    void SetSize(CSize size) { _size = size; }
    CPoint GetRelativePos() { return _leftTop; }
    void SetRelativePos(CPoint leftTop) { _leftTop = leftTop; }
    bool IsHiden() { return ((_property & 0x01) != 0); }
    void SetHiden() { _property = _property | 0x01; }
	void SetShow(){ _property = _property & 0xfffffffe; };
    bool IsDisabled() { return ((_property & 0x02) != 0); }
    void SetDisabled() { _property = _property | 0x02; }
    H_ALIGN GetHAlign() { return (H_ALIGN)((_property & 0x0C) >> 2); }
    void SetHAlign(H_ALIGN align) { _property = _property | ((UINT32)align << 2); }
    V_ALIGN GetVAlign() { return (V_ALIGN)((_property & 0x30) >> 4); }
    void SetVAlign(V_ALIGN align) { _property = _property | ((UINT32)align << 4); }

private:
    CContainer* _pParent;
    CPoint      _leftTop;
    CSize       _size;
    UINT32      _property;    
};

class CStaticText : public CControl
{
public:
    typedef CControl inherited;
    CStaticText
        (CContainer* pParent
        , CPoint leftTop
        , CSize CSize
        ): inherited(pParent, leftTop, CSize)
        ,_pText(0)
        ,_xMarginL(0)
        ,_xMarginR(0)
        ,_yMarginT(0)
        ,_yMarginB(0)
        ,_pItem(0)
    {
    }
    virtual ~CStaticText();    
    void SetText(UINT8 *pText) { _pText = pText; }
    UINT8* GetText(){return _pText;};
    
    void SetStyle_NormalBrush(UINT16 brush){_brush = brush;}
    void SetStyle_NormalFont(UINT16 font){_font = font;}
    void SetStyle_HilightBrush(UINT16 brushHi){_brushHi = brushHi;}
    void SetStyle_HilightFont(UINT16 fontHi){_fontHi = fontHi;}
    void SetStyle_XMargin
		(int xMarginL
		, int xMarginR)
		{
			_xMarginL = xMarginL;
			_xMarginR = xMarginR;
		}
    void SetStyle_YMargin
		(int yMarginT
		, int yMarginB)
		{
			_yMarginT = yMarginT;
			_yMarginB = yMarginB;
		}
    void SetStyle
		(UINT16 brush
		, UINT16 font
		, UINT16 brushHi
		, UINT16 fontHi
		, int xMarginL
		, int xMarginR
		, int yMarginT
		, int yMarginB
		)
    {
        _brush = brush;
        _font = font;
        _brushHi = brushHi; 
        _fontHi = fontHi;
        _xMarginL = xMarginL;
		_xMarginR = xMarginR;
        _yMarginT = yMarginT;
		_yMarginB = yMarginB;
    }
	void SetStyle
		(UINT16 brush
		, UINT16 font
		, UINT16 brushHi
		, UINT16 fontHi
		)
    {
        _brush = brush;
        _font = font;
        _brushHi = brushHi; 
        _fontHi = fontHi;
    }

	void AdjustVertical(int a){_yMarginT+=a;};

    virtual void Draw(bool bHilight);
	virtual void DrawText(CFont* pFont);
    virtual bool OnEvent(CEvent *event);

	void SetLanguageItem(CLanguageItem* p){_pItem = p;};
protected:
    
private:
    UINT8  *_pText;
    UINT16 _brush; 
    UINT16 _font;
    UINT16 _brushHi; 
    UINT16 _fontHi;
    int _xMarginL;
	int _xMarginR;
	int _yMarginT;
    int _yMarginB;

	CLanguageItem *_pItem;
};

enum BtnState
{
	BtnState_normal = 0,
	BtnState_invalid,
	BtnState_pressed,
	BtnState_hilight,
};

class CBmpControl : public CControl
{
public:
	typedef CControl inherited;
	CBmpControl
		(CContainer* pParent
		,CPoint leftTop
		,CSize size
		,const char *path
		,const char *bmpName);
	~CBmpControl();
	virtual void Draw(bool bHilight);
    void setSource(const char *path, const char *bmpName);
private:
	CBmp* _pBmp;
};


class CBmpBtn : public CControl
{
public:
	typedef CControl inherited;
	CBmpBtn
		(CContainer* pParent
		,CPoint leftTop
		,CSize size
		,int id
		,CBmp *bmp);
	~CBmpBtn();
	virtual void Draw(bool bHilight);
	virtual bool OnEvent(CEvent *event);
	void SetElapsed(bool b);
private:
	CBmp* 	_pBtnBmp;
	UINT16	_btnID;
	BtnState	_state;
	bool 	_bElapsed;    // for button show atleast 0.1 sec
};

class CBtn : public CStaticText
{   
public:
    typedef CStaticText inherited;
    CBtn(CContainer* pParent, CPoint leftTop, CSize CSize, int ID);
    virtual ~CBtn();
    virtual void Draw(bool bHilight);
    virtual bool OnEvent(CEvent *event);
    
    static void SetStyle_NormalBrush(UINT16 brush){_brush = brush;}
    static void SetStyle_NormalFont(UINT16 font){_font = font;}
    static void SetStyle_HilightBrush(UINT16 brushHi){_brushHi = brushHi;}
    static void SetStyle_HilightFont(UINT16 fontHi){_fontHi = fontHi;}
    static void SetBtnStyle(UINT16 brush, UINT16 font, UINT16 brushHi, UINT16 fontHi)
    {
        _brush = brush;
        _font = font;
        _brushHi = brushHi; 
        _fontHi = fontHi;
    };
    
private:
    UINT16 _buttonID;

    static UINT16 _brush; 
    static UINT16 _font;
    static UINT16 _brushHi; 
    static UINT16 _fontHi;
};

class CContainer : public CControl
{
public:
    typedef CControl inherited;
    CContainer(CContainer* pParent, CWnd* pOwner, CPoint leftTop, CSize CSize, UINT16 maxChildren);
    virtual ~CContainer();

    virtual CWnd* GetOwner();    
    bool AddControl(CControl* pControl);
    bool RemoveControl(CControl* pControl);
    UINT16 GetChildrenCount() { return _count; };
    CControl* GetControl(UINT16 index);
    void SetOwner(CWnd* pOwner){_pOwner = pOwner;};
    virtual bool OnEvent(CEvent *event);
	int getNumber(){return _number;};
	
protected:
    
private:
    CWnd*       _pOwner;
    CControl**  _pControlList;
    UINT16      _number;
    UINT16      _count;
};

class CPanel : public CContainer
{
public:
	typedef CContainer inherited;
	CPanel
		(
			CContainer* pParent
			,CWnd* pOwner
			,CPoint leftTop
			,CSize size
			,UINT16 maxChildren
		):inherited(pParent, pOwner, leftTop, size, maxChildren)
		,_privateBrush(NULL)
		,_currentHilight(0)
	{
	}
	virtual ~CPanel();

	static void SetStyle_HilightBrush(UINT16 brush){_brush = brush;}
	static void SetStyle_NormalBrush(UINT16 brushHi){_brushHi = brushHi;}
	static void SetStyle(UINT16 brush, UINT16 brushHi)
	{
	    _brush = brush; 
	    _brushHi = brushHi; 
	}

	virtual void Draw(bool bHilight);
	virtual bool OnEvent(CEvent *event);
	void SetPrivateBrush(CBrush *p){_privateBrush = p;};
	void SetCurrentHilight(int i){_currentHilight = i;};
	void SetCurrentHilight(CControl* p);
	int GetCurrentHilight(){return _currentHilight;};
protected:
	static UINT16	_brush;
	static UINT16	_brushHi;    
	CBrush		*_privateBrush;
	UINT16		_currentHilight;
    //static UINT16 _font;
};

class CPanelWithTopBtmBar : public CPanel
{
	public:
	typedef CPanel inherited;
	CPanelWithTopBtmBar
		(
			CContainer* pParent
			,CWnd* pOwner
			,CPoint leftTop
			,CSize size
			,UINT16 maxChildren
		);
	virtual ~CPanelWithTopBtmBar(){};
	virtual void Draw(bool bHilight);

	void SetBarStyle(int tph, int btmh, int brush)
	{
		_tpH = tph;
		_btmH = btmh;
		_barBrush = brush;
	};
private:
	int _tpH;
	int _btmH;
	int _barBrush;
};

class CTouchAdaptor : public CPanel
{
public:
	typedef CPanel inherited;
	CTouchAdaptor(CContainer* pParent, CWnd* pOwner, CPoint leftTop, CSize CSize, UINT16 maxChildren);
	~CTouchAdaptor();

	virtual bool OnEvent(CEvent *event);
protected:

private:
	
};


class CSecTimerBtn : public CStaticText
{   
public:
	typedef CStaticText inherited;
	CSecTimerBtn(CContainer* pParent
		,CPoint leftTop
		,CSize CSize);
	~CSecTimerBtn();
	void Start();
	void End();
	void Reset();
	void Update();

private:
	UINT64	_iCounter;		// in secs
	char 	_buffer[20];	
	UINT32 	_sec_low;
	UINT32	_sec_hi;	
	UINT64	_sec;
	int		_timer;
};


class CSwitcher : public CControl
{
#define Switcher_steps 20			// steps
#define Switcher_dynamic_time (2/10) //sec

public:
	typedef CControl inherited;
	CSwitcher
		(CContainer* pParent
		,CPoint leftTop
		,CSize CSize
		,CBmp* pFg
		,CBmp* pBg
		,CPoint stP
		,CPoint enP
		,int id
		);
	~CSwitcher();

	virtual void Draw(bool bHilight);
	virtual bool OnEvent(CEvent *event);
	void MovePosition();
	void SetOn(bool on){_bOn = on;};
	
private:
	CBmp* 	_pPointBmp;
	CBmp*	_pBgBmp;
	CPoint	_onPoint;
	CPoint	_offPoint;
	bool	_bOn;
	int		_position; // 0 ~ Switcher_steps-1
	UINT32	_btnID;
	int		_timerID;
};

class CMenu;
class CPosiBarV;
class CUIAction;

class CMenuItem : public CStaticText
{
public:
	typedef CStaticText inherited;
	CMenuItem
		(CMenu* menu
		, CLanguageItem* title
		, int index
		, int yOffset);
	~CMenuItem();
	virtual bool OnEvent(CEvent *event);
	void SetNextWnd(CWnd* nextWnd);
	void SetAction(CUIAction* action);
	virtual void Draw(bool bHilight);

	void SetTitle(CLanguageItem* item){_pTitle = item;};
	CLanguageItem* GetTitle(){return _pTitle;};
	void SetSymbles
		(CBmp* r
		,CBmp* u
		,CBmp* d
		,CBmp* ok			
		)
	{
		_right_symble = r;
		_up_symble = u;
		_down_symble = d;
		_ok_symble = ok;
	}

	void SetRightSymble(bool on){_bRight = on;};
	void SetUpSymble(bool on){_bUp = on;};
	void SetDownSymble(bool on){_bDown = on;};
	void SetOkSymble(bool on){_bOk = on;};
	
private:
	bool bHightlight;
	CWnd *_nextWnd;
	CUIAction *_action;
	CLanguageItem *_pTitle;

	CBmp* 	_right_symble;
	bool 	_bRight;
	CBmp* 	_up_symble;
	bool 	_bUp;
	CBmp* 	_down_symble;
	bool 	_bDown;
	CBmp* 	_ok_symble;
	bool 	_bOk;

};
class CMenuTitle : public CStaticText
{
public:
	typedef CStaticText inherited;
	CMenuTitle
		(CMenu* menu
		, CLanguageItem* title
		, int height);
	~CMenuTitle();
	virtual void Draw(bool bHilight);
    void SetLeftSymble(bool on){_bLeft = on;};
	void SetTitle(CLanguageItem* item){_pTitle = item;};
	void SetSymble(CBmp* l){_left_symble = l;};
private:
	CLanguageItem *_pTitle;
	
	CBmp* 	_left_symble;
	bool 	_bLeft;
};


class CMenu : public CContainer
{
public:
	typedef CContainer inherited;
	CMenu
		(CContainer* pParent
		, CWnd* pOwner
		, CPoint leftTop
		, int width
		, int itemHeight
		, int barWidth
		, int itemNum
		, int titleHigh);
	~CMenu();
	virtual bool OnEvent(CEvent *event);
	CSize getItemSize(){return _itemSize;};
	int GetBarWidth(){return _barWidth;};
	virtual void Draw(bool bHilight);
	void Clean()
		{			
			_pTitle->SetText(NULL);
			_pTitle->SetTitle(NULL);
			_ppStringlist = NULL;
			_stringNum = 0;
			_ppItemList =  NULL;
			_stringNum = 0;
		};
	void SetTitle(char* title);
	void SetTitleItem(CLanguageItem* item);

	void setStringList(char** list, int num);
	void setItemList(CLanguageItem** pTxt, int num);

	void setAction(CUIAction* a){_pAction = a;};
	void resetFocus(){_currentHilight = 0;_currentFirst = 0;};

	static void DeleteSymbles();
	static void LoadSymbles(const char* path);
	
	int getCurIndAtFullList(){return _currentHilight + _currentFirst;};
private:
	CMenuTitle *_pTitle;
	int 		_currentHilight;
	CSize 		_itemSize;
	int			_itemNum;
	int			_barWidth;

	//bool			_bItem;
	char**			_ppStringlist;
	
	int				_stringNum;
	CLanguageItem	**_ppItemList;
	
	int				_currentFirst;

	CPosiBarV* 		_pBar;
	CUIAction		*_pAction;
	
public:
	static CBmp* _left_symble;
	static CBmp* _right_symble;
	static CBmp* _up_symble;	
	static CBmp* _down_symble;
};

class CPosiBarH;
class CHSelector : public CContainer
{
public:
	typedef CContainer inherited;
	CHSelector(CContainer* pParent
		, CWnd* pOwner
		, CPoint leftTop
		, CSize ViewWindow
		, int barHeigh
		, int MaxitemNum
		, int itemPitch);
	~CHSelector();
	virtual bool OnEvent(CEvent *event);
	virtual void Draw(bool bHilight);

	void SetStyle(UINT16 brush, UINT16 brushHi)
	{
	    _brush = brush; 
		_brushHi = brushHi;
	}
	void SetCurrentHilight(int i){_currentHilight = i;};
	int GetCurrentHilight(){return _currentHilight + _currentFirst;};
	
private:
	void Adjust(bool bLeft);
	//CControl**	_pItemList;
	//int			_pItemNum;
	int				_currentFirst;
	int				_currentHilight;
	int				_showNum;
	int				_itemPitch;
	CPosiBarH		*_pPosiBar;
	UINT16			_brush;
	UINT16			_brushHi;
};


class CPosiBarV : public CControl
{
public:
	typedef CControl inherited;
	CPosiBarV(CContainer* pParent
		, CPoint leftTop
		, CSize CSize);
	~CPosiBarV();
	virtual void Draw(bool bHilight);
	void setLength
		(int wholeLen
		, int segLen)
		{_wholeLen = wholeLen; _segLen = segLen;};
	void SetPosi
		(int posi)
		{_currentPosi = posi;};
	void SetStyle
		(int backBrush
		, int frontBrush)
		{_backBrush = backBrush;_frontBrush = frontBrush;};
protected:
	int _wholeLen;
	int _segLen;
	int _currentPosi;
	int _backBrush;
	int _frontBrush;
};


class CPosiBarH : public CPosiBarV
{
public:
	typedef CPosiBarV inherited;
	CPosiBarH(CContainer* pParent
		, CPoint leftTop
		, CSize CSize);
	~CPosiBarH();
	virtual void Draw(bool bHilight);
	
private:
};

class CPosiBarSegH : public CPosiBarV
{
public:
	typedef CPosiBarV inherited;
	CPosiBarSegH(CContainer* pParent
		, CPoint leftTop
		, CSize size
		, CPoint posi
		, CSize fgSize
		, int Seg
		, int interlace);
	~CPosiBarSegH();
	virtual void Draw(bool bHilight);
	
private:
	CPoint		_posi;
	CSize		_fSize;
	int			_seg;
	int			_interlace;
};

class CPosiBarV1 : public CPosiBarV
{
public:
	typedef CPosiBarV inherited;
	CPosiBarV1(CContainer* pParent
		, CPoint leftTop
		, CSize bkSize
		, CPoint posi
		, CSize fgSize);
	~CPosiBarV1();
	virtual void Draw(bool bHilight);
	
private:
	CPoint _posi;
	CSize _fSize;
};

class CSymbleWithTxt : public CStaticText
{
public:
	typedef CStaticText inherited;
	CSymbleWithTxt(CContainer* pParent
		, CPoint leftTop
		, CSize CSize
		, bool left
		);
	~CSymbleWithTxt();	
	virtual void Draw(bool bHilight);	
	void SetSymbles
		(int i
		, CBmp** ppBmp);
	void SetTitles
		(int i
		, CLanguageItem** ppItem)
		{	
			_ppTitle = ppItem;
			_titleNum = i;
		};
	void SetCurrentSymble(int i){_currentSymble = i;};
	int getCurrentSymble(){return _currentSymble;};
	CBmp* getCurrentSymbleBmp(){return _ppSymbles[_currentSymble];};
	void SetCurrentSymbleBmp(CBmp* bmp){_ppSymbles[_currentSymble] = bmp;};
	void SetCurrentTitle(int i){_currentTitle = i;};

	void SetRltvPosi(CPoint point);
private:
	CBmp** _ppSymbles;
	int		_symbleNum;
	int 	_currentSymble;
	bool	_bSymbleLeft;

	CLanguageItem 	**_ppTitle;
	int				_currentTitle;
	int				_titleNum;
	CPoint			_pBmpPosi;
};

class CDigEdit : public CStaticText
{
public:
	typedef CStaticText inherited;
	CDigEdit
		(CContainer* pParent
		, CPoint leftTop
		, CSize CSize
		, int mini
		, int max
		, int digiNum
		, int current
		);
	~CDigEdit();

	virtual void Draw(bool bHilight);
	virtual bool OnEvent(CEvent *event);
	bool SetCurrentValue(int v)
	{
		if((v >= _minValue)&&(v<= _maxValue))
		{
			_currentValue = v;
			return true;
		}
		else
			return false;
	};

	bool setMaxLmitation(int max);

	int getCurrentValue(){return _currentValue;};

	static void SetIndication
		(CBmp* pUp, CBmp*pDown, COLOR cleanC)
	{	
		_pUp = pUp;
		_pDown = pDown;
		_cleanC = cleanC;
	};

	void setSign(bool b){_bWithSign = b;};
	void setStep(int step){_step = step;};
	
private:
	int		_maxValue;
	int		_minValue;
	char	*_pTxt;
	int		_digNum;
	int		_currentValue;

	static CBmp *_pUp;
	static CBmp *_pDown;
	static COLOR _cleanC;

	bool	_bInEdit;
	bool 	_bWithSign;
	int		_step;
	
};

class CBlock : public CControl
{
public:
	typedef CControl inherited;
	CBlock
		(CContainer* pParent
		, CPoint leftTop
		, CSize CSize
		, UINT16 b
		);
	~CBlock();
	virtual void Draw(bool bHilight);
private:
	UINT16 _brush; 
};

/*
CStaticText;
CButton;
CBitmapButton;
CDateCtrl;
CTimeCtrl;
CMenuItem;
CListItem;
CTreeNode;
CEdit;
CComboBox;
CProgressBar;
CSlide;

CMenu;
CListCtrl;
CTreeCtrl;
CPanel;
*/

//#ifdef WIN32
}
//#endif
#endif

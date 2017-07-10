/*******************************************************************************
**       Style.h
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
**      01a  8- 9-2011 linnsong CREATE AND MODIFY
**
*******************************************************************************/

#ifndef  __INCStyle_h
#define __INCStyle_h
//#ifdef WIN32
namespace ui{
//#endif

/*******************************************************************************
* <Forward declarations>
*******************************************************************************/ 
class CControl;
/*******************************************************************************
* <Macros>
*******************************************************************************/
enum BRUSH_STYLE_LIST
{
    BSL_DEFAULT = 0,
    BSL_TEXT_NORMAL,
    BSL_TEXT_HILIGHT,
    BSL_BUTTON_NORMAL,
    BSL_BUTTON_HILIGHT,
    BSL_BUTTON_PRESS,
    BSL_PANEL_NORMAL,
	BSL_CLEAR,
	BSL_BLACK,
	BSL_PRIVATE,
	BSL_MENU_BG,
	BSL_MENU_TITLE,
	BSL_MENU_ITEM_NORMAL,
	BSL_MENU_ITEM_HILIGHT,
	BSL_BAR_BG,
	BSL_BAR_FRONT,
	BSL_NOTICE_BAKC,
	BSL_Bit_Black_Pannel,
	BSL_Bit_White_Pannel,
	BSL_Bit_White_border,
	BSL_Bit_battery_bmp,
    BSL_NUMBER,
};

enum FONT_STYLE_LIST
{
    FSL_DEFAULT = 0,
    FSL_TEXT_NORMAL,
    FSL_TEXT_HILIGHT,
    FSL_BUTTON_NORMAL,
    FSL_BUTTON_HILIGHT,
    FSL_BUTTON_PRESS,
    FSL_MENU_TITLE, 
    FSL_MENU_ITEM_NORMAL,
	FSL_MENU_ITEM_HILIGHT,
	FSL_NOTICE_BLACK,
	FSL_Bit_Black_Text,
	FSL_Bit_White_Text,
	FSL_Bit_White_Text_small,
	FSL_Bit_Black_Text_small,
    FSL_NUMBER,
};

#define CORNER_LEFTTOP 0x02
#define CORNER_LEFTBTM 0x04
#define CORNER_RIGHTTOP 0x01
#define CORNER_RIGHTBTM 0x08

static const char* WndResourceDir = "/usr/local/share/ui/";
/*******************************************************************************
* <Types>
*******************************************************************************/
class CBrush
{
public:
    CBrush(BRUSH_STYLE_LIST id);
    virtual ~CBrush();
    virtual void Draw(CControl *pControl) = 0;
	virtual void Draw(CPoint topLeft ,CSize size, CCanvas *pCanvas){};
	CCanvas* getCanvas(){return _pCanvas;};
protected:
	CCanvas *_pCanvas;
private:    
};

class CBrushFlat : public CBrush
{
public:
    CBrushFlat(BRUSH_STYLE_LIST id, UINT16 radius, UINT8 corner, UINT16 borderWidth, COLOR main, COLOR border);
    ~CBrushFlat();

    virtual void Draw(CControl *pControl);
	virtual void Draw(CPoint topLeft ,CSize size, CCanvas *pCanvas);
	COLOR getMainColor(){return _mainClr;};
private:
    UINT16	_cornerRadius;
    UINT8	_cornerStyle;    
    UINT16	_borderWidth;
    COLOR   _mainClr;
    COLOR   _borderClr;

};

class CBrushBmp : public CBrush
{
public:
	CBrushBmp(BRUSH_STYLE_LIST id, UINT16 radius, UINT8 corner, UINT16 borderWidth, CBmp* main, COLOR border);
	~CBrushBmp();
	virtual void Draw(CControl *pControl);
	void SetBmp(CBmp* pBmp){_pBmp = pBmp;};
private:
	UINT16		_cornerRadius;
	UINT8		_cornerStyle;    
	UINT16		_borderWidth;
	COLOR		_mainClr;
	CBmp		*_pBmp;
	COLOR		_borderClr;	
};
/*******************************************************************************
*
*
*/
class CFont
{
public:
    CFont(FONT_STYLE_LIST id, CSize CSize, COLOR forClr, COLOR bkColor, UINT16 rowHeight);
    ~CFont();
    void Draw(CPoint topleft, CPoint rightBtm,const UINT8* string, CCanvas *pcan);
    CSize GetStringOccupyCSize(const UINT8* string, UINT16 widthLimit, UINT16 heighLimit );
    
private:
    CSize _size;
    COLOR _forClr;
    COLOR _bkClr;
    UINT16 _rowHeight;
    //COLOR _borderClr;
    //UINT16 _borderWidth;
};

/*******************************************************************************
*
*
*/
class CStyle
{
    
public:
    static void Create();
    static void Destroy();
    static CStyle* GetInstance(){return _pInstance;}

    void RegistBrush(UINT16 id, CBrush *pBrush);
    void RegistFont(UINT16 id, CFont *pFont);
    CBrush* GetBrush(UINT16 id);
    CFont*  GetFont(UINT16 id);

protected:
    CStyle(UINT16 brushNumber, UINT16 fontNumber);
    ~CStyle();

private:
    static CStyle*  _pInstance;
    CBrush**        _pBrushPool;
    UINT16          _brushNumber;    
    CFont**         _pFontPool;
    UINT16          _fontNumber;

	static CBmp 	*_noticeBack;
};


//#ifdef WIN32
};
//#endif


#endif

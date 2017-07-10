/*******************************************************************************
**      Style.cpp
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Use of Transee Design code is governed by terms and conditions 
**      stated in the accompanying licensing statement.
**
**      Description:
**
**      Revision History:
**      -----------------
**      01a  8- 9-2011 linnsong CREATE AND MODIFY
**
*******************************************************************************/

/*******************************************************************************
* <Includes>
****************************************************************************** 
*/

// add included here
#ifdef WIN32
#include <windows.h>
#include "CEvent.h"
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h"
#include "CWinThread.h"
#include "Control.h"
#include "Style.h" 
#include "FontLibApi.h"
#else
#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "FontLibApi.h"
#include "Style.h"
#include "Wnd.h"
#endif

const char* WndResourceDir = "/usr/local/share/ui/";

namespace ui{

/*******************************************************************************
* <Macros>
*******************************************************************************/

/*******************************************************************************
* <Local Functions>
*******************************************************************************/

/*******************************************************************************
* <Local variable>
*******************************************************************************/
CStyle*  CStyle::_pInstance = NULL;

/*******************************************************************************
* <Function elements>
*******************************************************************************/
/*******************************************************************************
*   CBrush::CBrush
*   
*/ 
 CBrush::CBrush
    ( BRUSH_STYLE_LIST id
    ):_pCanvas(NULL)
{
    CStyle* pStyle = CStyle::GetInstance();
    if(pStyle != NULL)
        pStyle->RegistBrush(id, this);
}

/*******************************************************************************
*   CBrush::~CBrush
*   
*/ 
CBrush::~CBrush()
{ 
}

/*******************************************************************************
*   CBrushFlat::CBrushFlat
*   
*/ 
 CBrushFlat::CBrushFlat
    (
    BRUSH_STYLE_LIST id,
    UINT16 radius,
    UINT8 corner,
    UINT16 borderWidth,
    COLOR main,
    COLOR border
    ) : CBrush(id),
    _cornerRadius(radius),
    _cornerStyle(corner),    
    _borderWidth(borderWidth),
    _mainClr(main),
    _borderClr(border)
{
    
}

/*******************************************************************************
*   CBrushFlat::~CBrushFlat
*   
*/ 
 CBrushFlat::~CBrushFlat( )
{
    
}

/*******************************************************************************
*   CBrushFlat::Draw
*   
*/ 
void CBrushFlat::Draw
    (
    CControl *pControl
    )
{
    _pCanvas = pControl->GetOwner()->GetManager()->GetCanvas();
    CSize size;
    CPoint topLeft;//, rightBottom;
    size = pControl->GetSize();
    topLeft = pControl->GetAbsPos();
    this->Draw(topLeft, size, _pCanvas);
}

void CBrushFlat::Draw
	(CPoint topLeft
    ,CSize size
    ,CCanvas *pCanvas)
{
	CPoint rightBottom;
	rightBottom.Set(topLeft.x + size.width, topLeft.y +size.height); 
	if(pCanvas)
		pCanvas->DrawRect(topLeft, rightBottom, _cornerStyle, _cornerRadius, _borderWidth, _mainClr, _borderClr);
}

/*******************************************************************************
*   CBrushFlat::SetRoundCorner
*   

void CBrushFlat::SetRoundCorner
    (
    
    )
{ 
}*/ 

/*******************************************************************************
*   CBrushBmp::CBrushBmp
*/
CBrushBmp::CBrushBmp
	(
		BRUSH_STYLE_LIST id
		,UINT16 radius
		,UINT8 corner
		,UINT16 borderWidth
		,CBmp* main
		,COLOR border
	): CBrush(id)
	,_cornerRadius(radius)
	,_cornerStyle(corner)
	,_borderWidth(borderWidth)
	,_pBmp(main)
	,_borderClr(border)
{

}

/*******************************************************************************
*   CBrushBmp::~CBrushBmp
*/
CBrushBmp::~CBrushBmp()
{

}

/*******************************************************************************
*   CBrushBmp::Draw
*/
void CBrushBmp::Draw
	(
		CControl *pControl
	)
{
	_pCanvas = pControl->GetOwner()->GetManager()->GetCanvas();
	CSize size;
	CPoint topLeft, rightBottom;
	size = pControl->GetSize();
	topLeft = pControl->GetAbsPos();
	rightBottom.Set(topLeft.x + size.width, topLeft.y +size.height); 
	_pCanvas->DrawRect(topLeft, size, _pBmp, CCanvas::BmpFillMode_repeat, 0);
	
}

/*******************************************************************************
*   CFont::CFont
*   
*/ 
 CFont::CFont
    (
	    FONT_STYLE_LIST id,
	    CSize size,
	    COLOR forClr,
	    COLOR bkColor,
	    UINT16 rowHeight
    ) : _size(size),
    _forClr(forClr),
    _bkClr(bkColor),
    _rowHeight(rowHeight)
{
    CStyle* pStyle = CStyle::GetInstance();
    if(pStyle != NULL)
        pStyle->RegistFont(id, this);
}

/*******************************************************************************
*   CFont::CFont
*   
*/ 
CFont::~CFont()
{ 
}

/*******************************************************************************
*   CFont::Draw
*   
*/ 
void CFont::Draw
    (CPoint leftTop
	,CPoint rightBtm
	,const UINT8* string
	,CCanvas* pCav
    )
{
	CFontLibApi* pFontApi = CFontLibApi::GetInstance();
	//CSize size = pCav->GetSize();
	pFontApi->SetColor(_forClr, _bkClr, 0);
	pFontApi->SetSize(_size);
	pFontApi->DrawStringToMem(pCav, (char*)string, leftTop.x, leftTop.y, rightBtm.x, rightBtm.y, _rowHeight);
	pCav->SetChange();
}
/*
void void CFont::DrawVertical
    (
	    CPoint leftTop,
	    CPoint rightBtm,
	    const UINT8* string
    )
{
	int hi = rightBtm.x - leftTop.x;
	int wi = rightBtm.y - leftTop.y;
	COLOR * tmp =new COLOR [hi * wi];
	CFontLibApi* pFontApi = CFontLibApi::GetInstance();
	CSize size = pCav->GeSize();
	pFontApi->SetColor(_forClr, _bkClr, 0);
	pFontApi->SetSize(_size);
	pFontApi->DrawStringToMem(tmp, size.width, size.height, (char*)string,0, 0, wi, hi, _rowHeight);

	//COLOR 
	for(int i = 0; i < wi; i++)
	{
		for(int j = 0; j < hi; j++)
		{
			pCav->GetCanvas()[leftTop.x] 
		}
	}
	pCav->SetChange();
	pCav->Update();
	delete[] tmp;
}*/

/*******************************************************************************
*   CFont::GetOccupyCSize
*   
*/ 
CSize CFont::GetStringOccupyCSize
    (
    const UINT8* string,
    UINT16 widthLimit,
    UINT16 heighLimit
    )
{
    CFontLibApi* pFontApi = CFontLibApi::GetInstance();
    pFontApi->SetSize(_size);
    return pFontApi->GetStrPixelSize((char*)string, widthLimit, heighLimit, _rowHeight);
}

/*******************************************************************************
*   CStyle::Create
*   
*/ 
CBmp* CStyle::_noticeBack;
void CStyle::Create()
{
    if(_pInstance == NULL)
    {
        _pInstance = new CStyle(BSL_NUMBER, FSL_NUMBER);
		new CBrushFlat(BSL_CLEAR, 0, 0, 0, 0, 0);
		new CBrushFlat(BSL_BLACK, 0, 0, 0, CCanvas::RGBto565(0,7,0), 0); 
		new CBrushFlat(BSL_DEFAULT, 5, 15, 0, CCanvas::RGBto565(128,128,128), 0);
		CSize sz(28, 28);
		new CFont(FSL_DEFAULT, sz, 0xffff, 0x0, 40);
		new CFont(FSL_MENU_TITLE, CSize(22,22), CCanvas::RGBto565(255,255,70), 0x0, 30);
		new CFont(FSL_BUTTON_NORMAL, CSize(22,22), CCanvas::RGBto565(200,200,200), 0x0, 30);
		new CFont(FSL_BUTTON_HILIGHT, CSize(22,22), CCanvas::RGBto565(20,20,20), 0x0, 30);
		new CFont(FSL_MENU_ITEM_HILIGHT, sz, CCanvas::RGBto565(0,7,0), 0x0, 40);
		new CFont(FSL_NOTICE_BLACK, CSize(22,22), CCanvas::RGBto565(0,7,0), 0x0, 40);	
		new CBrushFlat(BSL_TEXT_NORMAL, 20, 15, CCanvas::RGBto565(28,28,28), 0, 0);
		new CBrushFlat(BSL_MENU_BG, 0, 0, 0, CCanvas::RGBto565(20,20,20), 0); 
		new CBrushFlat(BSL_MENU_TITLE, 0, 0, 0, CCanvas::RGBto565(0,7,0), 0);
		new CBrushFlat(BSL_MENU_ITEM_HILIGHT, 8, 15, 0, CCanvas::RGBto565(255,255,70), 0);
		new CBrushFlat(BSL_MENU_ITEM_NORMAL, 8, 15, 0, CCanvas::RGBto565(51,51,51), 0);
		new CBrushFlat(BSL_BAR_BG, 0, 0, 0, CCanvas::RGBto565(77,77,77), 0);
		new CBrushFlat(BSL_BAR_FRONT, 2, 15, 0, CCanvas::RGBto565(204,204,51), 0);
		new CBrushFlat(BSL_BUTTON_NORMAL, 10, 15, 2, CCanvas::RGBto565(20,20,20), CCanvas::RGBto565(77,77,77));
		new CBrushFlat(BSL_BUTTON_HILIGHT, 10, 15, 2, CCanvas::RGBto565(220,220,220), CCanvas::RGBto565(77,77,77));

		new CBrushFlat(BSL_Bit_Black_Pannel, 0, 0, 0, 0, 0);
		new CBrushFlat(BSL_Bit_White_Pannel, 0, 0, 0, 1, 1);
		new CBrushFlat(BSL_Bit_White_border, 0, 0, 1, 0, 1);
		CBmp* pBmp = new  CBmp(WndResourceDir,"battery_back.bmp");
		new CBrushBmp(BSL_Bit_battery_bmp, 0, 0, 0, pBmp, 0);
		new CFont(FSL_Bit_Black_Text, CSize(22, 22), 0, 1, 22);
		new CFont(FSL_Bit_White_Text, CSize(22, 22), 1, 0, 22);
        new CFont(FSL_Bit_White_Text_normal, CSize(20, 20), 1, 0, 20);
		new CFont(FSL_Bit_White_Text_small, CSize(18, 18), 1, 0, 18);
		new CFont(FSL_Bit_Black_Text_small, CSize(18, 18), 0, 1, 18);
		new CFont(FSL_Bit_White_Text_tiny, CSize(14, 14), 1, 0, 14);
		new CFont(FSL_Bit_White_Text_tiny_12, CSize(12, 12), 1, 0, 12);

		_noticeBack = new CBmp(WndResourceDir, "noticeBk.bmp");
		new CBrushBmp(BSL_NOTICE_BAKC,0, 0, 0, _noticeBack, 0);
    }
}

void CStyle::Destroy()
{
    if(_pInstance != NULL)
        delete _pInstance;
	delete _noticeBack;
}

/*******************************************************************************
*   CStyle::RegistBrush
*   
*/ 
void CStyle::RegistBrush
    (
    UINT16 id,
    CBrush* pBrush
    )
{ 
    if(id < _brushNumber)
        *(_pBrushPool + id) = pBrush;
}

/*******************************************************************************
*   CStyle::RegistFont
*   
*/ 
void CStyle::RegistFont
    (
    UINT16 id,
    CFont* pFont
    )
{ 
    if(id < _fontNumber)
        *(_pFontPool + id) = pFont;
}

/*******************************************************************************
*   CStyle::GetBrush
*   
*/ 
CBrush* CStyle::GetBrush
    (
    UINT16 id
    )
{
    if(id < _brushNumber)
        return *(_pBrushPool + id);
    else
        return NULL;
}

/*******************************************************************************
*   CStyle::GetFont
*   
*/ 
CFont* CStyle::GetFont
    (
    UINT16 id
    )
{ 
    if(id < _fontNumber)
    {
    	if(*(_pFontPool + id) != NULL)
        	return *(_pFontPool + id);
		else
			return *(_pFontPool + 0);
    }
    else
        return *(_pFontPool + 0);;
}

/*******************************************************************************
*   CStyle::CStyle
*   
*/ 
 CStyle::CStyle
    (
    UINT16 brushNumber,
    UINT16 fontNumber
    ) : _brushNumber(brushNumber),
    _fontNumber(fontNumber)
{
    _pBrushPool = new CBrush* [_brushNumber] ;
    _pFontPool = new CFont* [_fontNumber];
    for(int i = 0; i < _brushNumber; i++)
    {
        *(_pBrushPool + i) = NULL;
    }
    for(int i = 0; i < _fontNumber; i++)
    {
        *(_pFontPool + i) = NULL;
    }
	
}

/*******************************************************************************
*   CStyle::CStyle
*   
*/ 
 CStyle::~CStyle()
{
	for(int i = 0; i < _brushNumber; i++)
	{
		if(_pBrushPool[i] != NULL)
			delete _pBrushPool[i];			
	}
	for(int i = 0; i < _fontNumber; i++)
	{
		if(_pFontPool[i] != NULL)
			delete _pFontPool[i];		
	}
	
	delete[] _pBrushPool;
	delete[] _pFontPool;
}

//#ifdef WIN32
};
//#endif


/*******************************************************************************
**      Control.cpp
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
*******************************************************************************/ 
/*#include "Debug.h"
#include "stdio.h"
#include <semLib.h>
#include <taskLib.h>
#include <msgQLib.h>
#include "vxWorks.h"
#include "string.h"*/

// add included here
#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#include "CWinThread.h"
#include "CEvent.h"
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Style.h"
#include "Wnd.h"
#include "App.h"

#else

#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
#include "Control.h"
#include "Style.h"
#include "Wnd.h"
#include "App.h"
#include "CLanguage.h"


#endif
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

/*******************************************************************************
* <Function elements>
*******************************************************************************/

/*******************************************************************************
*   CControl::CControl
*  */ 
 
CControl::CControl
	(
		CContainer* pParent,
		CPoint leftTop,
		CSize CSize
	):_pParent(pParent)
	,_leftTop(leftTop)
	,_size(CSize)
	,_property(0)
{
	if(_pParent != NULL)
		_pParent->AddControl(this);
}

/*******************************************************************************
*   CControl::CControl
*   
*/ 
CControl::~CControl()
{ 
}

/*******************************************************************************
*   CControl::GetAbsPos
*   recursion the absolut lefttop position.
*/ 
CPoint CControl::GetAbsPos()
{ 
    CPoint rt;
    if(_pParent != NULL)
        rt.Set(_pParent->GetAbsPos().x + _leftTop.x , _pParent->GetAbsPos().y + _leftTop.y);
    else
        rt.Set(_leftTop.x, _leftTop.y);
    return rt;
}

/*******************************************************************************
*   CControl::GetOwner
*   recursion the Owner, container should have this return no NULL
*/ 
CWnd* CControl::GetOwner()
{
    if(_pParent != NULL)
        return _pParent->GetOwner();
    else
        return NULL;        
}

/*******************************************************************************
*   CControl::Draw
*   keep empty
*/ 
void CControl::Draw
	(
		bool bHilight
	)
{
}

void CControl::Clear()
{
	CBrush* pBrush = (CBrushFlat*)CStyle::GetInstance()->GetBrush(BSL_CLEAR);
	if(pBrush)
		pBrush->Draw(this);
	this->GetOwner()->GetManager()->GetCanvas()->Update();
}

/*******************************************************************************
*   CControl::OnEvent
*   keep out from modify
*/ 
bool CControl::OnEvent
	(
		CEvent *event
	)
{
	bool rt = true;
	if(event->_type == eventType_touch)
	{
		rt = isTouchOn(event->_paload, event->_paload1);
	}
	return rt;
}


/*******************************************************************************
*   CControl::touchOn
*   keep out from modify
*/ 
bool CControl::isTouchOn
	(
		int x,
		int y
	)
{
	
	CPoint point = this->GetAbsPos();
	//CAMERALOG("isTouchOn, x : %d, posi.x : %d, y : %d, posi.y : %d", x, point.x, y, point.y);
	if((x >=  point.x)&&( x < point.x + _size.width)&&(y >= point.y)&&(y < point.y + _size.height))
		return true;
	else 
		return false;
}

/*******************************************************************************
*   CStaticText::CStaticText
*   
CStaticText::CStaticText
    (
    UINT8* pTxt
    ) : _pString(pTxt)    
{

}
 
UINT16 CStaticText::_brush = 0; 
UINT16 CStaticText::_font = 0;
UINT16 CStaticText::_brushHi = 0; 
UINT16 CStaticText::_fontHi = 0;
UINT16 CStaticText::_xMargin = 0;
UINT16 CStaticText::_yMargin = 0;*/

/*******************************************************************************
*   CStaticText::CStaticText
*   
*/ 
CStaticText::~CStaticText()
{

}

/*******************************************************************************
*   CStaticText::Draw
*   
*/ 
void CStaticText::Draw
    (
    bool bHilight
    )
{
    CPoint txtTL;
    CStyle* pStyle = CStyle::GetInstance();
    CBrush *pBrush = NULL;
    CFont* pFont = NULL;

    if(bHilight)
    {
        pBrush = pStyle->GetBrush(_brushHi);
        pFont = pStyle->GetFont(_fontHi);
    }
    else
    {
        pBrush = pStyle->GetBrush(_brush);
        pFont = pStyle->GetFont(_font);
    }
    //draw
    if(pBrush != NULL)
        pBrush->Draw(this);
    if(pFont != NULL)  
    { 
       this->DrawText(pFont);
    }
}

void CStaticText::DrawText(CFont* pFont)
{
	UINT8* txt;
	if(_pText == NULL)
	{
		if(_pItem == NULL)
		{
			//CAMERALOG("--no txt");
			return;
		}
		else
		{
			txt = (UINT8*)_pItem->getContent();
		}
	}
	else
		txt = _pText;
	
	CPoint tplft = GetAbsPos();
	CCanvas* pCanvas = this->GetOwner()->GetManager()->GetCanvas();
	//if(pCanvas->isBitCanvas())
	//	CAMERALOG("--CStaticText::Draw leftTop:%d, %d",tplft.x, tplft.y);
    CSize txCSize;
    txCSize = pFont->GetStringOccupyCSize(txt, GetSize().width - _xMarginL - _xMarginR, GetSize().height- _yMarginB - _yMarginT);
	//if(this->GetOwner()->GetManager()->GetCanvas()->isBitCanvas())
	//	CAMERALOG("--txCSize: %d, %d",txCSize.width, txCSize.height);

	switch(GetHAlign())
    {
        case LEFT:
            tplft.x += _xMarginL;
            break;
        case CENTER:
            tplft.x += (GetSize().width - txCSize.width) / 2;
            break;
        case RIGHT:
            tplft.x += (GetSize().width - txCSize.width) - _xMarginR;
            break;
    }
    switch(GetVAlign())
    {
        case TOP:
            tplft.y += _yMarginT;
            break;
        case MIDDLE:
            tplft.y += (GetSize().height- txCSize.height) / 2;
            break;
        case BOTTOM:
            tplft.y += (GetSize().height- txCSize.height) - _yMarginB ;
            break;
    }
    CPoint rtbtm = tplft;
    rtbtm.Mov(txCSize.width + 2, txCSize.height + 2);
	//CAMERALOG("--txt : %s, x:%d, y:%d, w:%d, h:%d", txt, tplft.x, tplft.y, txCSize.width, txCSize.height);
    pFont->Draw(tplft, rtbtm ,txt, pCanvas);
}

/*******************************************************************************
*   CStaticText::OnEvent
*   
*/ 
bool CStaticText::OnEvent
	(
		CEvent *event
	)
{
    return true;
}

/*******************************************************************************
*   CContainer::CContainer
*   
*/ 
CContainer::CContainer
	(
		CContainer* pParent, 
		CWnd* pOwner, 
		CPoint leftTop, 
		CSize CSize, 
		UINT16 maxChildren
	) : CControl(pParent, leftTop, CSize)
    ,_pOwner(pOwner)
    ,_pControlList(NULL)
    ,_number(maxChildren)
    ,_count(0)
{
    _pControlList = new CControl* [_number];
    for(int i = 0; i < _number; i++)
    {
        _pControlList[i] = NULL;
    }
}

/*******************************************************************************
*   CContainer::CContainer
*   
*/ 
CContainer::~CContainer()
{
    delete[] _pControlList;
}

/*******************************************************************************
*   CContainer::GetOwner
*   
*/ 
CWnd* CContainer::GetOwner()
{
    if(GetParent() != NULL)
        return GetParent()->GetOwner();
    else
        return _pOwner;      
}

/*******************************************************************************
*   CPanel::Draw
* 
void CPanel::Draw
	(
		bool bHilight
	)
{ 
	//this -> draw background,
	for(int i = 0; i < _number)
}  */ 

/*******************************************************************************
*   CPanel::OnEvent
*  
bool CPanel::OnEvent
	(
		CEvent *event;
	)
{

	return true;
}  */

/*******************************************************************************
*   CContainer::RegistControl
*   
*/ 
bool CContainer::AddControl
    (
    CControl* pControl
    )
{
    if(_count < _number)
    {
        *(_pControlList + _count) = pControl;
        _count++;
        return true;
    }
    else
        return false;
}

/*******************************************************************************
*   CContainer::UnRegistControl
*   
*/ 
bool CContainer::RemoveControl
    (
    CControl* pControl
    )
{
    int id = 0;
    while((*(_pControlList + id) != pControl)&&(id < _count))
    {
        id++;
    }
    if(id < _count)
    {
        for(int i = id; i < _count - 1; i ++)
        {
           *(_pControlList + i) = *(_pControlList + i + 1);
        }
        _count--;
        return true;
    }
    else
        return false;
}


CControl* CContainer::GetControl(UINT16 index)
{
	if(index < _count)
	{
		return _pControlList[index]; 
	}
	else
		return NULL;
}

bool CContainer::OnEvent(CEvent *event)
{
	bool live = true;
	// for touch event
	if(event->_type == eventType_key)
	{
		
	}
	else
	{
		for(int i = 0; live&&(i < GetChildrenCount()) ; i ++)
		{
			if((GetControl(i) != 0)&&(!GetControl(i)->IsHiden()))
			{
				live = GetControl(i)->OnEvent(event);
			}
		}
#ifdef PLATFORM22A
		if(((live)&&(event->_type == eventType_internal)
			&&((event->_event == InternalEvent_app_state_update)
				||(event->_event == InternalEvent_app_timer_update))))
		{
			this->GetOwner()->GetManager()->GetCanvas()->Update();
		}
#endif
	}
	
	return live;
}

/*******************************************************************************
*   CPanel::CPanel
*   
 CPanel::CPanel
    (
    UINT16 colNumber,
    UINT16 rowNumber,
    CPoint startLeftTop,
    UINT16 rowHeight,
    UINT16 colWidth
    ) : CContainer(colNumber * rowNumber),
    _colNumber(colNumber),
    _rowNumber(rowNumber),
    _startLeftTop(startLeftTop),
    _rowHeight(rowHeight), 
    _colWidth(colWidth),
    _currentHilight(0)
{ 
}*/

/*******************************************************************************
*   CPanel::CPanel
*   
*/
UINT16 CPanel::_brush = 0;
UINT16  CPanel::_brushHi = 0; 
CPanel::~CPanel()
{ 
}

/*******************************************************************************
*   CPanel::Draw
*   
*/ 
void CPanel::Draw
	(
		bool bHilight
	)
{
	CPoint txtTL;
	CStyle* pStyle = CStyle::GetInstance();
	CBrush *pBrush = NULL;

	if(_privateBrush != NULL)
	{
		_privateBrush->Draw(this);
	}
	else
	{
		if(bHilight)
		{
			pBrush = pStyle->GetBrush(_brushHi);
		}
		else
		{
			pBrush = pStyle->GetBrush(_brush);
		}
		if(pBrush != NULL)
			pBrush->Draw(this);
	}
	for(int i = 0; i < GetChildrenCount() ; i ++)
	{
		if((GetControl(i) != 0)&&(!GetControl(i)->IsHiden()))
		{
			GetControl(i)->Draw((_currentHilight == i)&&(bHilight));
		}
	}
	this->GetOwner()->GetManager()->GetCanvas()->Update();
}

/*******************************************************************************
*   CPanel::OnEvent
*   
*/ 
bool CPanel::OnEvent
	(
		CEvent *event
	)
{ 
	int i, j;
	bool rt = inherited::OnEvent(event);
	if(rt)
	{

		if(event->_type == eventType_key)
		{
			if(_currentHilight < this->GetChildrenCount())
				rt = this->GetControl(_currentHilight)->OnEvent(event);
			if(rt)
			{
				switch(event->_event)
				{
					case key_up:
						i = _currentHilight;
						j = 0;
						do
						{
							i --;
							if(i < 0)
							{
								i = this->GetChildrenCount() - 1;
							}
							j ++;
						}while((j < this->GetChildrenCount())&&((this->GetControl(i)->IsDisabled())||(this->GetControl(i)->IsHiden())));
						if((i != _currentHilight)&&(!this->GetControl(i)->IsDisabled()))
						{
							this->GetControl(i)->Draw(true);
							this->GetControl(_currentHilight)->Draw(false);
							if(i != _currentHilight)
							{
								_currentHilight = i;
								rt = false;
							}
						}
						break;
					case key_down:
						i = _currentHilight;
						j = 0;
						do
						{
							i ++;
							if(i >= this->GetChildrenCount())
							{
								i = 0;
							}
							j ++;
						}while((j < this->GetChildrenCount())&&((this->GetControl(i)->IsDisabled())||(this->GetControl(i)->IsHiden())));
						if((i != _currentHilight)&&(!this->GetControl(i)->IsDisabled()))
						{
							this->GetControl(i)->Draw(true);
							this->GetControl(_currentHilight)->Draw(false);
							if(i != _currentHilight)
							{
								_currentHilight = i;
								rt = false;
							}
						}
						break;
					case key_right:
					case key_ok:										
						break;
					default:
						break;
				}
			}
		}
	}
	if(!rt)
		this->GetOwner()->GetManager()->GetCanvas()->Update();
	return rt;
}


void CPanel::SetCurrentHilight(CControl* p)
{
	int i;
	for(i = 0; i < this->GetChildrenCount(); i++)
	{
		if(p == this->GetControl(i))
			break;
	}
	if(i < this->GetChildrenCount())
		_currentHilight = i;
}


/*******************************************************************************
*  CPanelWithTopBtmBar
*   
*/
CPanelWithTopBtmBar::CPanelWithTopBtmBar
		(
			CContainer* pParent
			,CWnd* pOwner
			,CPoint leftTop
			,CSize size
			,UINT16 maxChildren
		):inherited(pParent, pOwner, leftTop, size, maxChildren)
	{
		_tpH = 30;
		_btmH = 30;
		_barBrush = BSL_BLACK;
	}

void CPanelWithTopBtmBar::Draw(bool bHilight)
{
	CPoint txtTL;
	CStyle* pStyle = CStyle::GetInstance();
	CBrush *pBrush = NULL;
	CCanvas *pCan = this->GetOwner()->GetManager()->GetCanvas();
	if(_privateBrush != NULL)
	{
		_privateBrush->Draw(this);
	}
	else
	{
		if(bHilight)
		{
			pBrush = pStyle->GetBrush(_brushHi);
		}
		else
		{
			pBrush = pStyle->GetBrush(_brush);
		}
		//draw
		if(pBrush != NULL)
			pBrush->Draw(this);
	}
	
	pBrush = pStyle->GetBrush(BSL_BLACK);
	CPoint pt = this->GetAbsPos();
	CSize sz = this->GetSize();
	CSize sz1 = sz;	
	sz1.height = _tpH;
	pBrush->Draw(pt, sz1, pCan);
	
	//CPoint p1 = pt;
	pt.y = pt.y + sz.height - _btmH;
	sz1.height = _btmH;
	
	pBrush->Draw(pt, sz1, pCan);

	for(int i = 0; i < GetChildrenCount() ; i ++)
	{
		if((GetControl(i) != 0)&&(!GetControl(i)->IsHiden()))
		{
			GetControl(i)->Draw((_currentHilight == i)&&(bHilight));
		}
	}
	this->GetOwner()->GetManager()->GetCanvas()->Update();
}

/*******************************************************************************
*   CBmpBtn::CBmpBtn
*   
*/ 
void BmpBtnFlashDelay(void* p)
{
	CBmpBtn *bt = (CBmpBtn*) p;
	bt->SetElapsed(true);	
}

CBmpBtn::CBmpBtn
	(
		CContainer* pParent
		,CPoint leftTop
		,CSize size
		,int id
		,CBmp *bmp
	) : inherited(pParent, leftTop, size)
	,_pBtnBmp(bmp)
	,_btnID(id)
	,_state(BtnState_normal)
	,_bElapsed(true)
	//,_mutex()
{
	this->SetSize(CSize(_pBtnBmp->getWidth() /4, _pBtnBmp->getHeight()));
}

CBmpBtn::~CBmpBtn()
{

}

void CBmpBtn::Draw
	(
		bool bHilight
	)
{
	CPoint tplft = GetAbsPos();
	CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
	pCav->DrawRect(tplft, this->GetSize(), _pBtnBmp, CCanvas::BmpFillMode_repeat, _state*GetSize().width);
	pCav->Update();
}

void CBmpBtn::SetElapsed(bool b)
{
	if(_state == BtnState_pressed)
		_bElapsed = b;
	else
	{
		_bElapsed = true;
		this->GetOwner()->GetApp()->PushEvent(eventType_internal, InternalEvent_wnd_button_click, _btnID, 0);
		this->Draw(false);		
	}
}
bool CBmpBtn::OnEvent
	(
		CEvent *event
	)
{
	if(event->_type == eventType_touch)
	{
		bool bInControlTouch = isTouchOn(event->_paload, event->_paload1);
		if(bInControlTouch)
		{
			switch(_state)
			{
				case BtnState_pressed:
					switch(event->_event)
					{
						case TouchEvent_press:
							break;
						case TouchEvent_release:
							_state = BtnState_normal;
							if(_bElapsed)
							{
								this->GetOwner()->GetApp()->PushEvent(eventType_internal, InternalEvent_wnd_button_click, _btnID, 0);
								this->Draw(false);
							}
							return false;
							break;
					}
					break;
				case BtnState_normal:
				case BtnState_hilight:
					switch(event->_event)
					{
						case TouchEvent_press:
							_state = BtnState_pressed;
							_bElapsed = false;
							//CTimerThread::GetInstance()->ApplyTimer(30, BmpBtnFlashDelay, this, false);
							this->Draw(false);
							return false;
							break;
						case TouchEvent_release:
							_state = BtnState_normal;
							this->GetOwner()->GetApp()->PushEvent(eventType_internal, InternalEvent_wnd_button_click, _btnID, 0);
							this->Draw(false);							
							return false;
							break;
					}
					break;
				case BtnState_invalid:
					break;
			}
			
		}
		else
		{
			if(_state == BtnState_pressed)
			{
				switch(event->_event)
				{
					case TouchEvent_overon:						
						{
							_state = BtnState_normal;
							this->Draw(false);
							return false;
						}
						break;
				}
			}
		}
	}	
	return true;
}

/*******************************************************************************
*   CSecTimerBtn
*   
*/

void SecTimerBtnUpdate(void * p)
{
	CSecTimerBtn* pp = (CSecTimerBtn*) p;
	pp->Update();		
}

CSecTimerBtn::CSecTimerBtn
	(
		CContainer* pParent
		,CPoint leftTop
		,CSize CSize
	) : CStaticText(pParent, leftTop, CSize)
	, _iCounter(0)
{
	this->SetText((UINT8 *)_buffer);
	//SetTimer(0);
}

CSecTimerBtn::~CSecTimerBtn()
{
	End();
}

void CSecTimerBtn::Start()
{
	Reset();
	//_timer = CTimerThread::GetInstance()->ApplyTimer(20, SecTimerBtnUpdate, this, true);
}

void CSecTimerBtn::End()
{
	//CTimerThread::GetInstance()->CancelTimer(_timer);
}

void CSecTimerBtn::Reset()
{
	CSystemTime::GetSystemTime(&_sec);
}

void CSecTimerBtn::Update()
{
	UINT64 tt;
	CSystemTime::GetSystemTime(&tt);
	_iCounter = tt -_sec;
	memset(_buffer, 0, sizeof(_buffer));
	//sprintf(_buffer, "%d:%02d:%02d", _iCounter /3600, (_iCounter % 3600) /60, (_iCounter % 3600) %60);
	UINT64 hh =  _iCounter /3600;
	UINT64 mm = (_iCounter % 3600) /60;
	UINT64 ss = (_iCounter % 3600) %60;
	//sprintf(_buffer,  "%02d:%02d", mm, ss); 
	int j = 0;
	j  = sprintf( _buffer, "%lld:", hh);
   	j += sprintf( _buffer + j,"%02lld:", mm);
   	j += sprintf( _buffer + j,"%02lld", ss);

	this->Draw(false);	
}


/*******************************************************************************
*   CSwitcher
*   
*/
void SwitcherMoving(void* p)
{
	CSwitcher *sw = (CSwitcher*) p;
	//bt->SetElapsed(true);	
	sw->MovePosition();
}

CSwitcher::CSwitcher
	(
		CContainer* pParent
		,CPoint leftTop
		,CSize size
		,CBmp* pFg
		,CBmp* pBg
		,CPoint stP
		,CPoint enP
		,int id
	) : inherited(pParent, leftTop, size)
	,_pPointBmp(pFg)
	,_pBgBmp(pBg)
	,_onPoint(stP)
	,_offPoint(enP)
	,_bOn(true)
	,_position(0)
	,_btnID(id)
{
	this->SetSize(CSize(_pBgBmp->getWidth(), _pBgBmp->getHeight()));
}
CSwitcher::~CSwitcher()
{
	
}

void CSwitcher::Draw(bool bHilight)
{
	CPoint tplft = GetAbsPos();
    CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
	pCav->DrawRect(tplft, this->GetSize(), _pBgBmp, CCanvas::BmpFillMode_repeat, 0);
	CPoint cp(_onPoint.x + _position * (_offPoint.x - _onPoint.x) /Switcher_steps , _onPoint.y + _position * (_offPoint.y - _onPoint.y) /Switcher_steps);
	pCav->DrawRect(CPoint(tplft.x + cp.x, tplft.y +cp.y), CPoint(tplft.x + cp.x+ _pPointBmp->getWidth(), tplft.y + cp.y + _pPointBmp->getHeight()), _pPointBmp, CCanvas::BmpFillMode_with_Transparent);
	pCav->Update();
}

bool CSwitcher::OnEvent(CEvent *event)
{
	bool rt = true;
	if(event->_type == eventType_touch)
	{
		bool bInControlTouch = isTouchOn(event->_paload, event->_paload1);
		if(bInControlTouch)
		{
			switch(event->_event)
			{
				case TouchEvent_press:
					break;
				case TouchEvent_release:
					if((_position == 0)||(_position == Switcher_steps -1))
						this->GetOwner()->GetApp()->PushEvent(eventType_internal, InternalEvent_wnd_button_click, _btnID, 0);
					rt = false;
					break;
			}
		}
	}
	else if(event->_type == eventType_internal)
	{
		switch(event->_event)
		{
			case InternalEvent_wnd_button_click:
				if(event->_paload == _btnID)
				{
					if(_bOn)
					{
						_bOn = false;
						_position = 0;
					}
					else
					{
						_bOn = true;
						_position = Switcher_steps -1;
					}
					//_timerID = CTimerThread::GetInstance()->ApplyTimer(1, SwitcherMoving, this, true);
					rt = false;
				}
				break;
			default:
				break;
		}
	}
	return rt;
}
void CSwitcher::MovePosition()
{
	if(_bOn)
	{
		Draw(false);
		_position --;
		if(_position < 0)
		{
			 //CTimerThread::GetInstance()->CancelTimer(_timerID);
			 _position = 0;
		}
	}
	else
	{
		Draw(false);
		_position ++;
		if(_position >= Switcher_steps)
		{
			 //CTimerThread::GetInstance()->CancelTimer(_timerID);
			 _position = Switcher_steps -1;
		}
	}
}

/*******************************************************************************
*   CTouchAdaptor
*   
*/
CTouchAdaptor::CTouchAdaptor
	(
		CContainer* pParent
		, CWnd* pOwner
		, CPoint leftTop
		, CSize size
		, UINT16 maxChildren
	) : inherited(pParent, pOwner, leftTop, size, maxChildren)
{

}

CTouchAdaptor::~CTouchAdaptor()
{

}

bool CTouchAdaptor::OnEvent(CEvent *event)
{
	CPoint point = GetAbsPos();
	if(event->_type == eventType_touch)
	{
		CEvent ev = *event;
		ev._paload += point.x;
		ev._paload1 += point.y;
		return inherited::OnEvent(&ev);
	}
	return true;
}
/*******************************************************************************
*   CMenu
*   
*/
const static char* leftIcon = "left.bmp";
const static char* rightIcon = "right.bmp";
const static char* upIcon = "up.bmp";
const static char* downIcon = "down.bmp";

CBmp* CMenu::_left_symble = NULL;
CBmp* CMenu::_right_symble = NULL;
CBmp* CMenu::_up_symble = NULL;	
CBmp* CMenu::_down_symble = NULL;

CMenu::CMenu
	(CContainer* pParent
	, CWnd* pOwner
	, CPoint leftTop
	, int width
	, int itemHeight
	, int barWidth
	, int itemNum
	, int titleHigh
	): inherited(pParent, pOwner, leftTop, CSize(width, itemHeight*itemNum + titleHigh), itemNum + 2)
	,_currentHilight(0)
	,_itemSize(width - barWidth, itemHeight)
	,_itemNum(itemNum)
	,_barWidth(barWidth)
	,_ppStringlist(0)
	,_stringNum(0)
	,_ppItemList(0)
	,_currentFirst(0)
	,_pAction(NULL)
{
	for(int i = 0; i <= itemNum; i++)
	{
		CMenuItem *item = new CMenuItem(this, NULL, i, titleHigh);	
		item->SetSymbles(_right_symble, _up_symble, _down_symble, NULL);
	}
	_pTitle = new CMenuTitle(this, NULL, titleHigh);
	_pTitle->SetSymble(_left_symble);
	_pBar = new CPosiBarV(this, CPoint(0, titleHigh), CSize(barWidth, itemHeight*itemNum));

	

}
CMenu::~CMenu()
{
	//delete _pTitle;	
}
void CMenu::DeleteSymbles()
{
	delete _left_symble;
	delete _right_symble;
	delete _up_symble;
	delete _down_symble;
}
void CMenu::LoadSymbles(const char* path)
{
	if(_left_symble == 0)
	{
		_left_symble = new CBmp(path, leftIcon);
	}
	if(_right_symble == 0)
	{
		_right_symble = new CBmp(path, rightIcon);
	}
	if(_up_symble == 0)
	{
		_up_symble = new CBmp(path, upIcon);
	}
	if(_down_symble == 0)
	{
		_down_symble = new CBmp(path, downIcon);
	}	
}

bool CMenu::OnEvent
	(CEvent *event)
{	
	CControl *item = this->GetControl(_currentHilight);
	bool rt = item->OnEvent(event);
	if(!rt)
		return rt;
	if(event->_type == eventType_key)
	{
		
		switch(event->_event)
		{
			case key_up:
				item = this->GetControl(_currentHilight);
				item->Draw(false);
				_currentHilight--;

				if(_itemNum < _stringNum)
				{
					if(_currentHilight < 0)
					{
						if(_currentFirst <= 0)
						{
							_currentFirst = _stringNum - _itemNum;
							_currentHilight = _itemNum -1;
						}
						else if(_currentFirst > 0)
						{
							_currentFirst--;
							_currentHilight = 0;
						}
						this->Draw(true);
					}
					else
					{
						item = this->GetControl(_currentHilight);
						item->Draw(true);						
					}
				}
				else
				{
					if(_currentHilight < 0)
					{
						_currentHilight = _stringNum -1;
					}
					item = this->GetControl(_currentHilight);
					item->Draw(true);	
				}
				rt = false;
				break;
			case key_down:
				item = this->GetControl(_currentHilight);
				item->Draw(false);
				_currentHilight ++;
				
				if(_itemNum < _stringNum)
				{
					if(_currentHilight >= _itemNum)
					{		
						if(_currentFirst < _stringNum - _itemNum)
						{
							_currentFirst ++;
							_currentHilight = _itemNum - 1;
						}
						else
						{
							_currentFirst = 0;
							_currentHilight = 0;
						}
						this->Draw(true);
					}
					else
					{
						item = this->GetControl(_currentHilight);
						item->Draw(true);
					}
				}
				else{
					if(_currentHilight >= _stringNum)
					{
						_currentHilight = 0;						
					}
					item = this->GetControl(_currentHilight);
					item->Draw(true);
				}			
				rt = false;
				break;
			case key_right:
			case key_ok:				
				if(_pAction != NULL)
				{
					_pAction->Action(this->GetControl(_currentHilight));
					rt = false;
				}							
				break;
			default:
				break;
		}			
	}
	if(!rt)
		this->GetOwner()->GetManager()->GetCanvas()->Update();
	return rt;
}


void CMenu::Draw(bool bHilight)
{
	CBrush* pBkBrush = CStyle::GetInstance()->GetBrush(BSL_MENU_BG);
	pBkBrush->Draw(this);
	if(_pTitle)
	{
		if(this->GetOwner()->GetPreWnd()== NULL)
			_pTitle->SetLeftSymble(false);
		else
			_pTitle->SetLeftSymble(true);
		_pTitle->Draw(false);
	}
	if(_pBar)
	{	
		//CAMERALOG("menu draw: %d, %d", _stringNum, _itemNum);
		_pBar->setLength(_stringNum, _itemNum);
		_pBar->SetPosi(_currentFirst);
		_pBar->Draw(false);
	}
	for(int i = 0; i < _itemNum; i++)
	{
		CMenuItem *item = (CMenuItem*)this->GetControl(i);
		if(item == NULL)
			continue;
		if(_ppItemList)
		{
			if(_currentFirst + i < _stringNum)
				item->SetTitle(_ppItemList[_currentFirst + i]);
			else
			{
				item->SetTitle(NULL);
				item->SetText(NULL);
			}
		}
		else if(_ppStringlist)
		{
			if(_currentFirst + i < _stringNum)
				item->SetText((UINT8*)_ppStringlist[_currentFirst + i]);
			else
			{
				item->SetTitle(NULL);
				item->SetText(NULL);
			}
		}
		
		if(i == _currentHilight)
			item->Draw(true);
		else
			item->Draw(false);
	}
	this->GetOwner()->GetManager()->GetCanvas()->Update();
}

void CMenu::SetTitle(char* title)
{
	_pTitle->SetText((UINT8*)title);
}

void CMenu::SetTitleItem(CLanguageItem* item)
{
	_pTitle->SetTitle(item);
}


void CMenu::setStringList(char** list, int num)
{
	_ppStringlist = list;
	_stringNum = num;
	CMenuItem *item;
	if(num < _itemNum)
	{
		for(int i = num; i < _itemNum; i++)
		{
			item = (CMenuItem*)this->GetControl(i);
			item->SetTitle(NULL);
			item->SetText(NULL);
		}
	}
}

void CMenu::setItemList(CLanguageItem** pTxt, int num)
{
	if(num < _stringNum)
		resetFocus();
	_ppItemList =  pTxt;
	_stringNum = num;
}


/*******************************************************************************
*   CMenuItem
*   
*/
CMenuItem::CMenuItem
	(CMenu* menu
	, CLanguageItem* title
	, int index
	, int yOffset
	): inherited(menu, CPoint(menu->GetBarWidth() + 1, yOffset + index* menu->getItemSize().height + 1), CSize(menu->getItemSize().width - 2,menu->getItemSize().height - 2))
	,bHightlight(false)
	,_nextWnd(NULL)
	,_action(NULL)
	,_pTitle(title)
	,_bRight(true)
	,_bUp(true)
	,_bDown(true)
	,_bOk(false)
{	
	this->SetStyle(BSL_MENU_ITEM_NORMAL, FSL_MENU_ITEM_NORMAL,BSL_MENU_ITEM_HILIGHT,FSL_MENU_ITEM_HILIGHT, 30, 0, 2, 2);
	this->SetText(NULL);	
}
CMenuItem::~CMenuItem()
{

}

bool CMenuItem::OnEvent
	(CEvent *event)
{
	bool rt = true;
	if(event->_type == eventType_key)
	{
		switch(event->_event)
		{
			case key_right:
			case key_ok:
				if((_pTitle)&&(_pTitle->GetSubWnd() != NULL))
				{
					this->GetOwner()->PopWnd(_pTitle->GetSubWnd());
					rt = false;
				}
				if((_pTitle)&&(_pTitle->GetAction() != NULL))
				{
					_pTitle->GetAction()->Action(this);
					rt = false;
				}
				if(_action != NULL)
				{
					_action->Action(this);
					rt = false;
				}
				if(_nextWnd != NULL)
				{
					this->GetOwner()->PopWnd(_nextWnd);
					rt = false;
				}				
				break;
			default:
				break;
		}			
	}
	return rt;
}
void CMenuItem::Draw(bool bHilight)
{
	if(_pTitle)
	{
		this->SetText((UINT8*)_pTitle->getContent());
	}	
	inherited::Draw(bHilight);
	if(bHilight)
	{
		
		CPoint tplft = GetAbsPos();
		CPoint posi;
		CSize size = this->GetSize();
		CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
		//right
		if(_bRight)
		{
			CSize bmpSize = CSize(_right_symble->getWidth(), _right_symble->getHeight());
			posi.x = tplft.x + size.width - bmpSize.width - 4;
			posi.y = tplft.y + (size.height - bmpSize.height) /2 ;
			pCav->DrawRect(posi, bmpSize , _right_symble, CCanvas::BmpFillMode_repeat, 0);
		}
		if(_bUp)
		{
			CSize bmpSize = CSize(_up_symble->getWidth(), _up_symble->getHeight());
			posi.x = tplft.x + 2;
			posi.y = tplft.y + 2;
			pCav->DrawRect(posi, bmpSize , _up_symble, CCanvas::BmpFillMode_repeat, 0);
		}
		if(_bDown)
		{
			CSize bmpSize = CSize(_down_symble->getWidth(), _down_symble->getHeight());
			posi.x = tplft.x + 2;
			posi.y = tplft.y + size.height - bmpSize.height - 2;
			pCav->DrawRect(posi, bmpSize , _down_symble, CCanvas::BmpFillMode_repeat, 0);
		}
	}
}

/*******************************************************************************
*   CMenuTitle
*   
*/
CMenuTitle::CMenuTitle
	(CMenu* menu
	, CLanguageItem* title
	, int height
	): inherited(menu, CPoint(0, 0), CSize(menu->getItemSize().width + menu->GetBarWidth(), height))
	,_pTitle(title)
	,_bLeft(true)
{	
	this->SetStyle(BSL_MENU_TITLE, FSL_MENU_TITLE,BSL_MENU_TITLE,FSL_MENU_TITLE, 10, 10, 2, 2);
	this->SetHAlign(CENTER);
}

CMenuTitle::~CMenuTitle()
{

}

void CMenuTitle::Draw(bool bHilight)
{
	if(_pTitle)
		this->SetText((UINT8*)_pTitle->getContent());
	inherited::Draw(false);
	if(_bLeft)
	{
		CPoint tplft = GetAbsPos();
		CPoint posi;
		CSize size = this->GetSize();
		CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
		CSize bmpSize = CSize(_left_symble->getWidth(), _left_symble->getHeight());
		posi.x = tplft.x + 2;
		posi.y = tplft.y + (size.height - bmpSize.height) /2 ;
		pCav->DrawRect(posi, bmpSize , _left_symble, CCanvas::BmpFillMode_repeat, 0);
	}
}

/*******************************************************************************
*   CPosiBarV
*   
*/
CHSelector::CHSelector(CContainer* pParent
		, CWnd* pOwner
		, CPoint leftTop
		, CSize ViewWindow
		, int barHeigh
		, int MaxitemNum
		, int itemPitch
		): inherited(pParent, pOwner, leftTop, ViewWindow, MaxitemNum + 1)
		,_currentFirst(0)
		,_currentHilight(0)
		,_itemPitch(itemPitch)	
{
	//_pPosiBar = new CPosiBarH(this, CPoint(0, 0), CSize(ViewWindow.width, barHeigh));
	_showNum = (ViewWindow.width / _itemPitch);
}
CHSelector::~CHSelector()
{

}

bool CHSelector::OnEvent(CEvent *event)
{
	CControl *item = this->GetControl(_currentHilight);
	bool rt = item->OnEvent(event);
	int controlNum = GetChildrenCount();//remove posi bar
	if(!rt)
		return rt;
	if(event->_type == eventType_key)
	{
		
		switch(event->_event)
		{
			case key_up:
            case key_up_onKeyUp:
				item = this->GetControl(_currentHilight);
				item->Draw(false);
				_currentHilight--;

				if(_showNum < controlNum)
				{
					if(_currentHilight < 0)
					{
						if(_currentFirst <= 0)
						{
							_currentFirst = controlNum - _showNum;
							_currentHilight = _showNum -1;
						}
						else if(_currentFirst > 0)
						{
							_currentFirst--;
							_currentHilight = 0;
						}
						this->Draw(true);
					}
					else
					{
						item = this->GetControl(_currentHilight);
						item->Draw(true);						
					}
				}
				else
				{
					if(_currentHilight < 0)
					{
						_currentHilight = _showNum -1;
					}
					item = this->GetControl(_currentHilight);
					item->Draw(true);	
				}
				rt = false;
				break;
			case key_down:
            case key_down_onKeyUp:
				item = this->GetControl(_currentHilight);
				item->Draw(false);
				_currentHilight ++;
				
				if(_showNum < controlNum)
				{
					if(_currentHilight >= _showNum)
					{		
						if(_currentFirst < controlNum - _showNum)
						{
							_currentFirst ++;
							_currentHilight = _showNum - 1;
						}
						else
						{
							_currentFirst = 0;
							_currentHilight = 0;
						}
						this->Draw(true);
					}
					else
					{
						item = this->GetControl(_currentHilight);
						item->Draw(true);
					}
				}
				else{
					if(_currentHilight >= controlNum)
					{
						_currentHilight = 0;						
					}
					item = this->GetControl(_currentHilight);
					item->Draw(true);
				}			
				rt = false;
				break;
			case key_right:
			case key_ok:											
				break;
			default:
				break;
		}			
	}
	if(!rt)
		this->GetOwner()->GetManager()->GetCanvas()->Update();
	return rt;

}

void CHSelector::Draw(bool bHilight)
{
	CPoint p;
	CStyle* pStyle = CStyle::GetInstance();
	CBrush *pBrush = NULL;
	
	pBrush = pStyle->GetBrush(bHilight?_brushHi:_brush);	
	pBrush->Draw(this);
	Adjust(false);
	for(int i = _currentFirst; i < (_currentFirst + _showNum) ; i ++)
	{
		if((GetControl(i) != 0)&&(!GetControl(i)->IsHiden()))
		{
			GetControl(i)->Draw((_currentHilight + _currentFirst == i)&&(bHilight));
		}
	}
	this->GetOwner()->GetManager()->GetCanvas()->Update();
}

void CHSelector::Adjust(bool bLeft)
{
	int halfNum = _showNum / 2;
	int firstX = 0;
	if(_showNum % 2 == 0)
	{
		firstX = this->GetSize().width / 2 - halfNum * _itemPitch + _itemPitch / 2;
	}
	else
	{
		firstX = this->GetSize().width / 2 - halfNum * _itemPitch;
	}

	CPoint p;
	for(int i = _currentFirst; i < (_currentFirst + _showNum); i ++)
	{
		CControl* pItem = this->GetControl(i);
		if(this->GetControl(i) != NULL)
		{
			p.x = firstX + (i - _currentFirst)*_itemPitch - pItem->GetSize().width/2;
			p.y = (this->GetSize().height - pItem->GetSize().height)/2;
			pItem->SetRelativePos(p);
		}
	}
}


/*******************************************************************************
*   CPosiBarV
*   
*/
CPosiBarV::CPosiBarV
	(CContainer* pParent
	, CPoint leftTop
	, CSize Size
	): inherited(pParent, leftTop, Size)
	,_wholeLen(1)
	,_segLen(1)
	,_currentPosi(0)
	,_backBrush(BSL_BAR_BG)
	,_frontBrush(BSL_BAR_FRONT)
{	
}

CPosiBarV::~CPosiBarV()
{

}

void CPosiBarV::Draw(bool bHilight)
{
	CStyle* pStyle = CStyle::GetInstance();
	CBrush *pBrushB = pStyle->GetBrush(_backBrush);
	CBrush *pBrushF = pStyle->GetBrush(_frontBrush);
	//CAMERALOG("CPosiBarV::Draw: %d, %d", _wholeLen, _currentPosi);
	pBrushB->Draw(this);
	CPoint posi = this->GetAbsPos();
	CSize size = this->GetSize();
	posi.x = posi.x + 2;
	posi.y = posi.y + size.height * _currentPosi / _wholeLen;
	size.width = size.width - 4;
	if((_wholeLen > 0)&&(_segLen <= _wholeLen))
		size.height = size.height * _segLen / _wholeLen;
	pBrushF->Draw(posi, size, pBrushB->getCanvas());
}

/*******************************************************************************
*   CPosiBarH
*   
*/
CPosiBarH::CPosiBarH(CContainer* pParent
		, CPoint leftTop
		, CSize Size
		):inherited(pParent, leftTop, Size)
{
}

CPosiBarH::~CPosiBarH()
{

}

void CPosiBarH::Draw(bool bHilight)
{
	CStyle* pStyle = CStyle::GetInstance();
    CBrush *pBrushB = pStyle->GetBrush(_backBrush);
    CBrush *pBrushF = pStyle->GetBrush(_frontBrush);
	
	pBrushB->Draw(this);
	CPoint posi = this->GetAbsPos();
	CSize size = this->GetSize();
	posi.y = posi.y + 2;
	posi.x = posi.x + size.width * _currentPosi / _wholeLen;
	size.height = size.height- 4;
	size.width= size.width* _segLen / _wholeLen;
	
	pBrushF->Draw(posi, size, pBrushB->getCanvas());
}

/*******************************************************************************
*   CPosiBarSegH
*   
*/
CPosiBarSegH::CPosiBarSegH(CContainer* pParent
		, CPoint leftTop
		, CSize Size
		, CPoint posi
		, CSize fgSize
		, int SegNum
		, int interlace
		):inherited(pParent, leftTop, Size)
		,_posi(posi)
		,_fSize(fgSize)
		,_seg(SegNum)
		,_interlace(interlace)
{
}

CPosiBarSegH::~CPosiBarSegH()
{

}

void CPosiBarSegH::Draw(bool bHilight)
{
	CStyle* pStyle = CStyle::GetInstance();
    CBrush *pBrushB = pStyle->GetBrush(_backBrush);
    CBrush *pBrushF = pStyle->GetBrush(_frontBrush);
	
	pBrushB->Draw(this);
	CPoint posi = this->GetAbsPos();
	CSize size = this->GetSize();
	CSize segSize;
	
	posi.y = posi.y + _posi.y;
	posi.x = posi.x + _posi.x + size.width * _currentPosi / _wholeLen;
	//size.height = size.height- 4;
	//size.width= size.width* _segLen / _wholeLen;
	segSize.height = _fSize.height;
	segSize.width = (_fSize.width - _interlace * (_seg - 1)) / _seg;
	
	for(int i = 0; i < _segLen*_seg / _wholeLen; i++)
	{
		//CAMERALOG("---seg: %d, %d", posi.x, segSize.width);
		pBrushF->Draw(posi, segSize, pBrushB->getCanvas());
		posi.x += (segSize.width + _interlace);
	}
}

/*******************************************************************************
*   CPosiBarV1
*   
*/
CPosiBarV1::CPosiBarV1(CContainer* pParent
		, CPoint leftTop
		, CSize Size
		, CPoint posi
		, CSize fgSize
		):inherited(pParent, leftTop, Size)
		,_posi(posi)
		,_fSize(fgSize)
{
}

CPosiBarV1::~CPosiBarV1()
{
}

void CPosiBarV1::Draw(bool bHilight)
{
	CStyle* pStyle = CStyle::GetInstance();
    CBrush *pBrushB = pStyle->GetBrush(_backBrush);
    CBrush *pBrushF = pStyle->GetBrush(_frontBrush);
	
	pBrushB->Draw(this);
	
	CPoint posi = this->GetAbsPos();
	CSize size = this->GetSize();
	
	posi.y = posi.y + _posi.y + _fSize.height * _currentPosi / _wholeLen;
	posi.x = posi.x + _posi.x;
	size.height = _fSize.height* _segLen / _wholeLen;
	size.width = _fSize.width;
	
	pBrushF->Draw(posi, size, pBrushB->getCanvas());
}

/*******************************************************************************
*   CBmpControl
*   
*/
CBmpControl::CBmpControl
		(CContainer* pParent
		,CPoint leftTop
		,CSize size
		,const char *path
		,const char *bmpName
		): inherited(pParent, leftTop, size)
{
	_pBmp = new CBmp(path, bmpName);
	//_pBmp->ReadFile(bmpName);
	//_pBmp->fillBuffer();	 
}
CBmpControl::~CBmpControl()
{
	delete _pBmp;
}

void CBmpControl::setSource(const char *path, const char *bmpName) {
    if (_pBmp) {
        delete _pBmp;
    }
    _pBmp = new CBmp(path, bmpName);
}

void CBmpControl::Draw(bool bHilight)
{
	//CAMERALOG("---draw bmp control");
	CPoint tplft = GetAbsPos();
	CPoint posi;
	CSize size = this->GetSize();
	CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
	CSize bmpSize = CSize(_pBmp->getWidth(), _pBmp->getHeight());
	posi.x = tplft.x + (size.width - bmpSize.width) / 2;
	posi.y = tplft.y + (size.height - bmpSize.height) /2 ;
	pCav->DrawRect(posi, bmpSize , _pBmp, CCanvas::BmpFillMode_repeat, 0);
}



/*******************************************************************************
*   CSymbleWithTxt
*   
*/
CSymbleWithTxt::CSymbleWithTxt(CContainer* pParent
		, CPoint leftTop
		, CSize CSize
		, bool left
		) : CStaticText(pParent, leftTop, CSize)
		,_symbleNum(0)
		,_currentSymble(0)
		,_bSymbleLeft(left)
		,_currentTitle(0)
		,_titleNum(0)
{
	//_ppSymbles = new CBmp* [_symbleNum];
	//memset(_ppSymbles, 0, sizeof(_ppSymbles));
	//_ppTitle = new CLanguageItem*[_titleNum];
	//memset(_ppTitle, 0, sizeof(_ppTitle));	
}

CSymbleWithTxt::~CSymbleWithTxt()
{
	// delete symbles, titles will delete by LanguageLoader
	for(int i = 0; i < _symbleNum; i++)
	{
		if(_ppSymbles[i] != 0)
			delete _ppSymbles[i];
	}
	delete [] _ppTitle;
	delete [] _ppSymbles;
}

void CSymbleWithTxt::SetSymbles
	(int i
	,CBmp** ppBmp)
{	
	_ppSymbles = ppBmp;
	_symbleNum = i;
	int bmpWidth = 0;
	if((_symbleNum > 0)&&(_ppSymbles[_currentSymble] != NULL))
	{
		bmpWidth = _ppSymbles[_currentSymble]->getWidth();
	}
	CSize s= this->GetSize();
	CPoint p = this->GetRelativePos();
	if(_bSymbleLeft)
	{
		this->SetSize(CSize(s.width - bmpWidth, s.height));
		this->SetRelativePos(CPoint(p.x + bmpWidth, p.y));
		this->SetStyle(BSL_MENU_TITLE
			,FSL_MENU_TITLE
			,BSL_MENU_TITLE
			,FSL_MENU_TITLE
			,2, 0, 0, 0);
		this->SetHAlign(LEFT);
		//_pBmpPosi = p;
	}
	else
	{
		this->SetSize(CSize(s.width - bmpWidth, s.height));
		this->SetStyle(BSL_MENU_TITLE
			,FSL_MENU_TITLE
			,BSL_MENU_TITLE
			,FSL_MENU_TITLE
			,0, 2, 0, 0);
		this->SetHAlign(RIGHT);
		//_pBmpPosi = p;
		//_pBmpPosi.x = p.x + s.width - bmpWidth;
	}
};

void CSymbleWithTxt::SetRltvPosi(CPoint point)
{
	//void SetRelativePos(CPoint leftTop) { _leftTop = leftTop; }
	if(_bSymbleLeft)
	{
		int bmpWidth = _ppSymbles[_currentSymble]->getWidth();
		this->SetRelativePos(CPoint(point.x + bmpWidth, point.y));
	}
	else
	{
		this->SetRelativePos(CPoint(point.x, point.y));
	}
}

void CSymbleWithTxt::Draw(bool bHilight)
{
	
	int bmpWidth = 0;
	int bmpHeigh = 0;
	if((_symbleNum > 0)&&(_ppSymbles[_currentSymble] != NULL))
	{
		bmpWidth = _ppSymbles[_currentSymble]->getWidth();
		bmpHeigh = _ppSymbles[_currentSymble]->getHeight();
	}
	// draw text;
	if((_titleNum > 0)&&(_ppTitle[_currentTitle] != 0))		
		this->SetText((UINT8*)_ppTitle[_currentTitle]->getContent());
	inherited::Draw(false);	
	//draw symble;
	//CAMERALOG("-- _currentSymble : %d", _currentSymble);
	if((_symbleNum > 0)&&(_ppSymbles[_currentSymble] != 0))	
	{
		CPoint tplft = GetAbsPos();
		CPoint posi;
		CSize size = this->GetSize();
		CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
		if(_bSymbleLeft)
			posi.x = tplft.x - bmpWidth;
		else
			posi.x = tplft.x + size.width;
		posi.y = tplft.y + (size.height - bmpHeigh)/2;
		//if(pCav->isBitCanvas())
		//	CAMERALOG("-- _currentSymble : %d", posi.x);
		pCav->DrawRect
			(posi
			, CSize(bmpWidth, bmpHeigh)
			, _ppSymbles[_currentSymble]
			, CCanvas::BmpFillMode_repeat
			, 0);		
	}
}

/*******************************************************************************
*   CBtn
*   
*/
CBtn::CBtn
	(CContainer* pParent
	, CPoint leftTop
	, CSize CSize
	, int ID):inherited(pParent, leftTop, CSize)
	,_buttonID(ID)
{
	this->SetStyle(_brush, _font, _brushHi, _fontHi, 0, 0, 0, 0);
	this->SetHAlign(CENTER);
}

CBtn::~CBtn()
{

}

void CBtn::Draw(bool bHilight)
{
	inherited::Draw(bHilight);
}

bool CBtn::OnEvent(CEvent *event)
{
	bool rt = true;
	if(event->_type == eventType_key)
	{
		switch(event->_event)
		{
			case key_right:
			case key_ok:
				CAMERALOG("--on button : %p", this->GetOwner()->GetApp());
				if(this->GetOwner()->GetApp())
				{
					this->GetOwner()->GetApp()->PushEvent(eventType_internal, InternalEvent_wnd_button_click, _buttonID, 0);
					rt = false;
				}
				break;
			default:
				break;
		}			
	}
	return rt;
}

UINT16 CBtn::_brush; 
UINT16 CBtn::_font;
UINT16 CBtn::_brushHi; 
UINT16 CBtn::_fontHi;

/*******************************************************************************
*   CDigEdit
*   
*/
CBmp* CDigEdit::_pUp = NULL;
CBmp* CDigEdit::_pDown = NULL;
COLOR CDigEdit::_cleanC = 0;
CDigEdit::CDigEdit
	(CContainer* pParent
	, CPoint leftTop
	, CSize CSize
	, int mini
	, int max
	, int digiNum
	, int current
	):inherited(pParent, leftTop, CSize)
	,_maxValue(max)
	,_minValue(mini)
	,_digNum(digiNum)
	,_currentValue(current)
	,_bInEdit(false)
	,_bWithSign(false)
	,_step(1)
{
	char tmp[8];
	memset(tmp, 0, sizeof(tmp));
	sprintf(tmp, "%%0%dd", _digNum);
	_pTxt = new char[_digNum +1];
	memset(_pTxt, 0, _digNum +1);
	snprintf(_pTxt, _digNum + 1, tmp, current);
	this->SetText((UINT8*)_pTxt);
	this->SetHAlign(CENTER);
	this->SetStyle(BSL_BUTTON_NORMAL, FSL_BUTTON_NORMAL, BSL_BUTTON_HILIGHT, FSL_BUTTON_HILIGHT, 0, 0, 0, 0);
}

CDigEdit::~CDigEdit()
{
	delete[] _pTxt;
}

void CDigEdit::Draw(bool bHilight)
{
	char tmp[8];
	memset(tmp, 0, sizeof(tmp));
	if(_bWithSign)
		sprintf(tmp, "%%+0%dd", _digNum);
	else
		sprintf(tmp, "%%0%dd", _digNum);
	memset(_pTxt, 0, _digNum +1);
	snprintf(_pTxt, _digNum + 1, tmp, _currentValue);
	
	inherited::Draw(bHilight);
	
	if((_pUp != 0)&&(_pDown != 0))
	{
		CPoint tplft = GetAbsPos();
		CPoint posi;
		CSize size = this->GetSize();
		CCanvas* pCav = this->GetOwner()->GetManager()->GetCanvas();
		CSize bmpSize = CSize(_pUp->getWidth(), _pUp->getHeight());
		
		if(bHilight && _bInEdit)
		{
			posi.x = tplft.x + size.width / 2 - bmpSize.width / 2;
			posi.y = tplft.y - bmpSize.height;
			pCav->DrawRect(posi, bmpSize , _pUp, CCanvas::BmpFillMode_repeat, 0);

			posi.y = tplft.y + size.height;
			pCav->DrawRect(posi, bmpSize , _pDown, CCanvas::BmpFillMode_repeat, 0);	
		}
		else
		{
			posi.x = tplft.x + size.width / 2 - bmpSize.width / 2;
			posi.y = tplft.y - bmpSize.height;
			pCav->DrawRect(posi, CPoint(posi.x + _pUp->getWidth(), posi.y + _pUp->getHeight()), _cleanC);

			posi.y = tplft.y + size.height;
			pCav->DrawRect(posi, CPoint(posi.x + _pUp->getWidth(), posi.y + _pUp->getHeight()), _cleanC);
		}
	}
}

bool CDigEdit::OnEvent(CEvent *event)
{
	bool rt = true;
	if(event->_type == eventType_key)
	{
		switch(event->_event)
		{
			case key_up:
				CAMERALOG("-- on edit up");
				if(_bInEdit)
				{
					_currentValue += _step;
					if(_currentValue > _maxValue)
						_currentValue = _minValue;
					rt = false;
				}
				break;
			case key_down:
				if(_bInEdit)
				{
					_currentValue -= _step;
					if(_currentValue < _minValue)
						_currentValue = _maxValue;
					rt = false;
				}
				break;
				
			case key_right:
				if(_bInEdit)
				{
					_currentValue += _step;
					if(_currentValue > _maxValue)
						_currentValue = _minValue;
					rt = false;
				}
				break;
			case key_left:
				if(_bInEdit)
				{
					_currentValue -= _step;
					if(_currentValue < _minValue)
						_currentValue = _maxValue;
					rt = false;
				}
				break;
			case key_ok:
				_bInEdit = !_bInEdit;
				rt = false;
				break;
			default:
				break;
		}			
	}
	if(!rt)
		this->Draw(true);
	return rt;
}
bool CDigEdit::setMaxLmitation(int max)
{
	if(max == _maxValue)
		return false;
	_maxValue = max;
	if(_currentValue > _maxValue)
	{
		_currentValue = _maxValue;
		return true;
	}
	return false;
};


CBlock::CBlock
		(CContainer* pParent
		, CPoint leftTop
		, CSize CSize
		, UINT16 b
		):inherited(pParent, leftTop, CSize)
		,_brush(b)
{

}

CBlock::~CBlock()
{

}

void CBlock::Draw(bool bHilight)
{
	CStyle* pStyle = CStyle::GetInstance();
    CBrush *pBrush = pStyle->GetBrush(_brush);
    if(pBrush != NULL)
        pBrush->Draw(this);
}
//#ifdef WIN32
};
//#endif

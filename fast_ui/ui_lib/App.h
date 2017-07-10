/*******************************************************************************
**       App.h
**
**      Copyright (c) 2011 Transee Design
**      All rights reserved.
**
**      Description:
**      1.
**
**      Revision History:
**      -----------------
**      01a Jun-28-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

#ifndef __INCApp_h
#define __INCApp_h
//#ifdef WIN32
namespace ui{
//#endif

/*******************************************************************************
* <Forward declarations>
*******************************************************************************/ 

/*******************************************************************************
* <Macros>
*******************************************************************************/

/*******************************************************************************
* <Types>
*******************************************************************************/

/*******************************************************************************
*   CAppl
*   AUTHOR : Linn Song
*   DEPENDENCE: Class for application
*   
*   REVIEWER:
*/
#define Event_Queue_Number 10

class CAppl : public CThread
{
public:
	CAppl(CWndManager *pMain);
	~CAppl();

	bool PushEvent(
		UINT16 type,
		UINT16 event,
		UINT32 paload,
		UINT32 paload1
	);

	virtual bool OnEvent(CEvent* event) = 0;
	CWnd* GetWnd(){return _pWndManager->GetCurrentWnd();};
	//void ShowMainWnd(){_pWndManager->Show(true);};

protected:
	virtual void main(void *);
	
private:
	CWndManager*		_pWndManager;
	CSemaphore*			_pWaitEvent;
	CMutex*				_pLock;

	CEvent*	_pEvent;
	int		_top;
	int		_bottom;
};

//#ifdef WIN32
}
//#endif

#endif


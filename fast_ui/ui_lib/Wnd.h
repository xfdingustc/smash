/*******************************************************************************
**       Wnd.h
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
**      01a 27- 8-2011 Linn Song CREATE AND MODIFY
**
*******************************************************************************/

#ifndef __INCWnd_h
#define __INCWnd_h

namespace ui{

class CWnd;
/*******************************************************************************
*   CWndManager
*   AUTHOR : Linn Song
*   DEPENDENCE:
*/
#define maxWndNum 40

class CWndManager
{
public:
	static CWndManager *getInstance(){return _pInstance;};
	static void Create(CCanvas* canvas);
	static void Destroy();

	int AddWnd(CWnd* pWnd);
	bool OnEvent(CEvent* event);
	void setCurrentWnd(CWnd *pWnd);
	CCanvas* GetCanvas(){return _pCanvas;};
	int SetCanvas(CCanvas* p){_pCanvas = p;};

	CWndManager(CCanvas* canvas);
	~CWndManager();

	CWnd* GetCurrentWnd(){return _currentWnd;};
private:
	CWnd* 	_ppWndList[maxWndNum];
	int 	_number;
	CWnd* 	_currentWnd;
	

private:
	static CWndManager* _pInstance;
	CMutex *_lock;
	CCanvas		*_pCanvas;
};

/*******************************************************************************
*   CWnd
*   AUTHOR : Linn Song
*   DEPENDENCE: CWnd
*   
*   REVIEWER:
*/
class CAppl;
class CWnd
{
public:
	CWnd(const char *name="no named"
		, CWndManager* pManag = NULL);
	virtual ~CWnd();
	virtual void willHide();
	virtual void willShow();
	virtual bool OnEvent(CEvent* event);
	//virtual bool OnChildrenEvent() = 0;
	void SetApp(CAppl* p){_pParentApp = p;};
	CAppl* GetApp(){return _pParentApp;};
	void SetMainObject(CContainer* p){_pMainObject = p;};
	CContainer* GetMainObject(){return _pMainObject;};

	void Show(bool bResetFocus);
	void Hide(bool bResetFocus);
	void PopWnd(CWnd* pWnd);
	void Close();

	void SetPreWnd(CWnd* pWnd){_preWnd = pWnd;};
	CWnd* GetPreWnd(){return _preWnd;};

	const char* getName(){return _name;};
	
	int GetID(){return _wndID;};
	
    //void SetManager(CWndManager *pManager) { pWndManager = pManager; };
    CWndManager* GetManager() {return _pWndManager;};
private:
	CContainer		*_pMainObject;
	CWndManager		*_pWndManager;
	CAppl			*_pParentApp;
	int				_wndID;
	const char		*_name;
	bool		_bShown;
	CWnd*		_preWnd;
	
};

//#ifdef WIN32
}
//#endif

#endif

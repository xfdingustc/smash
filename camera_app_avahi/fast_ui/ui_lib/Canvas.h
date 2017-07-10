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

#ifndef __INCCanvas_h
#define __INCCanvas_h

//#ifdef WIN32
namespace ui{
//#endif

/*******************************************************************************
* <Forward declarations>
*******************************************************************************/ 
#define OSD16bit_RGB565 1
//#define OSD32bit_RGBA 1
//#define OSD8bit 1

#define Canvas_width 480
#define Canvas_height 800

/*******************************************************************************
* <Macros>
*******************************************************************************/

/*******************************************************************************
* <Types>
*******************************************************************************/
#ifdef OSD8bit
typedef UINT8 COLOR;
#elif OSD16bit_RGB565
typedef UINT16 COLOR;
#elif OSD32bit_RGBA
typedef UINT32 COLOR;
#endif



typedef void (*DRAW_FUNC_PTR)();


struct CPoint
{
    int x;
    int y;
    CPoint(){};
    CPoint( int xx, int yy){ x = xx; y = yy;}
    void Set( int xx, int yy){ x = xx; y = yy;}
    void Mov(int xx, int yy){ x += xx; y+= yy;}
};

struct CSize
{
    UINT16 width;
    UINT16 height;
    CSize(){};
    CSize( UINT16 w, UINT16 h){ width = w;  height = h;}
    void Set( UINT16 w, UINT16 h){ width = w;  height = h;}
};

#define Bit16_Transparent_color 0
class CCanvas
{
public:
	CCanvas
		(COLOR* pPOBBuffer
		,DRAW_FUNC_PTR pDrawHandler
		,int wi
		,int hi
		,bool bBitCanvas);
    virtual ~CCanvas();
public:
	enum BmpFillMode
	{
		BmpFillMode_repeat = 0,
		BmpFillMode_single,
		BmpFillMode_fill,
		BmpFillMode_with_Transparent,
	};
    virtual void FillAll(COLOR c);
	virtual void DrawRect(CPoint lefttop, CPoint rightbottom, COLOR c);
    virtual void DrawRect(CPoint lefttop, CPoint rightbottom, UINT16 borderWidth, COLOR main, COLOR border);
    virtual void DrawRect(CPoint lefttop, CPoint rightbottom, UINT8 cornerRd, UINT16 radius, UINT16 borderWidth, COLOR main, COLOR border);
    virtual void DrawRect
		(CPoint lefttop
    	,CPoint rightbottom
    	,UINT8 cornerRd
    	,UINT16 radius
    	,UINT16 borderWidth
    	,COLOR main
    	,COLOR borderDark
    	,COLOR borderBright
    	,bool bOut
    	);
	virtual void DrawRect
		(CPoint lefttop
		,CPoint rightbottom
		,CBmp* bmp
		,BmpFillMode mode
		);
	virtual void DrawRect
		(
		CPoint lefttop
		, CSize size
		, CBmp* bmp
		, BmpFillMode mode
		, int offset
		);
	void SetChange(){_bNoChanged = false;};
    void Update();
	static UINT16 RGBto565(UINT8 r, UINT8 g, UINT8 b);
	bool isBitCanvas(){return _bBitCanvas;};
    CSize GetSize(){return _size;}
    COLOR* GetCanvas(){return _pCanvas;}
	
protected:
    void DrawPoint(CPoint p, COLOR c);
    void DrawPoint(UINT16 x, UINT16 y, COLOR c);
    void DrawRow(CPoint p, UINT16 endX, COLOR c);
    void DrawRow(UINT16 x, UINT16 y, UINT16 endX, COLOR c);
    void DrawRectBorder(CPoint lefttop, CPoint rightbottom, UINT16 borderWidth, COLOR border);
    void DrawRectBorder(CPoint lefttop, CPoint rightbottom, UINT8 cornerRd, UINT16 radius, UINT16 borderWidth, COLOR border);
    void DrawRound(CPoint center, UINT8 part, UINT16 radius, UINT16 borderWidth, COLOR main, COLOR border);
    void DrawRoundBorder(CPoint center, UINT8 part, UINT16 radius, UINT16 borderWidth, COLOR border);
	
protected:
    CSize				_size;
    COLOR*				_pCanvas;
    bool				_bNoChanged;
    DRAW_FUNC_PTR		_pDrawHandler;
    UINT8				_colorDepth;
	bool				_bBitCanvas;

private:
	int Wmemset(UINT16* pBufer, UINT16 col, int length);

private:
    //static CCanvas*		_pInstance;
    
};

class CBitCanvas : public CCanvas
{
public:
	CBitCanvas(UINT8* pPOBBuffer, DRAW_FUNC_PTR pDrawHandler, int wi, int hi);
    virtual ~CBitCanvas();

	virtual void FillAll(COLOR c);
	void DrawRect
	    (
	    CPoint lefttop, 
	    CPoint rightbottom, 
	    UINT8 cornerRd, 
	    UINT16 radius, 
	    UINT16 borderWidth, 
	    COLOR main, 
	    COLOR border
	    );
	void DrawRect
	    (
	    CPoint lefttop,
	    CPoint rightbottom,
	    COLOR c
	    );
	void DrawRect
	    (
	    CPoint lefttop, 
	    CPoint rightbottom,
	    UINT16 borderWidth, 
	    COLOR main, 
	    COLOR border
	    );
	void DrawRow
		(CPoint p
		,UINT16 endX
		,bool bSet
		);
	void DrawRow
	(
		UINT16 x, 
		UINT16 y,
		UINT16 length,
		COLOR c
	);
	
	void DrawRect
		(CPoint lefttop
		,CSize rightbottom
		,CBmp* bmp
		,CCanvas::BmpFillMode mode
		,int offset
		);
	
	void MapBitMapToBitCanvas
		(UINT16 X
		,UINT16 Y
		,UINT8 *Bitmap
		,UINT16 BitMapWidth
		,UINT16 BitMapHeight
		,UINT16 ActuralWidth
		);
private:
	UINT8*			_pCanvasByte;
	DRAW_FUNC_PTR	_pDrawHandler;
	int				_bytePitch;
};


class CColorPalette
{
public:
    struct PALLET_COLOR           
    {
        UINT8 r;
        UINT8 g;
        UINT8 b;
        UINT8 a;
    };
    
public:
    CColorPalette();
    ~CColorPalette(){};
    PALLET_COLOR* GetPal(){return _pPallet;};
    
private:
    PALLET_COLOR* _pPallet;    
};

//#ifdef WIN32
};
//#endif

#endif

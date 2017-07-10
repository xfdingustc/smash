/*******************************************************************************
**      Canvas.cpp
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
**      01a  8- 9-2011 linn song CREATE AND MODIFY
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
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h" 
#include "wchar.h"
#else
#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"
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
//CCanvas* CCanvas::_pInstance = NULL;

/*******************************************************************************
* <Function elements>
*******************************************************************************/

/*******************************************************************************
*   CCanvas::Create
*   
*/ 

UINT16 CCanvas::RGBto565
	(UINT8 r
	,UINT8 g
	,UINT8 b)
{
	UINT16 rt;
	rt = r >> 3;
	rt = rt << 6;
	rt = rt | (g >> 2);
	rt = rt << 5;
	rt = rt | (b >> 3);
	return rt;
}

/*CCanvas* CCanvas::Create
	(COLOR* pPOBBuffer
	,DRAW_FUNC_PTR pDrawHandler
	,int wi
	,int hi
	,bool bBitBuffer
	)
{ 
    CCanvas* pInstance = new CCanvas(pPOBBuffer, pDrawHandler, wi, hi, bBitBuffer);
	return pInstance;
}

void CCanvas::Destroy(CCanvas* pCCanvas)
{
     if(pCCanvas != NULL)
     {
        delete pCCanvas;
     }
}*/

/*******************************************************************************
*   CCanvas::Wmemset
*   
*/ 
int CCanvas::Wmemset(UINT16* pBufer, UINT16 col, int length)
{
#ifdef WIN32
	wmemset((wchar_t *)pBufer, col, length);
	_bNoChanged = false;
#else
	// LINUX TBD
	int r = length;
	UINT32 color = col;
	color = (color << 16)|col;
	UINT32* p;
	while(r > 0)
	{
		if((UINT32)pBufer % 4 == 0)
		{
			if(r >= 8)
			{
				p = (UINT32*)pBufer;
				p[0] = color;
				p[1] = color;
				p[2] = color;
				p[3] = color;
				/*pBufer[0] = col;
				pBufer[1] = col;
				pBufer[2] = col;
				pBufer[3] = col;
				pBufer[4] = col;
				pBufer[5] = col;
				pBufer[6] = col;
				pBufer[7] = col;*/
				pBufer += 8;
				r -= 8;
			}
			else
			{
				pBufer[0] = col;
				pBufer ++;
				r --;
			}
		}
		else
		{
			pBufer[0] = col;
			pBufer ++;
			r --;
		}
	}
#endif
	return 0;
}


/*******************************************************************************
*   CCanvas::FillAll
*   
*/ 
void CCanvas::FillAll
	(COLOR c
	)
{
#ifdef OSD8bit
	memset(_pCanvas, c, _size.height * _size.width);
	_bNoChanged = false;
#elif  OSD16bit_RGB565
	Wmemset(_pCanvas, c, _size.height * _size.width);
#endif
	_bNoChanged = false;
	Update();
}

/*******************************************************************************
*   CCanvas::DrawPoint
*   
*/ 
void CCanvas::DrawPoint
	(CPoint p
	,COLOR c
	)
{
	if(p.y *_size.width + p.x < _size.height*_size.width)
	{
		*(_pCanvas + p.y *_size.width + p.x) = c;
		_bNoChanged = false;
	}
}

void CCanvas::DrawPoint
    (
    UINT16 x, 
    UINT16 y,
    COLOR c
    )
{
    if(y *_size.width + x < _size.height*_size.width)
    {
        *(_pCanvas + y *_size.width + x) = c;
        _bNoChanged = false;
    }
}


/*******************************************************************************
*   CCanvas::DrawRow
*   
*/ 
void CCanvas::DrawRow
	(
		CPoint p,
		UINT16 endX,
		COLOR c
	)
{
#ifdef OSD8bit
	memset(_pCanvas + p.y  * _size.width + p.x, c, endX - p.x);	
#elif OSD16bit_RGB565
	Wmemset(_pCanvas + p.y  * _size.width + p.x, c, endX - p.x);
#endif

	_bNoChanged = false;   
}

void CCanvas::DrawRow
	(
		UINT16 x, 
		UINT16 y,
		UINT16 length,
		COLOR c
	)
{
#ifdef OSD8bit
	memset(_pCanvas + y  * _size.width + x, c, length);	
#elif  OSD16bit_RGB565
	Wmemset(_pCanvas + y  * _size.width + x, c, length);
#endif
    
    _bNoChanged = false;
}

/*******************************************************************************
*   CCanvas::DrawRect
*   
*/ 
void CCanvas::DrawRect
    (
    CPoint lefttop,
    CPoint rightbottom,
    COLOR c
    )
{ 
    for(int i = lefttop.y; i < rightbottom.y; i++)
    {
#ifdef OSD8bit
	memset(_pCanvas + i  * _size.width + lefttop.x, c, rightbottom.x - lefttop.x);	
#elif  OSD16bit_RGB565
	Wmemset(_pCanvas + (i  * _size.width + lefttop.x), c, rightbottom.x - lefttop.x);
#endif        
    }
    _bNoChanged = false;
}

void CCanvas::DrawRect
	(
		CPoint lefttop
		, CSize size
		, CBmp* bmp
		, BmpFillMode mode
		, int offset
	)
{
	int j;
	UINT8* line;
	if(_bBitCanvas)
		printf("--wrong call\n");
	switch(mode)
	{
		case BmpFillMode_repeat : 
#ifdef  OSD16bit_RGB565
	
 			if(bmp->getColorDepth() == 16)
 			{
				for(int i = lefttop.y; i < lefttop.y + size.height; i++)
				{
					line = bmp->GetBuffer()+bmp->GetPitch()*(bmp->getHeight() - (i  - lefttop.y) % bmp->getHeight() - 1) + offset * 2;
					for(j = 0; j < size.width / bmp->getWidth(); j++)
					{
						memcpy(_pCanvas + i  * _size.width + lefttop.x + j*bmp->getWidth(), line, size.width* 2);
					}
					if(size.width % bmp->getWidth() > 0)
						memcpy(_pCanvas + i  * _size.width + lefttop.x + j*bmp->getWidth(), line, size.width * 2);
				} 
			}

#endif
			break;
		case BmpFillMode_single:
			
			break;
		default:
			
			break;
	}
	_bNoChanged =false;
}


void CCanvas::DrawRect
	(CPoint lefttop
	, CPoint rightbottom
	, CBmp* bmp
	, BmpFillMode mode
	)
{
	int j;
	UINT8* line;
	switch(mode)
	{
		case BmpFillMode_repeat : 
#ifdef  OSD16bit_RGB565
		
 			if(bmp->getColorDepth() == 16)
 			{
				for(int i = lefttop.y; i < rightbottom.y; i++)
				{
					line = bmp->GetBuffer()+bmp->GetPitch()*(bmp->getHeight() - i % bmp->getHeight() - 1);
					for(j = 0; j < (rightbottom.x - lefttop.x) / bmp->getWidth(); j++)
					{
						memcpy(_pCanvas + i  * _size.width + lefttop.x + j*bmp->getWidth(), line, bmp->getWidth()*(bmp->getColorDepth() / 8));
					}
					if((rightbottom.x - lefttop.x) % bmp->getWidth() > 0)
						memcpy(_pCanvas + i  * _size.width + lefttop.x + j*bmp->getWidth(), line, ((rightbottom.x - lefttop.x) % bmp->getWidth())*(bmp->getColorDepth() / 8));
				} 
			}
#endif
			break;
		case BmpFillMode_single:
			
			break;
		case BmpFillMode_with_Transparent:
#ifdef  OSD16bit_RGB565
			if(bmp->getColorDepth() == 16)
 			{
				for(int i = lefttop.y; i < rightbottom.y; i++)
				{
					line = bmp->GetBuffer()+bmp->GetPitch()*(bmp->getHeight() - (i - lefttop.y) - 1);
					for(j = lefttop.x ; j < rightbottom.x; j++)
					{
						//memcpy(_pCanvas + i  * _size.width + lefttop.x + j*bmp->getWidth(), line, bmp->getWidth()*(bmp->getColorDepth() / 8));
						if(*((UINT16*)(line + (j - lefttop.x)*2 )) != Bit16_Transparent_color)
							*(_pCanvas + i  * _size.width  + j) = *((COLOR*)line + j - lefttop.x);							
					}					
				} 
			}

#endif
			break;
		default:
			
			break;
	}
	_bNoChanged =false;
}



/*******************************************************************************
*   CCanvas::Update
*   
*/ 
//extern CWinThread* _pThreadPOB;
void CCanvas::Update( )
{
    if(!_bNoChanged)
    {
        if(_pDrawHandler != NULL)
        {
            (*_pDrawHandler)();
        }
    }
    _bNoChanged = true;
}

/*******************************************************************************
*   CCanvas::CCanvas
*   
*/
CCanvas::CCanvas
	(COLOR* pPOBBuffer
	,DRAW_FUNC_PTR pDrawHandler
	,int width
	,int height
	,bool bBitCanvas
	): _size(width, height)
	,_pCanvas(pPOBBuffer)
	,_bNoChanged(true)
	,_pDrawHandler(pDrawHandler)
	,_bBitCanvas(bBitCanvas)
{
	COLOR test;
	_colorDepth = sizeof(test);
}

/*******************************************************************************
*   CCanvas::CCanvas
*   
*/
CCanvas::~CCanvas()
{
}


/*******************************************************************************
*   CCanvas::DrawRound
*   
*/
void CCanvas::DrawRound
    (
    CPoint center,
    UINT8 part,
    UINT16 radius,
    UINT16 borderWidth,
    COLOR main,
    COLOR border
    )
{
    int i , j, k;
    //UINT8   uTmpColor;
    int Rsquire;
    Rsquire = (int)(radius*radius);
    Rsquire = Rsquire-1;

    int R_Wsquire;
    R_Wsquire = (int)((radius-borderWidth)*(radius-borderWidth));
    R_Wsquire = R_Wsquire-1;

    for(i = 0; i<=(int)radius; i++)
    {
        for(j = 0; j<=(int)radius; j++)
        {
            k=(int)(j*j+i*i);
            if(k<=Rsquire)
            {
                if(part&8)
                {
                    if(k<=R_Wsquire)
                        DrawPoint(center.x+i,center.y+j,main);				
                    else
                        DrawPoint(center.x+i,center.y+j,border);
                }
                if(part&4)
                {
                	if(k<=R_Wsquire)
                        DrawPoint(center.x-i,center.y+j,main);				
                	else
                        DrawPoint(center.x-i,center.y+j,border);
                }
                if(part&2)
                {
                    if(k<=R_Wsquire)
                        DrawPoint(center.x-i,center.y-j,main);				
                    else
                        DrawPoint(center.x-i,center.y-j,border);
                }
                if(part&1)
                {
                    if(k<=R_Wsquire)
                        DrawPoint(center.x+i,center.y-j,main);				
                    else
                        DrawPoint(center.x+i,center.y-j,border);
                }
            }
        }
    }
}

/*******************************************************************************
*   CCanvas::DrawRoundBorder
*   
*/
void CCanvas::DrawRoundBorder
    (
    CPoint center,
    UINT8 part,
    UINT16 radius,
    UINT16 borderWidth,
    COLOR border
    )
{
    int i , j, k;
    int Rsquire;
    Rsquire = (int)(radius*radius);
    Rsquire = Rsquire-1;

    int R_Wsquire;
    R_Wsquire = (int)((radius-borderWidth)*(radius-borderWidth));
    R_Wsquire = R_Wsquire-1;

    for(i = 0; i<=(int)radius; i++)
    {
        for(j = 0; j<=(int)radius; j++)
        {
            k=(int)(j*j+i*i);
            if(k<=Rsquire)
            {
                if(part&8)
                {
                    if(k > R_Wsquire)
                        DrawPoint(center.x+i,center.y+j,border);
                }
                if(part&4)
                {
                	if(k > R_Wsquire)
                        DrawPoint(center.x-i,center.y+j,border);
                }
                if(part&2)
                {
                    if(k > R_Wsquire)
                        DrawPoint(center.x-i,center.y-j,border);
                }
                if(part&1)
                {
                    if(k > R_Wsquire)
                        DrawPoint(center.x+i,center.y-j,border);
                }
            }
        }
    }

}

/*******************************************************************************
*   CCanvas::DrawRect
*   condition : 
*/
void CCanvas::DrawRect
    (
    CPoint lefttop, 
    CPoint rightbottom,
    UINT16 borderWidth, 
    COLOR main, 
    COLOR border
    )
{
    if(borderWidth == 0)
        DrawRect(lefttop, rightbottom, main);
    else
    {
        for (int j=0 ; j<(rightbottom.y - lefttop.y) ; j++){
        	if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        	{
        		DrawRow(lefttop.x, lefttop.y+j, (rightbottom.x - lefttop.x), border);
        	}
        	else
        	{
        		DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
        		DrawRow(lefttop.x+borderWidth, lefttop.y+j, (rightbottom.x - lefttop.x) - 2*borderWidth, main);
        		DrawRow(lefttop.x + (rightbottom.x - lefttop.x) - borderWidth, lefttop.y+j, borderWidth, border);			
        	}
        }
    }
}

/*******************************************************************************
*   CCanvas::DrawRect
*   
*/
void CCanvas::DrawRect
    (
    CPoint lefttop, 
    CPoint rightbottom, 
    UINT8 cornerRd, 
    UINT16 radius, 
    UINT16 borderWidth, 
    COLOR main, 
    COLOR border
    )
{
    CPoint point1, point2;
    int j;
    UINT16 width = rightbottom.x - lefttop.x;
    UINT16 height = rightbottom.y - lefttop.y;
    if(radius == 0)
    {
        if(borderWidth == 0)
            DrawRect(lefttop, rightbottom, main);
        else
            DrawRect(lefttop, rightbottom, borderWidth, main, border);
    }
    else if((radius < ((width) < (height) ? (width) : (height)) /2)&&(radius > borderWidth))
    {
        if(cornerRd&2)
        {
            point1.Set(radius+lefttop.x-1, radius+lefttop.y-1);
            DrawRound(point1, 2, radius, borderWidth, main, border);
        }
        else{
        	for (j = 0; j < radius; j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(lefttop.x, lefttop.y+j, radius, border);
        		}
        		else
        		{
        			DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
        			DrawRow(lefttop.x + borderWidth, lefttop.y+j, radius - borderWidth, main);
        		}
        	}
        }
        if(cornerRd&1){
            point1.Set(rightbottom.x -radius , radius + lefttop.y-1);
            DrawRound(point1, 1, radius, borderWidth, main, border);
        }
        else{
        	for (j=0;j<radius;j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(rightbottom.x - radius, lefttop.y + j, radius, border);
        		}
        		else
        		{
        			DrawRow(rightbottom.x - borderWidth, lefttop.y + j, borderWidth, border);
        			DrawRow(rightbottom.x - radius, lefttop.y + j, radius - borderWidth, main);
        		}				
        	}
        }
        if(cornerRd&4){
            point1.Set(radius + lefttop.x-1, rightbottom.y - radius);
            DrawRound(point1, 4, radius, borderWidth, main, border);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                DrawRow(lefttop.x, lefttop.y+j, radius, border);
                }
                else
                {
                DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
                DrawRow(lefttop.x + borderWidth, lefttop.y+j, radius - borderWidth, main);
                }
            }
        }
        if(cornerRd&8)
        {
        	 point1.Set(rightbottom.x -radius , rightbottom.y - radius);
            DrawRound(point1, 8, radius, borderWidth, main, border);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-radius, lefttop.y+j, radius, border);
                }
                else
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-borderWidth, lefttop.y+j, borderWidth, border);
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-radius, lefttop.y+j, radius - borderWidth, main);
                }	
            }
        }
        //
        point1.Set(lefttop.x + radius, lefttop.y);
        point2.Set(rightbottom.x - radius, lefttop.y + borderWidth);
        DrawRect(point1, point2, border);
        point1.Set(lefttop.x + radius, lefttop.y + borderWidth);
        point2.Set(rightbottom.x - radius, lefttop.y + radius);
        DrawRect(point1, point2, main);

        //
        point1.Set(lefttop.x, lefttop.y + radius);
        point2.Set(lefttop.x + borderWidth, rightbottom.y -radius);
        DrawRect(point1, point2, border);
        point1.Set(lefttop.x + borderWidth, lefttop.y + radius);
        point2.Set(rightbottom.x - borderWidth, rightbottom.y-radius);
        DrawRect(point1, point2, main);
        point1.Set(rightbottom.x - borderWidth, lefttop.y + radius);
        point2.Set(rightbottom.x, rightbottom.y-radius);
        DrawRect(point1, point2, border);

        point1.Set(lefttop.x + radius, rightbottom.y - radius);
        point2.Set(rightbottom.x - radius, rightbottom.y - borderWidth);
        DrawRect(point1, point2, main);
        point1.Set(lefttop.x + radius, rightbottom.y - borderWidth);
        point2.Set(rightbottom.x - radius, rightbottom.y);
        DrawRect(point1, point2, border);
    }   
}

void CCanvas::DrawRect
    (
    CPoint lefttop, 
    CPoint rightbottom, 
    UINT8 cornerRd, 
    UINT16 radius, 
    UINT16 borderWidth, 
    COLOR main, 
    COLOR borderDark,
    COLOR borderBright,
    bool bOut
    )
{
    CPoint point1, point2;
    int j;
    UINT16 width = rightbottom.x - lefttop.x;
    UINT16 height = rightbottom.y - lefttop.y;
    COLOR upC, downC;
    if(bOut)
    {
        upC = borderBright;
        downC = borderDark;
    }
    else
    {
        upC = borderDark;
        downC = borderBright;
    }
        
        
    if(radius == 0)
    {
        if(borderWidth == 0)
            DrawRect(lefttop, rightbottom, main);
        else
            DrawRect(lefttop, rightbottom, borderWidth, main, upC);
    }
    else if((radius < ((width) < (height) ? (width) : (height)) /2)&&(radius > borderWidth))
    {
        if(cornerRd&2)
        {
            point1.Set(radius+lefttop.x-1, radius+lefttop.y-1);
            DrawRound(point1, 2, radius, borderWidth, main, upC);
        }
        else{
        	for (j = 0; j < radius; j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(lefttop.x, lefttop.y+j, radius, upC);
        		}
        		else
        		{
        			DrawRow(lefttop.x, lefttop.y+j, borderWidth, upC);
        			DrawRow(lefttop.x + borderWidth, lefttop.y+j, radius - borderWidth, main);
        		}
        	}
        }
        if(cornerRd&1){
            point1.Set(rightbottom.x -radius , radius + lefttop.y-1);
            DrawRound(point1, 1, radius, borderWidth, main, upC);
        }
        else{
        	for (j=0;j<radius;j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(rightbottom.x - radius, lefttop.y + j, radius, upC);
        		}
        		else
        		{
        			DrawRow(rightbottom.x - borderWidth, lefttop.y + j, borderWidth, upC);
        			DrawRow(rightbottom.x - radius, lefttop.y + j, radius - borderWidth, main);
        		}				
        	}
        }
        if(cornerRd&4){
            point1.Set(radius + lefttop.x-1, rightbottom.y - radius);
            DrawRound(point1, 4, radius, borderWidth, main, downC);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                DrawRow(lefttop.x, lefttop.y+j, radius, downC);
                }
                else
                {
                DrawRow(lefttop.x, lefttop.y+j, borderWidth, downC);
                DrawRow(lefttop.x + borderWidth, lefttop.y+j, radius - borderWidth, main);
                }
            }
        }
        if(cornerRd&8)
        {
        	 point1.Set(rightbottom.x -radius , rightbottom.y - radius);
            DrawRound(point1, 8, radius, borderWidth, main, downC);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-radius, lefttop.y+j, radius, downC);
                }
                else
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-borderWidth, lefttop.y+j, borderWidth, downC);
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-radius, lefttop.y+j, radius - borderWidth, main);
                }	
            }
        }
        //
        point1.Set(lefttop.x + radius, lefttop.y);
        point2.Set(rightbottom.x - radius, lefttop.y + borderWidth);
        DrawRect(point1, point2, upC);
        point1.Set(lefttop.x + radius, lefttop.y + borderWidth);
        point2.Set(rightbottom.x - radius, lefttop.y + radius);
        DrawRect(point1, point2, main);

        //
        point1.Set(lefttop.x, lefttop.y + radius);
        point2.Set(lefttop.x + borderWidth, rightbottom.y -radius);
        DrawRect(point1, point2, upC);
        point1.Set(lefttop.x + borderWidth, lefttop.y + radius);
        point2.Set(rightbottom.x - borderWidth, rightbottom.y-radius);
        DrawRect(point1, point2, main);
        point1.Set(rightbottom.x - borderWidth, lefttop.y + radius);
        point2.Set(rightbottom.x, rightbottom.y-radius);
        DrawRect(point1, point2, downC);

        point1.Set(lefttop.x + radius, rightbottom.y - radius);
        point2.Set(rightbottom.x - radius, rightbottom.y - borderWidth);
        DrawRect(point1, point2, main);
        point1.Set(lefttop.x + radius, rightbottom.y - borderWidth);
        point2.Set(rightbottom.x - radius, rightbottom.y);
        DrawRect(point1, point2, downC);
    }   
}
/*******************************************************************************
*   CCanvas::DrawRectBorder
*   
*/
void CCanvas::DrawRectBorder
    (
    CPoint lefttop, 
    CPoint rightbottom,
    UINT16 borderWidth, 
    COLOR border
    )
{
    if(borderWidth > 0)
    {
        for (int j=0 ; j<(rightbottom.y - lefttop.y) ; j++){
        	if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        	{
        		DrawRow(lefttop.x, lefttop.y+j, (rightbottom.x - lefttop.x), border);
        	}
        	else
        	{
        		DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
        		DrawRow(lefttop.x + (rightbottom.x - lefttop.x) - borderWidth, lefttop.y+j, borderWidth, border);			
        	}
        }
    }
}

/*******************************************************************************
*   CCanvas::DrawRectBorder
*   
*/
void CCanvas::DrawRectBorder
    (
    CPoint lefttop, 
    CPoint rightbottom, 
    UINT8 cornerRd, 
    UINT16 radius, 
    UINT16 borderWidth, 
    COLOR border
    )
{
    CPoint point1, point2;
    int j;
    UINT16 width = rightbottom.x - lefttop.x;
    UINT16 height = rightbottom.y - lefttop.y;
    if(radius == 0)
    {
        DrawRectBorder(lefttop, rightbottom, borderWidth, border);
    }
    else if((radius < ((width) < (height) ? (width) : (height)) /2)&&(radius > borderWidth))
    {
        if(cornerRd&2)
        {
            point1.Set(radius+lefttop.x-1, radius+lefttop.y-1);
            DrawRoundBorder(point1, 2, radius, borderWidth, border);
        }
        else{
        	for (j = 0; j < radius; j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(lefttop.x, lefttop.y+j, radius, border);
        		}
        		else
        		{
        			DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
        		}
        	}
        }
        if(cornerRd&1){
            point1.Set(rightbottom.x -radius , radius + lefttop.y-1);
            DrawRoundBorder(point1, 1, radius, borderWidth, border);
        }
        else{
        	for (j=0;j<radius;j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(rightbottom.x - radius, lefttop.y + j, radius, border);
        		}
        		else
        		{
        			DrawRow(rightbottom.x - borderWidth, lefttop.y + j, borderWidth, border);
        		}				
        	}
        }
        if(cornerRd&4){
            point1.Set(radius + lefttop.x-1, rightbottom.y - radius);
            DrawRoundBorder(point1, 4, radius, borderWidth, border);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                DrawRow(lefttop.x, lefttop.y+j, radius, border);
                }
                else
                {
                DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
                }
            }
        }
        if(cornerRd&8)
        {
            point1.Set(rightbottom.x -radius , rightbottom.y - radius);
            DrawRoundBorder(point1, 8, radius, borderWidth, border);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-radius, lefttop.y+j, radius, border);
                }
                else
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-borderWidth, lefttop.y+j, borderWidth, border);
                }	
            }
        }
        //
        point1.Set(lefttop.x + radius, lefttop.y);
        point2.Set(rightbottom.x - radius, lefttop.y + borderWidth);
        DrawRect(point1, point2, border);

        //
        point1.Set(lefttop.x, lefttop.y + radius);
        point2.Set(lefttop.x + borderWidth, rightbottom.y -radius);
        DrawRect(point1, point2, border);
        point1.Set(rightbottom.x - borderWidth, lefttop.y + radius);
        point2.Set(rightbottom.x, rightbottom.y-radius);
        DrawRect(point1, point2, border);

        point1.Set(lefttop.x + radius, rightbottom.y - borderWidth);
        point2.Set(rightbottom.x - radius, rightbottom.y);
        DrawRect(point1, point2, border);
    }   
}
/*******************************************************************************
*      
*/
CColorPalette::CColorPalette()
{
    static CColorPalette::PALLET_COLOR pPAL[]= 
    {
        { 195, 63, 80, 240 },
        { 212, 89, 109, 240 },
        { 208, 69, 92, 240 },
        { 211, 122, 144, 240 },
        { 189, 95, 126, 240 },
        { 151, 59, 92, 240 },
        { 188, 58, 111, 240 },
        { 154, 43, 104, 240 },
        { 146, 40, 106, 240 },
        { 154, 72, 123, 240 },
        { 160, 92, 136, 240 },
        { 172, 124, 156, 240 },
        { 142, 92, 133, 240 },
        { 106, 34, 105, 240 },
        { 104, 34, 103, 240 },
        { 105, 34, 106, 240 },
        { 105, 49, 113, 240 },
        { 101, 51, 115, 240 },
        { 119, 88, 139, 240 },
        { 89, 54, 118, 240 },
        { 102, 71, 129, 240 },
        { 68, 33, 107, 240 },
        { 57, 23, 102, 240 },
        { 65, 32, 107, 240 },
        { 51, 19, 100, 240 },
        { 73, 47, 114, 240 },
        { 52, 25, 103, 240 },
        { 50, 26, 104, 240 },
        { 49, 28, 105, 240 },
        { 49, 35, 109, 240 },
        { 50, 49, 117, 240 },
        { 142, 142, 145, 240 },
        { 39, 48, 117, 240 },
        { 139, 147, 183, 200 },
        { 178, 179, 183, 240 },
        { 156, 157, 160, 240 },
        { 201, 202, 204, 240 },
        { 68, 102, 157, 240 },
        { 21, 86, 147, 240 },
        { 37, 93, 141, 240 },
        { 110, 115, 119, 240 },
        { 133, 137, 140, 240 },
        { 6, 114, 176, 240 },
        { 53, 59, 62, 240 },
        { 38, 42, 44, 240 },
        { 71, 77, 80, 240 },
        { 90, 97, 100, 240 },
        { 6, 147, 195, 240 },
        { 21, 59, 63, 240 },
        { 81, 167, 172, 240 },
        { 77, 135, 138, 240 },
        { 117, 173, 177, 240 },
        { 6, 149, 154, 240 },
        { 34, 96, 98, 240 },
        { 28, 29, 29, 240 },
        { 6, 156, 126, 240 },
        { 6, 152, 115, 240 },
        { 6, 136, 96, 240 },
        { 6, 97, 61, 240 },
        { 6, 151, 76, 240 },
        { 6, 140, 65, 240 },
        { 6, 136, 65, 240 },
        { 6, 147, 63, 240 },
        { 77, 137, 95, 240 },
        { 118, 175, 132, 240 },
        { 82, 168, 101, 240 },
        { 46, 159, 60, 240 },
        { 146, 190, 150, 240 },
        { 69, 139, 61, 240 },
        { 116, 174, 89, 240 },
        { 113, 173, 56, 240 },
        { 142, 188, 74, 240 },
        { 134, 137, 92, 240 },
        { 177, 183, 45, 240 },
        { 136, 140, 55, 240 },
        { 95, 97, 56, 240 },
        { 207, 209, 75, 240 },
        { 212, 214, 122, 240 },
        { 215, 216, 168, 240 },
        { 208, 208, 6, 240 },
        { 62, 60, 46, 240 },
        { 217, 172, 6, 240 },
        { 186, 183, 176, 240 },
        { 189, 139, 48, 240 },
        { 214, 129, 29, 240 },
        { 216, 172, 122, 240 },
        { 144, 97, 48, 240 },
        { 210, 90, 6, 240 },
        { 209, 88, 6, 240 },
        { 215, 130, 72, 240 },
        { 193, 99, 39, 240 },
        { 193, 63, 6, 240 },
        { 205, 58, 6, 240 },
        { 111, 57, 40, 240 },
        { 158, 53, 26, 240 },
        { 210, 91, 67, 240 },
        { 215, 128, 117, 240 },
        { 217, 172, 167, 240 },
        { 105, 59, 55, 240 },
        { 206, 61, 57, 240 },
        { 223, 223, 223, 240 },
        { 6, 6, 6, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 240 },
        { 0, 0, 0, 0},      
    } ;
    _pPallet = pPAL;
}

/*******************************************************************************
*   CBitCanvas::CBitCanvas
*   
*/
CBitCanvas::CBitCanvas
	(UINT8* pPOBBuffer
	, DRAW_FUNC_PTR pDrawHandler
	, int wi
	, int hi
	):CCanvas((COLOR*)pPOBBuffer
	,pDrawHandler
	,wi
	,hi
	,true)
{
	_pCanvasByte = pPOBBuffer;
	_bytePitch = wi / 8;
}

/*******************************************************************************
*   CBitCanvas::~CBitCanvas
*   
*/
CBitCanvas::~CBitCanvas()
{

}

/*******************************************************************************
*   CBitCanvas::FillAll
*   
*/
void CBitCanvas::FillAll(COLOR c)
{
	if(c == 0)
	{
		memset(_pCanvas, 0, _size.height*_bytePitch);
	}
	else
	{
		memset(_pCanvas, 0xff, _size.height*_bytePitch);
	}
}

/*******************************************************************************
*   CBitCanvas::DrawRect
*   
*/
void CBitCanvas::DrawRect
    (
    CPoint lefttop, 
    CPoint rightbottom, 
    UINT8 cornerRd, 
    UINT16 radius, 
    UINT16 borderWidth, 
    COLOR main, 
    COLOR border
    )
{
    CPoint point1, point2;
    int j;
    UINT16 width = rightbottom.x - lefttop.x;
    UINT16 height = rightbottom.y - lefttop.y;
    if(radius == 0)
    {
        if(borderWidth == 0)
        	DrawRect(lefttop, rightbottom, main);
        else
        	DrawRect(lefttop, rightbottom, borderWidth, main, border);
    }
    /*else if((radius < ((width) < (height) ? (width) : (height)) /2)&&(radius > borderWidth))
    {
        if(cornerRd&2)
        {
            point1.Set(radius+lefttop.x-1, radius+lefttop.y-1);
            DrawRound(point1, 2, radius, borderWidth, main, border);
        }
        else{
        	for (j = 0; j < radius; j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(lefttop.x, lefttop.y+j, radius, border);
        		}
        		else
        		{
        			DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
        			DrawRow(lefttop.x + borderWidth, lefttop.y+j, radius - borderWidth, main);
        		}
        	}
        }
        if(cornerRd&1){
            point1.Set(rightbottom.x -radius , radius + lefttop.y-1);
            DrawRound(point1, 1, radius, borderWidth, main, border);
        }
        else{
        	for (j=0;j<radius;j++){
        		if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        		{
        			DrawRow(rightbottom.x - radius, lefttop.y + j, radius, border);
        		}
        		else
        		{
        			DrawRow(rightbottom.x - borderWidth, lefttop.y + j, borderWidth, border);
        			DrawRow(rightbottom.x - radius, lefttop.y + j, radius - borderWidth, main);
        		}				
        	}
        }
        if(cornerRd&4){
            point1.Set(radius + lefttop.x-1, rightbottom.y - radius);
            DrawRound(point1, 4, radius, borderWidth, main, border);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                DrawRow(lefttop.x, lefttop.y+j, radius, border);
                }
                else
                {
                DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
                DrawRow(lefttop.x + borderWidth, lefttop.y+j, radius - borderWidth, main);
                }
            }
        }
        if(cornerRd&8)
        {
        	 point1.Set(rightbottom.x -radius , rightbottom.y - radius);
            DrawRound(point1, 8, radius, borderWidth, main, border);
        }
        else{
            for (j=(rightbottom.y - lefttop.y)-radius ; j<(rightbottom.y - lefttop.y) ; j++)
            {
                if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-radius, lefttop.y+j, radius, border);
                }
                else
                {
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-borderWidth, lefttop.y+j, borderWidth, border);
                    DrawRow(lefttop.x + (rightbottom.x - lefttop.x)-radius, lefttop.y+j, radius - borderWidth, main);
                }	
            }
        }
        //
        point1.Set(lefttop.x + radius, lefttop.y);
        point2.Set(rightbottom.x - radius, lefttop.y + borderWidth);
        DrawRect(point1, point2, border);
        point1.Set(lefttop.x + radius, lefttop.y + borderWidth);
        point2.Set(rightbottom.x - radius, lefttop.y + radius);
        DrawRect(point1, point2, main);

        //
        point1.Set(lefttop.x, lefttop.y + radius);
        point2.Set(lefttop.x + borderWidth, rightbottom.y -radius);
        DrawRect(point1, point2, border);
        point1.Set(lefttop.x + borderWidth, lefttop.y + radius);
        point2.Set(rightbottom.x - borderWidth, rightbottom.y-radius);
        DrawRect(point1, point2, main);
        point1.Set(rightbottom.x - borderWidth, lefttop.y + radius);
        point2.Set(rightbottom.x, rightbottom.y-radius);
        DrawRect(point1, point2, border);

        point1.Set(lefttop.x + radius, rightbottom.y - radius);
        point2.Set(rightbottom.x - radius, rightbottom.y - borderWidth);
        DrawRect(point1, point2, main);
        point1.Set(lefttop.x + radius, rightbottom.y - borderWidth);
        point2.Set(rightbottom.x - radius, rightbottom.y);
        DrawRect(point1, point2, border);
    } */  
}

/*******************************************************************************
*   CBitCanvas::DrawRect
*   
*/
void CBitCanvas::DrawRect
    (
    CPoint lefttop,
    CPoint rightbottom,
    COLOR c
    )
{ 
	//UINT8* p;
	//int j = 0;
	//int wi = (rightbottom.x - lefttop.x);
	//int r = lefttop.x % 8;
	//int s = 
	bool bset = c > 0;
    for(int i = lefttop.y; i < rightbottom.y; i++)
    {
    	DrawRow(CPoint(lefttop.x, i), rightbottom.x, bset);
    }
    _bNoChanged = false;
}

void CBitCanvas::DrawRect
    (
    CPoint lefttop, 
    CPoint rightbottom,
    UINT16 borderWidth, 
    COLOR main, 
    COLOR border
    )
{
    if(borderWidth == 0)
        DrawRect(lefttop, rightbottom, main);
    else
    {
        for (int j=0 ; j<(rightbottom.y - lefttop.y) ; j++){
        	if((j < borderWidth) | (j >= (rightbottom.y - lefttop.y)-borderWidth))
        	{
        		DrawRow(lefttop.x, lefttop.y+j, (rightbottom.x - lefttop.x), border);
        	}
        	else
        	{
        		DrawRow(lefttop.x, lefttop.y+j, borderWidth, border);
        		DrawRow(lefttop.x+borderWidth, lefttop.y+j, (rightbottom.x - lefttop.x) - 2*borderWidth, main);
        		DrawRow(rightbottom.x - borderWidth, lefttop.y+j, borderWidth, border);			
        	}
        }
    }
}

void CBitCanvas::DrawRect
	(CPoint lefttop
	,CSize rightbottom
	,CBmp* bmp
	,CCanvas::BmpFillMode mode
	,int offset
	)
{
	int j;
	UINT8* line;
	//printf("--CBitCanvas::DrawRect : %d\n", bmp->getColorDepth());
	switch(mode)
	{
		case CCanvas::BmpFillMode_repeat:  //TBD
			if(bmp->getColorDepth() == 1)
			{
 				MapBitMapToBitCanvas
					(lefttop.x
					, lefttop.y
					, bmp->GetBuffer()
					, bmp->GetPitch()*8
					, bmp->getHeight()
					, bmp->getWidth());
				}
			break;
		case CCanvas::BmpFillMode_single:
			if(bmp->getColorDepth() == 1)
			{
 				MapBitMapToBitCanvas
					(lefttop.x
					, lefttop.y
					, bmp->GetBuffer()
					, bmp->GetPitch()*8
					, bmp->getHeight()
					, bmp->getWidth());
				}
			break;
		default:
			break;
	}
	_bNoChanged =false;
}


void CBitCanvas::MapBitMapToBitCanvas
	(UINT16 X
	,UINT16 Y
	,UINT8 *Bitmap
	,UINT16 BitMapWidth
	,UINT16 BitMapHeight
	,UINT16 ActuralWidth
	)
{    
	
    INT32  height, width;
    UINT8 *canvas;
    UINT8  bit=0;
    UINT32 byte_in_bmpbuffer=0;
    UINT8* pMem = _pCanvasByte;
	UINT16 memWidth = _bytePitch;
	UINT16 memHeight = this->GetSize().height;
	
    register UINT8* memlimit = pMem + memWidth * memHeight;
    UINT8* ref = pMem + ( memWidth * Y + X / 8 );
    int r = X % 8;
	UINT8* p;
	UINT8 s;
	//printf("---bitmap:\n");
    for(height = BitMapHeight - 1 ; height >= 0 ; height--)
    {
        canvas = (BitMapHeight - height - 1) * memWidth + ref;
        for(width=0;width<ActuralWidth;width++)
        {
        	p = canvas + (r + width)/8;
			if(p >= memlimit)
                continue;
            byte_in_bmpbuffer=(height*BitMapWidth+width)/8;
            bit=((Bitmap[byte_in_bmpbuffer]<<(width%8))&0x80)>>((r + width)%8);
			//if(_forColor > 0)
			//{
			s = 0x80>>((r + width)%8);
				if(bit)
				{
					//printf("0");
					*p = *p&(~s);
				}
				else
				{
					//printf("1");
					*p = *p|s;
				}
			//}
			//else
			//{
			//	if(bit)
			//		*p = *p&(~bit);
			//	else
			//		*p = *p|bit;
			//}
        }
		//printf("\n");
    }
}

void CBitCanvas::DrawRow
	(
		UINT16 x, 
		UINT16 y,
		UINT16 length,
		COLOR c
	)
{
	DrawRow(CPoint(x, y), x+length, c > 0);
}

void CBitCanvas::DrawRow
	(CPoint pp
	,UINT16 endX
	,bool bSet
	)
{
	UINT8* p = _pCanvasByte + pp.y * _bytePitch + pp.x / 8;
	int r = pp.x % 8;
	int m = ((endX - pp.x) + r) > 8 ? (8 - r) : (endX - pp.x);
	int n = ((endX - pp.x) + r) / 8 - ((r > 0) ? 1 : 0);
	if(n < 0)
		n = 0;
	int l = endX - (pp.x + m + (n - m/8)*8);
	//_bNoChanged = false;   
	unsigned char b;
	//printf("r:%d, m:%d, n:%d, l:%d\n", r, m, n, l);
	if(!((r == 0)&&(m == 8)))
	{
		if(bSet)
		{
			b = 0xff;
			*p |= ((b >> (8 - m))<<(8 - m - r));
		}
		else
		{
			b = 0xff;
			*p &= ~((b >> (8 - m))<<(8 - m - r));
		}
		p = p + 1;
	}

	if(n > 0)
	{
		if(bSet)
		{
			memset(p ,0xff, n);
		}
		else
		{
			memset(p ,0, n);
		}
		p = p + n;
	}

	if(l > 0)
	{
		if(bSet)
		{
			b = 0xff;
			*p |= ((b >> (8 - l))<<(8 - l));
		}
		else
		{
			b = 0xff;
			*p &= ~((b >> (8 - l))<<(8 - l));
		}
	}
}

};
//#endif

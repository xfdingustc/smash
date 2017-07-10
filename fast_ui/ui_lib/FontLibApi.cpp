/*******************************************************************************
**      FontLibApi.cpp
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
****************************************************************************** */

// add included here
#ifdef WIN32
#include "afx.h"
#include "windows.h"
#include "CFile.h"
#include "CBmp.h"
#include "Canvas.h"
#include "FontLibApi.h"
//#include "font.h"
#include "ui_fontapi/include/ft2build.h"

#pragma warning(disable: 4244)

#else
#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
#include "Canvas.h"

#include "FontLibApi.h"
#include <ft2build.h>

#endif
#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_CACHE_IMAGE_H
#include FT_CACHE_SMALL_BITMAPS_H
namespace ui{

/*******************************************************************************
* <Macros>
*******************************************************************************/

/*******************************************************************************
* <Local Functions>
*******************************************************************************/
static int utf8_mbtowc(UINT16 *p, const UINT8 *s, int n);
static int GetOneUniCodeFromUTF8(UINT8* pString, UINT16* pUnicode);
static bool IsWordBreak ( UINT8 Letter );
static bool GetOneGlyphBitmap(UINT16 charcode,FT_Bitmap* bitmap,int *left,int *top,int *xadvance,int *yadvance);
static bool GetOneGlyphSize(UINT16 charcode, int *width,int *height);

/*******************************************************************************
* <Local variable>
*******************************************************************************/
struct TYPE_CONTROL
{
    FT_Library      library;
    FT_Face         face; 
    FTC_Manager     cache_man;
    FTC_SBitCache   sbit_cache;
    FTC_SBit 		sbit;
};
static TYPE_CONTROL _tc;

CFontLibApi* CFontLibApi::_pInstance = NULL;
/*******************************************************************************
* <Function elements>
*******************************************************************************/
void CFontLibApi::Create(const char* fontFile)
{
#ifdef printBootTime
		struct timeval tv, tv2;
		struct timezone tz;
		gettimeofday(&tv, &tz);
#endif
    if(_pInstance == NULL)
        _pInstance = new CFontLibApi(fontFile);
#ifdef printBootTime
		gettimeofday(&tv2, &tz);
		printf("----CFontLibApi::Create %d ms\n", (tv2.tv_sec - tv.tv_sec)*1000 + (tv2.tv_usec - tv.tv_usec) / 1000);
#endif
}

void CFontLibApi::Destroy()
{
	if(_pInstance != NULL)
		delete _pInstance;
	_pInstance = NULL;
}

/*******************************************************************************
*     
*/
CFontLibApi::CFontLibApi
    (const char* fontFile): _forColor(100),
    _bkColor(20),
    _outlineColor(30),
    _glyphSize(20, 16),
    _bkMode(BKM_TRANSPARENT),
    _forMode(FORM_NORMAL),
    _fontDate(NULL)
{
    int error; //,size;    
    TYPE_CONTROL* pTypeControl = &_tc;
    
    error = FT_Init_FreeType(&pTypeControl->library);

    memfontlib = NULL;

	/*CFile f(fontFile, CFile::FILE_READ);
	if(f.IsValid())
	{
		size = f.GetLength();	
		memfontlib = new unsigned char [size];
		f.seek(0);
		f.read(size, memfontlib,size);
		//f.Close();
	}
	  
    error = FT_New_Memory_Face( pTypeControl->library, memfontlib, size, 0, &(pTypeControl->face ));
   */
  error = FT_New_Face( pTypeControl->library,
              	fontFile,
               	0,
                &(pTypeControl->face));
	
	if ( error!=0 )
    	return ;
    /*if (language==CHINESE)
    	error = FT_Select_Charmap(pTypeControl->face,ft_encoding_gb2312);
    else*/
    error = FT_Select_Charmap(pTypeControl->face, ft_encoding_unicode);    
    if ( error!=0 )
    	return ;

    //error = FT_Set_Char_Size(pTypeControl->face,0,16<<6,72,72 );
    //if ( error!=0 )
    //	return ;
    error=FT_Set_Pixel_Sizes(pTypeControl->face, _glyphSize.width, _glyphSize.height); //this->pixel_Size);
    if ( error!=0 )
    	return ;
}

CFontLibApi::~CFontLibApi()
{
	if (memfontlib != NULL)
		delete[] memfontlib;
}
/*******************************************************************************
*   CFontLibApi::MapGlyphBitMapToMem
*   
*/
void CFontLibApi::MapGlyphBitMapToMem
	(//COLOR* pMem, 
	//UINT16 memWidth, 
	//UINT16 memHeight,  
	CCanvas	*pCanvas,
	int X, 
	int Y,
	UINT8 *Bitmap,
	UINT16 BitMapWidth,
	UINT16 BitMapHeight,
	UINT16 ActuralWidth
	)
{    
    INT32  height, width;
    COLOR *canvas;
    UINT8  bit=0;
    UINT32 byte_in_bmpbuffer=0;
    COLOR* pMem = pCanvas->GetCanvas();
	UINT16 memWidth = pCanvas->GetSize().width;
	UINT16 memHeight = pCanvas->GetSize().height;
    register COLOR* memlimit = pMem + memWidth * memHeight;
    COLOR* ref = pMem + ( memWidth * Y + X );
    
    for(height=0;height<BitMapHeight;height++)
    {
        canvas = height * memWidth + ref;
        for(width=0;width<ActuralWidth;width++)
        {
            byte_in_bmpbuffer=(height*BitMapWidth+width)/8;
            bit=(Bitmap[byte_in_bmpbuffer]<<(width%8))&0x80;
            if((canvas >= memlimit)||(canvas < pMem))
                continue;
            {
                if (bit)
                    *canvas=_forColor;
                else if (_bkMode == BKM_SOLID)
                    *canvas=_bkColor;
            }
            canvas++;
        }
    }
}

void CFontLibApi::MapGlyphBitMapToMem2
	(
	CCanvas	*pCanvas,
	int X, 
	int Y,
	UINT8 *Bitmap,
	UINT16 BitMapWidth,
	UINT16 BitMapHeight,
	UINT16 ActuralWidth
	)
{    
	//printf("MapGlyphBitMapToMem2:\n");
    int  height, width;
    UINT8 *canvas;
    UINT8  bit=0;
    UINT32 byte_in_bmpbuffer=0;
    UINT8* pMem = (UINT8*)pCanvas->GetCanvas();
	UINT16 memWidth = pCanvas->GetSize().width / 8;
	UINT16 memHeight = pCanvas->GetSize().height;
    register UINT8* memlimit = pMem + memWidth * memHeight;
    UINT8* ref = pMem + ( memWidth * Y + X / 8 );
    int r = X % 8;
	UINT8* p;
	UINT8 s;
    for(height=0;height<BitMapHeight;height++)
    {
        canvas = height * memWidth + ref;
        for(width=0;width<ActuralWidth;width++)
        {
        	p = canvas + (r + width)/8;
			if((p >= memlimit)||(p < pMem))
			{
				//printf("--y: %d\n", Y + height);
                continue;
			}
            byte_in_bmpbuffer=(height*BitMapWidth+width)/8;
            bit=((Bitmap[byte_in_bmpbuffer]<<(width%8))&0x80)>>((r + width)%8);
			s = 0x80>>((r + width)%8);
			if(_forColor > 0)
			{
				if(bit)
					*p = *p|s;
				else
					*p = *p&(~s);
			}
			else
			{
				if(bit)
					*p = *p&(~s);
				else
					*p = *p|s;
			}
        }
    }
}

/*******************************************************************************
*   CFontLibApi::DrawWordToMem
*   
*/
UINT16 CFontLibApi::DrawWordToMem
	(//COLOR 	*pMemPtr, 
	//UINT16 	memWidth, 
	//UINT16 	memHeight,
	CCanvas	*pCanvas,
	char* 	pWord,
	UINT16 	length,
	int 	startx,
	int 	starty
	)
{
    if((pWord != NULL)&&(pCanvas !=NULL))
    {
        int i = 0;
        UINT16 unicode = 0;
        FT_Bitmap bitmap;
		memset(&bitmap, 0, sizeof(FT_Bitmap));
        int top = 0, left = 0, xadvance = 0, yadvance = 0;
        int x = startx, y = starty;        
        while(i < length)
        {
            i += GetOneUniCodeFromUTF8((UINT8*)(pWord + i), &unicode);
            GetOneGlyphBitmap(unicode, &bitmap, &left, &top,  &xadvance, &yadvance);
			if(pCanvas->isBitCanvas())
			{
				MapGlyphBitMapToMem2(pCanvas, x + left, y + _glyphSize.height - top, bitmap.buffer, bitmap.pitch*8, bitmap.rows, bitmap.width);
			}
			else
				MapGlyphBitMapToMem(pCanvas, x + left, y + _glyphSize.height - top, bitmap.buffer, bitmap.pitch*8, bitmap.rows, bitmap.width);
            x += xadvance;
        }
        return x - startx;
    }
    else
        return 0;    
}

/*******************************************************************************
*   CFontLibApi::GetWordPixelWidth
*   
*/
CSize CFontLibApi::GetWordPixelSize
    (
    char*pWord,
    UINT16 length
    )
{
    CSize rt(0,0);
    if (pWord != NULL)
    {
        int i = 0;
        int glyphWidth = 0, glyphheight = 0;
        UINT16 unicode;
        while(i < length)
        {
            i += GetOneUniCodeFromUTF8((UINT8*)(pWord + i), &unicode);
            GetOneGlyphSize(unicode, &glyphWidth, &glyphheight);
            rt.width += glyphWidth;
            if(rt.height < glyphheight)
                rt.height = glyphheight;
        }
    }
    return rt;
}

/*******************************************************************************
*   CFontLibApi::GetStrPixelSize
*   
*/
CSize CFontLibApi::GetStrPixelSize
    (
    char* pString,
    UINT16 widthLimit,
    UINT16 heightLimit,
    UINT16 rowHeight
    )
{
    CSize sz(0, 0);
    if(pString == NULL)
        return sz;
    
    if(FT_Set_Pixel_Sizes(_tc.face, _glyphSize.width, _glyphSize.height) != 0)
        return sz;
    
    int num_chars = (int)strlen(pString);
    int i = 0;
    UINT16 x = 0, y= 0;
    int wordStep;
    UINT8 wordTmp[50];
    bool bOut = false;
    CSize wordSize;
    UINT16 height = rowHeight;
    
    while(( i < num_chars)&&(!bOut))
    {
        memcpy(wordTmp, pString + i, 2);
        if((wordTmp[0] == (UINT8)'\\')&&(wordTmp[1] == (UINT8)'n'))
        {
            x = 0;
            y += rowHeight;
            i += 2;
        }
        else if(wordTmp[0] == 0x20) // ' '
        {
            x += _glyphSize.width / 2;
            i += 1;
        }
		else if((wordTmp[0] == 0x0d)&&(wordTmp[1] == 0x0a)) // ' '
        {
            x = 0;
            y += rowHeight;
            i += 2;
        }
        else
        {
            wordStep = GetWord((UINT8*)pString + i, num_chars - i,  widthLimit - x);
			if(wordStep > 0)
            {
                memcpy(wordTmp, pString + i, wordStep);
                wordSize = GetWordPixelSize((char*)wordTmp, wordStep);
                x += wordSize.width;
                if(height < wordSize.height)
                    height = wordSize.height;
                i += wordStep;
            }
            else 
            {
                x = 0;
                y += height;
                height = rowHeight;
            }
        }
        if(x > widthLimit)
        {
            x = 0;
            y += height;
            height = rowHeight;
        }
        if(x > sz.width)
            sz.width = x;
        if( y > heightLimit)
        {
            bOut = true;
        }
        else
            sz.height = y + height;
    }
    return sz;   
}

/*******************************************************************************
*   CFontLibApi::DrawStringToMem
*   
*/
CSize CFontLibApi::DrawStringToMem
    (//COLOR *pMemPtr, 
    //UINT16 memWidth, 
    //UINT16 memHeight, 
    CCanvas* pCanvas,
    char* pString, 
    int startX, 
    int startY, 
    int endX, 
    int endY,
    UINT16 rowHeight
    )
{
    CSize sz(0, 0);
    if((pCanvas == NULL)||(pString == NULL))
        return sz;
    
    if(FT_Set_Pixel_Sizes(_tc.face, _glyphSize.width, _glyphSize.height) != 0)
        return sz;
    
    int num_chars = (int)strlen(pString);
    int i = 0;
    int x = startX, y= startY;
    int wordStep;
    UINT8 wordTmp[50];
    bool bOut = false;
    
    while(( i < num_chars)&&(!bOut))
    {
        memcpy(wordTmp, pString + i, 2);
        if((wordTmp[0] == (UINT8)'\\')&&(wordTmp[1] == (UINT8)'n'))
        {
            x = startX;
            y += rowHeight;
            i += 2;
        }
        else if(wordTmp[0] == 0x20) // ' '
        {
            x += _glyphSize.width / 2;
            i += 1;
        }
		else if((wordTmp[0] == 0x0d)&&(wordTmp[1] == 0x0a)) // ' '
        {
            x = 0;
            y += rowHeight;
            i += 2;
        }
        else
        {
            wordStep = GetWord((UINT8*)pString + i, num_chars - i,  endX - x);
            if(wordStep > 0)
            {
            	memset(wordTmp,0, sizeof(wordTmp));
                memcpy(wordTmp, pString + i, wordStep);
                x += DrawWordToMem(pCanvas, (char*)wordTmp, wordStep, x, y);
                i += wordStep;
            }
            else 
            {
                x = startX;
                y += rowHeight;
            }
        }
        if(x > endX)
        {
            x = startX;
            y += rowHeight;
        }
        if(x > startX + sz.width)
            sz.width = x - startX;
        if( y > endY)
            bOut = true;
        else
            sz.height = y + rowHeight - startY;
    }
    return sz;   
}

/*******************************************************************************
*   CFontLibApi::DrawStringToMem
*
*   if return +, means a normal word found, else it ovler flow the pixle length limit   
*/
int CFontLibApi::GetWord(UINT8* pString, UINT16 charLengthLimit, UINT16 pixelLengthLimit)
{
    int i = 0;
    int codeLength = 0;
    UINT16 pixlength = 0;
    int glyphWi = 0, glyphHi = 0;
    bool bOut = false;
    UINT16 unicode;
    while((*(pString + i)!= ' ')&&(!IsWordBreak(*(pString + i)))&&(!bOut)&&(i < charLengthLimit))
    {
         codeLength = GetOneUniCodeFromUTF8((UINT8*)(pString + i), &unicode);
         GetOneGlyphSize(unicode, &glyphWi, &glyphHi);
         
         if(pixlength + glyphWi < pixelLengthLimit)
         {
            pixlength += glyphWi;
            i += codeLength;
         }
         else
            bOut = true;
    }
    
    // magic number explanans: glyph code loger than 2 treat as chinese, which can be splited anywhere.
    if((bOut)&&(codeLength <=2)) 
    {
        return -i;
    }
	else if(IsWordBreak(*(pString + i)))
		return i + 1;
    else
        return i;    
}

// test
void CFontLibApi::Test(UINT8* pMem)
{   
    char s[] = "jyy";
    int x, y;
    UINT16 unicode;
    GetOneUniCodeFromUTF8((UINT8*)s, &unicode);
    GetOneGlyphSize(unicode, &x, &y);
}

/*******************************************************************************
*   static bool GetOneGlyphBitmap
*   
*/
static bool GetOneGlyphBitmap
    (
    UINT16 charcode, 
    FT_Bitmap* bitmap, 
    int *left, 
    int *top, 
    int *xadvance, 
    int *yadvance
    )
{
    TYPE_CONTROL* pTypeControl = &_tc;
    FT_GlyphSlot  slot = pTypeControl->face->glyph;
    int error = FT_Load_Char( pTypeControl->face, charcode, FT_LOAD_MONOCHROME|FT_LOAD_RENDER);
    if (error)
        return false;
    bitmap->buffer=slot->bitmap.buffer;
    bitmap->width=slot->bitmap.width;
    bitmap->rows=slot->bitmap.rows;
    bitmap->pitch=slot->bitmap.pitch;
    bitmap->num_grays=slot->bitmap.num_grays;
    *left=slot->bitmap_left;
    *top=slot->bitmap_top;
    *xadvance=slot->advance.x>>6; // because return a 26.6 formate
    *yadvance=slot->advance.y>>6; // because return a 26.6 formate
    return true;
}

/*******************************************************************************
*   static bool GetOneGlyphSize
*   
*/
static bool GetOneGlyphSize(UINT16 charcode, int *width, int *height)
{
    TYPE_CONTROL* pTypeControl = &_tc;
    FT_GlyphSlot  slot = pTypeControl->face->glyph;
    int error = FT_Load_Char( pTypeControl->face, charcode, FT_LOAD_NO_BITMAP);
    if (error)
        return false;
    *width = slot->advance.x>>6; // because return a 26.6 formate
    //*height = slot->metrics.height>>6; // because return a 26.6 formate
    *height = (slot->metrics.height *2 - slot->metrics.horiBearingY )>>6;
        
    return true;
}

/*******************************************************************************
*   Function transfer UTF8--> unicode
*   
*/
struct utf8_table 
{
    int     cmask;
    int     cval;
    int     shift;
    long    lmask;
    long    lval;
};

static struct utf8_table utf8_table[] =
{
    {0x80,  0x00,   0*6,    0x7F,           0,         /* 1 byte sequence */},
    {0xE0,  0xC0,   1*6,    0x7FF,          0x80,      /* 2 byte sequence */},
    {0xF0,  0xE0,   2*6,    0xFFFF,         0x800,     /* 3 byte sequence */},
    {0xF8,  0xF0,   3*6,    0x1FFFFF,       0x10000,   /* 4 byte sequence */},
    {0xFC,  0xF8,   4*6,    0x3FFFFFF,      0x200000,  /* 5 byte sequence */},
    {0xFE,  0xFC,   5*6,    0x7FFFFFFF,     0x4000000, /* 6 byte sequence */},
    {0,						       /* end of table    */}
};

int utf8_mbtowc
    (
    UINT16 *p, 
    const UINT8 *s, 
    int n
    )
{
    long l;
    int c0, c, nc;
    struct utf8_table *t;

    nc = 0;
    c0 = *s;
    l = c0;
    for (t = utf8_table; t->cmask; t++) 
    {
        nc++;
        if ((c0 & t->cmask) == t->cval) 
        {
            l &= t->lmask;
            if (l < t->lval)
                return -1;
            *p = l;
                return nc;
        }
        if (n <= nc)
            return -1;
        s++;
        c = (*s ^ 0x80) & 0xFF;
        if (c & 0xC0)
            return -1;
        l = (l << 6) | c;
    }
    return -1;
}


int GetOneUniCodeFromUTF8(UINT8* pString, UINT16* pUnicode)
{
    if (*pString<=127)
    {
        UINT8 value = *pString;
        *pUnicode = value;
        return 1;        
    }
    else
    {
        int length = 0;
        UINT8 code[6];
        memcpy(code, pString, 6);
        if((length = utf8_mbtowc(pUnicode, code, 6)) != -1)
            return length;
        else
        {
            *pUnicode = 0;
            return 2;
        }
    }
}

bool IsWordBreak ( UINT8 Letter )
{
	if ( ( Letter == ',' ) || ( Letter == '.' ) || ( Letter == '?' ) || ( Letter == '!' ) || 
		( Letter == '-' ) || ( Letter == ':' ) || ( Letter == ';' ) || ( Letter == 0x0d )||( Letter == 0x0a )||(Letter=='\\'))
		return true;
	return false;
}

//#ifdef WIN32
}
//#endif

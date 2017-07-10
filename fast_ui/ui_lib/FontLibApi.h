/*******************************************************************************
**       FontLibApi.h
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

#ifndef  __INCFontLibApi_h
#define __INCFontLibApi_h
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
class CFontLibApi
{
public:
    enum BKMODE
    {
        BKM_TRANSPARENT = 0,
        BKM_SOLID,
    };

    enum FORMODE
    {
        FORM_NORMAL = 0,
        FORM_BLACK,
        FORM_ITALIC,
        FORM_BORDERONLY,
        FORM_WITHBORDER,
    };
    
public:
    static void Create(const char* fontFile);
	static void Destroy();
    static CFontLibApi* GetInstance(){return _pInstance;};
    
    CSize GetStrPixelSize( char* pString, UINT16 widthLimit, UINT16 heightLimit, UINT16 rowHeight);
    CSize DrawStringToMem
		(
        CCanvas* pCanvas,
        char* pString,
        int startX, 
        int startY, 
        int endX, 
        int endY, 
        UINT16 rowHeight);

    void SetColor(COLOR forC, COLOR otlnC, COLOR bkC)
    {
        _forColor = forC;
        _bkColor = bkC;
        _outlineColor = otlnC;
    }
    void SetMode(BKMODE bkM, FORMODE forM)
    {
        _bkMode = bkM;
        _forMode = forM;
    }
    void SetSize(CSize sz){_glyphSize = sz;}
    void Test(UINT8* pMem);
    
protected:
    CFontLibApi(const char* fontFile);
    ~CFontLibApi();
    
private:
    void MapGlyphBitMapToMem
		(
		CCanvas* pCanvas,
		int x, 
		int y, 
		UINT8 *pBitmap, 
		UINT16 bitMapWidth,
		UINT16 bitMapHeight, 
		UINT16 acturalWidth );
	void MapGlyphBitMapToMem2
		(
		CCanvas* pCanvas,
		int x, 
		int y, 
		UINT8 *pBitmap, 
		UINT16 bitMapWidth,
		UINT16 bitMapHeight, 
		UINT16 acturalWidth );
    UINT16 DrawWordToMem
		(
        CCanvas* pCanvas,
        char* pWord, 
        UINT16 length, 
        int startx, 
        int starty);
    CSize GetWordPixelSize(char*pWord, UINT16 length);
    int GetWord(UINT8* pString, UINT16 charLengthLimit, UINT16 pixelLengthLimit);


private:
    static CFontLibApi* _pInstance;
    COLOR   _forColor;
    COLOR   _bkColor;
    COLOR   _outlineColor;
    CSize   _glyphSize;
    BKMODE  _bkMode;
    FORMODE _forMode;
    UINT8*  _fontDate;
    UINT16  pixel_Size;
	unsigned char *memfontlib;
};

//#ifdef WIN32
};
//#endif

#endif

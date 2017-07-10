#ifndef __H_CBmp__
#define __H_CBmp__

#include "ClinuxThread.h"

//#ifdef WIN32
namespace ui{
//#endif

#define offset_BFTYPE       0x0
#define offset_BFSIZE       0x02
#define offset_BFOFFBITS    0x0a

#define offset_BISIZE       0x0e
#define offset_BIWIDTH      0x12
#define offset_BIHEIGHT     0x16
#define offset_BIPLANES     0x1a
#define offset_BITCOUNT     0x1c
#define offset_BICOMPRESS   0x1e
#define offset_BISIZEIMAGE  0x22
#define offset_BIXPELSPERMETER  0x26
#define offset_BIYPERSPERMETER  0x2a
#define offset_BICOLORUSED      0x2E
#define offset_BICOLORIMPORTANT 0x32


struct BMP_FILE_HEADER
{
    unsigned short  bfType;
    DWORD           bfSize;
    unsigned short  bfReserved1;
    unsigned short  bfReserved2;
    DWORD           bfOffBits;
};

struct BMP_IMAGE_HEADER
{
    DWORD       biSize;
    LONG        biWidth;
    LONG        biHeight;
    WORD        biPlanes;
    WORD        biBitCount;
    DWORD       biCompression;
    DWORD       biSizeImage;
    LONG        biXPelsPerMeter;
    LONG        biYPelsPerMeter;
    DWORD       biClrUsed;
    DWORD       biClrImportant;
};

class CBmp
{
public:
    CBmp();
    CBmp(const char* dir, const char* file);
    CBmp(int wi, int hi, int colorD, int pitch, BYTE* buf);
    ~CBmp();

    bool fillBuffer();
    bool ReadFile(char*);
    bool WriteFile(char*);
    bool IsValid() { return _valid; }
    int getWidth() { return _imageHeader.biWidth; }
    int getHeight() {
        return (_imageHeader.biHeight > 0)? _imageHeader.biHeight : -_imageHeader.biHeight;
    }
    BYTE* GetBuffer() { return _pImageBuffer; }
    int getColorDepth() { return _imageHeader.biBitCount; }
    int GetPitch() { return _pitch; }

    //bool Binalize(unsigned middleGray, unsigned int* buffer, int size);

private:

    BMP_IMAGE_HEADER    _imageHeader;
    BMP_FILE_HEADER     _fileHeader;
    int                 _pitch;
    CFile               *_file;
    BYTE                *_pImageBuffer;
    bool                _valid;
    bool                _height;
    unsigned int        *_pBinalizedBUffer;
};

class CYuv
{
public:
    enum yuv_type
    {
        YUV_444 = 0,
        YUV_422,
        YUV_420
    };

    CYuv();
    void Intial(int width, int height, yuv_type type);
    void ConvertFromARGB(BYTE* aRGBbuffer, int pitch);
    bool ReadFile(char* path);
    bool WriteFile(char* path);

private:
    int         _width;
    int         _height;
    int         _YbitOffset;
    int         _YalphaOffset;
    int         _uvBitoffet;
    int         _uvAlphaOffset;
    int         _byteSize;
    yuv_type    _type;
    CFile       *_file;
    BYTE        *_imageBuffer;
};


//#ifdef WIN32
}
//#endif

#endif


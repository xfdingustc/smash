
#ifdef WIN32
#include<string.h>
#include<windows.h>
#include"CFile.h"
#include "CBmp.h"
#include "debug.h"
#else
#include "linux.h"
#include "ClinuxThread.h"
#include "CEvent.h"
#include "CBmp.h"
//#include "debug.h"

#include "GobalMacro.h"

namespace ui{

#endif

CBmp::CBmp(int wi, int hi, int colorD, int pitch, BYTE* buf)
    : _pitch(0)
    , _file(NULL)
    , _pImageBuffer(NULL)
{
    memset(&_imageHeader, 0x00, sizeof(_imageHeader));
    memset(&_fileHeader, 0x00, sizeof(_fileHeader));

    _imageHeader.biWidth = wi;
    _imageHeader.biHeight = hi;
    _imageHeader.biBitCount = colorD;
    _pitch = pitch;
    _pImageBuffer = buf;
}

CBmp::CBmp(const char* dir, const char* file)
    : _pitch(0)
    , _file(NULL)
    , _pImageBuffer(NULL)
{
    memset(&_imageHeader, 0x00, sizeof(_imageHeader));
    memset(&_fileHeader, 0x00, sizeof(_fileHeader));

    char tmp[512];
    memset(tmp, 0x00, sizeof(tmp));
    snprintf(tmp, 512, "%s%s", dir, file);
    this->ReadFile(tmp);
    this->fillBuffer();

    if (_file != NULL) {
        delete _file;
        _file = NULL;
    }
}

CBmp::~CBmp()
{
    if (_file != NULL) {
        delete _file;
    }

    if (_pImageBuffer != NULL) {
        delete[] _pImageBuffer;
        _pImageBuffer = NULL;
    }
}

void CBmp::_printFileHeader(char *file)
{
    CAMERALOG("BMP %s, file header:", file);
    CAMERALOG("  bfType = 0x%x", _fileHeader.bfType);
    CAMERALOG("  bfSize = %d", _fileHeader.bfSize);
    CAMERALOG("  bfOffBits = %d", _fileHeader.bfOffBits);
}

void CBmp::_printImageHeader(char *file)
{
    CAMERALOG("BMP %s, image header:", file);
    CAMERALOG("  biSize = %d", _imageHeader.biSize);
    CAMERALOG("  biWidth = %ld", _imageHeader.biWidth);
    CAMERALOG("  biHeight = %ld", _imageHeader.biHeight);
    CAMERALOG("  biPlanes = %d", _imageHeader.biPlanes);
    CAMERALOG("  biBitCount = %d", _imageHeader.biBitCount);
    CAMERALOG("  biCompression = %d", _imageHeader.biCompression);
    CAMERALOG("  biSizeImage = %d", _imageHeader.biSizeImage);
    CAMERALOG("  biXPelsPerMeter = %ld", _imageHeader.biXPelsPerMeter);
    CAMERALOG("  biYPelsPerMeter = %ld", _imageHeader.biYPelsPerMeter);
    CAMERALOG("  biClrUsed = %d", _imageHeader.biClrUsed);
    CAMERALOG("  biClrImportant = %d", _imageHeader.biClrImportant);
}

bool CBmp::ReadFile(char* path)
{
    _file = new CFile(path, CFile::FILE_READ);

    if (_file->IsValid()) {
        //CAMERALOG("--get bmp file content");
        _file->seek(offset_BFTYPE);
        _file->read(2, (BYTE*)&_fileHeader.bfType, 2);
        _file->seek(offset_BFSIZE);
        _file->read(4, (BYTE*)&_fileHeader.bfSize, 4);
        _file->seek(offset_BFOFFBITS);
        _file->read(4, (BYTE*)&_fileHeader.bfOffBits, 4);

        _file->seek(offset_BISIZE);
        _file->read(4, (BYTE*)&_imageHeader.biSize, 4);

        _file->seek(offset_BIWIDTH);
        _file->read(4, (BYTE*)&_imageHeader.biWidth, 4);
        _file->seek(offset_BIHEIGHT);
        _file->read(4, (BYTE*)&_imageHeader.biHeight, 4);
        _file->seek(offset_BIPLANES);
        _file->read(4, (BYTE*)&_imageHeader.biPlanes, 4);
        _file->seek(offset_BITCOUNT);
        _file->read(2, (BYTE*)&_imageHeader.biBitCount, 2);
        _file->seek(offset_BICOMPRESS);
        _file->read(2, (BYTE*)&_imageHeader.biCompression, 2);
        _file->seek(offset_BISIZEIMAGE);
        _file->read(4, (BYTE*)&_imageHeader.biSizeImage, 4);
        _file->seek(offset_BIXPELSPERMETER);
        _file->read(4, (BYTE*)&_imageHeader.biXPelsPerMeter, 4);
        _file->seek(offset_BIYPERSPERMETER);
        _file->read(4, (BYTE*)&_imageHeader.biYPelsPerMeter, 4);
        _file->seek(offset_BICOLORUSED);
        _file->read(4, (BYTE*)&_imageHeader.biClrUsed, 4);
        _file->seek(offset_BICOLORIMPORTANT);
        _file->read(4, (BYTE*)&_imageHeader.biClrImportant, 4);
        _pitch = (((_imageHeader.biBitCount*_imageHeader.biWidth)+31)>>5)<<2;

        //_printFileHeader(path);
        //_printImageHeader(path);
        return true;
    } else {
        CAMERALOG("bmp file open error %s", path);
    }
    return false;
}

bool CBmp::WriteFile(char* path)
{
    return true;
}
bool CBmp::fillBuffer()
{
    if (!_file->IsValid()){
        return false;
    }

    unsigned int imagesize = 0;

    if (_imageHeader.biHeight < 0) {
        imagesize = -_imageHeader.biHeight * _pitch;
    } else {
        imagesize = _imageHeader.biHeight * _pitch;
    }
    //CAMERALOG("imagesize = %d", imagesize);
    if ((imagesize == 0) || (imagesize >= _imageHeader.biSizeImage)) {
        imagesize = _imageHeader.biSizeImage;
    }
    //CAMERALOG("imagesize = %d", imagesize);

    _pImageBuffer = new BYTE [imagesize];
    _file->seek(_fileHeader.bfOffBits);
    _file->read(imagesize, _pImageBuffer, imagesize);
    return true;
}

CYuv::CYuv()
{
    _imageBuffer = NULL;
}

void CYuv::Intial(int width, int height, yuv_type type)
{
    _width = width;
    _height = height;
    _type = type;
}

void CYuv::ConvertFromARGB(BYTE* aRGBbuffer, int pitch)
{
    switch (_type) {
        case YUV_444:
            _byteSize = _width*_height*4;
            _imageBuffer = new BYTE [_byteSize];
            _YbitOffset = 0;
            _YalphaOffset = _width*_height;
            _uvBitoffet = _width*_height*2;
            _uvAlphaOffset = -1;

            float Y,U,V,A,R,G,B;
            int rowOff;
            for (int i = 0; i < _height; i++) {
                rowOff = i*pitch;
                for (int j = 0; j < _width; j++) {
                    R = aRGBbuffer[rowOff + j*4 + 2];
                    G = aRGBbuffer[rowOff + j*4 + 1];
                    B = aRGBbuffer[rowOff + j*4 + 0];
                    A = aRGBbuffer[rowOff + j*4 + 3];

                    Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16;
                    V =  (0.439 * R) - (0.368 * G) - (0.071 * B) + 128;
                    U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128;

                    _imageBuffer[_YbitOffset + _width*i + j]= (BYTE)Y;
                    _imageBuffer[_YalphaOffset + _width*i + j] = (BYTE)A;
                    _imageBuffer[_uvBitoffet + _width*i*2 + j*2] = (BYTE)U;
                    _imageBuffer[_uvBitoffet + _width*i*2 + j*2 + 1] =(BYTE)V;
                }
            }
            break;
        case YUV_422: //TBD
        /*_byteSize = _width*_height*4;
        _imageBuffer = new BYTE [_byteSize];
        _YbitOffset = 0;
        _YalphaOffset = _width*_height;
        _uvBitoffet = _width*_height*2;
        _uvAlphaOffset = -1;

        float Y,U,V,A,R,G,B;
        int rowOff;
        for(int i = 0; i < _height; i++)
        {
        rowOff = i*pitch;
        for(int j = 0; j < _width; j++)
        {
        R = aRGBbuffer[rowOff + j*4 + 2];
        G = aRGBbuffer[rowOff + j*4 + 1];
        B = aRGBbuffer[rowOff + j*4 + 0];
        A = aRGBbuffer[rowOff + j*4 + 3];

        Y = (0.257 * R) + (0.504 * G) + (0.098 * B) + 16;
        V =  (0.439 * R) - (0.368 * G) - (0.071 * B) + 128;
        U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128;



        _imageBuffer[_YbitOffset + _width*i + j]= (BYTE)Y;
        _imageBuffer[_YalphaOffset + _width*i + j] = (BYTE)A;
        _imageBuffer[_uvBitoffet + _width*i*2 + j*2] = (BYTE)U;
        _imageBuffer[_uvBitoffet + _width*i*2 + j*2 + 1] =(BYTE)V;
        }
        }*/
        break;
        case YUV_420:
            break;
    }
}

bool CYuv::ReadFile(char* path)
{
    return true;
}

bool CYuv::WriteFile(char* path)
{
    _file = new CFile(path, CFile::FILE_WRITE);
    _file->write((BYTE*)&_type,4);
    _file->write((BYTE*)&_width,4);
    _file->write((BYTE*)&_height,4);

    _YbitOffset += 28;
    _YalphaOffset += 28;
    _uvBitoffet += 28;
    _uvAlphaOffset += 28;

    _file->write((BYTE*)&_YbitOffset,4);
    _file->write((BYTE*)&_YalphaOffset,4);
    _file->write((BYTE*)&_uvBitoffet,4);
    _file->write((BYTE*)&_uvAlphaOffset,4);

    _file->write((BYTE*)_imageBuffer, _byteSize);

    delete _file;
    _file = NULL;
    delete[] _imageBuffer;
    _imageBuffer = NULL;

    return true;
}

//#ifdef WIN32
}
//#endif

/*****************************************************************************
 * qrCodeScan.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - , linnsong.
 *
 *
 *****************************************************************************/
#ifndef __H_class_qscan_client__
#define __H_class_qscan_client__

#include "GobalMacro.h"
#include "ClinuxThread.h"

#include <zxing/LuminanceSource.h>
#include <stdio.h>
#include "jpeglib.h"
#include <setjmp.h>

class StringEnvelope;

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

class JpegReader
{
public:
	JpegReader(int bufferSize);
	~JpegReader();	
	int read_JPEG_file (char * filename);
	unsigned char* _image_buffer;
	int _bufferSize;
	int _rowpitch;
	int _imageWidth;
	int _imageHeight;
private:
	
};

struct Pixel
{
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};


namespace zxing {

class JpegBitmapSource : public LuminanceSource
{
private:
  //Magick::Image image_;
  int width;
  int height;
  JpegReader* _pReader;

public:
  JpegBitmapSource(JpegReader*);
  ~JpegBitmapSource();

  int ReadFile(char* path);

  int getWidth()const;
  int getHeight()const;
  unsigned char* getRow(int y, unsigned char* row);
  unsigned char* getMatrix();
  bool isCropSupported() const;
  Ref<LuminanceSource> crop(int left, int top, int width, int height);
  bool isRotateSupported() const;
  Ref<LuminanceSource> rotateCounterClockwise();
};

}

class Code_Scan_Delegate
{
public:
	Code_Scan_Delegate(){_pStrEV = NULL;};
	~Code_Scan_Delegate()
	{
		if(_pStrEV)
			delete _pStrEV;
	};
	virtual void onScanFinish(bool result) = 0;
	bool setScanResult(char* result);
	char* getResult(){return _result;};
	StringEnvelope* getEnvelope(){return _pStrEV;};
	
private:
	char _result[4096];
	StringEnvelope*	_pStrEV;
	bool _bGot;
};

class Code_Scan_thread : public CThread
{
public:
	Code_Scan_thread(Code_Scan_Delegate *delegate);
	~Code_Scan_thread();

protected:
	virtual void main(void *);
	
private:
	Code_Scan_Delegate*		_delegate;
	zxing::JpegBitmapSource* _pJpegSource;
};

#endif

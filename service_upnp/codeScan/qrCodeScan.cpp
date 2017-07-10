/*****************************************************************************
 * qrCodeScan.h:
 *****************************************************************************
 * Author: linnsong <linnsong@hotmail.com>
 *
 * Copyright (C) 1979 - , linnsong.
 *
 *
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include "qrCodeScan.h"
#include "class_ipc_rec.h"

#include <zxing/qrcode/QRCodeReader.h>
#include <zxing/common/GlobalHistogramBinarizer.h>
//#include <zxing/common/LocalBlockBinarizer.h>
#include <zxing/Exception.h>
#include "../xml_cmd/sd_general_description.h"


using namespace std;
using namespace zxing;
using namespace zxing::qrcode;

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
  my_error_ptr myerr = (my_error_ptr) cinfo->err;
  (*cinfo->err->output_message) (cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

JpegReader::JpegReader
	(int buffersize
	):_bufferSize(buffersize)
{
	_image_buffer = new unsigned char[_bufferSize];
}

JpegReader::~JpegReader()
{
	printf("---~~JpegReader 1\n");
	delete[]_image_buffer;
}

int JpegReader::read_JPEG_file
	(char * filename
	)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	FILE * infile;
	JSAMPARRAY buffer;
	int row_stride;
	if ((infile = fopen(filename, "rb")) == NULL)
	{
		printf("can't open %s\n", filename);
		return 0;
	}

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return 0;
	}
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	(void) jpeg_read_header(&cinfo, TRUE);
	(void) jpeg_start_decompress(&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;
	_rowpitch = row_stride;
	_imageWidth = cinfo.output_width;
	_imageHeight = cinfo.output_height;
	printf("----bytes per row : %d, width: %d, heigh: %d\n", _rowpitch, _imageWidth, _imageHeight);
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
	int rlines = 0;
	int wline = 0;
	int remainSize = _bufferSize;
	memset((unsigned char*)(*buffer), 0, row_stride);
	while (cinfo.output_scanline < cinfo.output_height)
	{
		rlines = jpeg_read_scanlines(&cinfo, buffer, 1); // buffer ? 
		if(remainSize >= rlines*_rowpitch)
		{
			memcpy(_image_buffer + wline*_rowpitch, buffer[0], rlines*_rowpitch);
			wline += rlines;
			remainSize -= rlines*_rowpitch;
		}else
		{
			printf("----not enough mem, w lines:%d\n", wline);
			break; 
		}
	}
	(void) jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	fclose(infile);

	if(wline == _imageHeight)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


namespace zxing {

JpegBitmapSource::JpegBitmapSource(JpegReader* reader)
{
	_pReader = reader;
}

JpegBitmapSource::~JpegBitmapSource()
{
	printf("---~~JpegBitmapSource 1\n");
	//delete _pReader;
}

int JpegBitmapSource::ReadFile
	(char* path)
{
	int rt = _pReader->read_JPEG_file(path);
	return rt;
}

int JpegBitmapSource::getWidth()const
{
  return _pReader->_imageWidth;
}

int JpegBitmapSource::getHeight()const{
  return _pReader->_imageHeight;
}

unsigned char* JpegBitmapSource::getRow
	(int y
	,unsigned char* row)
{
	Pixel* pixelBuff = (Pixel*)(_pReader->_image_buffer + (_pReader->_rowpitch * y));
	int width = getWidth();
	if (row == NULL)
	{
		row = new unsigned char[width];
	}
	for (int x = 0; x < width; x++)
	{
		const Pixel* p = pixelBuff + x;
		row[x] = (unsigned char)(((306 * p->red) + (601 * p->green) + (117 * p->blue)) >> 10);
	}
	return row;
}

/** This is a more efficient implementation. */
unsigned char* JpegBitmapSource::getMatrix()
{
	//printf("--getMatrix %d\n");
	Pixel* pixelBuff = (Pixel*)_pReader->_image_buffer;
	int width = getWidth();
	int height =  getHeight();
	unsigned char* matrix = new unsigned char[width*height];
	unsigned char* m = matrix;
	const Pixel* p = pixelBuff;
	for (int y = 0; y < height; y++) 
	{
		for (int x = 0; x < width; x++)
		{
			*m = (unsigned char)(((306 * p->red) + (601 * p->green) +
	    		(117 * p->blue)) >> 10);
			m++;
			p++;
		}
	}
	return matrix;
}

bool JpegBitmapSource::isRotateSupported() const {
  return false; //true
}

Ref<LuminanceSource> JpegBitmapSource::rotateCounterClockwise()
{
  //Magick::Image rotated(image_);
  //rotated.modifyImage();
  //rotated.rotate(90); // Image::rotate takes CCW degrees as an argument
  //rotated.syncPixels();
  //return Ref<JpegBitmapSource> (new JpegBitmapSource(rotated));
  return Ref<JpegBitmapSource>(this);
}

bool JpegBitmapSource::isCropSupported() const{
  return false; //true;
}

Ref<LuminanceSource> JpegBitmapSource::crop
	(int left
	, int top
	, int width
	, int height)
{
  /* TODO Investigate memory leak:
   * This method "possibly leaks" 160 bytes in 1 block */
  //Image copy(image_);
  //copy.modifyImage();
  //copy.crop( Geometry(width,height,left,top));
  //copy.syncPixels();
  //return Ref<JpegBitmapSource>(new JpegBitmapSource(copy));
  return Ref<JpegBitmapSource>(this);
}
}
/*
void decode_image(bool localized) {
  try {
    Ref<JpegBitmapSource> source(new JpegBitmapSource(image));
    
    Ref<Binarizer> binarizer(NULL);
    if (localized) {
      binarizer = new LocalBlockBinarizer(source);
    } else {
      binarizer = new GlobalHistogramBinarizer(source);
    }
    
    Ref<BinaryBitmap> image(new BinaryBitmap(binarizer));
    QRCodeReader reader;
    Ref<Result> result(reader.decode(image));
    
    cout << result->getText()->getText() << endl;
  } catch (zxing::Exception& e) {
    cerr << "Error: " << e.what() << endl;
  }
}*/

#define jpegImageReadSize 3*1024*1024 
static char* captureJpegPath = "/tmp/scan.jpg";
Code_Scan_thread::Code_Scan_thread
	(Code_Scan_Delegate *delegate
	):CThread("scan thread", CThread::NORMAL, 0, NULL)
	,_delegate(delegate)
{
}

Code_Scan_thread::~Code_Scan_thread()
{

}

void Code_Scan_thread::main(void *)
{
	JpegReader reader(jpegImageReadSize);
	while(1)
	{
		IPC_AVF_Client::getObject()->SyncCaptureNextJpeg(captureJpegPath);
		_pJpegSource = new zxing::JpegBitmapSource(&reader);
		if(_pJpegSource->ReadFile(captureJpegPath) <= 0)
		{
			printf("read file failed\n");
			continue;
		}
		zxing::Ref<JpegBitmapSource> source(_pJpegSource);
		zxing::Ref<Binarizer> binarizer(NULL);
		zxing::Ref<BinaryBitmap> image(NULL);
		try {
		    //if (localized) {
		      //binarizer = new LocalBlockBinarizer(source);
		    //} else {
		    binarizer = new GlobalHistogramBinarizer(source);
		    //}
		    DecodeHints hints(DecodeHints::DEFAULT_HINT);
    		hints.setTryHarder(false);
		    image = new BinaryBitmap(binarizer);
		    QRCodeReader reader;
		    zxing::Ref<Result> result(reader.decode(image, hints));
		    cout << result->getText()->getText() << endl;
			if(_delegate)
			{
				bool r = _delegate->setScanResult((char*)result->getText()->getText().c_str());
				_delegate->onScanFinish(r);
			}
			break;
	  	} catch (zxing::Exception& e) {
	    	cerr << "Error: " << e.what() << endl;
	  	}
	}
}


bool Code_Scan_Delegate::setScanResult(char* result)
{
	//
	strcpy(_result, result);
	if(_pStrEV)
		delete _pStrEV;
	_pStrEV = new StringEnvelope(_result, strlen(_result));
	_bGot = _pStrEV->isNotEmpty();
	return _bGot;
};
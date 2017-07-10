
#define LOG_TAG "iframedec_jni"

#include "avf_common.h"
#include "avf_if.h"
#include "avf_osal.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "android/bitmap.h"
#include "jni.h"

static struct {
	jmethodID createBitmap; // IFrameDecoder.createBitmap
} g_info;

#undef kClassPathName
#define kClassPathName "com/transee/vdb/IFrameDecoder"

static void jni_iframedec_native_init(JNIEnv *env, jobject thiz)
{
}

static jobject iframedec_decode(JNIEnv *env, jobject thiz, const char *pInputFile, int pos_ms)
{
	AVFormatContext *pFormatCtx = NULL;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = NULL;
	AVDictionary *optionsDict = NULL;
	int index = -1;
	AVFrame *pFrame = NULL;
	AVFrame *pFrame_RGBA = NULL;
	jobject bitmap = NULL;
	int success = 0;
	void *buffer = NULL;
	SwsContext *sws_ctx = NULL;
	AVPacket packet;
	int result;

	// open video file
	// avformat_close_input
	if ((result = avformat_open_input(&pFormatCtx, pInputFile, NULL, NULL)) < 0) {
		AVF_LOGE("open %s failed, error=%d", pInputFile, result);
		goto Fail;
	}

	// retrieve stream info
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		AVF_LOGE("cannot find stream info");
		goto Fail;
	}

	// find the first video stream
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		AVF_LOGE("no video stream");
		goto Fail;
	}

	// get codec context
	pCodecCtx = pFormatCtx->streams[index]->codec;

	// find decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		AVF_LOGE("cannot find decoder");
		goto Fail;
	}

	// open codec
	// avcodec_close
	if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0) {
		AVF_LOGE("cannot open decoder");
		goto Fail;
	}

	// av_free
	pFrame = av_frame_alloc();
	pFrame_RGBA = av_frame_alloc();
	if (pFrame == NULL || pFrame_RGBA == NULL) {
		AVF_LOGE("cannot alloc frames");
		goto Fail;
	}

	bitmap = env->CallObjectMethod(thiz, g_info.createBitmap, pCodecCtx->width, pCodecCtx->height);
	if (bitmap == NULL) {
		AVF_LOGE("cannot create bitmap");
		goto Fail;
	}

	// AndroidBitmap_unlockPixels
	if (AndroidBitmap_lockPixels(env, bitmap, &buffer) == 0) {
		// sws_freeContext
		sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
			pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
			AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

		avpicture_fill((AVPicture*)pFrame_RGBA, (uint8_t*)buffer, AV_PIX_FMT_RGBA,
			pCodecCtx->width, pCodecCtx->height);

		while (av_read_frame(pFormatCtx, &packet) >= 0) {
			if (packet.stream_index == index) {
				int frame_finished = 0;
				AVF_LOGI("decode video");
				avcodec_decode_video2(pCodecCtx, pFrame, &frame_finished, &packet);
				if (!frame_finished) {
					AVF_LOGI("flush");
					packet.data = NULL;
					packet.size = 0;
					avcodec_decode_video2(pCodecCtx, pFrame, &frame_finished, &packet);
				}
				if (frame_finished) {
					AVF_LOGI("frame finished");
					sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
						pFrame->linesize, 0, pCodecCtx->height,
						pFrame_RGBA->data, pFrame_RGBA->linesize);
					success = 1;
					break;
				}
			}
		}

		AndroidBitmap_unlockPixels(env, bitmap);

		if (!success) {
			AVF_LOGE("failed to decode one Iframe");
			bitmap = NULL;
		}
	}

Fail:
	if (sws_ctx != NULL) {
		sws_freeContext(sws_ctx);
	}
	if (pFrame_RGBA != NULL) {
		av_free(pFrame_RGBA);
	}
	if (pFrame != NULL) {
		av_free(pFrame);
	}
	if (pCodecCtx != NULL) {
		avcodec_close(pCodecCtx);
	}
	if (pFormatCtx != NULL) {
		avformat_close_input(&pFormatCtx);
	}
	return bitmap;
}

static jobject jni_iframedec_native_decode(JNIEnv *env, jobject thiz,
	jstring filename, jint pos_ms)
{
	const char *pInputFile = NULL;

	pInputFile = env->GetStringUTFChars(filename, NULL);

	AVF_LOGI("decode %s", pInputFile);

	jobject result = iframedec_decode(env, thiz, pInputFile, pos_ms);

	env->ReleaseStringUTFChars(filename, pInputFile);

	return result;
}

static const JNINativeMethod gMethods[] = {
	{"native_init",		"()V",		(void*)jni_iframedec_native_init},
	{"native_decode",	"(Ljava/lang/String;I)Landroid/graphics/Bitmap;",
		(void*)jni_iframedec_native_decode},
};

int init_iframedec_class(JNIEnv *env)
{
	jclass clazz;
	int nMethods;

	// find Java class
	clazz = env->FindClass(kClassPathName);
	if (clazz == NULL) {
		AVF_LOGE("Error: cannot find class " kClassPathName);
		return -1;
	}

	// install native methods
	nMethods = sizeof(gMethods) / sizeof(gMethods[0]);
	if (env->RegisterNatives(clazz, gMethods, nMethods) < 0) {
		AVF_LOGE("Error: RegisterNatives failed");
		return -1;
	}

	g_info.createBitmap = env->GetMethodID(clazz, "createBitmap", "(II)Landroid/graphics/Bitmap;");
	if (g_info.createBitmap == NULL) {
		AVF_LOGE("cannot find createBitmap()");
		return -1;
	}

	return 0;
}



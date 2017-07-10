

// how to build
// 1. install android ndk under vmware/ubuntu
// 2. cd .../jni
// 3. ndk-build
// 4. got android\libs\armeabi\libremux.so
// 5. copy it to \libs\armeabi

#define LOG_TAG "remuxer_jni"

#include "avf_common.h"
#include "avf_if.h"
#include "avf_osal.h"
#include "avf_remuxer_api.h"

#include "jni.h"

static struct {
	JavaVM *jvm; // Java Virtual Machine
	jfieldID contextID; // HttpRemuxer.mNativeContext
	jmethodID notifyID; // HttpRemuxer.notify()
} g_info;

#define MY_JNI_VER	JNI_VERSION_1_4

#undef kClassPathName
#define kClassPathName	"com/transee/vdb/HttpRemuxer"

static void remuxer_callback(void *context, int event, int arg1, int arg2)
{
	jobject obj = (jobject)context;
	JNIEnv *env = NULL;

	// attach the avf thread so it can call Java
	if (event == AVF_REMUXER_ATTACH_THREAD) {
		AVF_LOGI("attach thread");
		if (g_info.jvm->AttachCurrentThread(&env, NULL) != 0) {
			AVF_LOGE("AttachCurrentThread failed");
		}
		return;
	}

	// get env
	if (g_info.jvm->GetEnv((void**)&env, MY_JNI_VER) != JNI_OK) {
		AVF_LOGE("GetEnv() failed");
		return;
	}

	// on receiving this event, the avf thread is no longer running
	// so detach it
	if (event == AVF_REMUXER_DETACH_THREAD) {
		AVF_LOGI("detach thread");
		env->DeleteGlobalRef(obj);
		g_info.jvm->DetachCurrentThread();
		return;
	}

	// callback Java
	env->CallVoidMethod(obj, g_info.notifyID, event, arg1, arg2);
}

INLINE static avf_remuxer_t *get_remuxer(JNIEnv *env, jobject thiz)
{
	return (avf_remuxer_t*)env->GetIntField(thiz, g_info.contextID);
}

INLINE static void set_remuxer(JNIEnv *env, jobject thiz, avf_remuxer_t *remuxer)
{
	env->SetIntField(thiz, g_info.contextID, (int)remuxer);
}

static void jni_remuxer_native_init(JNIEnv *env, jobject thiz)
{
	AVF_LOGI("native init");

	jobject obj = env->NewGlobalRef(thiz);

	avf_remuxer_t *remuxer;
	if ((remuxer = avf_remuxer_create(remuxer_callback, (void*)obj)) == NULL) {
		env->DeleteGlobalRef(obj);
		return;
	}

	set_remuxer(env, thiz, remuxer);
}

static void native_release(JNIEnv *env, jobject thiz)
{
	avf_remuxer_t *remuxer = get_remuxer(env, thiz);
	if (remuxer != NULL) {
		// the avf thread will be terminated
		AVF_LOGI("destroy remuxer");
		avf_remuxer_destroy(remuxer);
		set_remuxer(env, thiz, NULL);
	}
}

static void jni_remuxer_native_finalize(JNIEnv *env, jobject thiz)
{
	AVF_LOGI("native finalize");
	native_release(env, thiz);
}

static void jni_remuxer_native_release(JNIEnv *env, jobject thiz)
{
	AVF_LOGI("native release");
	native_release(env, thiz);
}

static int jni_remuxer_set_iframe_only(JNIEnv *env, jobject thiz, jboolean bIframeOnly)
{
	AVF_LOGI("native set iframe only");

	avf_remuxer_t *remuxer = get_remuxer(env, thiz);
	if (remuxer == NULL) {
		AVF_LOGE("Error: jni_remuxer_set_iframe_only no remuxer");
		return -1;
	}

	avf_remuxer_set_iframe_only(remuxer, (bool)bIframeOnly);

	return 0;
}

static int jni_remuxer_set_audio_fade_ms(JNIEnv *env, jobject thiz, jint audio_fade_ms)
{
	AVF_LOGI("native set audio fade ms");

	avf_remuxer_t *remuxer = get_remuxer(env, thiz);
	if (remuxer == NULL) {
		AVF_LOGE("Error: jni_remuxer_set_audio_fade_ms no remuxer");
		return -1;
	}

	avf_remuxer_set_audio_fade_ms(remuxer, (int)audio_fade_ms);

	return 0;
}

static jint jni_remuxer_set_audio(JNIEnv *env, jobject thiz,
	jboolean disableAudio, jstring audioFileName, jstring format)
{
	AVF_LOGI("native set audio");

	avf_remuxer_t *remuxer = get_remuxer(env, thiz);
	if (remuxer == NULL) {
		AVF_LOGE("Error: jni_remuxer_set_audio no remuxer");
		return -1;
	}

	bool bDisableAudio = false;
	const char *pAudioFileName = NULL;
	const char *pFormat = NULL;

	bDisableAudio = (bool)disableAudio;
	pAudioFileName = audioFileName ? env->GetStringUTFChars(audioFileName, NULL) : NULL;
	pFormat = format ? env->GetStringUTFChars(format, NULL) : NULL;

	AVF_LOGI("audio: %d, %s", bDisableAudio, pAudioFileName);

	avf_remuxer_set_audio(remuxer, bDisableAudio, pAudioFileName, pFormat);

	if (audioFileName) {
		env->ReleaseStringUTFChars(audioFileName, pAudioFileName);
	}
	if (format) {
		env->ReleaseStringUTFChars(format, pFormat);
	}

	return 0;
}

static jint jni_remuxer_native_run(JNIEnv *env, jobject thiz,
	jstring inputFile, jstring inputFormat, 
	jstring outputFile, jstring outputFormat, jint duration_ms)
{
	AVF_LOGI("native run\n");

	avf_remuxer_t *remuxer = get_remuxer(env, thiz);
	if (remuxer == NULL) {
		AVF_LOGE("Error: jni_remuxer_native_run no remuxer");
		return -1;
	}

	const char *pInputFile = NULL;
	const char *pInputFormat = NULL;
	const char *pOutputFile = NULL;
	const char *pOutputFormat = NULL;

	pInputFile = env->GetStringUTFChars(inputFile, NULL);
	pInputFormat = env->GetStringUTFChars(inputFormat, NULL);
	pOutputFile = env->GetStringUTFChars(outputFile, NULL);
	pOutputFormat = env->GetStringUTFChars(outputFormat, NULL);

	int rval = avf_remuxer_run(remuxer, pInputFile, pInputFormat, pOutputFile, pOutputFormat, duration_ms);

	env->ReleaseStringUTFChars(inputFile, pInputFile);
	env->ReleaseStringUTFChars(inputFormat, pInputFormat);
	env->ReleaseStringUTFChars(outputFile, pOutputFile);
	env->ReleaseStringUTFChars(outputFormat, pOutputFormat);

	return rval;
}

static const JNINativeMethod gMethods[] = {
	{"native_init",		"()V",		(void*)jni_remuxer_native_init},
	{"native_release",	"()V",		(void*)jni_remuxer_native_release},
	{"native_set_iframe_only",	"(Z)I",	(void*)jni_remuxer_set_iframe_only},
	{"native_set_audio_fade_ms", "(I)I", (void*)jni_remuxer_set_audio_fade_ms},
	{"native_setAudio",	"(ZLjava/lang/String;Ljava/lang/String;)I",		(void*)jni_remuxer_set_audio},
	{"native_run",		"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)I",
									(void*)jni_remuxer_native_run},
	{"native_finalize", "()V",		(void*)jni_remuxer_native_finalize},
};

static int init_remuxer_class(JNIEnv *env)
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

	// get mNativeContext field
	g_info.contextID = env->GetFieldID(clazz, "mNativeContext", "I");
	if (g_info.contextID == NULL) {
		AVF_LOGE("Error: cannot find mNativeContext");
		return -1;
	}

	// get notify() method
	g_info.notifyID = env->GetMethodID(clazz, "notify", "(III)V");
	if (g_info.notifyID == NULL) {
		AVF_LOGE("Error: cannot find notify()");
		return -1;
	}

	return 0;
}

int init_iframedec_class(JNIEnv *env);
int init_mp4i_class(JNIEnv *env);
int init_mp4raw_class(JNIEnv *env);

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv *env = NULL;
	jint result = JNI_ERR;

#if 0
	avf_set_logs(ALL_LOGS);
#endif

	AVF_LOGI("JNI_OnLoad");

	// save jvm
	g_info.jvm = vm;

	if (vm->GetEnv((void**)&env, MY_JNI_VER) != JNI_OK) {
		AVF_LOGE("Error: GetEnv failed");
		goto End;
	}

	if (init_remuxer_class(env) < 0) {
		goto End;
	}

	if (init_iframedec_class(env) < 0) {
		goto End;
	}

	if (init_mp4i_class(env) < 0) {
		goto End;
	}

	if (init_mp4raw_class(env) < 0) {
		goto End;
	}

	avf_module_init();

	result = MY_JNI_VER;

End:
	return result;
}


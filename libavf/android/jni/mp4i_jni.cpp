
#define LOG_TAG "mp4i_jni"

#include "avf_common.h"
#include "avf_util.h"
#include "avf_mp4i.h"
#include "jni.h"

static struct {
	jfieldID clip_date;
	jfieldID clip_length_ms;
	jfieldID clip_created_date;
	jfieldID stream_version;
	jfieldID video_coding;
	jfieldID video_frame_rate;
	jfieldID video_width;
	jfieldID video_height;
	jfieldID audio_coding;
	jfieldID audio_num_channels;
	jfieldID audio_sampling_freq;
	jfieldID has_gps;
	jfieldID has_acc;
	jfieldID has_obd;
} g_info;

#undef kClassPathName
#define kClassPathName	"com/transee/vdb/Mp4Info"

#undef kClassPathName2
#define kClassPathName2	"com/transee/vdb/LocalRawData"

static void jni_mp4i_native_init(JNIEnv *env, jobject thiz)
{
}

static int jni_mp4i_native_write_info(JNIEnv *env, jobject thiz,
	jstring filename,
	jbyteArray jpgData,
	jbyteArray gpsData,
	jbyteArray accData,
	jbyteArray obdData)
{
	mp4i_info_t info;
	::memset(&info, 0, sizeof(info));

	info.clip_date = env->GetIntField(thiz, g_info.clip_date);
	info.clip_length_ms = env->GetIntField(thiz, g_info.clip_length_ms);
	//info.clip_created_date = env->GetIntField(thiz, g_info.clip_created_date);
	info.clip_created_date = avf_get_sys_time(NULL);

	info.stream_version = env->GetIntField(thiz, g_info.stream_version);
	info.video_coding = env->GetIntField(thiz, g_info.video_coding);
	info.video_frame_rate = env->GetIntField(thiz, g_info.video_frame_rate);
	info.video_width = env->GetIntField(thiz, g_info.video_width);
	info.video_height = env->GetIntField(thiz, g_info.video_height);

	info.audio_coding = env->GetIntField(thiz, g_info.audio_coding);
	info.audio_num_channels = env->GetIntField(thiz, g_info.audio_num_channels);
	info.audio_sampling_freq = env->GetIntField(thiz, g_info.audio_sampling_freq);

	const char *pFileName = env->GetStringUTFChars(filename, NULL);

	jboolean isCopy = false;
	jbyte *jpg_data = jpgData ? env->GetByteArrayElements(jpgData, &isCopy) : NULL;
	jbyte *gps_data = gpsData ? env->GetByteArrayElements(gpsData, &isCopy) : NULL;
	jbyte *acc_data = accData ? env->GetByteArrayElements(accData, &isCopy) : NULL;
	jbyte *obd_data = obdData ? env->GetByteArrayElements(obdData, &isCopy) : NULL;

	int rval = mp4i_write_info(pFileName, &info,
		jpgData ? env->GetArrayLength(jpgData) : 0,
		(const uint8_t*)jpg_data,
		(const uint8_t*)gps_data,
		(const uint8_t*)acc_data,
		(const uint8_t*)obd_data);

Done:
	env->ReleaseStringUTFChars(filename, pFileName);
	if (jpg_data) {
		env->ReleaseByteArrayElements(jpgData, jpg_data, 0);
	}
	if (gps_data) {
		env->ReleaseByteArrayElements(gpsData, gps_data, 0);
	}
	if (acc_data) {
		env->ReleaseByteArrayElements(accData, acc_data, 0);
	}
	if (obd_data) {
		env->ReleaseByteArrayElements(obdData, obd_data, 0);
	}
	return rval;
}

static int jni_mp4i_native_read_info(JNIEnv *env, jobject thiz, jstring filename)
{
	const char *pFileName = env->GetStringUTFChars(filename, NULL);

	mp4i_info_t info;
	::memset(&info, 0, sizeof(info));

	int result;

	result = mp4i_read_info(pFileName, &info);
	if (result < 0)
		goto Done;

	env->SetIntField(thiz, g_info.clip_date, info.clip_date);
	env->SetIntField(thiz, g_info.clip_length_ms, info.clip_length_ms);

	env->SetIntField(thiz, g_info.stream_version, info.stream_version);
	env->SetIntField(thiz, g_info.video_coding, info.video_coding);
	env->SetIntField(thiz, g_info.video_frame_rate, info.video_frame_rate);
	env->SetIntField(thiz, g_info.video_width, info.video_width);
	env->SetIntField(thiz, g_info.video_width, info.video_width);

	env->SetIntField(thiz, g_info.audio_coding, info.audio_coding);
	env->SetIntField(thiz, g_info.audio_num_channels, info.audio_num_channels);
	env->SetIntField(thiz, g_info.audio_sampling_freq, info.audio_sampling_freq);

	env->SetBooleanField(thiz, g_info.has_gps, info.has_gps);
	env->SetBooleanField(thiz, g_info.has_acc, info.has_acc);
	env->SetBooleanField(thiz, g_info.has_obd, info.has_obd);

Done:
	env->ReleaseStringUTFChars(filename, pFileName);
	return result;
}

static jbyteArray jni_mp4i_native_read_poster(JNIEnv *env, jobject thiz, jstring filename)
{
	const char *pFileName = env->GetStringUTFChars(filename, NULL);
	jbyteArray result = NULL;

	uint32_t jpg_size = 0;
	uint8_t *jpg_data = mp4i_read_poster(pFileName, &jpg_size);

	if (jpg_data != NULL) {
		result = env->NewByteArray(jpg_size);
		if (result != NULL) {
			env->SetByteArrayRegion(result, 0, jpg_size, (jbyte*)jpg_data);
		}
		mp4i_free_poster(jpg_data);
	}

	env->ReleaseStringUTFChars(filename, pFileName);
	return result;
}

static const JNINativeMethod gMethods[] = {
	{"native_init",		"()V",		(void*)jni_mp4i_native_init},
	{"native_write_info",	"(Ljava/lang/String;[B[B[B[B)I",	(void*)jni_mp4i_native_write_info},
	{"native_read_info",	"(Ljava/lang/String;)I", (void*)jni_mp4i_native_read_info},
	{"native_read_poster", "(Ljava/lang/String;)[B", (void*)jni_mp4i_native_read_poster},
};

static int get_field(JNIEnv *env, jclass clazz, const char *field, const char *type, jfieldID *id)
{
	*id = env->GetFieldID(clazz, field, type);
	if (*id == NULL) {
		AVF_LOGE("Error: cannot find %s, type %s", field, type);
		return -1;
	}
	return 0;
}

int init_mp4i_class(JNIEnv *env)
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

	// 
	int result;

	if ((result = get_field(env, clazz, "clip_date", "I", &g_info.clip_date)) < 0)
		return result;

	if ((result = get_field(env, clazz, "clip_length_ms", "I", &g_info.clip_length_ms)) < 0)
		return result;

	if ((result = get_field(env, clazz, "clip_created_date", "I", &g_info.clip_created_date)) < 0)
		return result;

	if ((result = get_field(env, clazz, "stream_version", "I", &g_info.stream_version)) < 0)
		return result;

	if ((result = get_field(env, clazz, "video_coding", "I", &g_info.video_coding)) < 0)
		return result;

	if ((result = get_field(env, clazz, "video_frame_rate", "I", &g_info.video_frame_rate)) < 0)
		return result;

	if ((result = get_field(env, clazz, "video_width", "I", &g_info.video_width)) < 0)
		return result;

	if ((result = get_field(env, clazz, "video_height", "I", &g_info.video_height)) < 0)
		return result;

	if ((result = get_field(env, clazz, "audio_coding", "I", &g_info.audio_coding)) < 0)
		return result;

	if ((result = get_field(env, clazz, "audio_num_channels", "I", &g_info.audio_num_channels)) < 0)
		return result;

	if ((result = get_field(env, clazz, "audio_sampling_freq", "I", &g_info.audio_sampling_freq)) < 0)
		return result;

	if ((result = get_field(env, clazz, "has_gps", "Z", &g_info.has_gps)) < 0)
		return result;

	if ((result = get_field(env, clazz, "has_acc", "Z", &g_info.has_acc)) < 0)
		return result;

	if ((result = get_field(env, clazz, "has_obd", "Z", &g_info.has_obd)) < 0)
		return result;

	return 0;
}

static jfieldID g_mp4raw_contextID;

INLINE static mp4i_raw_data_t *get_mp4i_raw_data(JNIEnv *env, jobject thiz)
{
	return (mp4i_raw_data_t*)env->GetIntField(thiz, g_mp4raw_contextID);
}

INLINE static void set_mp4i_raw_data(JNIEnv *env, jobject thiz, mp4i_raw_data_t *data)
{
	env->SetIntField(thiz, g_mp4raw_contextID, (int)data);
}

static void jni_mp4raw_native_unload(JNIEnv *env, jobject thiz)
{
	mp4i_raw_data_t *data = get_mp4i_raw_data(env, thiz);
	if (data != NULL) {
		mp4i_free_raw_data(data);
		set_mp4i_raw_data(env, thiz, NULL);
	}
}

static void jni_mp4raw_native_load(JNIEnv *env, jobject thiz, jstring filename)
{
	jni_mp4raw_native_unload(env, thiz);
	const char *pFileName = env->GetStringUTFChars(filename, NULL);
	mp4i_raw_data_t *data = mp4i_read_raw_data(pFileName);
	env->ReleaseStringUTFChars(filename, pFileName);
	set_mp4i_raw_data(env, thiz, data);
}

static void jni_mp4raw_native_init(JNIEnv *env, jobject thiz)
{
	set_mp4i_raw_data(env, thiz, NULL);
}

static void jni_mp4raw_native_finalize(JNIEnv *env, jobject thiz)
{
	jni_mp4raw_native_unload(env, thiz);
}

static jbyteArray return_ack(JNIEnv *env, mp4i_ack_t *ack)
{
	jbyteArray result = env->NewByteArray(ack->size);
	if (result != NULL) {
		env->SetByteArrayRegion(result, 0, ack->size, (jbyte*)ack->data);
	}
	mp4i_free_ack(ack);
	return result;
}

static jbyteArray jni_mp4raw_native_read(JNIEnv *env, jobject thiz,
	jint clip_time_ms, int data_types)
{
	mp4i_raw_data_t *data = get_mp4i_raw_data(env, thiz);
	if (data == NULL)
		return NULL;

	mp4i_ack_t ack;
	if (mp4i_get_raw_data(data, clip_time_ms, data_types, &ack) < 0)
		return NULL;

	return return_ack(env, &ack);
}

static jbyteArray jni_mp4raw_native_read_block(JNIEnv *env, jobject thiz,
	jint clip_time_ms, jint length_ms, jint data_type)
{
	mp4i_raw_data_t *data = get_mp4i_raw_data(env, thiz);
	if (data == NULL)
		return NULL;

	mp4i_ack_t ack;
	if (mp4i_get_raw_data_block(data, clip_time_ms, length_ms, data_type, &ack) < 0)
		return NULL;

	return return_ack(env, &ack);
}

static const JNINativeMethod gMethods2[] = {
	{"native_init",	"()V",	(void*)jni_mp4raw_native_init},
	{"native_finalize",	"()V", (void*)jni_mp4raw_native_finalize},
	{"native_load",	"(Ljava/lang/String;)V",	 (void*)jni_mp4raw_native_load},
	{"native_unload",	"()V",	(void*)jni_mp4raw_native_unload},
	{"native_read",	"(II)[B", (void*)jni_mp4raw_native_read},
	{"native_read_block", "(III)[B", (void*)jni_mp4raw_native_read_block},
};

int init_mp4raw_class(JNIEnv *env)
{
	jclass clazz;
	int nMethods;

	// find java class
	clazz = env->FindClass(kClassPathName2);
	if (clazz == NULL) {
		AVF_LOGE("Error: cannot find class " kClassPathName2);
		return -1;
	}

	// install native methods
	nMethods = sizeof(gMethods2) / sizeof(gMethods2[0]);
	if (env->RegisterNatives(clazz, gMethods2, nMethods) < 0) {
		AVF_LOGE("Error: RegisterNatives failed");
		return -1;
	}

	g_mp4raw_contextID = env->GetFieldID(clazz, "mNativeContext", "I");
	if (g_mp4raw_contextID == NULL) {
		AVF_LOGE("Error: cannot find mNativeContext");
		return -1;
	}

	return 0;
}


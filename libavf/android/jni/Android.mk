

# ~/android-ndk-r9d/ndk-build APP_PLATFORM=android-14

LOCAL_PATH := $(call my-dir)/../..

FDKAAC_TOPDIR := $(LOCAL_PATH)/../../ambarella/rootfs/packages/fdk-aac/usr
FFMPEG_TOPDIR := $(HOME)/ffmpeg-2.3.3/android/arm

#####################################################################################################
#  fdk-aac
#####################################################################################################
include $(CLEAR_VARS)
LOCAL_MODULE := LIB_FDK_AAC
LOCAL_SRC_FILES := $(FDKAAC_TOPDIR)/lib/libfdk-aac.a
LOCAL_EXPORT_C_INCLUDES := $(FDKAAC_TOPDIR)/include/fdk-aac
include $(PREBUILT_STATIC_LIBRARY) 

#####################################################################################################
#  ffmpeg
#####################################################################################################
include $(CLEAR_VARS)
LOCAL_MODULE := LIB_FFMPEG_AVCODEC
LOCAL_SRC_FILES := $(FFMPEG_TOPDIR)/lib/libavcodec.a
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_TOPDIR)/include
include $(PREBUILT_STATIC_LIBRARY) 

include $(CLEAR_VARS)
LOCAL_MODULE := LIB_FFMPEG_AVFORMAT
LOCAL_SRC_FILES := $(FFMPEG_TOPDIR)/lib/libavformat.a
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_TOPDIR)/include
include $(PREBUILT_STATIC_LIBRARY) 

include $(CLEAR_VARS)
LOCAL_MODULE := LIB_FFMPEG_AVUTIL
LOCAL_SRC_FILES := $(FFMPEG_TOPDIR)/lib/libavutil.a
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_TOPDIR)/include
include $(PREBUILT_STATIC_LIBRARY) 

include $(CLEAR_VARS)
LOCAL_MODULE := LIB_FFMPEG_SWSCALE
LOCAL_SRC_FILES := $(FFMPEG_TOPDIR)/lib/libswscale.a
LOCAL_EXPORT_C_INCLUDES := $(FFMPEG_TOPDIR)/include
include $(PREBUILT_STATIC_LIBRARY) 

#####################################################################################################
#  remuxer
#####################################################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := avfmedia

LOCAL_CFLAGS := -DANDROID_OS -DREMUXER -DSUPPORT_MP3 -DUSE_FFMPEG -DAAC_ENCODER -DUSE_FDK_AAC

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/source/include \
	$(LOCAL_PATH)/source/avio \
	$(LOCAL_PATH)/source/engines \
	$(LOCAL_PATH)/source/filters \
	$(LOCAL_PATH)/source/format \
	$(LOCAL_PATH)/source/players \
	$(LOCAL_PATH)/source/vdb

LOCAL_SRC_FILES := \
	android/jni/remuxer_jni.cpp \
	android/jni/iframedec_jni.cpp \
	android/jni/mp4i_jni.cpp \
	\
	source/base/avf_new.cpp \
	source/base/avf_osal_linux.cpp \
	source/base/avf_socket.cpp \
	source/base/avf_util.cpp \
	source/base/mem_stream.cpp \
	source/base/avf_queue.cpp \
	source/base/avf_base.cpp \
	source/base/avf_if.cpp \
	source/base/avf_config.cpp \
	source/base/rbtree.cpp \
	source/base/avf_registry.cpp \
	source/base/timer_manager.cpp \
	\
	source/avio/avio.cpp \
	source/avio/file_io.cpp \
	source/avio/buffer_io.cpp \
	source/avio/sys_io.cpp \
	source/avio/http_io.cpp \
	\
	source/players/avf_media_control.cpp \
	source/players/player_creator.cpp \
	source/players/base_player.cpp \
	source/players/remuxer.cpp \
	source/players/avf_remuxer_api.cpp \
	\
	source/filters/file_demuxer.cpp \
	source/filters/mp3_decoder.cpp \
	source/filters/aac_encoder.cpp \
	source/filters/filter_if.cpp \
	source/filters/file_muxer.cpp \
	source/filters/filter_list.cpp \
	\
	source/format/mp3_reader.cpp \
	source/format/mp4_reader.cpp \
	source/format/ts_reader.cpp \
	source/format/mp4_writer.cpp \
	source/format/async_writer.cpp \
	source/format/writer_list.cpp \
	source/format/sample_queue.cpp \
	source/format/ts_writer.cpp \
	source/format/mpeg_utils.cpp \
	\
	source/vdb/avf_mp4i.cpp \
	\
	misc/avf_module.cpp \

LOCAL_LDLIBS := -llog -ljnigraphics

LOCAL_STATIC_LIBRARIES := \
	LIB_FDK_AAC \
	LIB_FFMPEG_AVUTIL \
	LIB_FFMPEG_AVFORMAT \
	LIB_FFMPEG_AVCODEC \
	LIB_FFMPEG_SWSCALE \
	LIB_FFMPEG_AVUTIL

include $(BUILD_SHARED_LIBRARY)

#####################################################################################################
#  vdb native
#####################################################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := vdbnative

LOCAL_CFLAGS := -DANDROID_OS

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/source/include \
	$(LOCAL_PATH)/source/avio \
	$(LOCAL_PATH)/source/engines \
	$(LOCAL_PATH)/source/filters \
	$(LOCAL_PATH)/source/format \
	$(LOCAL_PATH)/source/players \
	$(LOCAL_PATH)/source/vdb

LOCAL_SRC_FILES := \
	source/base/avf_malloc.cpp \
	source/base/avf_new.cpp \
	source/base/avf_osal_linux.cpp \
	source/base/avf_socket.cpp \
	source/base/avf_util.cpp \
	source/base/mem_stream.cpp \
	source/base/avf_queue.cpp \
	source/base/avf_base.cpp \
	source/base/avf_if.cpp \
	source/base/avf_config.cpp \
	source/base/rbtree.cpp \
	source/base/timer_manager.cpp \
	source/base/avf_tcp_server.cpp \
	\
	source/avio/avio.cpp \
	source/avio/file_io.cpp \
	source/avio/buffer_io.cpp \
	source/avio/sys_io.cpp \
	source/avio/http_io.cpp \
	source/avio/tcp_io.cpp \
	\
	source/vdb/vdb_util.cpp \
	source/vdb/vdb_file_list.cpp \
	source/vdb/vdb_clip.cpp \
	source/vdb/vdb_format.cpp \
	source/vdb/vdb.cpp \
	source/vdb/vdb_reader.cpp \
	source/vdb/vdb_server.cpp \
	source/vdb/vdb_native.cpp \
	source/vdb/picture_list.cpp \
	source/vdb/vdb_if.cpp \
	\
	misc/avf_module.cpp

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)

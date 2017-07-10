
# put this file under ffmpeg/jni
# and do $(NDK)/ndk-build

LOCAL_PATH := $(call my-dir)/..
include $(CLEAR_VARS)

LOCAL_MODULE := ffmpeg

LOCAL_CFLAGS := -include $(LOCAL_PATH)/config.h

LOCAL_C_INCLUDES := 

LOCAL_ARM_MODE := arm	# remove this if you want thumb mode
LOCAL_ARM_NEON := false	# remove this if you want armv5 mode

LOCAL_SRC_FILES := \
	libavformat/allformats.c \
	libavformat/avio.c \
	libavformat/aviobuf.c \
	libavformat/cutils.c \
	libavformat/dump.c \
	libavformat/format.c \
	libavformat/id3v1.c \
	libavformat/id3v2.c \
	libavformat/metadata.c \
	libavformat/mux.c \
	libavformat/options.c \
	libavformat/os_support.c \
	libavformat/riff.c \
	libavformat/sdp.c \
	libavformat/seek.c \
	libavformat/url.c \
	libavformat/utils.c \
	libavcodec/allcodecs.c \
	libavcodec/arm/fft_fixed_init_arm.c \
	libavcodec/arm/fft_fixed_neon.S \
	libavcodec/arm/fft_init_arm.c \
	libavcodec/arm/fft_neon.S \
	libavcodec/arm/fft_vfp.S \
	libavcodec/arm/fmtconvert_init_arm.c \
	libavcodec/arm/fmtconvert_neon.S \
	libavcodec/arm/fmtconvert_vfp.S \
	libavcodec/arm/fmtconvert_vfp_armv6.S \
	libavcodec/arm/mpegaudiodsp_fixed_armv6.S \
	libavcodec/arm/mpegaudiodsp_init_arm.c \
	libavcodec/arm/rdft_neon.S \
	libavcodec/audioconvert.c \
	libavcodec/avdct.c \
	libavcodec/avfft.c \
	libavcodec/avpacket.c \
	libavcodec/avpicture.c \
	libavcodec/bitstream.c \
	libavcodec/bitstream_filter.c \
	libavcodec/codec_desc.c \
	libavcodec/dct.c \
	libavcodec/dct32_fixed.c \
	libavcodec/dct32_float.c \
	libavcodec/dv_profile.c \
	libavcodec/fft_fixed.c \
	libavcodec/fft_fixed_32.c \
	libavcodec/fft_float.c \
	libavcodec/fft_init_table.c \
	libavcodec/fmtconvert.c \
	libavcodec/mathtables.c \
	libavcodec/mpegaudio.c \
	libavcodec/mpegaudiodata.c \
	libavcodec/mpegaudiodec_fixed.c \
	libavcodec/mpegaudiodecheader.c \
	libavcodec/mpegaudiodsp.c \
	libavcodec/mpegaudiodsp_data.c \
	libavcodec/mpegaudiodsp_fixed.c \
	libavcodec/mpegaudiodsp_float.c \
	libavcodec/options.c \
	libavcodec/parser.c \
	libavcodec/raw.c \
	libavcodec/rdft.c \
	libavcodec/resample.c \
	libavcodec/resample2.c \
	libavcodec/utils.c \
	libavutil/adler32.c \
	libavutil/aes.c \
	libavutil/arm/cpu.c \
	libavutil/arm/float_dsp_init_arm.c \
	libavutil/arm/float_dsp_init_neon.c \
	libavutil/arm/float_dsp_init_vfp.c \
	libavutil/arm/float_dsp_neon.S \
	libavutil/arm/float_dsp_vfp.S \
	libavutil/atomic.c \
	libavutil/audio_fifo.c \
	libavutil/avstring.c \
	libavutil/base64.c \
	libavutil/blowfish.c \
	libavutil/bprint.c \
	libavutil/buffer.c \
	libavutil/channel_layout.c \
	libavutil/cpu.c \
	libavutil/crc.c \
	libavutil/des.c \
	libavutil/dict.c \
	libavutil/display.c \
	libavutil/downmix_info.c \
	libavutil/error.c \
	libavutil/eval.c \
	libavutil/fifo.c \
	libavutil/file.c \
	libavutil/file_open.c \
	libavutil/fixed_dsp.c \
	libavutil/float_dsp.c \
	libavutil/frame.c \
	libavutil/hash.c \
	libavutil/hmac.c \
	libavutil/imgutils.c \
	libavutil/intfloat_readwrite.c \
	libavutil/intmath.c \
	libavutil/lfg.c \
	libavutil/lls1.c \
	libavutil/lls2.c \
	libavutil/log.c \
	libavutil/log2_tab.c \
	libavutil/mathematics.c \
	libavutil/md5.c \
	libavutil/mem.c \
	libavutil/murmur3.c \
	libavutil/opt.c \
	libavutil/parseutils.c \
	libavutil/pixdesc.c \
	libavutil/random_seed.c \
	libavutil/rational.c \
	libavutil/rc4.c \
	libavutil/ripemd.c \
	libavutil/samplefmt.c \
	libavutil/sha.c \
	libavutil/sha512.c \
	libavutil/stereo3d.c \
	libavutil/threadmessage.c \
	libavutil/time.c \
	libavutil/timecode.c \
	libavutil/tree.c \
	libavutil/utils.c \
	libavutil/xga_font_data.c \
	libavutil/xtea.c \


include $(BUILD_STATIC_LIBRARY)

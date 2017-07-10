
#ifndef __AVF_CONFIGS_H__
#define __AVF_CONFIGS_H__

// ---------------------------------------------------------------------

#define NAME_HLS_M3U8				"config.hls.m3u8"
#define VALUE_HLS_M3U8				"/tmp/mmcblk0p1/camera.m3u8"

#define NAME_HLS_SEGMENTS			"config.hls.segments"
#define VALUE_HLS_SEGMENTS			0

// ---------------------------------------------------------------------

#define NAME_FILE_CURRENT			"state.file.current"
#define VALUE_FILE_CURRENT			""

#define NAME_FILE_PREVIOUS			"state.file.previous"
#define VALUE_FILE_PREVIOUS			""

#define NAME_MUXER_PATH				"config.muxer.path"
#define VALUE_MUXER_PATH			"/tmp/"

#define NAME_MUXER_FILE				"config.muxer.file"
#define VALUE_MUXER_FILE			"/tmp/file001.dat"

#define NAME_MEDIAWRITER_FORMAT		"config.MediaWriter.format"

// ---------------------------------------------------------------------

#define NAME_MJPEG_FPS				"config.mjpeg.fps"
#define VALUE_MJPEG_FPS				""

// ---------------------------------------------------------------------

#define NAME_OBD_MASK				"config.obd.mask"
#define VALUE_OBD_MASK				""

#define NAME_VIN_MASK				"config.vin.mask"
#define VALUE_VIN_MASK				""

#define NAME_GPS_INTERVAL			"config.gps.interval"
#define VALUE_GPS_INTERVAL			1000

#define NAME_ACC_INTERVAL			"config.acc.interval"
#define VALUE_ACC_INTERVAL			200

#define NAME_OBD_INTERVAL			"config.obd.interval"
#define VALUE_OBD_INTERVAL			1000

#define NAME_OBD_ENABLE				"config.obd.enable"
#define VALUE_OBD_ENABLE			false

#define NAME_GPS_SATSNR				"config.gps.satsnr"
#define VALUE_GPS_SATSNR			"0,0"

#define NAME_GPS_ACCURACY			"config.gps.accuracy"
#define VALUE_GPS_ACCURACY			0

#define NAME_STATUS_GPS				"status.gps"

#define NAME_STATUS_GPS_NUM_SVS		"status.gps.num_svs"

// ---------------------------------------------------------------------

#define NAME_SUBTITLE_TIMEFMT		"config.subtitle.timefmt"
#define VALUE_SUBTITLE_TIMEFMT		"%04Y.%02m.%02d %02H:%02M:%02S "

#define NAME_SUBTITLE_PREFIX		"config.subtitle.prefix"
#define VALUE_SUBTITLE_PREFIX		""
	
#define NAME_SUBTITLE_TIME			"config.subtitle.time"
#define VALUE_SUBTITLE_TIME			""

#define NAME_SUBTITLE_SLIMIT_STR	"config.subtitle.slimit-str"
#define VALUE_SUBTITLE_SLIMIT_STR	""

#define NAME_SUBTITLE_GPS_INFO		"config.subtitle.gpsinfo"
#define VALUE_SUBTITLE_GPS_INFO		""

#define NAME_SUBTITLE_INTERVAL		"config.subtitle.interval"
#define VALUE_SUBTITLE_INTERVAL		1000

#define NAME_SUBTITLE_SHOW_GPS		"config.subtitle.show-gps"
#define VALUE_SUBTITLE_SHOW_GPS		false

#define NAME_SUBTITLE_ENABLE		"config.subtitle.enable"
#define VALUE_SUBTITLE_ENABLE		0

#define NAME_SUBTITLE_FLAGS			"config.subtitle.flags"
#define VALUE_SUBTITLE_FLAGS		0xFFFF

#define NAME_SUBTITLE_SLIMIT		"config.subtitle.slimit"
#define VALUE_SUBTITLE_SLIMIT		-1

// ---------------------------------------------------------------------

#define NAME_UPLOAD_PATH_RAW		"config.upload.path.raw"
#define VALUE_UPLOAD_PATH_RAW		"/tmp/"

#define NAME_UPLOAD_PATH_PICTURE	"config.upload.path.picture"
#define VALUE_UPLOAD_PATH_PICTURE	"/tmp/"

#define NAME_UPLOAD_PATH_VIDEO		"config.upload.path.video"
#define VALUE_UPLOAD_PATH_VIDEO		"/tmp/"

#define NAME_UPLOAD_PICTURE			"config.upload.picture"
#define VALUE_UPLOAD_PICTURE		false

#define NAME_UPLOAD_VIDEO			"config.upload.video"
#define VALUE_UPLOAD_VIDEO			false

#define NAME_UPLOAD_GPS				"config.upload.gps"
#define VALUE_UPLOAD_GPS			0

#define NAME_UPLOAD_ACC				"config.upload.acc"
#define VALUE_UPLOAD_ACC			0

#define NAME_UPLOAD_OBD				"config.upload.obd"
#define VALUE_UPLOAD_OBD			0

#define NAME_UPLOAD_RAW				"config.upload.raw"
#define VALUE_UPLOAD_RAW			0

#define NAME_UPLOAD_INTERVAL_RAW	"config.upload.interval.raw"
#define VALUE_UPLOAD_INTERVAL_RAW	1000

#define NAME_UPLOAD_VIDEO_NUM		"config.upload.video.num"

#define NAME_UPLOAD_VIDEO_PREV		"state.upload.video.prev"
#define NAME_UPLOAD_PICTURE_PREV	"state.upload.picture.prev"
#define NAME_UPLOAD_RAW_PREV		"state.upload.raw.prev"

#define NAME_UPLOAD_VIDEO_INDEX		"state.upload.video.index"
#define NAME_UPLOAD_PICTURE_INDEX	"state.upload.picture.index"
#define NAME_UPLOAD_RAW_INDEX		"state.upload.raw.index"

// ---------------------------------------------------------------------

#define NAME_MMS_PATH_MODE1			"config.mms.path.mode1"
#define NAME_MMS_PATH_MODE2			"config.mms.path.mode2"
#define VALUE_MMS_PATH_MODE1		"/tmp/"
#define VALUE_MMS_PATH_MODE2		"/tmp/"

#define NAME_MMS_NUMFILES_MODE1		"config.mms.numfiles.mode1"
#define NAME_MMS_NUMFILES_MODE2		"config.mms.numfiles.mode2"

#define NAME_MMS_OPT_MODE1			"config.mms.opt.mode1"	// for rtsp
#define NAME_MMS_OPT_MODE2			"config.mms.opt.mode2"	// for rtmp

// fileindex;filesize;framecount
#define NAME_MMS_STATE_MODE1		"state.mms.mode1"
#define NAME_MMS_STATE_MODE2		"state.mms.mode2"

// ---------------------------------------------------------------------

#define NAME_VIDEO_OVERLAY_FONTNAME		"config.video.overlay.fontname"
#define VALUE_VIDEO_OVERLAY_FONTNAME	"/usr/share/fonts/truetype/tsfont.ttf"

#define NAME_VIDEO_OVERLAY		"config.video.overlay"
#define VALUE_VIDEO_OVERLAY		""

#define NAME_VIDEO_LOGO			"config.video.logo"
#define VALUE_VIDEO_LOGO		""

#define NAME_VIDEO_LOGO_PATH	"config.video.logo.path"
#define VALUE_VIDEO_LOGO_PATH	""

// ---------------------------------------------------------------------

#define NAME_VIN				"config.vin"
#define VALUE_VIN				""

#define NAME_CLOCK_MODE			"config.clockmode"
#define VALUE_CLOCK_MODE		""

#define NAME_VIDEO				"config.video"
#define VALUE_VIDEO				""

#define NAME_VIDEO_FRAMERATE	"config.video.framerate"
#define VALUE_VIDEO_FRAMERATE	""

#define NAME_VIDEO_SEGMENT		"config.video.segment"
#define VALUE_VIDEO_SEGMENT		0

#define NAME_PICTURE_SEGMENT	"config.picture.segment"
#define VALUE_PICTURE_SEGMENT	0

#define NAME_CAMER_START_RECORD_TICKS	"camera.record.tick"

// ---------------------------------------------------------------------

#define NAME_PB_EOS_WAIT		"pb.eos-wait"
#define VALUE_PB_EOS_WAIT		0

#define NAME_PB_AUDIO_POS		"pb.audio-pos"
#define VALUE_PB_AUDIO_POS		0

#define NAME_PB_VIDEO_POS		"pb.video-pos"
#define VALUE_PB_VIDEO_POS		0

#define NAME_PB_AUDIO_STOPPED	"pb.audio-stopped"
#define VALUE_PB_AUDIO_STOPPED	0

#define NAME_PB_FASTPLAY		"pb.fastplay"
#define VALUE_PB_FASTPLAY		0

#define NAME_PB_FRAME_SEEK		"pb.frame-seek"
#define VALUE_PB_FRAME_SEEK		0

#define NAME_PB_PAUSE			"pb.pause"
#define VALUE_PB_PAUSE			0

#define NAME_MP4_MOOV_SIZE		"mp4.moov_size"
#define VALUE_MP4_MOOV_SIZE		0

#define NAME_MUXER_LARGE_FILE	"info.muxer.large_file"
#define VALUE_MUXER_LARGE_FILE	false

// 0: no encode, no record
// 1: encode & record
// 2: encode only
#define NAME_RECORD				"config.record"
//#define VALUE_RECORD			true

#define NAME_AVSYNC				"config.record.avsync"
#define VALUE_AVSYNC			true

#define NAME_HIDE_AUDIO			"config.demuxer.hideaudio"
#define VALUE_HIDE_AUDIO		false

#define NAME_EXTRA_AUDIO		"config.remuxer.extraaudio"
#define VALUE_EXTRA_AUDIO		""

#define NAME_REMUXER_DURATION	"config.remuxer.duration"
#define VALUE_REMUXER_DURATION	0

#define NAME_REMUXER_FADE_MS	"config.remuxer.fade"
#define VALUE_REMUXER_FADE_MS	0

#define NAME_ALIGN_TO_VIDEO		"config.muxer.aligntovideo"
#define VALUE_ALIGN_TO_VIDEO	false

// max,overrun,overflow
// unit is MB
#define NAME_MEM_LIMIT			"config.mem.limit"

//-----------------------------------------------------------------
// global registry
//-----------------------------------------------------------------

#define NAME_VDB_FS		"vdb.fs"
#define VALUE_VDB_FS	"/tmp/mmcblk0p1"

#define NAME_VDB_AUTODEL	"config.vdb.autodel"
#define VALUE_VDB_AUTODEL	1

#define NAME_VDB_AUTODEL_FREE	"config.vdb.autodel.free"
#define NAME_VDB_AUTODEL_MIN	"config.vdb.autodel.min"
//#define NAME_VDB_AUTODEL_QUOTA	"config.vdb.autodel.quota"

#define NAME_VDB_NO_DELETE		"config.vdb.nodelete"
#define VALUE_VDB_NO_DELETE		0

#define NAME_CLOCK_MODE_ENABLE		"config.clockmode.enable"
#define VALUE_CLOCK_MODE_ENABLE		1

#define NAME_DISK_MONITOR			"config.diskmonitor.params"
#define VALUE_DISK_MONITOR			"0:60000,1000,20000"	// method:period,timeout,total_timeout
//#define VALUE_DISK_MONITOR		"1:60000,1000,10"		// method:period,timeout,num_timeouts

#define NAME_DBG_OVERLAY_FONTSIZE		"dbg.overlay.fontsize"
#define VALUE_DBG_OVERLAY_FONTSIZE		0

#define NAME_DBG_OVERLAY_HEIGHT			"dbg.overlay.height"
#define VALUE_DBG_OVERLAY_HEIGHT		0

#define NAME_DBG_SHOWOSD		"dbg.showosd"
#define VALUE_DBG_SHOWOSD		""

#define NAME_DBG_VMEM			"dbg.vmem"
#define VALUE_DBG_VMEM			0

#define NAME_INTERNAL_LOGO		"config.video.intlogo"
#define VALUE_INTERNAL_LOGO		0

#define NAME_AVF_VERSION		"state.avf.version"
#define VALUE_AVF_VERSION		"1.0.20160825"

#endif


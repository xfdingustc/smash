

#include "avf_common.h"
#include "avf_if.h"

#ifdef USE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
}
#endif

extern "C" int avf_module_init()
{
#ifdef USE_FFMPEG
	av_register_all();
#endif
	return 0;
}

extern "C" int avf_module_cleanup()
{
	return 0;
}


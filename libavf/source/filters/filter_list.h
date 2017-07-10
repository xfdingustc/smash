
#ifndef __FILTER_LIST_H__
#define __FILTER_LIST_H__

struct CConfigNode;

IFilter *avf_create_filter_by_name(const char *pFilterType, const char *pFilterName, 
	IEngine *pEngine, CConfigNode *pFilterConfig);
IFilter *avf_create_filter_by_io(IEngine *pEngine, IAVIO *io);
IFilter *avf_create_filter_by_media_format(CMediaFormat *pFormat, IEngine *pEngine);
IMediaFileReader *avf_create_media_file_reader_by_io(IEngine *pEngine, IAVIO *io);
IMediaFileReader *avf_create_media_file_reader_by_format(IEngine *pEngine, const char *format, IAVIO *io);

#endif


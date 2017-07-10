
#ifndef __WRITER_LIST_H__
#define __WRITER_LIST_H__

class IEngine;
class IMediaFileWriter;
struct CConfigNode;

IMediaFileWriter* avf_create_file_writer(IEngine *pEngine, 
	const char *name, CConfigNode *node, int index, const char *avio);

#endif



#ifndef __ASYNC_WRITER_H__
#define __ASYNC_WRITER_H__

class CAsyncWriter;
class CThread;
struct CConfigNode;

//-----------------------------------------------------------------------
//
//  CAsyncWriter
//
//-----------------------------------------------------------------------
class CAsyncWriter: public CMediaFileWriter
{
	typedef CMediaFileWriter inherited;

private:
	enum {
		CMD_QUIT = 0,

		CMD_START_WRITER,
		CMD_END_WRITER,

		CMD_START_FILE,
		CMD_END_FILE,

		CMD_SET_RAW_DATA,

		CMD_WRITE_SAMPLE,
		CMD_WRITE_POSTER,

		CMD_CREATE_SAMPLE_PROVIDER,
	};

	typedef struct cmd_s {
		int cmd_code;
		union {
			// CMD_QUIT

			// StartWriter
			struct {
				bool bEnableRecord;
			} StartWriter;

			// EndWriter
			struct {
				int dummy;
			} EndWriter;

			// StartFile
			struct {
				string_t *video_name;
				avf_time_t video_time;
				string_t *picture_name;
				avf_time_t picture_time;
				string_t *basename;
			} StartFile;

			// EndFile
			struct {
				bool bLast;
				avf_u64_t nextseg_ms;
			} EndFile;

			struct {
				avf_size_t stream;
				raw_data_t *raw;
			} SetRawData;

			// WriteSample
			struct {
				int stream;
				CBuffer *pBuffer;	// need to release
			} WriteSample;

			// WritePoster
			struct {
				string_t *filename;
				avf_time_t gen_time;
				CBuffer *pBuffer;	// need to release
			} WritePoster;

			// CreateSampleProvider
			struct {
				avf_size_t before;
				avf_size_t after;
				ISampleProvider **ppProvider;
			} CreateSampleProvider;
		} u;

		cmd_s(int code): cmd_code(code) {}
		cmd_s() {}
	} cmd_t;

public:
	static IMediaFileWriter* Create(IEngine *pEngine, CConfigNode *node, int index, const char *avio);

protected:
	CAsyncWriter(IEngine *pEngine, CConfigNode *node, int index, const char *avio);
	virtual ~CAsyncWriter();
	void RunThread();

// IMediaFileWriter
public:
	virtual avf_status_t StartWriter(bool bEnableRecord);
	virtual avf_status_t EndWriter();

	virtual avf_status_t StartFile(string_t *video_name, avf_time_t video_time,
		string_t *picture_name, avf_time_t picture_time, string_t *basename);
	virtual IAVIO *GetIO() { return NULL; }
	virtual avf_status_t EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms);
	virtual const char *GetCurrentFile();

	virtual avf_status_t SetRawData(avf_size_t stream, raw_data_t *raw);

	virtual avf_status_t ClearMediaFormats();
	virtual avf_status_t CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat);
	virtual avf_status_t CheckStreams();
	virtual avf_status_t SetStreamAttr(avf_stream_attr_s& stream_attr);

	virtual avf_status_t WriteSample(int stream, CBuffer* pBuffer);
	virtual avf_status_t WritePoster(string_t *filename, avf_time_t gen_time, CBuffer* pBuffer);

	virtual bool ProvideSamples();
	virtual ISampleProvider *CreateSampleProvider(avf_size_t before, avf_size_t after);

private:
	INLINE void PostCmd(cmd_t& cmd) {
		avf_status_t status = mpQueue->PostMsg(&cmd, sizeof(cmd));
		AVF_ASSERT_OK(status);
	}

	INLINE avf_status_t SendCmd(cmd_t& cmd) {
		return mpQueue->SendMsg(&cmd, sizeof(cmd));
	}

	INLINE avf_status_t SendCmd(int code) {
		cmd_t cmd(code);
		return SendCmd(cmd);
	}

	INLINE void ReplyCmd(avf_status_t status) {
		mpQueue->ReplyMsg(status);
	}

private:
	static void ThreadEntry(void *p);
	void ThreadLoop();

private:
	void AddToFileList(const char *filename);
	void UpdatePlaylist(bool bEnd);

private:
	void DoStartWriter(bool bEnableRecord);
	void DoEndWriter();
	void DoSetDir(string_t *dir);
	void DoStartFile(string_t *video_name, avf_time_t video_time, 
		string_t *picture_name, avf_time_t picture_time, string_t *basename);
	void DoEndFile(bool bLast, avf_u64_t nextseg_ms);
	void DoSetRawData(avf_size_t stream, raw_data_t *raw);
	void DoWriteSample(avf_size_t stream, CBuffer *pBuffer);
	void DoWritePoster(string_t *filename, avf_time_t gen_time, CBuffer *pBuffer);
	void ClosePosterFile();

private:
	void UpdateDuration(avf_u64_t timestamp_ms, avf_u32_t duration_ms);

private:
	IMediaFileWriter *mpWriter;
	IRecordObserver *mpObserver;
	IAVIO *mpPosterIO;
	ap<string_t> mPosterFilename;
	avf_time_t mPosterGenTime;

	CMsgQueue *mpQueue;
	CThread *mpThread;

	bool mbStarted;
	avf_pts_t mDurationMs;
	avf_pts_t mStartMs;
	avf_pts_t mNextMs;

private:
	struct FileInfo {
		char *pFileName;	// need free
		avf_size_t lengthMs;
	};

	int mIndex;
	avf_u32_t mSegLength_ms;

	FileInfo *mpFileList;
	avf_size_t mFileListSize;	// mSegments + 3 or 0
	avf_size_t mSegments;
	avf_size_t mNumFiles;
	avf_size_t mSequence;

private:
	bool mb_uploading;
	avf_u32_t m_upload_counter;
	avf_u64_t m_upload_start_pts;

private:
	void GetUploadingFilename(char *buffer, int len, avf_uint_t index);
	void StartUploading(CBuffer *pBuffer);
	void EndUploading(bool bLast);

private:
	struct FileNode {
		list_head_t list_node;
		char *pFileName;	// need free
		static FileNode *from_list_node(list_head_t *node) {
			return list_entry(node, FileNode, list_node);
		}
	};

	list_head_t m_list_del;	// to be deleted
};

#endif


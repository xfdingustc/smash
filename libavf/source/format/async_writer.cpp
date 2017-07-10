
#define LOG_TAG "async_writer"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "avio.h"
#include "file_io.h"
#include "buffer_io.h"
#include "sys_io.h"

#include "avf_util.h"
#include "avf_config.h"

#include "writer_list.h"
#include "async_writer.h"

//-----------------------------------------------------------------------
//
//  CAsyncWriter
//
//-----------------------------------------------------------------------
IMediaFileWriter* CAsyncWriter::Create(IEngine *pEngine, CConfigNode *node,
		int index, const char *avio)
{
	CAsyncWriter* result = avf_new CAsyncWriter(pEngine, node, index, avio);
	if (result) {
		result->RunThread();
	}
	CHECK_STATUS(result);
	return result;
}

CAsyncWriter::CAsyncWriter(IEngine *pEngine, CConfigNode *node, int index, const char *avio):
	inherited(pEngine),
	mpWriter(NULL),
	mpObserver(NULL),
	mpPosterIO(NULL),
	mpQueue(NULL),
	mpThread(NULL),

	mIndex(-1),
	mSegLength_ms(0),
	mpFileList(NULL),
	mFileListSize(0),
	mSegments(0),
	mNumFiles(0),
	mSequence(0),

	mb_uploading(false),
	m_upload_counter(0)
{
	list_init(m_list_del);

	mpObserver = GetRecordObserver();

	ap<string_t> muxer;

	if (node == NULL) {
		muxer = "mp4";
		mIndex = -1;
	} else {
		CConfigNode *pAttr = node->FindChild("format");
		muxer = pAttr ? pAttr->GetValue() : "mp4";

		pAttr = node->FindChild("index");
		mIndex = pAttr ? ::atoi(pAttr->GetValue()) : 0;
	}

	if (mIndex >= 0) {
		mSegments = mpEngine->ReadRegInt32(NAME_HLS_SEGMENTS, mIndex, VALUE_HLS_SEGMENTS);
		if (mSegments > 0) {
			mFileListSize = mSegments + 3;
			CREATE_OBJ(mpFileList = avf_malloc_array(FileInfo, mFileListSize));
		}
	}

	CREATE_OBJ(mpWriter = avf_create_file_writer(pEngine, muxer->string(), node, mIndex, avio));

	CREATE_OBJ(mpQueue = CMsgQueue::Create("async-writer", sizeof(cmd_t), 10));

	AVF_LOGD("AsyncWriter(%s,%d) created", muxer->string(), mIndex);
}

void CAsyncWriter::RunThread()
{
	CREATE_OBJ(mpThread = CThread::Create("AsyncWriter", ThreadEntry, this));

	if (mpEngine->IsRealTime()) {
		mpThread->SetThreadPriority(CThread::PRIO_LOW);
	}
}

CAsyncWriter::~CAsyncWriter()
{
	if (mpThread) {
		cmd_t cmd(CMD_QUIT);
		PostCmd(cmd);
		mpThread->Release();
	}

	avf_safe_release(mpQueue);
	avf_safe_release(mpWriter);
	avf_safe_release(mpPosterIO);

	for (avf_size_t i = 0; i < mNumFiles; i++) {
		avf_remove_file(mpFileList[i].pFileName, true);
		avf_safe_free(mpFileList[i].pFileName);
	}
	avf_safe_free(mpFileList);

	list_head_t *node;
	list_head_t *next;
	list_for_each_del(node, next, m_list_del) {
		FileNode *file = FileNode::from_list_node(node);
		avf_free(file->pFileName);
		avf_free(file);
	}
}

// synchronous
avf_status_t CAsyncWriter::StartWriter(bool bEnableRecord)
{
	cmd_t cmd(CMD_START_WRITER);
	cmd.u.StartWriter.bEnableRecord = bEnableRecord;
	return SendCmd(cmd);
}

void CAsyncWriter::DoStartWriter(bool bEnableRecord)
{
	mbStarted = false;
	mStartMs = 0;
	mNextMs = 0;
	mDurationMs = 0;
	avf_status_t status = mpWriter->StartWriter(bEnableRecord);
	ReplyCmd(status);
}

// synchronous
avf_status_t CAsyncWriter::EndWriter()
{
	return SendCmd(CMD_END_WRITER);
}

void CAsyncWriter::DoEndWriter()
{
	UpdatePlaylist(true);
	ReplyCmd(mpWriter->EndWriter());
}

avf_status_t CAsyncWriter::StartFile(string_t *video_name, avf_time_t video_time,
	string_t *picture_name, avf_time_t picture_time, string_t *basename)
{
	cmd_t cmd(CMD_START_FILE);
	cmd.u.StartFile.video_name = video_name ? video_name->acquire() : NULL;
	cmd.u.StartFile.video_time = video_time;
	cmd.u.StartFile.picture_name = picture_name ? picture_name->acquire() : NULL;
	cmd.u.StartFile.picture_time = picture_time;
	cmd.u.StartFile.basename = basename ? basename->acquire() : NULL;
	PostCmd(cmd);
	return E_OK;
}

void CAsyncWriter::DoStartFile(string_t *video_name, avf_time_t video_time,
	string_t *picture_name, avf_time_t picture_time, string_t *basename)
{
	if (mbEnableRecord) {
		avf_status_t status = mpWriter->StartFile(video_name, video_time, picture_name, picture_time, basename);
		if (status != E_OK) {
			PostFilterMsg(IEngine::MSG_ERROR, status);
			return;
		}

		AddToFileList(mpWriter->GetCurrentFile());

		if (mpObserver) {
			mpObserver->StartSegment(video_time, picture_time, mIndex);
		}
	}
}

const char *CAsyncWriter::GetCurrentFile()
{
	return NULL;
}

// fsize is ignored
avf_status_t CAsyncWriter::EndFile(avf_u32_t *fsize, bool bLast, avf_u64_t nextseg_ms)
{
	cmd_t cmd(CMD_END_FILE);
	cmd.u.EndFile.bLast = bLast;
	cmd.u.EndFile.nextseg_ms = nextseg_ms;
	PostCmd(cmd);
	return E_OK;
}

void CAsyncWriter::DoEndFile(bool bLast, avf_u64_t nextseg_ms)
{
	if (mbEnableRecord) {
		avf_u32_t fsize = 0;

		mpWriter->EndFile(&fsize, bLast, nextseg_ms);
		UpdatePlaylist(false);
		ClosePosterFile();

		if (mpObserver) {
			if (!bLast) {
				UpdateDuration(nextseg_ms, 0);
			}
			mpObserver->EndSegment(mNextMs - mStartMs, fsize, mIndex, bLast);
		}
	}
}

avf_status_t CAsyncWriter::SetRawData(avf_size_t stream, raw_data_t *raw)
{
	cmd_t cmd(CMD_SET_RAW_DATA);
	cmd.u.SetRawData.stream = stream;
	cmd.u.SetRawData.raw = raw->acquire();
	PostCmd(cmd);
	return E_OK;
}

void CAsyncWriter::DoSetRawData(avf_size_t stream, raw_data_t *raw)
{
	if (mbEnableRecord) {
		mpWriter->SetRawData(stream, raw);
	}
}

avf_status_t CAsyncWriter::ClearMediaFormats()
{
	return mpWriter->ClearMediaFormats();
}

avf_status_t CAsyncWriter::CheckMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	return mpWriter->CheckMediaFormat(stream, pMediaFormat);
}

avf_status_t CAsyncWriter::CheckStreams()
{
	return mpWriter->CheckStreams();
}

avf_status_t CAsyncWriter::SetStreamAttr(avf_stream_attr_s& stream_attr)
{
	return mpWriter->SetStreamAttr(stream_attr);
}

avf_status_t CAsyncWriter::SetMediaFormat(avf_size_t stream, CMediaFormat *pMediaFormat)
{
	return mpWriter->SetMediaFormat(stream, pMediaFormat);
}

avf_status_t CAsyncWriter::WriteSample(int stream, CBuffer* pBuffer)
{
	cmd_t cmd(CMD_WRITE_SAMPLE);
	cmd.u.WriteSample.stream = stream;
	cmd.u.WriteSample.pBuffer = pBuffer->acquire();
	PostCmd(cmd);
	return E_OK;
}

void CAsyncWriter::UpdateDuration(avf_u64_t timestamp_ms, avf_u32_t duration_ms)
{
	if (!mbStarted) {
		mbStarted = true;
		mStartMs = timestamp_ms;
		mNextMs = timestamp_ms;
		mDurationMs = duration_ms;
	} else {
		mNextMs = timestamp_ms;
		avf_pts_t tmp = (timestamp_ms - mStartMs) + duration_ms;
		if (mDurationMs < tmp)
			mDurationMs = tmp;
	}
}

void CAsyncWriter::DoWriteSample(avf_size_t stream, CBuffer *pBuffer)
{
	if (stream == 0) {	// todo - video stream
		UpdateDuration(pBuffer->pts_to_ms(pBuffer->m_pts), pBuffer->pts_to_ms(pBuffer->m_duration));
	}

	mpWriter->WriteSample(stream, pBuffer);
}

avf_status_t CAsyncWriter::WritePoster(string_t *filename, avf_time_t gen_time, CBuffer* pBuffer)
{
	cmd_t cmd(CMD_WRITE_POSTER);
	cmd.u.WritePoster.filename = filename->acquire();
	cmd.u.WritePoster.gen_time = gen_time;
	cmd.u.WritePoster.pBuffer = pBuffer->acquire();
	PostCmd(cmd);
	return E_OK;
}

void CAsyncWriter::DoWritePoster(string_t *filename, avf_time_t gen_time, CBuffer *pBuffer)
{
	if (mbEnableRecord) {
		if (mpPosterIO == NULL) {
			if ((mpPosterIO = CBufferIO::Create()) != NULL) {	// todo
				mPosterGenTime = gen_time;
				mPosterFilename = ap<string_t>(filename, ".jpg");
				mpPosterIO->CreateFile(mPosterFilename->string());
				AVF_LOGD("Create poster file: %s", mPosterFilename->string());
			}
		}

		if (mpPosterIO) {
			mpPosterIO->Write(pBuffer->GetData(), pBuffer->GetDataSize());
			if (pBuffer->mpNext)
				mpPosterIO->Write(pBuffer->mpNext->GetData(), pBuffer->mpNext->GetDataSize());
		}
	}
}

void CAsyncWriter::ClosePosterFile()
{
	if (mpPosterIO) {
		avf_safe_release(mpPosterIO);
		ModifyFileTime(mPosterFilename->string(), mPosterGenTime);
	}
}

bool CAsyncWriter::ProvideSamples()
{
	return mpWriter->ProvideSamples();
}

ISampleProvider* CAsyncWriter::CreateSampleProvider(avf_size_t before, avf_size_t after)
{
	cmd_t cmd(CMD_CREATE_SAMPLE_PROVIDER);
	ISampleProvider *pProvider = NULL;
	cmd.u.CreateSampleProvider.before = before;
	cmd.u.CreateSampleProvider.after = after;
	cmd.u.CreateSampleProvider.ppProvider = &pProvider;
	SendCmd(cmd);
	return pProvider;
}

void CAsyncWriter::ThreadEntry(void *p)
{
	((CAsyncWriter*)p)->ThreadLoop();
}

void CAsyncWriter::AddToFileList(const char *filename)
{
	if (mSegments == 0)
		return;

	if (mNumFiles == mFileListSize) {
		// queue is full, delete the oldest file
		if (!avf_remove_file(mpFileList[0].pFileName, true)) {
			FileNode *file = avf_malloc_type(FileNode);
			file->pFileName = mpFileList[0].pFileName;
			list_add_tail(&file->list_node, m_list_del);
		} else {
			avf_free(mpFileList[0].pFileName);
		}

		for (avf_size_t i = 0; i < mFileListSize - 1; i++) {
			mpFileList[i] = mpFileList[i + 1];
		}
	} else {
		mNumFiles++;
	}

	FileInfo *pFileInfo = mpFileList + (mNumFiles - 1);
	pFileInfo->pFileName = avf_strdup(filename);
	pFileInfo->lengthMs = 0;	// patch later

	mSequence++;

	// try to delete the files that could not be deleted
	list_head_t *node;
	list_head_t *next;
	list_for_each_del(node, next, m_list_del) {
		FileNode *file = FileNode::from_list_node(node);
		if (avf_remove_file(file->pFileName, true)) {
			list_del(node);
			avf_free(file->pFileName);
			avf_free(file);
		}
	}
}

void CAsyncWriter::UpdatePlaylist(bool bEnd)
{
	char buffer[512];
	char reg[REG_STR_MAX];
	FileInfo *pFileInfo;
	IAVIO *io;
	avf_size_t from;
	avf_size_t to;	// exclusive
	avf_size_t sequence;

	if (mNumFiles == 0)
		return;

	// patch length
	pFileInfo = mpFileList + (mNumFiles - 1);
	pFileInfo->lengthMs = mDurationMs;
	if (mSegLength_ms == 0) {
		mSegLength_ms = pFileInfo->lengthMs;
	}

	if (mNumFiles <= mSegments) {
		from = 0;
		to = mNumFiles;
		sequence = mSequence - mNumFiles;
	} else {
		from = mNumFiles - mSegments;
		to = mNumFiles;
		sequence = mSequence - mSegments;
	}

	io = CFileIO::Create();
	if (io) {
		mpEngine->ReadRegString(NAME_HLS_M3U8, mIndex, reg, VALUE_HLS_M3U8);
		io->CreateFile(reg);

		io->write_string("#EXTM3U\n");

		sprintf(buffer, "#EXT-X-TARGETDURATION:%d\n", mSegLength_ms/1000);
		io->write_string(buffer);

		io->write_string("#EXT-X-VERSION:3\n");

		sprintf(buffer, "#EXT-X-MEDIA-SEQUENCE:%d\n", sequence);
		io->write_string(buffer);

		for (avf_size_t i = from; i < to; i++) {
			pFileInfo = mpFileList + i;

			int seconds_10 = (pFileInfo->lengthMs + 50) / 100;
			sprintf(buffer, "#EXTINF:%d.%d,\n", seconds_10/10, seconds_10%10);
			io->write_string(buffer);

			const char *filename = avf_extract_filename(pFileInfo->pFileName);
			io->write_string(filename);
			io->write_string("\n");
		}

		if (bEnd) {
			io->write_string("#EXT-X-ENDLIST\n");
		}

		io->Release();
	}
}

void CAsyncWriter::ThreadLoop()
{
	while (true) {
		cmd_t cmd;
		mpQueue->GetMsg(&cmd, sizeof(cmd));

		switch (cmd.cmd_code) {
		case CMD_QUIT:
			return;

		case CMD_START_WRITER:
			mbEnableRecord = cmd.u.StartWriter.bEnableRecord;
			DoStartWriter(mbEnableRecord);
			break;

		case CMD_END_WRITER:
			mbEnableRecord = false;
			DoEndWriter();
			break;

		case CMD_START_FILE:
			DoStartFile(
				cmd.u.StartFile.video_name,
				cmd.u.StartFile.video_time,
				cmd.u.StartFile.picture_name,
				cmd.u.StartFile.picture_time,
				cmd.u.StartFile.basename);
			avf_safe_release(cmd.u.StartFile.video_name);
			avf_safe_release(cmd.u.StartFile.picture_name);
			avf_safe_release(cmd.u.StartFile.basename);
			break;

		case CMD_END_FILE:
			DoEndFile(cmd.u.EndFile.bLast, cmd.u.EndFile.nextseg_ms);
			break;

		case CMD_SET_RAW_DATA:
			DoSetRawData(cmd.u.SetRawData.stream, cmd.u.SetRawData.raw);
			cmd.u.SetRawData.raw->Release();
			break;

		case CMD_WRITE_SAMPLE:
			DoWriteSample(cmd.u.WriteSample.stream, cmd.u.WriteSample.pBuffer);
			cmd.u.WriteSample.pBuffer->Release();
			break;

		case CMD_WRITE_POSTER:
			if (mpObserver) {
				if (mbEnableRecord) {
					mpObserver->AddPicture(cmd.u.WritePoster.pBuffer, mIndex);
				}
			} else {
				DoWritePoster(cmd.u.WritePoster.filename,
					cmd.u.WritePoster.gen_time,
					cmd.u.WritePoster.pBuffer);
			}

			// upload
			if (mpEngine->ReadRegBool(NAME_UPLOAD_PICTURE, 0, VALUE_UPLOAD_PICTURE)) {
				if (mb_uploading) {
					// continue uploading: start a new file
					EndUploading(false);
					StartUploading(cmd.u.WritePoster.pBuffer);
				} else {
					// start uploading
					m_upload_counter = 1;
					m_upload_start_pts = cmd.u.WritePoster.pBuffer->m_pts;
					StartUploading(cmd.u.WritePoster.pBuffer);
				}

			} else {

				if (mb_uploading) {
					// stop uploading
					EndUploading(true);
				} else {
					// do nothing
				}
			}

			cmd.u.WritePoster.filename->Release();
			cmd.u.WritePoster.pBuffer->Release();

			break;

		case CMD_CREATE_SAMPLE_PROVIDER: {
				avf_size_t before = cmd.u.CreateSampleProvider.before;
				avf_size_t after = cmd.u.CreateSampleProvider.after;
				ISampleProvider *pProvider = mpWriter->CreateSampleProvider(before, after);
				*cmd.u.CreateSampleProvider.ppProvider = pProvider;
				mpQueue->ReplyMsg(E_OK);
			}
			break;

		default:
			AVF_LOGE("Unknown cmd: %d", cmd.cmd_code);
			break;
		}
	}
}

void CAsyncWriter::GetUploadingFilename(char *buffer, int len, avf_uint_t index)
{
	char path[REG_STR_MAX];
	mpEngine->ReadRegString(NAME_UPLOAD_PATH_PICTURE, 0, path, VALUE_UPLOAD_PATH_PICTURE);
	avf_snprintf(buffer, len, "%s%d.jpg", path, index);
	buffer[len-1] = 0;
}

void CAsyncWriter::StartUploading(CBuffer *pBuffer)
{
	mb_uploading = true;

	char filename[256];
	GetUploadingFilename(with_size(filename), m_upload_counter);

	IAVIO *io = CSysIO::Create();
	if (io->CreateFile(filename) != E_OK) {
		AVF_LOGE("cannot create %s", filename);
		io->Release();
		return;
	}

	avf_u32_t size = pBuffer->GetDataSize();
	size += pBuffer->mpNext ? pBuffer->mpNext->GetDataSize() : 0;

	upload_header_v2_t header;
	header.u_data_type = VDB_DATA_JPEG;
	header.u_data_size = size;
	header.u_timestamp = (pBuffer->m_pts - m_upload_start_pts) * 1000 / pBuffer->GetTimeScale();
	header.u_stream = 0;
	header.u_duration = 0;
	header.u_version = UPLOAD_VERSION_v2;
	header.u_flags = 0;
	header.u_param1 = 0;
	header.u_param2 = 0;
	io->Write(&header, sizeof(header));

	io->Write(pBuffer->GetData(), pBuffer->GetDataSize());
	CBuffer *pNext = pBuffer->mpNext;
	if (pNext) {
		io->Write(pNext->GetData(), pNext->GetDataSize());
	}

	set_upload_end_header_v2(&header, sizeof(upload_header_v2_t) + size);
	io->Write(&header, sizeof(header));

	io->Release();

	// notify app
	mpEngine->WriteRegString(NAME_UPLOAD_PICTURE_PREV, 0, filename);
	mpEngine->WriteRegInt32(NAME_UPLOAD_PICTURE_INDEX, 0, m_upload_counter);
	PostFilterMsg(IEngine::MSG_UPLOAD_PICTURE);
}

void CAsyncWriter::EndUploading(bool bLast)
{
	m_upload_counter++;

	// remove oldest file
	if (m_upload_counter > 10) {
		char filename[256];
		GetUploadingFilename(with_size(filename), m_upload_counter - 10);
		avf_remove_file(filename, false);
	}

	if (bLast) {
		mb_uploading = false;

		/// todo - write end flag
	}
}



#include <fcntl.h>
#include <sys/ioctl.h>
#include "ipc_drv.h"

//-----------------------------------------------------------------------
//
//  CRawReg
//
//-----------------------------------------------------------------------
class CRawReg: public CObject
{
public:
	static CRawReg* Create(CSubtitleFilter *pOwner);

private:
	CRawReg(CSubtitleFilter *pOwner);
	virtual ~CRawReg();

public:
	avf_status_t AddEntryByConfig(const char *str);
	avf_status_t StartThread();
	avf_status_t StopThread();
	void ReleaseThread();

public:
	raw_data_t *ReadReg(uint32_t fcc, const char *name, int index);

private:
	static void ThreadEntry(void *p);
	void ThreadLoop();

private:
	struct reg_item_s {
		uint32_t fcc;
		char *name;
		int index;
		ipc_reg_id_t reg_id;
	};

private:
	CSubtitleFilter *mpOwner;
	CThread *mpThread;
	int m_fd;

private:
	avf_status_t EnableReg(int enable);

	INLINE uint64_t ts_to_ticks(const struct timespec& ts) {
		return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;
	}
};

//-----------------------------------------------------------------------
//
//  CRawReg
//
//-----------------------------------------------------------------------
CRawReg* CRawReg::Create(CSubtitleFilter *pOwner)
{
	CRawReg *result = avf_new CRawReg(pOwner);
	CHECK_STATUS(result);
	return result;
}

CRawReg::CRawReg(CSubtitleFilter *pOwner):
	mpOwner(pOwner),
	mpThread(NULL),
	m_fd(-1)
{
	m_fd = avf_open_file("/dev/ipc", O_RDWR, 0);
	if (m_fd < 0) {
		AVF_LOGP("/dev/ipc");
		mStatus = E_UNKNOWN;
	}
}

CRawReg::~CRawReg()
{
	StopThread();
	ReleaseThread();
	avf_safe_close(m_fd);
}

// "fcc"
avf_status_t CRawReg::AddEntryByConfig(const char *str)
{
	if (strlen(str) != 4)
		return E_INVAL;

	uint32_t fcc = MKFCC(str[0], str[1], str[2], str[3]);

	// create reg_id
	
	ipc_query_reg_id_t param;

	param.type = fcc;
	param.name = (char*)"";
	param.name_length = 0;
	param.index = 0;
	param.options = REG_ID_OPT_CREATE;
	param.reg_id = INVALID_IPC_REG_ID;

	if (ioctl(m_fd, IPC_REG_QUERY_ID, &param) == 0) {
		AVF_LOGD("reg created: %s", str);
		return E_OK;
	} else {
		AVF_LOGD("create reg '%s' failed");
		return E_ERROR;
	}
}

avf_status_t CRawReg::StartThread()
{
	AVF_ASSERT(mpThread == NULL);

	EnableReg(1);
	mpThread = CThread::Create("rawreg", ThreadEntry, (void*)this);

	if (mpThread == NULL) {
		AVF_LOGP("cannot create thread rawreg");
		EnableReg(0);
		return E_ERROR;
	}

	mpThread->SetThreadPriority(CThread::PRIO_MANAGER);
	AVF_LOGD("start rawreg thread");

	return E_OK;
}

avf_status_t CRawReg::StopThread()
{
	return EnableReg(0);
}

void CRawReg::ReleaseThread()
{
	avf_safe_release(mpThread);
}

void CRawReg::ThreadEntry(void *p)
{
	CRawReg *self = (CRawReg*)p;
	self->ThreadLoop();
}

void CRawReg::ThreadLoop()
{
	avf_u8_t buf[MAX_REG_DATA];

	for (;;) {
		ipc_reg_read_write_t rw;

		rw.reg_id = INVALID_IPC_REG_ID;
		rw.ptr = (void*)buf;
		rw.size = MAX_REG_DATA;
		rw.options = 0;
		rw.timeout = -1;

		if (ioctl(m_fd, IPC_REG_READ_ANY, &rw) < 0) {
			AVF_LOGD("IPC_REG_READ_ANY : %d", errno);
			break;
		}

		if (rw.size > IPC_REG_READ_ANY) {
			AVF_LOGW("IPC_REG_READ_ANY returns %d bytes", rw.size);
			continue;
		}

		mpOwner->OnRawData(rw.type, buf, rw.size, ts_to_ticks(rw.ts_write));
	}
}

raw_data_t *CRawReg::ReadReg(uint32_t fcc, const char *name, int index)
{
	ipc_query_reg_id_t param;

	param.type = fcc;
	param.name = (char*)name;
	param.name_length = name ? strlen(name) : 0;
	param.index = index;
	param.options = 0;
	param.reg_id = INVALID_IPC_REG_ID;

	if (ioctl(m_fd, IPC_REG_QUERY_ID, &param) < 0)
		return NULL;

	avf_u8_t buf[MAX_REG_DATA];
	ipc_reg_read_write_t rw;

	rw.reg_id = param.reg_id;
	rw.ptr = (void*)buf;
	rw.size = MAX_REG_DATA;
	rw.options = REG_OPT_QUICK_READ;
	rw.timeout = -1;

	if (ioctl(m_fd, IPC_REG_READ, &rw) < 0) {
		return NULL;
	}

	raw_data_t *raw = avf_alloc_raw_data(rw.size, fcc);
	if (raw) {
		memcpy(raw->GetData(), buf, rw.size);
	}

	return raw;
}

avf_status_t CRawReg::EnableReg(int enable)
{
	int32_t value = enable;
	if (ioctl(m_fd, IPC_REG_ENABLE, &value) < 0) {
		AVF_LOGP("IPC_REG_ENABLE");
		return E_ERROR;
	}
	return E_OK;
}



#define LOG_TAG "avio"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "avio.h"
#include "avf_util.h"

CMutex CIOLock::m_io_lock;
IDiskNotify *CIOLock::m_inotify;
avf_uint_t CIOLock::m_timeout_ticks = 1000;
CDiskMonitor *CDiskMonitor::g_disk_monitor;

//#define NAME_DISK_MONITOR		"config.diskmonitor.params"

//#define VALUE_DISK_MONITOR		"0:60000,1000,20000"	// method:period,timeout,total_timeout
//	method 0 - in 60s, sum(write-time >= 1s) >= 20s

//#define VALUE_DISK_MONITOR		"1:60000,1000,10"		// method:period,timeout,num_timeouts
//	method 1 - in 60s, total(write-time >= 1s) >= 10 (times)

//-----------------------------------------------------------------------
//
//  CIOLock
//
//-----------------------------------------------------------------------
CIOLock::CIOLock(int reason, const char *action, int line):
	m_reason(reason),
	m_action(action),
	m_line(line)
{
	if (m_reason & LOCK_ALLOWED) {
		m_io_lock.Lock();
		m_tick = avf_get_curr_tick();
	}
}

CIOLock::~CIOLock()
{
	if (m_reason & LOCK_ALLOWED) {
		avf_u64_t ticks = avf_get_curr_tick() - m_tick;
		m_io_lock.Unlock();

		if (ticks >= m_timeout_ticks) {
			AVF_LOGW(C_LIGHT_PURPLE "%s costs " LLD " ms (line %d)" C_NONE, m_action, ticks, m_line);
			if (CDiskMonitor::g_disk_monitor) {
				CDiskMonitor::g_disk_monitor->Add(m_tick, ticks);
			}
		}
	}
}

void CIOLock::SetDiskNotify(IDiskNotify *inotify)
{
	AUTO_LOCK(m_io_lock);
	m_inotify = inotify;
}

// caller holds the lock
void CIOLock::DiskError()
{
	if (m_inotify) {
		m_inotify->OnDiskError();
	}
}

// caller holds the lock
void CIOLock::DiskSlow()
{
	if (m_inotify) {
		m_inotify->OnDiskSlow();
	}
}

CDiskMonitor::CDiskMonitor(int method, unsigned period, unsigned timeout, unsigned value)
{
	mStatus = E_OK;

	list_init(m_time_list);
	m_num_items = 0;

	m_method = method;
	m_period = period;
	m_value = value;

	m_timeout_ticks_saved = CIOLock::m_timeout_ticks;
	CIOLock::m_timeout_ticks = timeout;
}

CDiskMonitor::~CDiskMonitor()
{
	CIOLock::m_timeout_ticks = m_timeout_ticks_saved;

	list_head_t *node;
	list_head_t *next;
	time_item_s *item;

	list_for_each_del(node, next, m_time_list) {
		item = time_item_s::from_list_node(node);
		__list_del(node);
		avf_free(item);
		m_num_items--;
	}
}

void CDiskMonitor::Add(avf_u64_t tick, avf_u64_t ticks)
{
	avf_u64_t now = tick + ticks;

	// remove old ones
	list_head_t *node;
	list_head_t *next;
	time_item_s *item;

	list_for_each_del(node, next, m_time_list) {
		item = time_item_s::from_list_node(node);
		if (item->tick + item->ticks + m_period <= now) {
			__list_del(node);
			avf_free(item);
			m_num_items--;
		}
	}

	// add this one
	item = avf_malloc_type(time_item_s);
	item->tick = tick;
	item->ticks = ticks;
	list_add_tail(&item->list_node, m_time_list);
	m_num_items++;

	AVF_LOGD("there are %d timeout items", m_num_items);

	// check
	if (m_method == 0 || m_method == 2) {	// check total
		avf_u64_t total_timeout = 0;
		list_for_each(node, m_time_list) {
			item = time_item_s::from_list_node(node);
			if (m_method == 0) {
				total_timeout += item->ticks;
			} else {
				total_timeout += (item->ticks > CIOLock::m_timeout_ticks) ?
					item->ticks - CIOLock::m_timeout_ticks : 0;
			}
		}

		AVF_LOGW("total timeout " LLD " (limit %d) in period %d", total_timeout, m_value, m_period);
		if (total_timeout >= m_value) {
			CIOLock::DiskSlow();
		}
		return;
	}

	if (m_method == 1) {
		AVF_LOGW("total %d (limit %d) timeouts in period %d", m_num_items, m_value, m_period);
		if (m_num_items >= m_value) {
			CIOLock::DiskSlow();
		}
		return;
	}
}

avf_status_t CDiskMonitor::Init(int method, unsigned period, unsigned timeout, unsigned value)
{
	AUTO_LOCK(CIOLock::m_io_lock);
	avf_safe_delete(g_disk_monitor);
	g_disk_monitor = avf_new CDiskMonitor(method, period, timeout, value);
	CHECK_STATUS(g_disk_monitor);
	return g_disk_monitor ? E_OK : E_NOMEM;
}

void CDiskMonitor::Release()
{
	AUTO_LOCK(CIOLock::m_io_lock);
	avf_safe_delete(g_disk_monitor); 
}

//-----------------------------------------------------------------------
//
//  CAVIO, IAVIO
//
//-----------------------------------------------------------------------

void *CAVIO::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_IAVIO)
		return static_cast<IAVIO*>(this);
	return inherited::GetInterface(refiid);
}

#define AVIO_CP_SIZE	(16*KB)
avf_status_t CAVIO::CopyFile(IAVIO *input, avf_u32_t size, IAVIO *output)
{
	avf_u8_t *mem = avf_malloc_bytes(AVIO_CP_SIZE);
	if (mem == NULL)
		return E_NOMEM;

	avf_status_t status;
	while (size > 0) {
		avf_uint_t tocopy = MIN(size, AVIO_CP_SIZE);
		if (input->Read(mem, tocopy) != (int)tocopy) {
			status = E_IO;
			goto Done;
		}
		if (output->Write(mem, tocopy) != (int)tocopy) {
			status = E_IO;
			goto Done;
		}
		size -= tocopy;
	}
	status = E_OK;

Done:
	avf_free(mem);
	return status;
}

//-----------------------------------------------------------------------
//
//  CShareReadIO
//
//-----------------------------------------------------------------------
CShareReadIO* CShareReadIO::Create(IAVIO *io)
{
	CShareReadIO *result = avf_new CShareReadIO(io);
	CHECK_STATUS(result);
	return result;
}

CShareReadIO::CShareReadIO(IAVIO *io)
{
	io->AddRef();
	mp_io = io;
	m_pos = 0;
	m_size = io->GetSize();
}

CShareReadIO::~CShareReadIO()
{
	avf_safe_release(mp_io);
}

int CShareReadIO::Read(void *buffer, avf_size_t size)
{
	int rval = mp_io->ReadAt(buffer, size, m_pos);
	if (rval >= 0) {
		m_pos += rval;
	}
	return rval;
}

avf_status_t CShareReadIO::Seek(avf_s64_t pos, int whence)
{
	avf_u64_t new_file_pos;

	switch (whence) {
	case kSeekSet:
		new_file_pos = pos;
		break;
	case kSeekCur:
		new_file_pos = m_pos + pos;
		break;
	case kSeekEnd:
		new_file_pos = m_pos + pos;
		break;
	default:
		AVF_LOGE("bad seek %d", whence);
		return E_INVAL;
	}

	if (new_file_pos > m_size) {
		AVF_LOGE("cannot seek to " LLD ", filesize: " LLD, new_file_pos, m_size);
		return E_INVAL;
	}

	m_pos = new_file_pos;

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CNullIO
//
//-----------------------------------------------------------------------
IAVIO* CNullIO::Create()
{
	CNullIO* result = avf_new CNullIO();
	CHECK_STATUS(result);
	return result;
}

CNullIO::CNullIO():
	m_file_pos(0),
	m_file_size(0)
{
}

CNullIO::~CNullIO()
{
}

avf_status_t CNullIO::CreateFile(const char *url)
{
	return E_OK;
}

avf_status_t CNullIO::OpenRead(const char *url)
{
	return E_OK;
}

avf_status_t CNullIO::OpenWrite(const char *url)
{
	return E_OK;
}

avf_status_t CNullIO::OpenExisting(const char *url)
{
	return E_OK;
}

avf_status_t CNullIO::Close()
{
	return E_OK;
}

bool CNullIO::IsOpen()
{
	return false;
}

int CNullIO::Read(void *buffer, avf_size_t size)
{
	return E_PERM;
}

int CNullIO::Write(const void *buffer, avf_size_t size)
{
	m_file_pos += size;

	if (m_file_pos > m_file_size)
		m_file_size = m_file_pos;

	return size;
}

avf_status_t CNullIO::Flush()
{
	return E_OK;
}

avf_status_t CNullIO::Seek(avf_s64_t pos, int whence)
{
	avf_u64_t new_file_pos;

	switch (whence) {
	case kSeekSet:
		new_file_pos = pos;
		break;
	case kSeekCur:
		new_file_pos = m_file_pos + pos;
		break;
	case kSeekEnd:
		new_file_pos = m_file_size + pos;
		break;
	default:
		AVF_LOGE("bad seek %d", whence);
		return E_INVAL;
	}

	m_file_pos = new_file_pos;
	if (new_file_pos > m_file_size)
		m_file_size = new_file_pos;

	return E_OK;
}

avf_u64_t CNullIO::GetSize()
{
	return m_file_size;
}

avf_u64_t CNullIO::GetPos()
{
	return m_file_pos;
}

//-----------------------------------------------------------------------
//
//  CWriteBuffer
//
//-----------------------------------------------------------------------

CWriteBuffer::CWriteBuffer(IAVIO *io, avf_uint_t buffer_size)
{
	mpIO = io;

	m_buffer = avf_malloc_bytes(buffer_size);
	m_buffer_size = buffer_size;

	m_ptr = m_buffer;
	m_remain = m_buffer_size;
}

CWriteBuffer::~CWriteBuffer()
{
	if (mpIO) Flush();
	avf_free(m_buffer);
}

void CWriteBuffer::Flush()
{
	avf_uint_t size = m_buffer_size - m_remain;
	if (size > 0) {
		if (mpIO->Write(m_buffer, size) != (int)size) {
			AVF_LOGE("CWriteBuffer::Flush() failed!");
		}
		m_ptr = m_buffer;
		m_remain = m_buffer_size;
	}
}

void CWriteBuffer::write_be_64(avf_u64_t val)
{
	if (m_remain < 8) {
		Flush();
	}

	avf_u32_t hi = (avf_u32_t)(val >> 32);
	avf_u32_t lo = (avf_u32_t)(val);

	m_ptr[0] = hi >> 24;
	m_ptr[1] = hi >> 16;
	m_ptr[2] = hi >> 8;
	m_ptr[3] = hi;

	m_ptr[4] = lo >> 24;
	m_ptr[5] = lo >> 16;
	m_ptr[6] = lo >> 8;
	m_ptr[7] = lo;

	m_ptr += 8;
	m_remain -= 8;
}

void CWriteBuffer::write_le_64(avf_u64_t val)
{
	if (m_remain < 8)
		Flush();

	avf_u32_t hi = (avf_u32_t)(val >> 32);
	avf_u32_t lo = (avf_u32_t)(val);

	m_ptr[0] = lo;
	m_ptr[1] = lo >> 8;
	m_ptr[2] = lo >> 16;
	m_ptr[3] = lo >> 24;

	m_ptr[4] = hi;
	m_ptr[5] = hi >> 8;
	m_ptr[6] = hi >> 16;
	m_ptr[7] = hi >> 24;

	m_ptr += 8;
	m_remain -= 8;
}

void CWriteBuffer::awrite_le_64(avf_u64_t val)
{
	if (m_remain < 8)
		Flush();

	avf_u32_t hi = (avf_u32_t)(val >> 32);
	avf_u32_t lo = (avf_u32_t)(val);

	*(avf_u32_t*)(m_ptr) = lo;
	*(avf_u32_t*)(m_ptr + 4) = hi;

	m_ptr += 8;
	m_remain -= 8;
}

void CWriteBuffer::write_mem(const avf_u8_t *p, avf_uint_t size)
{
	if (m_remain < size)
		Flush();

	if (m_remain < size) {
		mpIO->Write(p, size);
	} else {
		::memcpy(m_ptr, p, size);
		m_ptr += size;
		m_remain -= size;
	}
}

void CWriteBuffer::write_zero(avf_uint_t count)
{
	while (count) {
		if (m_remain == 0)
			Flush();

		avf_uint_t towrite = MIN(count, m_remain);

		::memset(m_ptr, 0, towrite);
		m_ptr += towrite;
		m_remain -= towrite;

		count -= towrite;
	}
}

//-----------------------------------------------------------------------
//
//  CReadBuffer
//
//-----------------------------------------------------------------------

CReadBuffer::CReadBuffer(IAVIO *io, avf_uint_t buffer_size)
{
	mpIO = io;
	m_error = E_OK;

	m_buffer = avf_malloc_bytes(buffer_size);
	m_buffer_size = buffer_size;

	m_ptr = m_buffer;
	m_remain = 0;
}

CReadBuffer::~CReadBuffer()
{
	avf_free(m_buffer);
}

avf_status_t CReadBuffer::SeekTo(avf_u64_t pos)
{
	avf_u64_t curr_pos = mpIO->GetPos();

	if (pos > curr_pos || pos + m_remain < curr_pos) {
		avf_status_t status = mpIO->Seek(pos);
		if (status != E_OK) {
			m_error = status;
			return status;
		}

		m_ptr = m_buffer;
		m_remain = 0;

		return status;
	}

	// inside the buffer
	avf_size_t skip = m_remain - (avf_size_t)(curr_pos - pos);
	m_ptr += skip;
	m_remain -= skip;

	return E_OK;
}

void CReadBuffer::DoRefill(int min_bytes)
{
	if (m_remain) {
		::memmove(m_buffer, m_ptr, m_remain);
	}
	m_ptr = m_buffer;

	// fill 0 when error happened
	if (IOError()) {
Error:
		avf_size_t tofill = m_buffer_size - m_remain;
		::memset(m_buffer + m_remain, 0, tofill);
		m_remain += tofill;
		return;
	}

	int bytes = mpIO->Read(m_buffer + m_remain, m_buffer_size - m_remain);
	if (bytes < 0 || bytes + (int)m_remain < min_bytes) {
		AVF_LOGE("DoRefill() failed to read %d bytes, returned %d", min_bytes, bytes);
		m_error = E_IO;
		if (bytes >= 0) {
			m_remain += bytes;
		}
		goto Error;
	}

	m_remain += bytes;
}

void CReadBuffer::DoSkip(avf_uint_t size)
{
	mpIO->Seek(size - m_remain, IAVIO::kSeekCur);
	m_ptr = m_buffer;
	m_remain = 0;
}

avf_u64_t CReadBuffer::read_le_64()
{
	Refill(8);
	avf_u32_t lo = __read_le_32();
	avf_u32_t hi = __read_le_32();
	return MK_U64(lo, hi);
}

avf_u64_t CReadBuffer::aread_le_64()
{
	Refill(8);
	avf_u32_t lo = __aread_le_32();
	avf_u32_t hi = __aread_le_32();
	return MK_U64(lo, hi);
}

bool CReadBuffer::read_mem(avf_u8_t *p, avf_uint_t size)
{
	if (IOError()) {
		::memset(p, 0, size);
		return false;
	}

	// copy from buffer
	avf_size_t tocopy = MIN(size, m_remain);
	if (tocopy > 0) {
		::memcpy(p, m_ptr, tocopy);

		m_ptr += tocopy;
		m_remain -= tocopy;

		p += tocopy;
		size -= tocopy;
	}

	// read remaining from IO
	if (size > 0) {
		int bytes = mpIO->Read(p, size);
		if (bytes < 0 || bytes != (int)size) {
			AVF_LOGE("%s::Read() failed, requested: %d, returned: %d", mpIO->GetIOType(), size, bytes);
			AVF_LOGE("io pos: %d, size: %d", (int)mpIO->GetPos(), (int)mpIO->GetSize());
			::memset(p, 0, size);
			m_error = E_IO;
			return false;
		}
	}

	return true;
}


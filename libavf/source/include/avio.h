
#ifndef __AVIO_H__
#define __AVIO_H__

class CIOLock;
class CDiskMonitor;

class IAVIO;
class IBlockIO;
class ITSMapInfo;

class CAVIO;
class CShareReadIO;
class CNullIO;

class CWriteBuffer;
class CReadBuffer;

#ifdef LINUX_OS
#define ENABLE_IO_LOCK
#else
#undef ENABLE_IO_LOCK
#endif

#define VDB_CLIPS_PROT		"vdb://clips"

//-----------------------------------------------------------------------
//
//  CIOLock
//
//-----------------------------------------------------------------------
class CIOLock
{
	friend class CDiskMonitor;

public:
	CIOLock(int reason, const char *action, int line);
	~CIOLock();

public:
	static void SetDiskNotify(IDiskNotify *inotify);
	static void DiskError();
	static void DiskSlow();

private:
	static CMutex m_io_lock;
	static IDiskNotify *m_inotify;
	static avf_uint_t m_timeout_ticks;

private:
	int const m_reason;
	const char *const m_action;
	int const m_line;
	avf_u64_t m_tick;	// for debug
};

#define LOCK_FOR_WRITE			(1 << 0)
#define LOCK_FOR_CREATE_FILE	(1 << 1)
#define LOCK_FOR_CREATE_DIR		(1 << 2)
#define LOCK_FOR_REMOVE_FILE	(1 << 3)
#define LOCK_FOR_REMOVE_DIR		(1 << 4)

#define LOCK_ALLOWED \
	(LOCK_FOR_WRITE|LOCK_FOR_CREATE_FILE|LOCK_FOR_CREATE_DIR|LOCK_FOR_REMOVE_FILE|LOCK_FOR_REMOVE_DIR)

#ifdef ENABLE_IO_LOCK

#define __LOCK_IO__(_reason, _action) \
	CIOLock __io_lock(_reason, _action, __LINE__)

#define __IO_ERROR(_msg) \
	CIOLock::DiskError()

#define __IO_SLOW(_msg) \
	CIOLock::DiskSlow()

#else

#define __LOCK_IO__(_reason, _action)
#define __IO_ERROR(_msg)
#define __IO_SLOW(_msg)

#endif


class CDiskMonitor
{
	friend class CIOLock;

private:
	CDiskMonitor(int method, unsigned period, unsigned timeout, unsigned value);
	~CDiskMonitor();

public:
	static avf_status_t Init(int method, unsigned period, unsigned timeout, unsigned value);
	static void Release();

private:
	static CDiskMonitor *g_disk_monitor;
	avf_status_t Status() { return mStatus; }
	void Add(avf_u64_t tick, avf_u64_t ticks);

private:
	struct time_item_s {
		list_head_t list_node;
		avf_u64_t tick;
		avf_u64_t ticks;
		static time_item_s *from_list_node(list_head_t *node) {
			return container_of(node, time_item_s, list_node);
		}
	};

private:
	avf_status_t mStatus;
	int m_method;
	avf_uint_t m_period;
	avf_uint_t m_value;
	avf_uint_t m_timeout_ticks_saved;
	list_head_t m_time_list;
	avf_uint_t m_num_items;
};

//-----------------------------------------------------------------------
//
//  IAVIO
//
//-----------------------------------------------------------------------

class IAVIO: public IInterface
{
public:
	enum {
		kSeekSet = 0,	// from beginning, SEEK_SET
		kSeekCur = 1,	// from current, SEEK_CUR
		kSeekEnd = 2,	// from end, SEEK_END
	};

public:
	DECLARE_IF(IAVIO, IID_IAVIO);

	virtual avf_status_t CreateFile(const char *url) = 0;
	virtual avf_status_t OpenRead(const char *url) = 0;
	virtual avf_status_t OpenWrite(const char *url) = 0;
	virtual avf_status_t OpenExisting(const char *url) = 0;
	virtual avf_status_t Close() = 0;
	virtual bool IsOpen() = 0;

	virtual const char *GetIOType() = 0;

	virtual int Read(void *buffer, avf_size_t size) = 0;	// return size or < 0 if error
	virtual int Write(const void *buffer, avf_size_t size) = 0;	// return size or < 0 if error
	virtual avf_status_t Flush() = 0;

	virtual int ReadAt(void *buffer, avf_size_t size, avf_u64_t pos) = 0;
	virtual int WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos) = 0;

	virtual bool CanSeek() = 0;
	virtual avf_status_t Seek(avf_s64_t pos, int whence = kSeekSet) = 0;

	virtual avf_u64_t GetSize() = 0;
	virtual avf_u64_t GetPos() = 0;

	virtual avf_u64_t UpdateSize() = 0;
	virtual avf_status_t Disconnect() = 0;	// for TCP connection

public:
	INLINE avf_status_t Skip(avf_u64_t size) {
		return Seek(size, kSeekCur);
	}

	INLINE bool ReadAndTest(void *buffer, avf_size_t size) {
		return Read(buffer, size) == (int)size;
	}

public:
	INLINE void write_le_8(avf_u8_t val) {
		Write(&val, 1);
	}

	INLINE void write_be_8(avf_u8_t val) {
		Write(&val, 1);
	}

	INLINE void write_le_16(avf_u16_t val) {
		avf_u8_t buf[2];
		buf[0] = val;
		buf[1] = val >> 8;
		Write(buf, 2);
	}

	INLINE void write_be_16(avf_u16_t val) {
		avf_u8_t buf[2];
		buf[0] = val >> 8;
		buf[1] = val;
		Write(buf, 2);
	}

	INLINE void write_le_32(avf_u32_t val) {
		avf_u8_t buf[4];
		buf[0] = val;
		buf[1] = val >> 8;
		buf[2] = val >> 16;
		buf[3] = val >> 24;
		Write(buf, 4);
	}

	INLINE void write_be_32(avf_u32_t val) {
		avf_u8_t buf[4];
		buf[0] = val >> 24;
		buf[1] = val >> 16;
		buf[2] = val >> 8;
		buf[3] = val;
		Write(buf, 4);
	}

	INLINE void write_fcc(const char *fcc) {
		Write(fcc, 4);
	}

	INLINE void write_string(const char *string) {
		Write(string, ::strlen(string));
	}

	INLINE void write_mem(const avf_u8_t *p, avf_u32_t size) {
		Write(p, size);
	}

public:
	INLINE avf_uint_t read_le_8(avf_uint_t def = 0) {
		avf_u8_t buf[1];
		if (Read(buf, 1) < 0)
			return def;
		return buf[0];
	}

	INLINE avf_uint_t read_le_16(avf_uint_t def = 0) {
		avf_u8_t buf[2];
		if (Read(buf, 2) < 0)
			return def;
		return (buf[1] << 8) | buf[0];
	}

	INLINE avf_u32_t read_le_32(avf_uint_t def = 0) {
		avf_u8_t buf[4];
		if (Read(buf, 4) < 0)
			return def;
		return (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	}

	INLINE avf_uint_t read_be_8(avf_uint_t def = 0) {
		return read_le_8(def);
	}

	INLINE avf_uint_t read_be_16(avf_uint_t def = 0) {
		avf_u8_t buf[2];
		if (Read(buf, 2) < 0)
			return def;
		return (buf[0] << 8) | buf[1];
	}

	INLINE avf_u32_t read_be_32(avf_uint_t def = 0) {
		avf_u8_t buf[4];
		if (Read(buf, 4) < 0)
			return def;
		return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	}
};

//-----------------------------------------------------------------------
//
//  IBlockIO
//
//-----------------------------------------------------------------------
class IBlockIO: public IInterface
{
public:
	struct block_desc_s {
		avf_size_t file_remain;
		avf_size_t align_size;
		avf_size_t block_size;
		avf_u8_t *ptr;
		avf_size_t remain;
	};

public:
	DECLARE_IF(IBlockIO, IID_IBlockIO);

	virtual avf_status_t _GetBlockDesc(block_desc_s& desc) = 0;
	virtual avf_status_t _EndWrite() = 0;
	virtual avf_status_t _UpdateWrite(block_desc_s& desc, const void *buf, avf_size_t size) = 0;
};

//-----------------------------------------------------------------------
//
//  ITSMapInfo
//
//-----------------------------------------------------------------------
struct ts_group_item_s;

class ITSMapInfo: public IInterface
{
public:
	DECLARE_IF(ITSMapInfo, IID_ITSMapInfo);

public:
	struct map_info_s {
		raw_data_t *v_desc;
		raw_data_t *a_desc;
		uint64_t start_pts;
		uint32_t group_count;
		ts_group_item_s *group_items;
		uint32_t ts_size;
	};

public:
	virtual void ReadInfo(map_info_s *info) = 0;
};

//-----------------------------------------------------------------------
//
//  CAVIO
//
//-----------------------------------------------------------------------
class CAVIO: public CObject, public IAVIO
{
	typedef CObject inherited;
	DEFINE_INTERFACE;

public:
	virtual avf_u64_t UpdateSize() {
		return GetSize();
	}

	virtual avf_status_t Disconnect() {
		return E_OK;
	}

	virtual int ReadAt(void *buffer, avf_size_t size, avf_u64_t pos) {
		if (Seek(pos) != E_OK)
			return -1;
		return Read(buffer, size);
	}

	virtual int WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos) {
		if (Seek(pos) != E_OK)
			return -1;
		return Write(buffer, size);
	}

public:
	static avf_status_t CopyFile(IAVIO *input, avf_u32_t size, IAVIO *output);
};

//-----------------------------------------------------------------------
//
//  CShareReadIO
//
//-----------------------------------------------------------------------
class CShareReadIO: public CAVIO
{
	typedef CObject inherited;

public:
	static CShareReadIO* Create(IAVIO *io);

private:
	CShareReadIO(IAVIO *io);
	virtual ~CShareReadIO();

public:
	virtual avf_status_t CreateFile(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenRead(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenWrite(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t OpenExisting(const char *url) {
		return E_ERROR;
	}
	virtual avf_status_t Close() {
		return E_ERROR;
	}
	virtual bool IsOpen() {
		return true;
	}

	virtual const char *GetIOType() {
		return "ShareReadIO";
	}

	virtual int Read(void *buffer, avf_size_t size);	// return size or < 0 if error

	virtual int Write(const void *buffer, avf_size_t size) {
		return E_ERROR;
	}
	virtual avf_status_t Flush() {
		return E_ERROR;
	}

	virtual int ReadAt(void *buffer, avf_size_t size, avf_u64_t pos) {
		return mp_io->ReadAt(buffer, size, pos);
	}
	virtual int WriteAt(const void *buffer, avf_size_t size, avf_u64_t pos) {
		return E_ERROR;
	}

	virtual bool CanSeek() {
		return true;
	}
	virtual avf_status_t Seek(avf_s64_t pos, int whence = kSeekSet);

	virtual avf_u64_t GetSize() {
		return m_size;
	}
	virtual avf_u64_t GetPos() {
		return m_pos;
	}

	//virtual avf_status_t Disconnect() = 0;	// for TCP connection

private:
	IAVIO *mp_io;
	avf_u64_t m_pos;
	avf_u64_t m_size;
};

//-----------------------------------------------------------------------
//
//  CNullIO
//
//-----------------------------------------------------------------------
class CNullIO: public CAVIO
{
public:
	static IAVIO* Create();

private:
	CNullIO();
	virtual ~CNullIO();

public:
	virtual avf_status_t CreateFile(const char *url);
	virtual avf_status_t OpenRead(const char *url);
	virtual avf_status_t OpenWrite(const char *url);
	virtual avf_status_t OpenExisting(const char *url);
	virtual avf_status_t Close();
	virtual bool IsOpen();
	virtual const char *GetIOType() { return "CNullIO"; }
	virtual int Read(void *buffer, avf_size_t size);
	virtual int Write(const void *buffer, avf_size_t size);
	virtual avf_status_t Flush();
	virtual bool CanSeek() { return true; }
	virtual avf_status_t Seek(avf_s64_t pos, int whence);
	virtual avf_u64_t GetSize();
	virtual avf_u64_t GetPos();

private:
	avf_u64_t m_file_pos;
	avf_u64_t m_file_size;
};

//-----------------------------------------------------------------------
//
//  CWriteBuffer - a buffer between AVIO and its user
//	It is used to speed up writing
//
//-----------------------------------------------------------------------
class CWriteBuffer
{
public:
	CWriteBuffer(IAVIO *io = NULL, avf_uint_t buffer_size = 8*KB);
	~CWriteBuffer();

public:
	INLINE void SetIO(IAVIO *io) {
		mpIO = io;
		m_ptr = m_buffer;
		m_remain = m_buffer_size;
	}

	INLINE IAVIO *GetIO() { return mpIO; }
	void Flush();

public:
	INLINE void __flush(avf_size_t room) {
		if (m_remain < room)
			Flush();
	}

	// big endian

	INLINE void __write_be_8(avf_u8_t val) {
		m_ptr[0] = val;

		m_ptr++;
		m_remain--;
	}
	INLINE void write_be_8(avf_u8_t val) {
		__flush(1);
		__write_be_8(val);
	}

	INLINE void __write_be_16(avf_u16_t val) {
		m_ptr[0] = val >> 8;
		m_ptr[1] = val;

		m_ptr += 2;
		m_remain -= 2;
	}
	INLINE void write_be_16(avf_u16_t val) {
		__flush(2);
		__write_be_16(val);
	}

	INLINE void __write_be_24(avf_u32_t val) {
		m_ptr[0] = val >> 16;
		m_ptr[1] = val >> 8;
		m_ptr[2] = val;

		m_ptr += 3;
		m_remain -= 3;
	}
	INLINE void write_be_24(avf_u32_t val) {
		__flush(3);
		__write_be_24(val);
	}

	INLINE void __write_be_32(avf_u32_t val) {
		m_ptr[0] = val >> 24;
		m_ptr[1] = val >> 16;
		m_ptr[2] = val >> 8;
		m_ptr[3] = val;

		m_ptr += 4;
		m_remain -= 4;
	}
	INLINE void write_be_32(avf_u32_t val) {
		__flush(4);
		__write_be_32(val);
	}

	void write_be_64(avf_u64_t val);
	void write_mem(const avf_u8_t *p, avf_uint_t size);

	INLINE void write_mem(const void *p, avf_uint_t size) {
		write_mem((const avf_u8_t*)p, size);
	}

	INLINE void write_string(const char *p, avf_uint_t size) {
		write_mem((const avf_u8_t*)p, size);
	}

	void write_zero(avf_uint_t count);

	INLINE void __write_fcc(const char *fcc) {
		m_ptr[0] = fcc[0];
		m_ptr[1] = fcc[1];
		m_ptr[2] = fcc[2];
		m_ptr[3] = fcc[3];

		m_ptr += 4;
		m_remain -= 4;
	}
	INLINE void write_fcc(const char *fcc) {
		__flush(4);
		__write_fcc(fcc);
	}

	// little endian

	INLINE void __write_le_8(avf_u8_t val) {
		__write_be_8(val);
	}
	INLINE void write_le_8(avf_u8_t val) {
		__flush(1);
		__write_le_8(val);
	}

	INLINE void __write_le_16(avf_u16_t val) {
		m_ptr[0] = val;
		m_ptr[1] = val >> 8;

		m_ptr += 2;
		m_remain -= 2;
	}
	INLINE void write_le_16(avf_u16_t val) {
		__flush(2);
		__write_le_16(val);
	}

	INLINE void __write_le_32(avf_u32_t val) {
		m_ptr[0] = val;
		m_ptr[1] = val >> 8;
		m_ptr[2] = val >> 16;
		m_ptr[3] = val >> 24;

		m_ptr += 4;
		m_remain -= 4;
	}
	INLINE void write_le_32(avf_u32_t val) {
		__flush(4);
		__write_le_32(val);
	}

	INLINE void __awrite_le_32(avf_u32_t val) {
		*(avf_u32_t*)m_ptr = val;
		m_ptr += 4;
		m_remain -= 4;
	}
	INLINE void awrite_le_32(avf_u32_t val) {
		__flush(4);
		__awrite_le_32(val);
	}

	INLINE void awrite_begin(avf_size_t size) {
		__flush(size);
	}
	INLINE void awrite_end() {
	}

	void write_le_64(avf_u64_t val);
	void awrite_le_64(avf_u64_t val);

	avf_u64_t GetWritePos() {
		return mpIO->GetPos() + (m_ptr - m_buffer);
	}

private:
	avf_u8_t *m_ptr;
	avf_uint_t m_remain;

	avf_u8_t *m_buffer;
	avf_uint_t m_buffer_size;

	IAVIO *mpIO;
};

//-----------------------------------------------------------------------
//
//  CReadBuffer - a buffer between AVIO and its user
//	It is used to speed up reading
//
//-----------------------------------------------------------------------
class CReadBuffer
{
public:
	CReadBuffer(IAVIO *io, avf_uint_t buffer_size = 8*KB);
	~CReadBuffer();

public:
	INLINE bool IOError() {
		return m_error != E_OK;
	}

	INLINE avf_status_t IOStatus() {
		return m_error;
	}

	INLINE void SetIO(IAVIO *io) {
		mpIO = io;
		m_error = E_OK;
		m_ptr = m_buffer;
		m_remain = 0;
	}

	void DoRefill(int min_bytes);
	void DoSkip(avf_uint_t size);
	avf_status_t SeekTo(avf_u64_t pos);

	INLINE void Skip(avf_uint_t size) {
		if (size < m_remain) {
			m_ptr += size;
			m_remain -= size;
		} else if (size > 0) {
			DoSkip(size);
		}
	}

	INLINE void Refill(avf_size_t min_bytes) {
		if (m_remain < min_bytes) {
			DoRefill(min_bytes);
		}
	}

	INLINE const avf_u8_t *Peek(avf_size_t bytes) {
		if (bytes > m_buffer_size) {
			return NULL;
		}
		if (m_remain < bytes) {
			DoRefill(bytes);
			return IOError() ? NULL : m_ptr;
		}
		return m_ptr;
	}

	// ------------------------------
	// little endian
	// ------------------------------

	INLINE avf_uint_t __read_le_8() {
		avf_uint_t result = m_ptr[0];
		m_ptr++;
		m_remain--;
		return result;
	}
	INLINE avf_uint_t read_le_8() {
		Refill(1);
		return __read_le_8();
	}

	INLINE avf_uint_t __read_le_16() {
		avf_uint_t result = m_ptr[0] | (m_ptr[1] << 8);
		m_ptr += 2;
		m_remain -= 2;
		return result;
	}
	INLINE avf_uint_t read_le_16() {
		Refill(2);
		return __read_le_16();
	}

	INLINE avf_u32_t __read_le_32() {
		avf_u32_t result = m_ptr[0] | (m_ptr[1] << 8) | (m_ptr[2] << 16) | (m_ptr[3] << 24);
		m_ptr += 4;
		m_remain -= 4;
		return result;
	}
	INLINE avf_u32_t read_le_32() {
		Refill(4);
		return __read_le_32();
	}

	INLINE avf_u32_t __aread_le_32() {
		avf_u32_t result = *(avf_u32_t*)m_ptr;
		m_ptr += 4;
		m_remain -= 4;
		return result;
	}
	INLINE avf_u32_t aread_le_32() {
		Refill(4);
		return __aread_le_32();
	}

	bool read_mem(avf_u8_t *p, avf_uint_t size);

	INLINE bool read_mem(void *p, avf_uint_t size) {
		return read_mem((avf_u8_t*)p, size);
	}

	avf_u64_t read_le_64();
	avf_u64_t aread_le_64();

	INLINE void aread_begin(avf_size_t size) {
		Refill(size);
	}
	INLINE void aread_end() {
	}

	// ------------------------------
	// big endian
	// ------------------------------

	INLINE avf_uint_t __read_be_16() {
		avf_uint_t result = (m_ptr[0] << 8) | m_ptr[1];
		m_ptr += 2;
		m_remain -= 2;
		return result;
	}

	INLINE avf_uint_t read_be_16() {
		Refill(2);
		return __read_be_16();
	}

	INLINE avf_u32_t __read_be_32() {
		avf_u32_t result = m_ptr[3] | (m_ptr[2] << 8) | (m_ptr[1] << 16) | (m_ptr[0] << 24);
		m_ptr += 4;
		m_remain -= 4;
		return result;
	}

	INLINE avf_u32_t read_be_32() {
		Refill(4);
		return __read_be_32();
	}

	INLINE avf_u64_t GetReadPos() {
		return mpIO->GetPos() - m_remain;
	}

	INLINE avf_u64_t GetFileSize() {
		return mpIO->GetSize();
	}

	// ---------------------------------------------------------------------------------------------

	INLINE avf_uint_t read_16(bool b_le) {
		return b_le ? read_le_16() : read_be_16();
	}

	INLINE avf_uint_t read_32(bool b_le) {
		return b_le ? read_le_32() : read_be_32();
	}

private:
	avf_u8_t *m_ptr;
	avf_uint_t m_remain;

	avf_u8_t *m_buffer;
	avf_uint_t m_buffer_size;

	IAVIO *mpIO;
	avf_status_t m_error;
};

#endif


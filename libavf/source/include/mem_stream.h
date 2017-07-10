
#ifndef __MEM_STREAM_H__
#define __MEM_STREAM_H__

//-----------------------------------------------------------------------
//
//  CMemBuffer - 1 or n blocks of memory
//  bSimple - only 1 block
//
//-----------------------------------------------------------------------
class CMemBuffer: public CObject
{
	typedef CObject inherited;

public:
	static CMemBuffer *Create(avf_uint_t block_size, bool bSimple = false);

private:
	CMemBuffer(avf_uint_t block_size, bool bSimple);
	virtual ~CMemBuffer();

public:
	INLINE void MakeRoom(avf_size_t room) {
		if (m_remain < room)
			Resize(room);
	}

	// char
	INLINE void __write_char(int c) {
		m_ptr[0] = c;
		m_ptr++;
		m_remain--;
	}
	INLINE void write_char(int c) {
		MakeRoom(1);
		__write_char(c);
	}

	// n char
	INLINE void write_char_n(int c, int count) {
		avf_u8_t *p = Alloc(count);
		for (int i = count; i > 0; i--)
			*p++ = c;
	}

	// 1 byte
	INLINE void __write_le_8(avf_u8_t val) {
		m_ptr[0] = val;
		m_ptr++;
		m_remain--;
	}
	INLINE void write_le_8(avf_u8_t val) {
		MakeRoom(1);
		__write_le_8(val);
	}

	// 2 bytes
	INLINE void __write_le_16(avf_u16_t val) {
		m_ptr[0] = val;
		m_ptr[1] = val >> 8;
		m_ptr += 2;
		m_remain -= 2;
	}
	INLINE void write_le_16(avf_u16_t val) {
		MakeRoom(2);
		__write_le_16(val);
	}

	// 4 bytes
	INLINE void __write_le_32(avf_u32_t val) {
		m_ptr[0] = val;
		m_ptr[1] = val >> 8;
		m_ptr[2] = val >> 16;
		m_ptr[3] = val >> 24;
		m_ptr += 4;
		m_remain -= 4;
	}
	INLINE void __awrite_le_32(avf_u32_t val) {
		*(avf_u32_t*)m_ptr = val;
		m_ptr += 4;
		m_remain -= 4;
	}
	INLINE void write_le_32(avf_u32_t val) {
		MakeRoom(4);
		__write_le_32(val);
	}

	void write_le_64(avf_u64_t val);
	void write_mem(const avf_u8_t *p, avf_uint_t size);

	// string
	INLINE void write_string(const char *str) {
		write_mem((const avf_u8_t*)str, ::strlen(str));
	}

	INLINE void write_string(const char *str, int len) {
		write_mem((const avf_u8_t*)str, len);
	}

	INLINE void write_string(string_t *str) {
		write_mem((const avf_u8_t*)str->string(), str->size());
	}

	avf_size_t write_string_aligned(const char *str, avf_uint_t size = 0);

	void write_zero(avf_size_t count);

	void write_int(int number);

	// fcc
	INLINE void __write_fcc(const char *fcc) {
		m_ptr[0] = fcc[0];
		m_ptr[1] = fcc[1];
		m_ptr[2] = fcc[2];
		m_ptr[3] = fcc[3];
		m_ptr += 4;
		m_remain -= 4;
	}
	INLINE void write_fcc(const char *fcc) {
		MakeRoom(4);
		__write_fcc(fcc);
	}

	void write_length_string(const char *str, int length);

	void *CollectData(avf_size_t extra = 0);
	static void ReleaseData(void *data);

public:
	struct block_t {
		struct block_t *next;
		avf_uint_t size;
		// follows memory
		INLINE avf_u8_t *GetMem() {
			return (avf_u8_t*)(this + 1);
		}
		INLINE avf_uint_t GetSize() {
			return size;
		}
		INLINE struct block_t *GetNext() {
			return next;
		}
	};

public:
	avf_u8_t *Alloc(avf_uint_t size);
	avf_u8_t *AllocBlock();
	void Clear();

	// should not be simple
	INLINE avf_size_t GetBlockSize() {
		return m_block_size;
	}

	// should not be simple
	INLINE void SetCurrBlockSize(avf_size_t size) {
		m_ptr = m_curr_block->GetMem() + size;
		m_remain = m_block_size - size;
	}

	INLINE avf_u32_t GetTotalSize() {
		if (m_curr_block == NULL)
			return 0;
		__UpdateSize();
		return m_total_size1 + m_curr_block->size;
	}

	block_t *GetFirstBlock();
	block_t *GetBlockOf(avf_uint_t offset, avf_uint_t *block_offset);

	avf_status_t Append(CMemBuffer *other);

private:
	INLINE void Reset() {
		m_total_size1 = 0;
		m_first_block = NULL;
		m_curr_block = NULL;
		m_ptr = NULL;
		m_remain = 0;
	}

	INLINE void __UpdateSize() {
		m_curr_block->size = m_ptr - m_curr_block->GetMem();
	}

	void Resize(avf_size_t room);

private:
	const bool mb_simple;
	const avf_uint_t m_block_size;

	avf_u32_t m_total_size1;

	block_t *m_first_block;
	block_t *m_curr_block;

	avf_u8_t *m_ptr;
	avf_uint_t m_remain;
};

#endif


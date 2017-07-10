
#define LOG_TAG "mem_stream"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"
#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "mem_stream.h"

//-----------------------------------------------------------------------
//
//  CMemBuffer
//
//-----------------------------------------------------------------------
CMemBuffer* CMemBuffer::Create(avf_uint_t block_size, bool bSimple)
{
	CMemBuffer *result = avf_new CMemBuffer(block_size, bSimple);
	CHECK_STATUS(result);
	return result;
}

CMemBuffer::CMemBuffer(avf_uint_t block_size, bool bSimple):
	mb_simple(bSimple),
	m_block_size(block_size)
{
	Reset();
}

CMemBuffer::~CMemBuffer()
{
	Clear();
}

void CMemBuffer::Clear()
{
	block_t *block = GetFirstBlock();
	while (block) {
		block_t *next = block->GetNext();
		avf_free(block);
		block = next;
	}
	Reset();
}

CMemBuffer::block_t *CMemBuffer::GetFirstBlock()
{
	if (m_curr_block == NULL)
		return NULL;
	__UpdateSize();
	return m_first_block;
}

CMemBuffer::block_t *CMemBuffer::GetBlockOf(avf_uint_t offset, avf_uint_t *block_offset)
{
	avf_uint_t start = 0;
	for (block_t *block = GetFirstBlock(); block; block = block->GetNext()) {
		avf_uint_t next_start = start + block->GetSize();
		if (offset >= start && offset < next_start) {
			*block_offset = offset - start;
			return block;
		}
		start = next_start;
	}
	return NULL;
}

avf_status_t CMemBuffer::Append(CMemBuffer *other)
{
	block_t *block = other->GetFirstBlock();
	for (; block; block = block->GetNext()) {
		avf_u8_t *ptr = Alloc(block->GetSize());
		::memcpy(ptr, block->GetMem(), block->GetSize());
	}
	return E_OK;
}

void CMemBuffer::Resize(avf_size_t room)
{
	if (mb_simple) {
		avf_size_t total_size = GetTotalSize();
		avf_size_t inc;

		inc = (total_size == 0) ? m_block_size : total_size / 2;
		if (inc < room) {
			inc = room;
		}

		avf_size_t size = total_size + inc;

		block_t *block = (block_t*)avf_malloc(sizeof(block_t) + size);
		AVF_ASSERT(block);
		block->next = NULL;

		m_ptr = block->GetMem();
		m_remain = inc;

		if (m_curr_block) {
			::memcpy(m_ptr, m_curr_block->GetMem(), m_curr_block->size);
			m_ptr += m_curr_block->size;
			avf_free(m_curr_block);
		}

		m_first_block = block;
		m_curr_block = block;

	} else {

		if (room < m_block_size)
			room = m_block_size;

		block_t *block = (block_t*)avf_malloc(sizeof(block_t) + room);
		AVF_ASSERT(block);
		block->next = NULL;

		if (m_curr_block) {
			__UpdateSize();
			m_total_size1 += m_curr_block->size;
			m_curr_block->next = block;
		} else {
			m_first_block = block;
		}

		m_curr_block = block;

		m_ptr = block->GetMem();
		m_remain = room;
	}
}

avf_u8_t *CMemBuffer::Alloc(avf_uint_t size)
{
	MakeRoom(size);

	avf_u8_t *ptr = m_ptr;
	m_ptr += size;
	m_remain -= size;

	return ptr;
}

avf_u8_t *CMemBuffer::AllocBlock()
{
	Resize(0);
	return m_ptr;
}

void CMemBuffer::write_le_64(avf_u64_t val)
{
	MakeRoom(8);

	avf_u32_t lo = U64_LO(val);
	m_ptr[0] = lo;
	m_ptr[1] = lo >> 8;
	m_ptr[2] = lo >> 16;
	m_ptr[3] = lo >> 24;

	avf_u32_t hi = U64_HI(val);
	m_ptr[4] = hi;
	m_ptr[5] = hi >> 8;
	m_ptr[6] = hi >> 16;
	m_ptr[7] = hi >> 24;

	m_ptr += 8;
	m_remain -= 8;
}

void CMemBuffer::write_mem(const avf_u8_t *p, avf_uint_t size)
{
	MakeRoom(size);
	::memcpy(m_ptr, p, size);
	m_ptr += size;
	m_remain -= size;
}

// 4-byte aligned
avf_size_t CMemBuffer::write_string_aligned(const char *str, avf_uint_t size)
{
	if (size == 0) {
		size = ::strlen(str) + 1;
	}
	write_le_32(size);
	write_mem((const avf_u8_t *)str, size);
	if ((size & 3) != 0) {
		avf_size_t n = 4 - (size & 3);
		size += n;
		for (; n; n--) {
			write_le_8(0);
		}
	}
	return 4 + size;
}

void CMemBuffer::write_zero(avf_size_t count)
{
	while (count > 0) {
		avf_size_t n = MIN(count, 16);
		MakeRoom(n);

		avf_u8_t *p = m_ptr;
		for (avf_size_t i = n; i; i--)
			*p++ = 0;

		m_ptr += n;
		m_remain -= n;

		count -= n;
	}
}

void CMemBuffer::write_int(int number)
{
	char buffer[20];
	::sprintf(buffer, "%d", number);
	write_string(buffer);
}

void CMemBuffer::write_length_string(const char *str, int length)
{
	if (length <= 0) {
		length = (str == NULL) ? 0 : ::strlen(str);
	}
	write_le_32(length);
	if (str) {
		write_mem((const avf_u8_t*)str, length);
	}
}

// call avf_free() to free the result
void *CMemBuffer::CollectData(avf_size_t extra)
{
	avf_u32_t size = GetTotalSize();
	void *result = avf_malloc_bytes(size + extra);
	if (result == NULL)
		return NULL;

	block_t *block = GetFirstBlock();
	void *ptr = result;
	for (; block; block = block->GetNext()) {
		::memcpy(ptr, block->GetMem(), block->GetSize());
		ptr = (void*)((avf_u8_t*)ptr + block->GetSize());
	}

	return result;
}

void CMemBuffer::ReleaseData(void *data)
{
	avf_safe_free(data);
}


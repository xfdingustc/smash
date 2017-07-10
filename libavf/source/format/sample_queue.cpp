
#define LOG_TAG "sample_queue"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"

#include "sample_queue.h"

//-----------------------------------------------------------------------
//
//  media_block_t
//
//-----------------------------------------------------------------------

static void mblock_release(media_block_t *self)
{
	stream_raw_data_t *pRaw = self->m_stream_raw_data;
	for (avf_size_t i = self->m_num_raw; i; i--, pRaw++) {
		pRaw->m_data->Release();
	}
	avf_safe_free(self->m_stream_raw_data);
	avf_safe_free(self->filename);
	avf_free(self->mpSamples);
	avf_free(self);
}

static media_block_t *mblock_create(string_t *filename, avf_size_t size)
{
	media_block_t *self;

	if ((self = avf_malloc_type(media_block_t)) == NULL)
		return NULL;

	self->m_sample_count = 0;
	self->m_block_size = size;

	self->mRefCount = 1;
	self->DoRelease = mblock_release;

	self->m_num_raw = 0;
	self->m_max_raw = 0;
	self->m_stream_raw_data = NULL;

	self->filename = filename->acquire();

	if ((self->mpSamples = avf_malloc_array(raw_sample_t, size)) == NULL) {
		self->filename->Release();
		avf_free(self);
		return NULL;
	}

	return self;
}

static avf_status_t mblock_resize(media_block_t *self, avf_size_t inc)
{
	avf_size_t size = self->m_block_size + inc;
	raw_sample_t *pNewSamples;

	if ((pNewSamples = avf_malloc_array(raw_sample_t, size)) == NULL) {
		return E_NOMEM;
	}

	avf_size_t bytes = self->m_block_size * sizeof(raw_sample_t);
	::memcpy(pNewSamples, self->mpSamples, bytes);

	avf_free(self->mpSamples);
	self->mpSamples = pNewSamples;
	self->m_block_size = size;

	return E_OK;
}

static avf_status_t mblock_add_raw_data(media_block_t *self, avf_size_t index, raw_data_t *raw)
{
	if (self->m_num_raw == self->m_max_raw) {
		avf_size_t bytes = (self->m_max_raw + 4) * sizeof(stream_raw_data_t);
		stream_raw_data_t *array = (stream_raw_data_t*)avf_malloc(bytes);
		if (array == NULL)
			return E_NOMEM;
		if (self->m_stream_raw_data) {
			bytes = self->m_num_raw * sizeof(stream_raw_data_t);
			::memcpy(array, self->m_stream_raw_data, bytes);
			avf_free(self->m_stream_raw_data);
		}
		self->m_stream_raw_data = array;
		self->m_max_raw += 4;
	}

	stream_raw_data_t *pRaw = self->m_stream_raw_data + self->m_num_raw;
	pRaw->m_index = index;
	pRaw->m_data = raw->acquire();

	self->m_num_raw++;

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CSampleProvider
//
//-----------------------------------------------------------------------

CSampleQueue* CSampleQueue::Create(avf_size_t max_blocks)
{
	CSampleQueue* result = avf_new CSampleQueue(max_blocks);
	CHECK_STATUS(result);
	return result;
}

CSampleQueue::CSampleQueue(avf_size_t max_blocks):
	mpProvider(NULL),
	m_provider_remain(0),
	m_provider_skip(0),

	m_max_blocks(max_blocks),
	m_num_blocks(0),

	m_max_num_tmp(0),
	m_num_tmp_blocks(0),

	m_max_block_size(BLOCK_INIT_SIZE),

	m_block_remain(0),
	mp_sample_ptr(NULL),
	mp_curr_block(NULL)
{
	list_init(m_block_list);
	list_init(m_tmp_block_list);
}

CSampleQueue::~CSampleQueue()
{
	list_head_t *node;
	list_head_t *next;

	AVF_LOGD("Max tmp list: %d", m_max_num_tmp);

	list_for_each_del(node, next, m_block_list) {
		media_block_t *block = media_block_t::FromNode(node);
		block->Release();
	}

	list_for_each_del(node, next, m_tmp_block_list) {
		media_block_t *block = media_block_t::FromNode(node);
		block->Release();
	}

	avf_safe_release(mp_curr_block);

	avf_safe_release(mpProvider);
}

avf_status_t CSampleQueue::PushRawData(avf_size_t index, raw_data_t *raw)
{
	avf_status_t status;

	if (mp_curr_block == NULL) {
		if ((status = CreateCurrentBlock()) != E_OK)
			return status;
	}

	status = mblock_add_raw_data(mp_curr_block, index, raw);
	return status;
}

avf_status_t CSampleQueue::PushSample(avf_size_t index, 
	CBuffer *pBuffer, const media_sample_t *pSample)
{
	while (true) {
		if (m_block_remain > 0) {
			m_block_remain--;

			mp_sample_ptr->m_index = index;
			mp_sample_ptr->m_size = pSample->m_size;
			mp_sample_ptr->m_pos = pSample->m_pos;
			mp_sample_ptr->frame_type = pBuffer->frame_type;
			mp_sample_ptr->mFlags = pBuffer->mFlags | CBuffer::F_RAW;
			mp_sample_ptr->m_dts = pBuffer->m_dts;
			mp_sample_ptr->m_pts = pBuffer->m_pts;
			mp_sample_ptr->m_duration = pBuffer->m_duration;

			mp_sample_ptr++;
			return E_OK;
		}

		avf_status_t status;

		if (mp_curr_block == NULL) {
			if ((status = CreateCurrentBlock()) != E_OK)
				return status;
		} else {
			avf_size_t ptr_index = mp_sample_ptr - mp_curr_block->mpSamples;
			if ((status = mblock_resize(mp_curr_block, BLOCK_INC)) != E_OK)
				return status;
			m_block_remain += BLOCK_INC;
			mp_sample_ptr = mp_curr_block->mpSamples + ptr_index;
		}
	}
}

avf_status_t CSampleQueue::CreateCurrentBlock()
{
	mp_curr_block = mblock_create(mFileName.get(), m_max_block_size);
	if (mp_curr_block == NULL)
		return E_NOMEM;
	m_block_remain = mp_curr_block->m_block_size;
	mp_sample_ptr = mp_curr_block->mpSamples;
	return E_OK;
}

void CSampleQueue::FinishBlock()
{
	if (mp_curr_block == NULL)
		return;

	// the max block size
	m_max_block_size = MAX(m_max_block_size, mp_curr_block->m_block_size);
	mp_curr_block->m_sample_count = mp_sample_ptr - mp_curr_block->mpSamples;

	if (mp_curr_block->m_sample_count == 0) {
		mp_curr_block->Release();
	} else {
		// append to tmp list
		list_add_tail(&mp_curr_block->list_block, m_tmp_block_list);
		m_num_tmp_blocks++;
		if (m_max_num_tmp < m_num_tmp_blocks)
			m_max_num_tmp = m_num_tmp_blocks;
	}

	// clear current
	m_block_remain = 0;
	mp_sample_ptr = NULL;
	mp_curr_block = NULL;

	//AVF_LOGD("block finished, total %d", mNumBlocks);
}

void CSampleQueue::ModifyLists()
{
	// trim block list
	if (m_num_blocks >= m_max_blocks) {
		list_head_t *old = m_block_list.next;
		__list_del(old);
		m_num_blocks--;
		media_block_t *block = media_block_t::FromNode(old);
		block->Release();
	}

	// remove from temp list
	list_head_t *node = m_tmp_block_list.next;
	__list_del(node);
	m_num_tmp_blocks--;

	// add to block list
	list_add_tail(node, m_block_list);
	m_num_blocks++;

	// notify provider
	if (m_provider_remain > 0) {
		media_block_t *block = media_block_t::FromNode(node);
		mpProvider->PushMediaBlock(block);
		if (--m_provider_remain == 0) {
			mpProvider->PushEOS();
			avf_safe_release(mpProvider);
		}
		AVF_LOGD("remain %d", m_provider_remain);
	}
}

void CSampleQueue::UpdateQueue(avf_u64_t fpos)
{
	while (m_num_tmp_blocks > 0) {
		list_head_t *node = m_tmp_block_list.next;

		media_block_t *block = media_block_t::FromNode(node);
		raw_sample_t *sample = block->mpSamples + (block->m_sample_count - 1);
		avf_u64_t last_address = sample->m_pos + sample->m_size;
		if (last_address > fpos) {
			//AVF_LOGD("block not written yet");
			break;
		}

		ModifyLists();
	}
}

void CSampleQueue::FlushQueue(bool bEnd)
{
	// current block
	FinishBlock();

	// tmp -> cache
	while (m_num_tmp_blocks > 0)
		ModifyLists();

	// notify provider
	if (bEnd && mpProvider) {
		mpProvider->PushEOS();
		avf_safe_release(mpProvider);
	}
}

// return number of blocks
avf_status_t CSampleQueue::InitSampleProvider(
	CSampleProvider *pProvider, avf_size_t before, avf_size_t after)
{
	avf_status_t status;

	if (mpProvider) {
		AVF_LOGE("SampleQueue is busy");
		return E_BUSY;
	}

	AVF_LOGD("InitSampleProvider, ready=%d, tmp=%d, samples=%d, %d+%d",
		m_num_blocks, m_num_tmp_blocks, GetPendingSamples(), before, after);

	avf_size_t ready = m_num_blocks + m_num_tmp_blocks;

	if (before > ready)
		before = ready;

	if (before > m_num_tmp_blocks) {

		ready = before - m_num_tmp_blocks;
		m_provider_skip = 0;

		list_head_t *node = &m_block_list;
		for (avf_size_t i = ready; i; i--)
			node = node->prev;

		AVF_LOGD("Push %d blocks", ready);

		for (avf_size_t i = ready; i; i--, node = node->next) {
			media_block_t *block = media_block_t::FromNode(node);
			if ((status = pProvider->PushMediaBlock(block)) != E_OK)
				return status;
		}
	} else {
		m_provider_skip = m_num_tmp_blocks - before;
	}

	m_provider_remain = m_num_tmp_blocks - m_provider_skip + after;
	AVF_LOGD("There're %d blocks remain", m_provider_remain);

	if (m_provider_remain > 0) {
		avf_assign(mpProvider, pProvider);
	}

	return E_OK;
}

//-----------------------------------------------------------------------
//
//  CSampleProvider
//
//-----------------------------------------------------------------------
CSampleProvider* CSampleProvider::Create()
{
	CSampleProvider *result = avf_new CSampleProvider();
	CHECK_STATUS(result);
	return result;
}

CSampleProvider::CSampleProvider()
{
	mpQueue = NULL;
	SET_ARRAY_NULL(mpFormats);
	CREATE_OBJ(mpQueue = CMsgQueue::Create("SampleProvider", sizeof(media_block_t*), 10));
}

CSampleProvider::~CSampleProvider()
{
	if (mpQueue) {
		media_block_t *pBlock;
		while (mpQueue->PeekMsg(&pBlock, sizeof(media_block_t*))) {
			AVF_LOGD("Release unused media blocks");
			pBlock->Release();
		}
	}

	avf_safe_release(mpQueue);
	SAFE_RELEASE_ARRAY(mpFormats);
}

void *CSampleProvider::GetInterface(avf_refiid_t refiid)
{
	if (refiid == IID_ISampleProvider)
		return static_cast<ISampleProvider*>(this);
	return inherited::GetInterface(refiid);
}

CMediaFormat *CSampleProvider::QueryMediaFormat(avf_size_t index)
{
	CMediaFormat *pFormat = index <= MAX_STREAMS ? mpFormats[index] : NULL;
	if (pFormat)
		pFormat->AddRef();
	return pFormat;
}

avf_status_t CSampleProvider::SetMediaFormat(avf_size_t index, CMediaFormat *pFormat)
{
	if (index >= MAX_STREAMS)
		return E_NOENT;

	mpFormats[index] = pFormat->acquire();

	return E_OK;
}

avf_status_t CSampleProvider::PushMediaBlock(media_block_t *pBlock)
{
	if (pBlock) {
		pBlock->AddRef();
	}
	mpQueue->PostMsg(&pBlock, sizeof(media_block_t*));
	return E_OK;
}

media_block_t *CSampleProvider::PopMediaBlock()
{
	media_block_t *pBlock;
	mpQueue->GetMsg(&pBlock, sizeof(media_block_t*));
	return pBlock;
}


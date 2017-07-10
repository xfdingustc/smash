
#define LOG_TAG	"avf_mp4i"

#include "avf_common.h"
#include "avf_new.h"
#include "avf_osal.h"
#include "avf_queue.h"

#include "avf_if.h"
#include "avf_base_if.h"
#include "avf_base.h"
#include "mem_stream.h"
#include "avio.h"
#include "sys_io.h"

#include "vdb_cmd.h"
#include "avf_mp4i.h"

#define MP4I_VER	1
#define INFO_SIZE	256

#define MP4I_GUID		"2DDA9CB7-1C71-4703-9976-F99E072D13C9"
#define MP4I_GUID_LEN	36

#define TAG_xJPG	MKFCC('x','J','P','G')
#define TAG_xRAW	MKFCC('x','R','A','W')
#define TAG_xINF	MKFCC('x','I','N','F')
#define TAG_GPS0	MKFCC('G','P','S','0')
#define TAG_ACC0	MKFCC('A','C','C','0')
#define TAG_OBD0	MKFCC('O','B','D','0')

static int _mp4i_calc_raw_size(const uint8_t *data)
{
	if (data == NULL)
		return 0;

	vdb_ack_GetRawDataBlock_t *raw = (vdb_ack_GetRawDataBlock_t*)data;
	if (raw->num_items == 0)
		return 0;

	int size = raw->num_items * sizeof(vdb_raw_data_index_t) + raw->data_size;

	return 16 + size;
}

static void _mp4i_write_raw_data(CWriteBuffer& buffer, uint32_t tag, const uint8_t *data)
{
	if (data == NULL)
		return;

	vdb_ack_GetRawDataBlock_t *raw = (vdb_ack_GetRawDataBlock_t*)data;
	if (raw->num_items == 0)
		return;

	int index_size = raw->num_items * sizeof(vdb_raw_data_index_t);
	int size = index_size + raw->data_size;

	buffer.write_be_32(tag);
	buffer.write_be_32(size);
	buffer.write_be_32(raw->num_items);
	buffer.write_be_32(raw->data_size);

	vdb_raw_data_index_t *index = (vdb_raw_data_index_t*)(raw + 1);
	buffer.write_mem((const avf_u8_t*)index, index_size);
	buffer.write_mem((const avf_u8_t*)index + index_size, raw->data_size);
}

static int _mp4i_write_info(CWriteBuffer& buffer, const mp4i_info_t *info,
	uint32_t _jpg_size, const uint8_t *jpg_data,
	const uint8_t *gps_ack_data,
	const uint8_t *acc_ack_data,
	const uint8_t *obd_ack_data)
{
	int jpg_size = (jpg_data == NULL) ? 0 : (_jpg_size + 3) & ~3;
	int gps_size = _mp4i_calc_raw_size(gps_ack_data);
	int acc_size = _mp4i_calc_raw_size(acc_ack_data);
	int obd_size = _mp4i_calc_raw_size(obd_ack_data);
	int jpg_total_size = 8 + 4 + jpg_size + INFO_SIZE;
	int total_size = 8 + MP4I_GUID_LEN + gps_size + acc_size + obd_size + jpg_total_size;

	int jpg_tmp = 8 + 4 + jpg_size;
	int gps_offset = gps_size ? gps_size + acc_size + obd_size + jpg_tmp : 0;
	int acc_offset = acc_size ? acc_size + obd_size + jpg_tmp : 0;
	int obd_offset = obd_size ? obd_size + jpg_tmp : 0;

	// xRAW
	buffer.write_be_32(total_size);
	buffer.write_be_32(TAG_xRAW);

	buffer.write_string(MP4I_GUID, MP4I_GUID_LEN);

	_mp4i_write_raw_data(buffer, TAG_GPS0, gps_ack_data);
	_mp4i_write_raw_data(buffer, TAG_ACC0, acc_ack_data);
	_mp4i_write_raw_data(buffer, TAG_OBD0, obd_ack_data);

	// xJPG
	buffer.write_be_32(jpg_total_size);
	buffer.write_be_32(TAG_xJPG);

	// jpg
	if (jpg_data == NULL) {
		buffer.write_be_32(0);
	} else {
		buffer.write_be_32(_jpg_size);
		buffer.write_mem(jpg_data, _jpg_size);
		for (int i = jpg_size - _jpg_size; i > 0; i--) {
			buffer.write_be_8(0);
		}
	}

	// info
	buffer.write_be_32(TAG_xINF);	// TAG_xJPG
	buffer.write_be_32(MP4I_VER);

	buffer.write_string(MP4I_GUID, MP4I_GUID_LEN);

	buffer.write_be_32(info->clip_date);
	buffer.write_be_32(info->clip_length_ms);
	buffer.write_be_32(info->clip_created_date);
	buffer.write_be_32(jpg_size);

	buffer.write_be_32(info->stream_version);
	buffer.write_be_8(info->video_coding);
	buffer.write_be_8(info->video_frame_rate);
	buffer.write_be_16(info->video_width);
	buffer.write_be_16(info->video_height);
	buffer.write_be_8(info->audio_coding);
	buffer.write_be_8(info->audio_num_channels);
	buffer.write_be_32(info->audio_sampling_freq);

	buffer.write_be_32(gps_offset);
	buffer.write_be_32(acc_offset);
	buffer.write_be_32(obd_offset);

	buffer.write_zero(INFO_SIZE - (13*4 + MP4I_GUID_LEN));

	return 0;
}

int mp4i_write_info(const char *mp4_filename,
	const mp4i_info_t *info,
	uint32_t jpg_size, const uint8_t *jpg_data,
	const uint8_t *gps_ack_data,
	const uint8_t *acc_ack_data,
	const uint8_t *obd_ack_data)
{
	AVF_LOGD("%s: jpg_size: %d; gps: %p, acc: %p, obd: %p", mp4_filename, 
		jpg_size, gps_ack_data, acc_ack_data, obd_ack_data);

	IAVIO *io = CSysIO::Create();
	avf_status_t status = io->OpenExisting(mp4_filename);
	if (status != E_OK) {
		io->Release();
		return status;
	}
	io->Seek(io->GetSize());

	CWriteBuffer buffer(io, 4*KB);

	int result = _mp4i_write_info(buffer, info, jpg_size, jpg_data,
		gps_ack_data, acc_ack_data, obd_ack_data);

	buffer.Flush();
	io->Release();

	return result;
}

static int _mp4i_read_tag(CReadBuffer& buffer, const char *filename)
{
	uint32_t tag = buffer.read_be_32();
	if (tag != TAG_xJPG && tag != TAG_xINF) {
		AVF_LOGD("invalid tag %x, %s", tag, filename);
		return E_UNKNOWN;
	}

	uint32_t version = buffer.read_be_32();
	if (version != MP4I_VER) {
		AVF_LOGD("unknown version %d, %s", version, filename);
		return E_UNKNOWN;
	}

	if (tag == TAG_xINF) {
		uint8_t guid[MP4I_GUID_LEN];
		buffer.read_mem((avf_u8_t*)guid, MP4I_GUID_LEN);
		if (memcmp(guid, MP4I_GUID, MP4I_GUID_LEN) != 0) {
			AVF_LOGD("no guid");
			return E_UNKNOWN;
		}
	}

	return 0;
}

static int _mp4i_read_info(CReadBuffer& buffer, mp4i_info_t *info, const char *filename)
{
	if (_mp4i_read_tag(buffer, filename) < 0)
		return E_UNKNOWN;

	info->clip_date = buffer.read_be_32();
	info->clip_length_ms = buffer.read_be_32();
	info->clip_created_date = buffer.read_be_32();

	buffer.Skip(4);	// jpg_size_aligned

	info->stream_version = buffer.read_be_32();
	info->video_coding = buffer.read_le_8();
	info->video_frame_rate = buffer.read_le_8();
	info->video_width = buffer.read_be_16();
	info->video_height = buffer.read_be_16();
	info->audio_coding = buffer.read_le_8();
	info->audio_num_channels = buffer.read_le_8();
	info->audio_sampling_freq = buffer.read_be_32();
	info->has_gps = buffer.read_be_32() != 0;
	info->has_acc = buffer.read_be_32() != 0;
	info->has_obd = buffer.read_be_32() != 0;

	AVF_LOGD("%s: has_gps %d, has_acc %d, has_obd %d",
		filename, info->has_gps, info->has_acc, info->has_obd);

	return buffer.IOStatus();
}

static IAVIO *_mp4i_open_file(const char *mp4_filename)
{
	IAVIO *io = CSysIO::Create();
	avf_status_t status = io->OpenRead(mp4_filename);
	if (status != E_OK || io->GetSize() < 1*KB) {
		AVF_LOGD("cannot open %s", mp4_filename);
		io->Release();
		return NULL;
	}
	return io;
}

int mp4i_read_info(const char *mp4_filename, mp4i_info_t *info)
{
	IAVIO *io = _mp4i_open_file(mp4_filename);
	if (io == NULL)
		return E_ERROR;

	io->Seek(io->GetSize() - INFO_SIZE);
	CReadBuffer buffer(io, INFO_SIZE);

	int result = _mp4i_read_info(buffer, info, mp4_filename);
	io->Release();

	return result;
}

uint8_t *mp4i_read_poster(const char *mp4_filename, uint32_t *psize)
{
	IAVIO *io = _mp4i_open_file(mp4_filename);
	if (io == NULL)
		return NULL;

	io->Seek(io->GetSize() - INFO_SIZE);
	CReadBuffer buffer(io, 1*KB);

	uint8_t *result = NULL;
	int jpg_aligned_size;

	if (_mp4i_read_tag(buffer, mp4_filename) < 0)
		goto Done;

	buffer.Skip(4*3);	// clip_date, clip_length, clip_created_date

	jpg_aligned_size = buffer.read_be_32();
	if (jpg_aligned_size == 0) {
		AVF_LOGD("no jpg data");
		goto Done;
	}

	io->Seek(io->GetSize() - (4 + jpg_aligned_size + INFO_SIZE));
	buffer.SetIO(io);

	*psize = buffer.read_be_32();
	if (*psize == 0) {
		AVF_LOGD("jpg size is 0");
		goto Done;
	}

	result = avf_malloc_bytes(*psize);
	if (result == NULL) {
		AVF_LOGD("cannot malloc %d", *psize);
		goto Done;
	}

	if (!buffer.read_mem(result, *psize)) {
		avf_safe_free(result);
		goto Done;
	}

Done:
	io->Release();
	return result;
}

void mp4i_free_poster(uint8_t *poster_data)
{
	avf_safe_free(poster_data);
}

typedef struct mp4i_raw_data_comp_s {
	int num_items;
	int data_size;
	// ---------------------------
	vdb_raw_data_index_t *index;
	avf_u8_t *data;
	uint32_t *offset;
	// ---------------------------
	avf_u8_t *mem;	// need be freed
} mp4i_raw_data_comp_t;

struct mp4i_raw_data_s
{
	uint32_t clip_date;
	uint32_t clip_length;
	mp4i_raw_data_comp_t gps;
	mp4i_raw_data_comp_t acc;
	mp4i_raw_data_comp_t obd;
};

static int _mp4i_read_raw_comp(CReadBuffer& buffer, uint32_t tag, mp4i_raw_data_comp_t *comp)
{
	if (buffer.read_be_32() != tag) {
		AVF_LOGD("tag not match %x", tag);
		return E_ERROR;
	}
	buffer.Skip(4);	// size

	comp->num_items = buffer.read_be_32();
	comp->data_size = buffer.read_be_32();
	int total = comp->num_items * sizeof(vdb_raw_data_index_t) + comp->data_size;
	int offset_size = comp->num_items * sizeof(uint32_t);
	if ((comp->mem = avf_malloc_bytes(total + offset_size)) == NULL)
		return E_ERROR;

	buffer.read_mem(comp->mem, total);
	if (buffer.IOError())
		return buffer.IOStatus();

	comp->index = (vdb_raw_data_index_t*)comp->mem;
	comp->data = (avf_u8_t*)(comp->index + comp->num_items);
	comp->offset = (uint32_t*)(comp->data + comp->data_size);

	uint32_t offset = 0;
	for (int i = 0; i < comp->num_items; i++) {
		comp->offset[i] = offset;
		offset += comp->index[i].data_size;
	}

	return E_OK;
}

mp4i_raw_data_t *mp4i_read_raw_data(const char *mp4_filename)
{
	IAVIO *io = _mp4i_open_file(mp4_filename);
	if (io == NULL)
		return NULL;

	io->Seek(io->GetSize() - INFO_SIZE);
	CReadBuffer buffer(io, 2*KB);

	mp4i_raw_data_t *result = NULL;
	int gps_offset;
	int acc_offset;
	int obd_offset;

	if (_mp4i_read_tag(buffer, mp4_filename) < 0) 
		goto Done;

	result = avf_malloc_type(mp4i_raw_data_t);
	if (result == NULL)
		goto Done;
	memset(result, 0, sizeof(mp4i_raw_data_t));

	result->clip_date = buffer.read_be_32();
	result->clip_length = buffer.read_be_32();

	buffer.Skip(24);
	gps_offset = buffer.read_be_32();
	acc_offset = buffer.read_be_32();
	obd_offset = buffer.read_be_32();

	if (gps_offset > 0) {
		buffer.SeekTo(io->GetSize() - (gps_offset + INFO_SIZE));
		if (_mp4i_read_raw_comp(buffer, TAG_GPS0, &result->gps) < 0)
			goto Error;
	}

	if (acc_offset > 0) {
		buffer.SeekTo(io->GetSize() - (acc_offset + INFO_SIZE));
		if (_mp4i_read_raw_comp(buffer, TAG_ACC0, &result->acc) < 0)
			goto Error;
	}

	if (obd_offset > 0) {
		buffer.SeekTo(io->GetSize() - (obd_offset + INFO_SIZE));
		if (_mp4i_read_raw_comp(buffer, TAG_OBD0, &result->obd) < 0)
			goto Error;
	}

	if (buffer.IOError()) {
Error:
		mp4i_free_raw_data(result);
		result = NULL;
	}

Done:
	io->Release();
	return result;
}

void mp4i_free_raw_data(mp4i_raw_data_t *data)
{
	if (data) {
		avf_safe_free(data->gps.mem);
		avf_safe_free(data->acc.mem);
		avf_safe_free(data->obd.mem);
		avf_safe_free(data);
	}
}

void mp4i_free_ack(mp4i_ack_t *ack)
{
	if (ack->data) {
		CMemBuffer::ReleaseData(ack->data);
		ack->data = NULL;
	}
}

INLINE void init_ack(mp4i_ack_t *ack)
{
	ack->data = NULL;
	ack->size = 0;
}

INLINE void gen_ack(int rval, CMemBuffer *pmb, mp4i_ack_t *ack)
{
	if (rval >= 0) {
		ack->data = (avf_u8_t*)pmb->CollectData();
		ack->size = pmb->GetTotalSize();
	}
	pmb->Release();
}

static int _mp4i_find_item(mp4i_raw_data_comp_t *comp, int time)
{
	int a = 0, b = comp->num_items - 1;

	if (b < 0)
		return -1;

	vdb_raw_data_index_t *base = (vdb_raw_data_index_t*)comp->mem;

	vdb_raw_data_index_t *item = base + a;
	if (time <= item->time_offset_ms)
		return a;

	item = base + b;
	if (time >= item->time_offset_ms)
		return b;

	while (true) {
		int m = (a + b) / 2;
		item = base + m;
		if (time >= item->time_offset_ms) {
			if (a == m)
				return m;
			a = m;
		} else {
			b = m;
		}
	}
}

// return 1 on got data
static int _mp4i_get_raw_comp(mp4i_raw_data_comp_t *comp, 
	uint32_t clip_time_ms, int data_type, CMemBuffer *pmb)
{
	int index = _mp4i_find_item(comp, clip_time_ms);
	if (index < 0)
		return 0;

	vdb_raw_data_index_t *item = comp->index + index;
	if (item->data_size == 0) {
		// this is a separator, use previouse item
		if (index == 0)
			return 0;
		item--;
	}

	pmb->write_le_32(data_type);
	pmb->write_le_32(item->time_offset_ms);	// clip_time_ms_lo
	pmb->write_le_32(0);	// clip_time_ms_hi
	pmb->write_le_32(item->data_size);

	pmb->write_mem(comp->data + comp->offset[index], item->data_size);

	return 1;
}

// vdb_ack_GetRawData_t
static int _mp4i_get_raw_data(mp4i_raw_data_t *data, 
	uint32_t clip_time_ms, uint32_t data_types, CMemBuffer *pmb)
{
	pmb->write_le_32(0);	// clip_type
	pmb->write_le_32(0);	// clip_id
	pmb->write_le_32(data->clip_date);

	int n = 0;

	if (data_types & (1<<kRawData_GPS)) {
		if (_mp4i_get_raw_comp(&data->gps, clip_time_ms, kRawData_GPS, pmb))
			n++;
	}

	if (data_types & (1<<kRawData_ACC)) {
		if (_mp4i_get_raw_comp(&data->acc, clip_time_ms, kRawData_ACC, pmb))
			n++;
	}

	if (data_types & (1<<kRawData_OBD)) {
		if (_mp4i_get_raw_comp(&data->obd, clip_time_ms, kRawData_OBD, pmb))
			n++;
	}

	pmb->write_le_32(0);

	return E_OK;
}

int mp4i_get_raw_data(mp4i_raw_data_t *data, uint32_t clip_time_ms, uint32_t data_types, mp4i_ack_t *ack)
{
	init_ack(ack);

	CMemBuffer *pmb = CMemBuffer::Create(256);
	if (pmb == NULL)
		return E_NOMEM;

	int rval = _mp4i_get_raw_data(data, clip_time_ms, data_types, pmb);
	gen_ack(rval, pmb, ack);
	return rval;
}

// vdb_ack_GetRawDataBlock_t
static int _mp4i_get_raw_data_block(mp4i_raw_data_t *data, uint32_t clip_time_ms, uint32_t length_ms,
	uint32_t data_type, CMemBuffer *pmb)
{
	mp4i_raw_data_comp_t *comp;

	switch (data_type) {
	case kRawData_GPS: comp = &data->gps; break;
	case kRawData_ACC: comp = &data->acc; break;
	case kRawData_OBD: comp = &data->obd; break;
	default:
		AVF_LOGE("unknown data type %d", data_type);
		return -1;
	}

	int index = _mp4i_find_item(comp, clip_time_ms);
	if (index < 0)
		return index;

	int index_end = _mp4i_find_item(comp, clip_time_ms + length_ms - 1);
	if (index_end < 0)
		return index_end;

	pmb->write_le_32(0);	// clip_type
	pmb->write_le_32(0);	// clip_id
	pmb->write_le_32(data->clip_date);
	pmb->write_le_32(data_type);
	pmb->write_le_32(clip_time_ms);	// requested_time_ms_lo
	pmb->write_le_32(0);	// requested_time_ms_hi
	pmb->write_le_32(index_end + 1 - index);	// num_items

	int size = comp->offset[index_end] + comp->index[index_end].data_size -
		comp->offset[index];
	pmb->write_le_32(size);	// data_size

	// index
	pmb->write_mem((const avf_u8_t*)(comp->index + index),
		(index_end + 1 - index) * sizeof(vdb_raw_data_index_t));

	// data
	pmb->write_mem((const avf_u8_t*)comp->data + comp->offset[index], size);

	return E_OK;
}

int mp4i_get_raw_data_block(mp4i_raw_data_t *data, uint32_t clip_time_ms, uint32_t length_ms,
	uint32_t data_type, mp4i_ack_t *ack)
{
	init_ack(ack);

	CMemBuffer *pmb = CMemBuffer::Create(4*KB);
	if (pmb == NULL)
		return E_NOMEM;

	int rval = _mp4i_get_raw_data_block(data, clip_time_ms, length_ms, data_type, pmb);
	gen_ack(rval, pmb, ack);
	return rval;
}


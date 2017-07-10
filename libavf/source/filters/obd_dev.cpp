
#include <math.h>
#include <agobd.h>

static struct agobd_client_info_s *pagobd_client = NULL;

#undef USE_OBD_VERSION_1

int obd_dev_open()
{
	if (pagobd_client == NULL) {
		if ((pagobd_client = agobd_open_client()) == NULL) {
			AVF_LOGW("agobd_open_client() = NULL");
			return -1;
		}
	}
	return 0;
}

int obd_dev_close()
{
	if (pagobd_client) {
		agobd_close_client(pagobd_client);
		pagobd_client = NULL;
	}
	return 0;
}

// b_sync : sync mode
int obd_dev_read(int b_sync)
{
	int ret_val;

	if (!pagobd_client && obd_dev_open() < 0) {
		return -1;
	}

	if (b_sync) {
		ret_val = agobd_client_get_info_tmo(pagobd_client, 1500);
		if (ret_val < 0) {
//			AVF_LOGW("agobd_client_get_info_sync() = %d", ret_val);
			return ret_val;
		}
	} else {
		ret_val = agobd_client_get_info(pagobd_client);
		if (ret_val < 0) {
//			AVF_LOGW("agobd_client_get_info() = %d", ret_val);
			return ret_val;
		}
	}

	return 0;
}

// must be called after obd_dev_read()
raw_data_t *obd_dev_get_vin(uint32_t fcc)
{
	struct agobd_info_s *info = pagobd_client->pobd_info;

	const avf_u8_t *pvin = (const avf_u8_t*)info + info->vin_offset;
	if (*pvin == 0) {
		return NULL;
	}

	avf_size_t size = sizeof(info->vin);
	raw_data_t *raw = avf_alloc_raw_data(size, fcc);
	if (raw == NULL)
		return NULL;

	avf_u8_t *ptr = raw->GetData();
	::memcpy(ptr, pvin, size);
	ptr[size-1] = 0;

	return raw;
}

#ifdef USE_OBD_VERSION_1

// if num_pids < 0 then read all
raw_data_t *obd_dev_get_data(const uint8_t *pid_list, int num_pids, uint32_t fcc)
{
	struct agobd_info_s *info = pagobd_client->pobd_info;

	raw_data_t *raw = avf_alloc_raw_data(info->total_size, fcc);
	if (raw == NULL)
		return NULL;

	avf_u8_t *ptr = raw->GetData();
	::memcpy(ptr, info, info->total_size);

	return raw;
}

#else

#define TOTAL_PID	128
const uint8_t g_pid_data_size[TOTAL_PID] =
{
	4,4,2,2,1,1,2,2,		// 00 - 07
	2,2,1,1,2,1,1,1,		// 08 - 0F

	2,1,1,1,2,2,2,2,		// 10 - 17
	2,2,2,2,1,1,1,2,		// 18 - 1F

	4,2,2,2,4,4,4,4,		// 20 - 27
	4,4,4,4,1,1,1,1,		// 28 - 2F

	1,2,2,1,4,4,4,4,		// 30 - 37
	4,4,4,4,2,2,2,2,		// 38 - 3F

	4,4,2,2,2,1,1,1,		// 40 - 47
	1,1,1,1,1,2,2,4,		// 48 - 4F

	4,1,1,2,2,2,2,2,		// 50 - 57
	2,2,1,1,1,2,2,1,		// 58 - 5F

	4,1,1,2,5,2,5,3,		// 60 - 67
	7,7,5,5,5,6,5,3,		// 68 - 6F

	9,5,5,5,5,7,7,5,		// 70 - 77
	9,9,7,7,9,1,1,13,	// 78 - 7F
};

// see avf_std_media.h, obd_raw_data_v2_t
// must be called after obd_dev_read()
// if num_pids < 0 then read all
raw_data_t *obd_dev_get_data(const uint8_t *pid_list, int num_pids, uint32_t fcc)
{
	struct agobd_info_s *info = pagobd_client->pobd_info;

	if (num_pids < 0 || num_pids >= (int)ARRAY_SIZE(info->pid_info)) {
		pid_list = NULL;
		num_pids = ARRAY_SIZE(info->pid_info);
	}

	// calculate total size
	int total_size = 1;	// revision
	int total = 0;
	for (int i = 0; i < num_pids; i++) {
		unsigned pid = pid_list ? pid_list[i] : i;
		if (pid == 0) {
			continue;
		}
		if (pid >= TOTAL_PID) {
			AVF_LOGW("pid %d not supported", pid);
			continue;
		}
		struct agobd_pid_info_s *pid_info = info->pid_info + pid;
		if (pid_info->flag & 1) {
			total_size += 1 + g_pid_data_size[pid];	// pid, data
			total++;
		}
	}

	if (total == 0) {
//		AVF_LOGD("no pids");
		return NULL;
	}

	total_size++;	// 0
	total_size = ROUND_UP(total_size, 4);	// 4-byte aligned

	// alloc mem
	raw_data_t *raw = avf_alloc_raw_data(total_size, fcc);
	if (raw == NULL)
		return NULL;

	// copy pid data
	avf_u8_t *ptr = raw->GetData();
	*ptr++ = OBD_VERSION_2;
	for (int i = 0; i < num_pids; i++) {
		unsigned pid = pid_list ? pid_list[i] : i;
		if (pid == 0 || pid >= TOTAL_PID) {
			continue;
		}
		struct agobd_pid_info_s *pid_info = info->pid_info + pid;
		if (pid_info->flag & 1) {
			*ptr++ = pid;
			const avf_u8_t *p = info->pid_data + pid_info->offset;
			for (unsigned j = g_pid_data_size[pid]; j; j--)
				*ptr++ = *p++;
		}
	}

	for (unsigned i = total_size - (ptr - raw->GetData()); i; i--)
		*ptr++ = 0;

//	AVF_LOGD("total %d pids", total);

	return raw;
}

#endif


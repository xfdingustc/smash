
#include <math.h>
#include <aggps.h>

#include "avf_common.h"
#include "avf_std_media.h"
#include "avf_util.h"

static struct aggps_client_info_s *paggps_client = NULL;

static int g_gps_read_result = -1;	// for g_gps_location & g_gps_satellite
static int g_gps_num_svs = 0;
static float g_gps_speed = -1;
static struct aggps_location_info_s g_gps_location = {0};
static struct aggps_satellite_info_s g_gps_satellite = {0};

typedef gps_raw_data_v3_t gps_data_t;

int gps_dev_open()
{
	if (paggps_client)
		return 0;

	paggps_client = aggps_open_client();
	if (paggps_client == NULL) {
		AVF_LOGW("aggps_open_client() = NULL");
		return -1;
	}

	return 0;
}

int gps_dev_close()
{
	if (paggps_client == NULL)
		return 0;

	aggps_close_client(paggps_client);
	paggps_client = NULL;

	return 0;
}

static int read_data_gps(int b_sync)
{
	int rval;

	if (!paggps_client)
		return -1;

	if (b_sync) {
		rval = aggps_read_client_tmo(paggps_client, 1500);
	} else {
		rval = aggps_read_client(paggps_client);
	}

	if (rval) {
		paggps_client->pgps_info->location.set = 0;
		paggps_client->pgps_info->satellite.num_svs = 0;
	}

	return 0;
}

int get_gps_data(const struct aggps_location_info_s *plocation, gps_data_t *data)
{
	data->flags = 0;

	if (plocation->set & AGGPS_SET_LATLON) {
		data->flags |= GPS_F_LATLON;
		data->latitude = plocation->latitude;
		data->longitude = plocation->longitude;
	}

	if (plocation->set & AGGPS_SET_ALTITUDE) {
		data->flags |= GPS_F_ALTITUDE;
		data->altitude = plocation->altitude;
	}

	if (plocation->set & AGGPS_SET_SPEED) {
		data->flags |= GPS_F_SPEED;
		data->speed = plocation->speed;
	}

	if (plocation->set & AGGPS_SET_TIME) {
		data->flags |= GPS_F_TIME;
		data->utc_time = plocation->utc_time;
		data->utc_time_usec = plocation->utc_time_usec;
		//data->utc_tm = plocation->utc_tm;
	}

	if (plocation->set & AGGPS_SET_TRACK) {
		data->flags |= GPS_F_TRACK;
		data->track = plocation->track;
	}

	if (plocation->set & AGGPS_SET_DOP) {
		data->flags |= GPS_F_DOP;
		data->accuracy = plocation->hdop;
	}

	return 0;
}

void gps_dev_clear()
{
	g_gps_read_result = -1;	// no info
	g_gps_num_svs = 0;
	g_gps_speed = -1;	// invalid
}

int check_snr(int min_sat, int min_snr)
{
	struct aggps_satellite_info_s *psatellite = &paggps_client->pgps_info->satellite;
	int total = 0;
	int i;

	for (i = 0; i < psatellite->num_svs; i++) {
		if (psatellite->sv_list[i].snr >= min_snr) {
			total++;
			if (total >= min_sat)
				return 0;
		}
	}

	return -1;
}

void gps_dev_update(int b_sync, CMutex& mutex, int min_sat, int min_snr, int accuracy)
{
	struct aggps_location_info_s *plocation = NULL;
	struct aggps_satellite_info_s *psatellite = NULL;
	int result = 0;

	if (read_data_gps(b_sync) < 0) {
		result = -2;	// read failed
		goto Next;
	}

	if (min_sat > 0 && check_snr(min_sat, min_snr) < 0) {
		result = -1;	// no info
		goto Next;
	}

	plocation = &paggps_client->pgps_info->location;
	psatellite = &paggps_client->pgps_info->satellite;

	if (accuracy > 0 && plocation->hdop > ((float)accuracy)/100) {
		result = -1;	// no info
		goto Next;
	}

	if ((plocation->set & AGGPS_SET_LATLON) == 0) {
		result = -1;	// no info
		goto Next;
	}

Next:
	AUTO_LOCK(mutex);

	g_gps_read_result = result;

	if (result >= -1) {
		g_gps_num_svs = psatellite->num_svs;
	} else {
		g_gps_num_svs = 0;
	}

	if (plocation && (plocation->set & AGGPS_SET_SPEED)) {
		g_gps_speed = plocation->speed;
	} else {
		g_gps_speed = -1;	// invalid
	}

	if (plocation) {
		g_gps_location = *plocation;
	}

	if (psatellite) {
		g_gps_satellite = *psatellite;
	}
}

// call gps_dev_update() first
// -2: read failed
// -1: no position info
// >0: offset
int gps_dev_read_data(char *buffer, int buffer_size,
	gps_data_t *data, float *speed, int *num_svs)
{
	struct aggps_location_info_s *plocation = &g_gps_location;
//	struct aggps_satellite_info_s *psatellite = &g_gps_satellite;
	int offset = 0;

	*num_svs = g_gps_num_svs;
	*speed = g_gps_speed;

	if (g_gps_read_result < 0) {
		return g_gps_read_result;
	}

	if (buffer_size <= 0) {
		return -1;
	}

	if (data != NULL) {
		data->flags = 0;
		
		if (plocation->set & AGGPS_SET_LATLON) {
			data->flags |= GPS_F_LATLON;
			data->latitude = plocation->latitude;
			data->longitude = plocation->longitude;
		}
		
		if (plocation->set & AGGPS_SET_ALTITUDE) {
			data->flags |= GPS_F_ALTITUDE;
			data->altitude = plocation->altitude;
		}
		
		if (plocation->set & AGGPS_SET_SPEED) {
			data->flags |= GPS_F_SPEED;
			data->speed = plocation->speed;
		}
		
		if (plocation->set & AGGPS_SET_TIME) {
			data->flags |= GPS_F_TIME;
			data->utc_time = plocation->utc_time;
			data->utc_time_usec = plocation->utc_time_usec;
			//data->utc_tm = plocation->utc_tm;
		}
		
		if (plocation->set & AGGPS_SET_TRACK) {
			data->flags |= GPS_F_TRACK;
			data->track = plocation->track;
		}
		
		if (plocation->set & AGGPS_SET_DOP) {
			data->flags |= GPS_F_DOP;
			data->accuracy = plocation->hdop;
		}
	}

	// position
	if (offset < buffer_size) {
		if (plocation->set & AGGPS_SET_LATLON) {
			offset = avf_snprintf((buffer + offset),
				(buffer_size - offset),
				" %10.6f%c %9.6f%c",
				fabs(plocation->longitude),
				(plocation->longitude < 0) ? 'W' : 'E',
				fabs(plocation->latitude),
				(plocation->latitude < 0) ? 'S' : 'N');
			buffer[buffer_size - 1] = 0;
		} else {
			buffer[0] = 0;
		}
	}

	return offset;
}

int gps_dev_get_satellite(char *buffer, int buffer_size)
{
	struct aggps_location_info_s *plocation = &g_gps_location;
	struct aggps_satellite_info_s *psatellite = &g_gps_satellite;
	int offset = 0;
	int i;

	static int32_t snr_max = 0;

	if (g_gps_read_result < 0) {
		return g_gps_read_result;
	}

	if (buffer_size <= 0)
		return -1;

	if (plocation->set & AGGPS_SET_LATLON) {
		offset = avf_snprintf(buffer, buffer_size,
			" [SET:%d %d]", psatellite->num_svs, snr_max);
	} else {
		offset = avf_snprintf(buffer, buffer_size,
			" [SIV:%d %d]", psatellite->num_svs, snr_max);
	}

	for (i = 0; i < psatellite->num_svs; i++) {
		if ((offset < buffer_size) &&
			(psatellite->sv_list[i].snr > 0)) {
			if (snr_max < psatellite->sv_list[i].snr) {
				snr_max = psatellite->sv_list[i].snr;
			}
			offset += avf_snprintf((buffer + offset),
				(buffer_size - offset),
				" %d:%3.0f",
				psatellite->sv_list[i].prn,
				psatellite->sv_list[i].snr);
		}
	}

	if (plocation->set & AGGPS_SET_ALTITUDE) {
		if (buffer_size > offset) {
			offset += avf_snprintf((buffer + offset),
				(buffer_size - offset),
				"%4.1fM", plocation->altitude);
		}
	}
	if (plocation->set & AGGPS_SET_SPEED) {
		if (buffer_size > offset) {
			offset += avf_snprintf((buffer + offset),
				(buffer_size - offset),
				" %7.3fkm/h", plocation->speed);
		}
	}
	if (plocation->set & AGGPS_SET_TRACK) {
		if (buffer_size > offset) {
			offset += avf_snprintf((buffer + offset),
				(buffer_size - offset),
				" %6.2f Deg", plocation->track);
		}
	}
	if (plocation->set & AGGPS_SET_DOP) {
		if (buffer_size > offset) {
			offset += avf_snprintf((buffer + offset),
				(buffer_size - offset),
				" %6.2f HDOP", plocation->hdop);
		}
	}

	buffer[buffer_size - 1] = 0;

	return offset;
}


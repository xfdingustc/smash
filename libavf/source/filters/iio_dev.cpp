
#include "agsys_iio.h"

#if defined(ARCH_A5S)

typedef acc_raw_data_t iio_info_t;

struct agsys_iio_info_s *g_iio_dev;

void iio_dev_open()
{
	if (g_iio_dev)
		return;

	g_iio_dev = agsys_iio_lis3dh_open();
}

void iio_dev_close()
{
	if (g_iio_dev) {
		agsys_iio_lis3dh_close(g_iio_dev);
		g_iio_dev = NULL;
	}
}

int iio_dev_read(iio_info_t *info, int *data_size)
{
	if (g_iio_dev == NULL)
		return -1;

	struct agsys_iio_lis3dh_accel_xyz_s accel_xyz;
	if (agsys_iio_lis3dh_read_accel_xyz(g_iio_dev, &accel_xyz) < 0)
		return -1;

	info->accel_x = accel_xyz.accel_x;
	info->accel_y = accel_xyz.accel_y;
	info->accel_z = accel_xyz.accel_z;
	*data_size = 12;

	return 0;
}

#else

typedef iio_raw_data_t iio_info_t;

static int b_iio_opened = 0;
static struct agsys_iio_accel_info_s *g_iio_accel_info = NULL;
static struct agsys_iio_gyro_info_s *g_iio_gyro_info = NULL;
static struct agsys_iio_magn_info_s *g_iio_magn_info = NULL;
static struct agsys_iio_orientation_info_s *g_iio_orientation_info = NULL;
static struct agsys_iio_pressure_info_s *g_iio_pressure_info = NULL;

void iio_dev_open()
{
	if (b_iio_opened)
		return;

	if ((g_iio_accel_info = agsys_iio_accel_open()) == NULL) {
		AVF_LOGW("agsys_iio_accel_open failed");
	}
	if ((g_iio_gyro_info = agsys_iio_gyro_open()) == NULL) {
		AVF_LOGW("agsys_iio_gyro_open failed");
	}
	if ((g_iio_magn_info = agsys_iio_magn_open()) == NULL) {
		AVF_LOGW("agsys_iio_magn_open");
	}
	if ((g_iio_orientation_info = agsys_iio_orientation_open()) == NULL) {
		AVF_LOGW("agsys_iio_orientation_open");
	}
	if ((g_iio_pressure_info = agsys_iio_pressure_open()) == NULL) {
		AVF_LOGW("agsys_iio_pressure_open");
	}

	b_iio_opened = 1;
}

void iio_dev_close()
{
	if (!b_iio_opened)
		return;

	if (g_iio_accel_info) {
		agsys_iio_accel_close(g_iio_accel_info);
		g_iio_accel_info = NULL;
	}
	if (g_iio_gyro_info) {
		agsys_iio_gyro_close(g_iio_gyro_info);
		g_iio_gyro_info = NULL;
	}
	if (g_iio_magn_info) {
		agsys_iio_magn_close(g_iio_magn_info);
		g_iio_magn_info = NULL;
	}
	if (g_iio_orientation_info) {
		agsys_iio_orientation_close(g_iio_orientation_info);
		g_iio_orientation_info = NULL;
	}
	if (g_iio_pressure_info) {
		agsys_iio_pressure_close(g_iio_pressure_info);
		g_iio_pressure_info = NULL;
	}

	b_iio_opened = 0;
}

int iio_dev_read(iio_info_t *info, int *data_size)
{
	int ret_val;
	float tmp_x_f;
	float tmp_y_f;
	float tmp_z_f;

	memset(info, 0, sizeof(iio_info_t));

	if (g_iio_accel_info) {
		struct agsys_iio_accel_xyz_s iio_accel_xyz;

		ret_val = agsys_iio_accel_read_xyz(g_iio_accel_info,
			&iio_accel_xyz);
		if (ret_val == 0) {
			tmp_x_f = (iio_accel_xyz.accel_x *
				g_iio_accel_info->accel_scale);
			tmp_y_f = (iio_accel_xyz.accel_y *
				g_iio_accel_info->accel_scale);
			tmp_z_f = (iio_accel_xyz.accel_z *
				g_iio_accel_info->accel_scale);
			info->accel_x = (tmp_x_f * 1000);
			info->accel_y = (tmp_y_f * 1000);
			info->accel_z = (tmp_z_f * 1000);
			info->flags |= IIO_F_ACCEL;
		}
	}
	if (g_iio_gyro_info) {
		struct agsys_iio_gyro_xyz_s iio_gyro_xyz;

		ret_val = agsys_iio_gyro_read_xyz(g_iio_gyro_info,
			&iio_gyro_xyz);
		if (ret_val == 0) {
			tmp_x_f = (iio_gyro_xyz.gyro_x *
				g_iio_gyro_info->gyro_scale);
			tmp_y_f = (iio_gyro_xyz.gyro_y *
				g_iio_gyro_info->gyro_scale);
			tmp_z_f = (iio_gyro_xyz.gyro_z *
				g_iio_gyro_info->gyro_scale);
			info->gyro_x = (tmp_x_f * 1000);
			info->gyro_y = (tmp_y_f * 1000);
			info->gyro_z = (tmp_z_f * 1000);
			info->flags |= IIO_F_GYRO;
		}
	}
	if (g_iio_magn_info) {
		struct agsys_iio_magn_xyz_s iio_magn_xyz;

		ret_val = agsys_iio_magn_read_xyz(g_iio_magn_info,
			&iio_magn_xyz);
		if (ret_val == 0) {
			tmp_x_f = (iio_magn_xyz.magn_x *
				g_iio_magn_info->magn_scale);
			tmp_y_f = (iio_magn_xyz.magn_y *
				g_iio_magn_info->magn_scale);
			tmp_z_f = (iio_magn_xyz.magn_z *
				g_iio_magn_info->magn_scale);
			info->magn_x = (tmp_x_f * 1000000);
			info->magn_y = (tmp_y_f * 1000000);
			info->magn_z = (tmp_z_f * 1000000);
			info->flags |= IIO_F_MAGN;
		}
	}
	if (g_iio_orientation_info) {
		struct agsys_iio_orientation_s iio_orientation;

		ret_val = agsys_iio_orientation_read(g_iio_orientation_info,
			&iio_orientation);
		if (ret_val == 0) {
			tmp_x_f = (iio_orientation.euler_heading *
				g_iio_orientation_info->orientation_scale);
			tmp_y_f = (iio_orientation.euler_roll *
				g_iio_orientation_info->orientation_scale);
			tmp_z_f = (iio_orientation.euler_pitch *
				g_iio_orientation_info->orientation_scale);
			info->euler_heading = (tmp_x_f * 1000);
			info->euler_roll = (tmp_y_f * 1000);
			info->euler_pitch = (tmp_z_f * 1000);
			info->quaternion_w = iio_orientation.quaternion_w;
			info->quaternion_x = iio_orientation.quaternion_x;
			info->quaternion_y = iio_orientation.quaternion_y;
			info->quaternion_z = iio_orientation.quaternion_z;
			info->flags |= IIO_F_EULER | IIO_F_QUATERNION;
		}
	}
	if (g_iio_pressure_info) {
		struct agsys_iio_pressure_s iio_pressure;

		ret_val = agsys_iio_pressure_read(g_iio_pressure_info,
			&iio_pressure);
		if (ret_val == 0) {
			tmp_x_f = (iio_pressure.pressure *
				g_iio_pressure_info->pressure_scale);
			info->pressure = (tmp_x_f * 1000);
			info->flags |= IIO_F_PRESSURE;
		}
	}

	if (info->flags == 0) {
		// got nothing
//		AVF_LOGD(C_LIGHT_PURPLE "iio no flags" C_NONE);
		return -1;
	}

	info->version = IIO_VERSION;
	info->size = sizeof(*info);

	if (info->flags == IIO_F_ACCEL) {
		// only has accel data
		*data_size = 12;
	} else {
		*data_size = sizeof(*info);
	}

	return 0;
}

#endif


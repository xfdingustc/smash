
extern "C" {
#include "img_struct_arch.h"
#include "img_dsp_interface_arch.h"
#include "img_api.h"
}

//    filename = /calibration/bpc.bin  or /calibration/bpc_mirror.bin
int calib_load_fpn_table(int iav_fd, const char* filename)
{
	fpn_correction_t fpn;
	u8* fpn_table;
	int appendix[4];
	int file, count;

	if((file = avf_open_file(filename, O_RDONLY, 0))<0) {
		AVF_LOGW("%s not found", filename);
		return -1;
	}

	count = read(file, appendix, 4*4);
	if(count!=16) {
		AVF_LOGW("read bpc header err");
		return -1;
	}

	AVF_LOGD("file %d %d %d %d size %d",
		appendix[0], appendix[1],appendix[2],appendix[3], appendix[1]*appendix[2]);

	fpn_table = (u8*)malloc(appendix[1]*appendix[2]);
	if(fpn_table == NULL) {
		AVF_LOGW("allocate bpc memory failed");
		return -1;
	}

	memset(fpn_table, 0, appendix[1]*appendix[2]);
	count = read(file, fpn_table, appendix[1]*appendix[2]);
	if(count != appendix[1]*appendix[2]) {
		free(fpn_table);
		AVF_LOGW("read bpc map err");
		return -1;
	}

	fpn.enable = appendix[0];
	fpn.fpn_pitch = appendix[1];
	fpn.pixel_map_height = appendix[2];
	fpn.pixel_map_width = appendix[3];
	fpn.pixel_map_size = appendix[1]*appendix[2];
	fpn.pixel_map_addr = (u32)fpn_table;

	img_dsp_set_static_bad_pixel_correction(iav_fd, &fpn);

	free(fpn_table);

	return 0;
}

//filename = /calibration/vignette.bin
int calib_load_vig_table(int iav_fd, const char* filename)
{
	int fd_lenshading = -1;
	int count, w, h;
	u32 gain_shift = 0;
	u16 *vignette_table;//[33*33*4] = {0};
	vignette_info_t vignette_info;

	if((fd_lenshading = avf_open_file(filename, O_RDONLY, 0))<0) {
		AVF_LOGW("%s not found", filename);
		return -1;
	}

	int appendix[3];
	count = read(fd_lenshading, appendix, 3*4);
	if(count!=12) {
		AVF_LOGW("read vig header err");
		return -1;
	}
	gain_shift = appendix[0];
	w = appendix[1];
	h = appendix[2];

	vignette_table = (u16*)malloc(VIGNETTE_MAX_SIZE*4*sizeof(u16));
	if(vignette_table == NULL) {
		AVF_LOGW("allocate vig table memory failed");
		return -1;
	}
	count = read(fd_lenshading, vignette_table, 4*VIGNETTE_MAX_SIZE*sizeof(u16));
	if(count != 4*VIGNETTE_MAX_SIZE*sizeof(u16)){
		free(vignette_table);
		AVF_LOGW("read lens_shading.bin error");
		return -1;
	}

	AVF_LOGD("%dx%d gain_shift is %d",w, h, gain_shift);
	vignette_info.enable = 1;
	vignette_info.gain_shift = (u8)gain_shift;
	vignette_info.vignette_red_gain_addr = (u32)(vignette_table + 0*VIGNETTE_MAX_SIZE);
	vignette_info.vignette_green_even_gain_addr = (u32)(vignette_table + 1*VIGNETTE_MAX_SIZE);
	vignette_info.vignette_green_odd_gain_addr = (u32)(vignette_table + 2*VIGNETTE_MAX_SIZE);
	vignette_info.vignette_blue_gain_addr = (u32)(vignette_table + 3*VIGNETTE_MAX_SIZE);

	img_dsp_set_vignette_compensation(iav_fd, &vignette_info);

	free(vignette_table);

	return 0;
}

// filename = /calibration/wb.bin
//shall be called after start_aaa
int calib_load_wb_shift(const char* filename)
{
	int file, count;
	int appendix[12];
	wb_gain_t orig_gain[2], ref_gain[2];

	if((file = avf_open_file(filename, O_RDONLY, 0))<0) {
		AVF_LOGW("%s not found", filename);
		return -1;
	}

	count = read(file, appendix, 12*sizeof(int));
	if(count != 12*sizeof(int)) {
		AVF_LOGW("read wb shift err");
		return -1;
	}

	ref_gain[0].r_gain = appendix[0];
	ref_gain[0].g_gain = appendix[1];
	ref_gain[0].b_gain = appendix[2];
	ref_gain[1].r_gain = appendix[3];
	ref_gain[1].g_gain = appendix[4];
	ref_gain[1].b_gain = appendix[5];

	orig_gain[0].r_gain = appendix[6];
	orig_gain[0].g_gain = appendix[7];
	orig_gain[0].b_gain = appendix[8];
	orig_gain[1].r_gain = appendix[9];
	orig_gain[1].g_gain = appendix[10];
	orig_gain[1].b_gain = appendix[11];

	return img_awb_set_wb_shift(orig_gain, ref_gain);
}


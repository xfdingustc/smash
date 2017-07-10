
#include <getopt.h>

#include "avf_common.h"
#include "avf_util.h"
#include "libipc.h"

#include "test_common.cpp"

static int reg_type = 0;
static int reg_index = 0;
static char reg_name[MAX_REG_NAME];
static char reg_value[MAX_REG_DATA];

static int opt_loop = 0;

static void do_write_reg(int menu)
{
	if (menu) {
		if (!get_input("Input name: ", reg_name, sizeof(reg_name)))
			return;

		if (!get_input("Input value: ", reg_value, sizeof(reg_value)))
			return;
	}

	ipc_reg_id_t reg_id = ipc_reg_create_for_write(reg_type, reg_name, reg_index);
	if (reg_id == INVALID_IPC_REG_ID) {
		AVF_LOGE("create reg '%s' failed", reg_name);
		return;
	}

	int length = strlen(reg_value) + 1;
	ipc_reg_read_write_t param;

	param.reg_id = reg_id;
	param.ptr = (void*)reg_value;
	param.size = length;
	param.options = REG_OPT_WRITE_TIMESTAMP;
	clock_gettime(CLOCK_MONOTONIC, &param.ts_write);

	if (ipc_reg_write(&param) < 0) {
		AVF_LOGE("write reg '%s' failed", reg_name);
		return;
	}

	AVF_LOGI("write %d bytes.", length);
}

#define TS_FMT		"ts_write: %ld.%06d, ts_read: %ld.%06d"
#define p_ts(_rw)	(_rw).ts_write.tv_sec, (int)((_rw).ts_write.tv_nsec/1000), (_rw).ts_read.tv_sec, (int)((_rw).ts_read.tv_nsec/1000)

static void do_read_reg(int menu, int no_wait)
{
	if (menu) {
		if (!get_input("Input name: ", reg_name, sizeof(reg_name)))
			return;
	}

	ipc_reg_id_t reg_id = ipc_reg_create_for_read(reg_type, reg_name, reg_index, 0);
	if (reg_id == INVALID_IPC_REG_ID) {
		AVF_LOGE("create reg '%s' failed", reg_name);
		return;
	}

Again:
	ipc_reg_read_write_t param;

	param.reg_id = reg_id;
	param.ptr = reg_value;
	param.size = sizeof(reg_value);
	param.options = no_wait ? REG_OPT_IGNORE_UPDATED : 0;
	param.timeout = -1;

	if (ipc_reg_read(&param) < 0) {
		AVF_LOGE("read '%s' failed", reg_name);
		return;
	}

	AVF_LOGI("read %d bytes: " C_YELLOW "%s" C_NONE ", " TS_FMT,
		param.size, reg_value, p_ts(param));

	if (opt_loop)
		goto Again;
}

static void do_read_reg_any(int menu)
{
	char name[MAX_REG_NAME];
	char value[MAX_REG_DATA];

Again:
	ipc_reg_read_write_t param;

	memset(&param, 0, sizeof(param));
	param.ptr = value;
	param.size = sizeof(value);
	param.options = menu ? 0 : REG_OPT_READ_ALL;
	param.timeout = -1;

	if (ipc_reg_read_any(&param) < 0) {
		AVF_LOGE("ipc_reg_read_any failed");
		return;
	}

	ipc_query_reg_id_t idinfo;
	idinfo.reg_id = param.reg_id;
	idinfo.name = name;
	idinfo.name_length = sizeof(name);

	if (ipc_reg_info(&idinfo) < 0) {
		AVF_LOGE("ipc_reg_info failed");
		return;
	}

	AVF_LOGI(C_CYAN "%s" C_NONE " : " C_YELLOW "%s" C_NONE ", " TS_FMT,
		name, value, p_ts(param));

	if (opt_loop)
		goto Again;
}

static void menu_write_reg(void)
{
	do_write_reg(1);
}

static void menu_read_reg(void)
{
	do_read_reg(1, 0);
}

static void menu_read_no_wait(void)
{
	do_read_reg(1, 1);
}

static void menu_read_reg_any(void)
{
	do_read_reg_any(1);
}

static int is_string(const char *ptr, int size)
{
	for (; size > 1; size--, ptr++) {
		if (!isprint(*ptr))
			return 0;
	}
	return *ptr == 0;
}

static const char *get_value_str(const char *ptr, int size, char *buffer)
{
	if (size == 0)
		return "[empty]";

	if (is_string(ptr, size))
		return ptr;

	sprintf(buffer, "[%d bytes]", size);
	return buffer;
}

static const char *get_type_str(uint32_t type, char *buf)
{
	buf[0] = (type >> 24) & 0xff;
	buf[1] = (type >> 16) & 0xff;
	buf[2] = (type >> 8) & 0xff;
	buf[3] = (type) & 0xff;
	buf[4] = 0;

	if (isalnum(buf[0]) && isalnum(buf[1]) && isalnum(buf[2]) && isalnum(buf[3]))
		return buf;

	sprintf(buf, "%d", type);
	return buf;
}

static void dump_reg(void)
{
	ipc_reg_id_item_t *items = NULL;
	int length = MAX_REG_NUM;
	int nread;

	items = (ipc_reg_id_item_t*)malloc(length * sizeof(ipc_reg_id_item_t));
	if (items == NULL) {
		AVF_LOGE("no memory");
		return;
	}

	nread = ipc_get_all(items, length);
	if (nread < 0) {
		free(items);
		AVF_LOGE("ipc_get_all failed");
		return;
	}

	if (nread == 0) {
		AVF_LOGI("registry is empty");
		goto Fail;
	}

	for (int i = 0; i < nread; i++) {
		ipc_reg_id_item_t *p = items + i;

		char name[MAX_REG_NAME];
		char value[MAX_REG_DATA];
		char buffer[64];
		char buffer2[64];

		ipc_query_reg_id_t idinfo;
		idinfo.reg_id = p->reg_id;
		idinfo.name = name;
		idinfo.name_length = sizeof(name);

		if (ipc_reg_info(&idinfo) < 0) {
			AVF_LOGE("ipc_reg_info failed");
			goto Fail;
		}

		ipc_reg_read_write_t param;
		param.reg_id = p->reg_id;
		param.ptr = value;
		param.size = sizeof(value);
		param.options = REG_OPT_QUICK_READ;
		param.timeout = -1;

		if (ipc_reg_read(&param) < 0) {
			AVF_LOGE("read '%s' failed", name);
			continue;
		}

		printf("%d " C_CYAN "%s" C_NONE " : " C_YELLOW "%s" C_NONE
			", type: " C_CYAN "%s" C_NONE ", index: %d, seqnum: %ld, " TS_FMT "\n",
			i, name[0] ? name : "[null]", get_value_str(value, param.size, buffer),
			get_type_str(idinfo.type, buffer2), idinfo.index, param.seqnum, p_ts(param));
	}

Fail:
	free(items);
}

static void reg_dmesg(void)
{
	ipc_dmesg();
}

typedef struct sample_raw_data_s {
	uint32_t	m1;
	uint32_t	m2;
	uint32_t	m3;
} sample_raw_data_t;

// generate raw data
static void menu_gen_raw_data(void)
{
	ipc_reg_id_t reg_id = ipc_reg_create_for_write(reg_type, reg_name, reg_index);
	if (reg_id == INVALID_IPC_REG_ID) {
		AVF_LOGE("cannot create reg");
		return;
	}

	uint32_t m1 = 0;
	srand((int)avf_get_curr_tick());

	avf_u64_t next_tick = avf_get_curr_tick();
	for (;;) {
		next_tick += 10;	// 100Hz or 10 ms

		sample_raw_data_t raw;

		raw.m1 = m1++;
		raw.m2 = rand();
		raw.m3 = raw.m1 + raw.m2;

		ipc_reg_read_write_t param;

		param.reg_id = reg_id;
		param.ptr = (void*)&raw;
		param.size = sizeof(raw);

		if (1) {
			param.options = 0;
		} else {
			param.options = REG_OPT_WRITE_TIMESTAMP;
			clock_gettime(CLOCK_MONOTONIC, &param.ts_write);
		}

		if (ipc_reg_write(&param) < 0) {
			AVF_LOGE("write reg '%s' failed", reg_name);
			return;
		}

		avf_u64_t now = avf_get_curr_tick();
		if (now < next_tick) {
			avf_msleep((int)next_tick - now);
		}
	}
}

static void show_help(void);

static const menu_t g_main_menu[] = {
	{"Write", menu_write_reg},
	{"Read", menu_read_reg},
	{"Read no wait", menu_read_no_wait},
	{"Read any", menu_read_reg_any},
	{"Dump registry", dump_reg},
	{"Dmesg", reg_dmesg},
	{"Generate Raw Data", menu_gen_raw_data},
	{"help", show_help},
};

static struct option long_options[] = {
	{"write", HAS_ARG, 0, 'w'},
	{"read", HAS_ARG, 0, 'r'},
	{"monitor", NO_ARG, 0, 'm'},
	{"dump", NO_ARG, 0, 'd'},
	{"dmesg", NO_ARG, 0, 'D'},
	{"gen", NO_ARG, 0, 'g'},
	{"type", HAS_ARG, 0, 't'},
	{"index", HAS_ARG, 0, 'i'},
	{"value", HAS_ARG, 0, 'v'},
	{"loop", NO_ARG, 0, 'l'},
	{"help", NO_ARG, 0, 'h'},
	{NULL, NO_ARG, 0, 0}
};

static const char *short_options = "w:r:mdt:i:v:lhDg";

#define OPT_SUB_STR(_name, _arg, _desc) \
	"  --" C_CYAN _name C_NONE " " _arg " : " C_GREEN _desc C_NONE "\n"

#define OPT_STR \
	OPT_SUB_STR("write(w)", "name(string)", "write reg entry") \
	OPT_SUB_STR("read(r)", "name(string)", "read reg entry") \
	OPT_SUB_STR("monitor(m)", "", "monitor all writes") \
	OPT_SUB_STR("dump(d)", "", "print all reg info") \
	OPT_SUB_STR("dmesg", "", "driver dmesg") \
	OPT_SUB_STR("gen(g)", "", "generate random raw data") \
	OPT_SUB_STR("type(t)", "int", "specify reg type") \
	OPT_SUB_STR("index(i)", "int", "specify reg index") \
	OPT_SUB_STR("value(v)", "string", "specify reg value") \
	OPT_SUB_STR("loop", "", "loop forever, for read and read no wait") \
	OPT_SUB_STR("help", "", "show this info")

enum {
	ACT_NONE,
	ACT_WRITE,
	ACT_READ,
	ACT_MONITOR,
	ACT_DUMP,
	ACT_DMESG,
	ACT_GEN,
	ACT_HELP,
};

static int act = ACT_NONE;

int init_param(int argc, char **argv)
{
	int option_index = 0;
	int ch;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'd':	act = ACT_DUMP;		break;
		case 'D':	act = ACT_DMESG;	break;
		case 'g':	act = ACT_GEN;		break;

		case 'r':
			act = ACT_READ;
			strncpy(reg_name, optarg, sizeof(reg_name));
			reg_name[sizeof(reg_name)-1] = 0;
			break;

		case 'w':
			act = ACT_WRITE;
			strncpy(reg_name, optarg, sizeof(reg_name));
			reg_name[sizeof(reg_name)-1] = 0;
			break;

		case 'm':
			act = ACT_MONITOR;
			break;

		case 'l':
			opt_loop = 1;
			break;

		case 'h':
			act = ACT_HELP;
			break;

		case 't':
			if (!isdigit(optarg[0]) && strlen(optarg) == 4) {
				// fcc
				reg_type = (optarg[0]<<24) | (optarg[1]<<16) | (optarg[2]<<8) | optarg[3];
			} else {
				// number
				reg_type = atoi(optarg);
			}
			break;

		case 'i':
			reg_index = atoi(optarg);
			break;

		case 'v':
			strncpy(reg_value, optarg, sizeof(reg_value));
			reg_value[sizeof(reg_value)-1] = 0;
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

static void show_help(void)
{
	printf("ipc_test\n" OPT_STR);
}

int main(int argc, char **argv)
{
	if (init_param(argc, argv) < 0)
		return -1;

	if (act == ACT_NONE || act == ACT_HELP) {
		show_help();
		if (act == ACT_HELP)
			return 0;
	}

	if (ipc_init() < 0) {
		AVF_LOGE("ipc_init failed");
		return -1;
	}

	switch (act) {
	case ACT_DUMP:
		dump_reg();
		return 0;

	case ACT_DMESG:
		reg_dmesg();
		return 0;

	case ACT_GEN:
		menu_gen_raw_data();
		return 0;

	case ACT_READ:
		do_read_reg(0, 0);
		return 0;

	case ACT_WRITE:
		do_write_reg(0);
		return 0;

	case ACT_MONITOR:
		for (;;) {
			do_read_reg_any(0);
		}
		return 0;

	default:
		RUN_MENU("Menu", g_main_menu);
		break;
	}

	return 0;
}


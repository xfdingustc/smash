
#include <getopt.h>
#include "avf_common.h"
#include "avf_if.h"
#include "avf_util.h"
#include "avf_converter_api.h"

#include "test_common.cpp"

static avf_converter_t *g_converter = NULL;
static char *g_input_file = NULL;
static char *g_output_file = NULL;
static int g_conv_type = CONV_NONE;

static struct option long_options[] = {
	{"input",	HAS_ARG,	0,	'i'},
	{"output",	HAS_ARG,	0,	'o'},
	{"type",	HAS_ARG,	0,	't'},
	{NULL, NO_ARG, 0, 0}
};

static const char *short_options = "i:o:t:";

static void converter_callback(void *context, int event, int arg1, int arg2)
{
	switch (event) {
	case AVF_CONV_FINISHED:
		printf("convertion finished\n");
		break;

	case AVF_CONV_ERROR:
		printf("convertion error\n");
		break;

	case AVF_CONV_PROGRESS:
		printf("progress: %d\n", arg1);
		break;
	}
}

int init_param(int argc, char **argv)
{
	int option_index = 0;
	int ch;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'i':
			if (g_input_file)
				free(g_input_file);
			g_input_file = strdup(optarg);
			break;

		case 'o':
			if (g_output_file)
				free(g_output_file);
			g_output_file = strdup(optarg);
			break;

		case 't':
			if (strcmp(optarg, "mp3aac") == 0) {
				g_conv_type = CONV_TYPE_MP3_AAC;
			} else if (strcmp(optarg, "mp3ts") == 0) {
				g_conv_type = CONV_TYPE_MP3_TS;
			} else if (strcmp(optarg, "mp3aacts") == 0) {
				g_conv_type = CONV_TYPE_MP3_AAC_TS;
			} else {
				printf("unknown convertion type: %s\n", optarg);
				return -1;
			}
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

static int do_exit(void);

static const menu_t g_converter_main_menu[] = {
	{"** Exit **", do_exit}
};

static int do_exit(void)
{
	return -1;
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		if (init_param(argc, argv) < 0)
			return -1;
	}

	if (g_input_file == NULL) {
		printf("no input file\n");
		return -1;
	}
	if (g_output_file == NULL) {
		printf("no output file\n");
		return -1;
	}
	if (g_conv_type == CONV_NONE) {
		printf("converter type not specified\n");
		return -1;
	}

	g_converter = avf_converter_create(converter_callback, NULL);
	avf_converter_run(g_converter, g_conv_type, g_input_file, g_output_file);

	RUN_MENU("Converter", g_converter_main_menu);

	avf_converter_destroy(g_converter);
	free(g_input_file);
	free(g_output_file);

	avf_mem_info();
	avf_file_info();
	avf_socket_info();

	return 0;
}


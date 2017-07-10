
#include <getopt.h>
#include "avf_common.h"
#include "avf_if.h"
#include "avf_util.h"
#include "avf_remuxer_api.h"

#include "test_common.cpp"

static avf_remuxer_t *g_remuxer = NULL;
static char *g_input_files = NULL;
static char *g_output_file = NULL;
static char *g_input_format = NULL;
static char *g_output_format = NULL;
static char *g_audio_file = NULL;
static int g_disable_audio = 0;

static struct option long_options[] = {
	{"input",	HAS_ARG,	0,	'i'},
	{"output",	HAS_ARG,	0,	'o'},
	{"iformat",	HAS_ARG,	0,	'm'},
	{"oformat",	HAS_ARG,	0,	'f'},
	{"audio",	HAS_ARG,	0,	'a'},
	{"mute",	NO_ARG,		0,	'M'},
	{NULL,		NO_ARG,		0,	0},
};

static const char *short_options = "f:i:m:o:a:M";

static void remux_add_file(const char *filename)
{
	int length1 = g_input_files ? strlen(g_input_files) : 0;
	int length2 = strlen(filename);
	char *new_buffer;

	new_buffer = (char*)malloc(length1 + length2 + 16);
	sprintf(new_buffer, "%s%s,0,-1;", g_input_files ? g_input_files : "", filename);

	if (g_input_files)
		free(g_input_files);

	g_input_files = new_buffer;
}

int init_param(int argc, char **argv)
{
	int option_index = 0;
	int ch;

	opterr = 0;
	while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (ch) {
		case 'i':
			remux_add_file(optarg);
			break;

		case 'o':
			if (g_output_file)
				free(g_output_file);
			g_output_file = strdup(optarg);
			break;

		case 'f':
			if (g_output_format)
				free(g_output_format);
			g_output_format = strdup(optarg);
			break;

		case 'm':
			if (g_input_format)
				free(g_input_format);
			g_input_format = strdup(optarg);
			break;

		case 'a':
			if (g_audio_file)
				free(g_audio_file);
			g_audio_file = strdup(optarg);
			break;

		case 'M':
			g_disable_audio = 1;
			break;

		default:
			printf("unknown option found: %c\n", ch);
			return -1;
		}
	}

	return 0;
}

static void remux_callback(void *context, int event, int arg1, int arg2)
{
	switch (event) {
	case AVF_REMUXER_FINISHED:
		printf("finished\n");
		break;
	case AVF_REMUXER_ERROR:
		printf("error\n");
		break;
	case AVF_REMUXER_PROGRESS:
		printf("progress %d\n", arg1);
		break;
	}
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		if (init_param(argc, argv) < 0)
			return -1;
	}

	if (g_input_files == NULL) {
		printf("No input files!\n");
		return -1;
	}
	if (g_output_file == NULL) {
		printf("No output file\n");
		return -1;
	}

	g_remuxer = avf_remuxer_create(remux_callback, NULL);
	if (g_disable_audio || g_audio_file) {
		avf_remuxer_set_audio(g_remuxer, g_disable_audio, g_audio_file, "mp3");	// todo
	}

	avf_remuxer_run(g_remuxer, g_input_files, 
		g_input_format ? g_input_format : "ts",
		g_output_file,
		g_output_format ? g_output_format : "mp4",
		0);

	while (1) {
		sleep(1);
	}

	avf_remuxer_destroy(g_remuxer);
	free(g_input_files);
	free(g_output_file);

	avf_mem_info(1);
	avf_file_info();
	avf_socket_info();

	return 0;
}


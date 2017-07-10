
#include <setjmp.h>
#include <signal.h>

#include "avf_common.h"

#define NO_ARG	0
#define HAS_ARG	1

typedef struct menu_s {
	const char *title;
	void (*action)(void);
} menu_t;

static void remove_newline(char *ptr)
{
	for (; *ptr; ptr++) {
		if (*ptr == '\n' || *ptr == '\r') {
			*ptr = 0;
			break;
		}
	}
}

static int get_input(const char *prompt, char *buffer, int size)
{
	printf("%s", prompt);
	fflush(stdout);
	char *rval = fgets(buffer, size, stdin);
	AVF_UNUSED(rval);
	remove_newline(buffer);
	return buffer[0] != 0;
}

static void show_menu(const menu_t *menu, int nitems)
{
	for (int i = 0; i < nitems; i++)
		printf("[" C_CYAN "%d" C_NONE "] %s\n", i + 1, menu[i].title);
	printf("[" C_CYAN "Ctrl+C" C_NONE "] Exit\n");
}

jmp_buf g_jmpbuf;

static void signal_handler(int signo)
{
	//AVF_LOGI("\nsignal: %d", signo);
	printf("\n");
	longjmp(g_jmpbuf, 1);
}

static void install_signal_handler(void (*handler)(int))
{
	signal(SIGINT, handler);
#ifndef WIN32_OS
	signal(SIGQUIT, handler);
#endif
	signal(SIGTERM, handler);
}

void run_menu(const char *name, const menu_t *menu, int nitems)
{
	char buffer[256];
	int show = 1;

	if (setjmp(g_jmpbuf))
		return;

	install_signal_handler(signal_handler);

	while (1) {
		if (show) {
			show = 0;
			printf("\n[ " C_GREEN "%s" C_NONE " ]\n", name);
			show_menu(menu, nitems);
			printf(C_GREEN "Input your choice: " C_NONE);
			fflush(stdout);
		}

		get_input("", buffer, sizeof(buffer));
		if (buffer[0] == 0) {
			show = 1;
			continue;
		}

		char *end;
		int index = strtoul(buffer, &end, 10);
		if (end != buffer && index >= 1 && index <= nitems) {
			const menu_t *item = menu + (index - 1);
			printf("  === %s ===\n", item->title);
			item->action();
		}
	}
}

#define RUN_MENU(_name, _menu) \
	run_menu(_name, _menu, sizeof(_menu) / sizeof(_menu[0]))


//-----------------------------------------------------------------------
//
//	server
//
//-----------------------------------------------------------------------

#define VDB_SERVER_PORT	18089

enum {
	SERVER_CMD_None,
	SERVER_CMD_StartRecord,
	SERVER_CMD_StopRecord,
	SERVER_CMD_AddData,
};

typedef struct server_cmd_s {
	uint32_t cmd_code;
	union {
		struct {
			uint32_t has_video;
			uint32_t has_picture;
		} StartRecord;

		struct {
			uint32_t data_size;
		} AddData;

		uint32_t reserved[63];
	} u;
} server_cmd_t;


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <dirent.h>
#include <arpa/inet.h> //struct in_addr

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

#include "fast_Camera.h"

using namespace ui;

static void signal_segment_fault(int sig) {
    void *array[15];
    size_t size;
    char **strings;
    size_t i;

    size = backtrace(array, 15);
    strings = backtrace_symbols(array, size);

    CAMERALOG("Obtained %zd stack frames.", size);

    for (i = 0; i < size; i++) {
        CAMERALOG("%2d %s", i, strings[i]);
    }

    free (strings);

    exit(0);
}

int main(int argc, char **argv)
{
    printf("--Begin\n");

    signal(SIGSEGV, signal_segment_fault);
    signal(SIGFPE, signal_segment_fault);

    CFastCamera cam;
    cam.Start();

    //int i = 0;
    while(1) {
        //printf("i = %d\n", i++);
        sleep(1);
    }

    printf("--Finish\n");
    return 0;
}


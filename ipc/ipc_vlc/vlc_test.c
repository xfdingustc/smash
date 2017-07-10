
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "libipc.h"
#include "ipc_vlc.h"


#define vlc_media	"HVGAp30(LowDelay)"

static void vlc_event_callback( int type, void *pdata )
{
	switch (type) {
	case VLC_VLM_EventMediaAdded: printf(">>> VLC_VLM_EventMediaAdded\n"); break;
	case VLC_VLM_EventMediaRemoved: printf(">>> VLC_VLM_EventMediaRemoved\n"); break;
	case VLC_VLM_EventMediaChanged: printf(">>> VLC_VLM_EventMediaChanged\n"); break;
	case VLC_VLM_EventMediaInstanceStarted: printf(">>> VLC_VLM_EventMediaInstanceStarted\n"); break;

	case VLC_VLM_EventMediaInstanceStopped: printf(">>> VLC_VLM_EventMediaInstanceStopped\n"); break;
	case VLC_VLM_EventMediaInstanceStatusInit: printf(">>> VLC_VLM_EventMediaInstanceStatusInit\n"); break;
	case VLC_VLM_EventMediaInstanceStatusOpening: printf(">>> VLC_VLM_EventMediaInstanceStatusOpening\n"); break;
	case VLC_VLM_EventMediaInstanceStatusPlaying: printf(">>> VLC_VLM_EventMediaInstanceStatusPlaying\n"); break;

	case VLC_VLM_EventMediaInstanceStatusPause: printf(">>> VLC_VLM_EventMediaInstanceStatusPause\n"); break;
	case VLC_VLM_EventMediaInstanceStatusEnd: printf(">>> VLC_VLM_EventMediaInstanceStatusEnd\n"); break;
	case VLC_VLM_EventMediaInstanceStatusError: printf(">>> VLC_VLM_EventMediaInstanceStatusError\n"); break;

	default: printf("Unknwon event %d\n", type); break;
	}
}

int main(int argc, char **argv)
{
	ipc_libvlc_instance_t *p_vlc_instance;
	int i;
	char input[1024];
	char output[1024];

	if (ipc_init() < 0)
		return -1;

	p_vlc_instance = ipc_libvlc_new(0, NULL);
	if (p_vlc_instance == NULL) {
		printf("ipc_libvlc_new failed\n");
		return -1;
	}

	printf("call ipc_libvlc_vlm_set_event_callback\n");
	if (ipc_libvlc_vlm_set_event_callback(p_vlc_instance,
			vlc_event_callback, NULL) < 0) {
		printf("ipc_libvlc_vlm_set_event_callback failed\n");
		return -1;
	}

	sprintf(input, 
		"ambcam://enc_stream0=%dx%dx%d:enc_rate0=%dx%dx%d"
		":enc_stream1=%dx%dx%d:enc_rate1=%dx%dx%d"
		":enc_stream2=%dx%dx%d:enc_rate2=%dx%dx%d"
		":enc_stream3=%dx%dx%d:enc_rate3=%dx%dx%d"
		":audio=%dx%dx%d:misc=%dx%dx%d"
		":vin=%dx%dx%dx%.02fx%d:vout=%dx%dx%dx%.02fx%d",
		2, 480, 320, 0, 2500000, 2500000,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0,
		0, 44100, 128000,
		3, 67000, 0,
		0, 2304, 1296, 29.97, 2,
		3, 720, 480, 59.94, 2);

	printf("input: %s\n", input);

	sprintf(output,
		"#ages{"
		"skip=1,mux=mpjpeg,dst=:%d,access=http{"
		"mime='multipart/x-mixed-replace;boundary=7b3cc56e5f51db803f790dad720ed50a'},"
		"}",
		8081);

	printf("output: %s\n", output);

	printf("call ipc_libvlc_vlm_add_broadcast\n");
	if (ipc_libvlc_vlm_add_broadcast(p_vlc_instance,
			vlc_media, input, output, 0, NULL, false, false) < 0) {
		printf("ipc_libvlc_vlm_add_broadcast failed\n");
		return -1;
	}

	printf("call ipc_libvlc_vlm_set_enabled\n");
	if (ipc_libvlc_vlm_set_enabled(p_vlc_instance,
			vlc_media, true) < 0) {
		printf("ipc_libvlc_vlm_set_enabled failed\n");
		return -1;
	}

	printf("call ipc_libvlc_vlm_play_media\n");
	if (ipc_libvlc_vlm_play_media(p_vlc_instance,
			vlc_media) < 0) {
		printf("ipc_libvlc_vlm_play_media failed\n");
		return -1;
	}

	for (i = 10; i > 0; i--) {
		printf("counting down %d\n", i - 1);
		sleep(1);
	}

	printf("call ipc_libvlc_vlm_stop_media\n");
	if (ipc_libvlc_vlm_stop_media(p_vlc_instance,
			vlc_media) < 0) {
		printf("ipc_libvlc_vlm_stop_media failed\n");
	}

	printf("call ipc_libvlc_vlm_set_enabled\n");
	if (ipc_libvlc_vlm_set_enabled(p_vlc_instance,
			vlc_media, false) < 0) {
		printf("ipc_libvlc_vlm_set_enabled failed\n");
	}

	printf("call ipc_libvlc_vlm_del_media\n");
	if (ipc_libvlc_vlm_del_media(p_vlc_instance,
			vlc_media) < 0) {
		printf("ipc_libvlc_vlm_del_media failed\n");
	}

	printf("call ipc_libvlc_vlm_release\n");
	ipc_libvlc_vlm_release(p_vlc_instance);

	printf("call ipc_libvlc_release\n");
	ipc_libvlc_release(p_vlc_instance);

	printf("vlc_test exits\n");
	return 0;
}



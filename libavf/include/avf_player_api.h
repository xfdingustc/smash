
#ifndef __AVF_PLAYER_API_H__
#define __AVF_PLAYER_API_H__

typedef struct avf_player_s avf_player_t;

enum {
	AVF_PLAYER_FINISHED,
	AVF_PLAYER_ERROR,
	AVF_PLAYER_PROGRESS,
};

typedef void (*avf_player_callback_t)(void *context, int event, int arg1, int arg2);

avf_player_t *avf_player_create(avf_player_callback_t callback, void *context);

void avf_player_destroy(avf_player_t *a2ts);

#endif


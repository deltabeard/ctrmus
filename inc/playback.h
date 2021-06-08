/**
 * Playback.c: Handling of audio playback and playlist functions.
 * Copyright (c) 2016-2021 Mahyar Koshkouei
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 */

#pragma once

typedef struct playback_ctx playback_ctx_s;

typedef enum {
	PLAYBACK_CTRL_PLAY,
	PLAYBACK_CTRL_PAUSE
} playback_control_e;

typedef enum {
	PLAYBACK_CTRL_PLAYING,
	PLAYBACK_CTRL_PAUSED
} playback_status_e;

/* Initialise playback context. */
playback_ctx_s *playback_init(void);

/* Add file to playlist. file_name must be an absolute file path. */
int playback_add_file(playback_ctx_s *ctx, const char *file_name);

/* Remove file from playlist with given file name. */
int playback_remove_entry(playback_ctx_s *ctx, int entry);

/* Return an array of file names that are within the playlist. */
const char *playback_get_list(playback_ctx_s *ctx, int *entries);

/* Select file from playback to play. */
int playback_select_file(playback_ctx_s *ctx, int entry);

/* Control playback. */
int playback_control(playback_ctx_s *ctx, playback_control_e ctrl);

/* Get playback information. */
playback_status_e playback_get_status(playback_ctx_s *ctx);

/* Free playback context. */
void playback_exit(playback_ctx_s *ctx);

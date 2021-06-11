/**
 * Playback.c: Handling of audio playback and playlist functions.
 * Copyright (c) 2016-2021 Mahyar Koshkouei
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 */

#pragma once

typedef struct playback_ctx playback_ctx_s;
typedef struct playlist_entry playlist_entry_s;

typedef enum {
	PLAYBACK_CTRL_PAUSE = 0,
	PLAYBACK_CTRL_PLAY
} playback_control_e;

typedef enum {
	PLAYBACK_STAT_PAUSED = 0,
	PLAYBACK_STAT_PLAYING
} playback_status_e;

/* Initialise playback context. */
playback_ctx_s *playback_init(void);

/* Add file to playlist. file_name must be an absolute file path. */
playlist_entry_s *playback_add_file(playback_ctx_s *ctx, const char *file_name);

/* Remove file from playlist with given file name. */
void playback_remove_entry(playback_ctx_s *ctx, playlist_entry_s *entry);

/* Remove all files from playlist. */
void playback_remove_all(playback_ctx_s *ctx);

/* Select file from playback to play. */
void playback_play_file(playback_ctx_s *ctx, playlist_entry_s *entry);

/* Control playback. */
int playback_control(playback_ctx_s *ctx, playback_control_e ctrl);

/* Get playback information. */
playback_status_e playback_get_status(playback_ctx_s *ctx);

/* Free playback context. */
void playback_exit(playback_ctx_s *ctx);

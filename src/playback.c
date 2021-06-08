/**
 * Playback.c: Handling of audio playback and playlist functions.
 * Copyright (c) 2016-2021 Mahyar Koshkouei
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 */

#include <platform.h>
#include <playback.h>

struct playback_ctx {
	platform_mutex_s *mtx;
	
	struct playlist {
		char **file_names;
		unsigned entries;
	} playing_playlist;
};

playback_ctx_s *playback_init(void)
{
	playback_ctx_s *ctx = calloc(1, sizeof(playback_ctx_s));
	ctx->mtx = platform_create_mutex();
	return ctx;
}

int playback_add_file(playback_ctx_s *ctx, const char *file_name)
{
	int ret = -1;
	unsigned array_entries;
	size_t file_name_sz;
	char *f;
	
	if(file_name == NULL)
		goto out;
	
	file_name_sz = strlen(file_name);
	/* convert relative file to absolute. */
	f = malloc(file_name_sz);
	if(f == NULL)
		goto out;
	
	strcpy(f, file_name);
	
	ctx->playing_playlist.entries++;
	{
		char **file_names = realloc(ctx->playing_playlist.file_names,
			sizeof(char **) * ctx->playing_playlist.entries);
			
		if(file_names == NULL)
		{
			ctx->playing_playlist.entries--;
			free(f);
			goto out;
		}
		
		ctx->playing_playlist.file_names = file_names;
	}
	
	ctx->playing_playlist.file_names[ctx->playing_playlist.entries] = f;
	
	ret = ctx->playing_playlist.entries;
	
out:
	goto ret;
}

int playback_remove_file(playback_ctx_s *ctx, int entry);

const char *playback_get_list(playback_ctx_s *ctx, int *entries);

int playback_select_file(playback_ctx_s *ctx, const char *file_name);

int playback_control(playback_ctx_s *ctx, playback_control_e ctrl);

playback_status_e playback_get_status(playback_ctx_s *ctx);

void playback_exit(playback_ctx_s *ctx);
{
	for(int i = 0; i < ctx->playing_playlist.entries; i++)
		free(ctx->playlist_playlist.file_names[i]);
	
	free(ctx);
	return;
}

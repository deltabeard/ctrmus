/**
 * Playback.c: Handling of audio playback and playlist functions.
 * Copyright (c) 2016-2021 Mahyar Koshkouei
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 */

#include <platform.h>
#include <playback.h>
#include <stdlib.h>
#include <string.h>

struct playlist_entry
{
	char *file_name;
	struct playlist_entry *next;
	struct playlist_entry *prev;
};

struct playback_ctx {
	playback_status_e stat;

	struct playlist_entry *first_entry;
	struct playlist_entry *playing_entry;
};

playback_ctx_s *playback_init(void)
{
	return calloc(1, sizeof(playback_ctx_s *));
}

playlist_entry_s *playback_add_file(playback_ctx_s *ctx, const char *file_name)
{
	unsigned array_entries;
	size_t file_name_sz;
	char *f;
	struct playlist_entry *last_entry;
	
	if(file_name == NULL)
		goto out;

	last_entry = ctx->first_entry;
	while(last_entry != NULL)
		last_entry = last_entry->next;

	last_entry = malloc(sizeof(struct playlist_entry));
	if(last_entry == NULL)
		goto out;
	
	file_name_sz = strlen(file_name);
	/* convert relative file to absolute. */
	last_entry->file_name = malloc(file_name_sz);
	if(f == NULL)
	{
		free(last_entry);
		last_entry = NULL;
		goto out;
	}
	
	strcpy(f, file_name);
	
out:
	return last_entry;
}

void playback_remove_all(playback_ctx_s *ctx)
{
	struct playlist_entry *entry = ctx->first_entry;

	while(entry != NULL)
	{
		struct playlist_entry *next = entry->next;

		free(entry->file_name);
		free(entry);

		entry = next;
	}

	ctx->playing_entry = NULL;
	ctx->stat = PLAYBACK_STAT_PAUSED;
	return;
}

void playback_remove_entry(playback_ctx_s *ctx, playlist_entry_s *entry)
{
	entry->prev->next = entry->next;

	if(ctx->playing_entry == entry)
	{
		ctx->stat = PLAYBACK_STAT_PAUSED;
	}

	free(entry->file_name);
	free(entry);
	return;
}

void playback_play_file(playback_ctx_s *ctx, playlist_entry_s *entry)
{
	ctx->playing_entry = entry;


	// TODO: Play file.
	return;
}

int playback_control(playback_ctx_s *ctx, playback_control_e ctrl)
{
	// TODO: Actually perform control here.
	ctx->stat = (playback_status_e)ctrl;
	return 0;
}

playback_status_e playback_get_status(playback_ctx_s *ctx)
{
	return ctx->stat;
}

void playback_exit(playback_ctx_s *ctx)
{
	playback_remove_all(ctx);
	free(ctx);
	return;
}

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
	FILE *f;
	struct playlist_entry *next;
	struct playlist_entry *prev;
};

struct playback_ctx {
	playback_status_e stat;

	struct playlist_entry *first_entry;
	struct playlist_entry *last_entry;
	struct playlist_entry *playing_entry;
};

playback_ctx_s *playback_init(void)
{
	return calloc(1, sizeof(playback_ctx_s));
}

playlist_entry_s *playback_add_file(playback_ctx_s *ctx, const char *file_name)
{
	unsigned array_entries;
	size_t file_name_sz;
	struct playlist_entry *new_entry = NULL;
	FILE *f;
	
	if(file_name == NULL)
		goto out;

	f = fopen(file_name, "rb");
	if(f == NULL)
		goto out;

	new_entry = malloc(sizeof(struct playlist_entry));
	if(new_entry == NULL)
		goto out;

	new_entry->f = f;
	file_name_sz = strlen(file_name);
	/* convert relative file to absolute. */
	new_entry->file_name = malloc(file_name_sz);
	if(new_entry->file_name == NULL)
	{
		free(new_entry);
		new_entry = NULL;
		goto out;
	}
	
	strcpy(new_entry->file_name, file_name);
	new_entry->prev = ctx->last_entry;
	new_entry->next = NULL;

	if(ctx->last_entry != NULL)
		ctx->last_entry->next = new_entry;

	ctx->last_entry = new_entry;

out:
	return new_entry;
}

void playback_remove_all(playback_ctx_s *ctx)
{
	struct playlist_entry *entry = ctx->first_entry;

	if(entry == NULL)
		return;

	while(entry != NULL)
	{
		struct playlist_entry *next = entry->next;

		fclose(entry->f);
		free(entry->file_name);
		free(entry);

		entry = next;
	}

	ctx->last_entry = NULL;
	ctx->playing_entry = NULL;
	ctx->stat = PLAYBACK_STAT_PAUSED;
	return;
}

void playback_remove_entry(playback_ctx_s *ctx, playlist_entry_s *entry)
{
	if(ctx->last_entry == entry)
		ctx->last_entry = entry->prev;

	if(entry->prev != NULL)
		entry->prev->next = entry->next;

	if(ctx->playing_entry == entry)
		ctx->stat = PLAYBACK_STAT_PAUSED;

	fclose(entry->f);
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

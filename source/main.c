/**
 * ctrmus - 3DS Music Player
 * Copyright (C) 2016 Mahyar Koshkouei
 *
 * This program comes with ABSOLUTELY NO WARRANTY and is free software. You are
 * welcome to redistribute it under certain conditions; for details see the
 * LICENSE file.
 */

#include <3ds.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "all.h"
#include "error.h"
#include "main.h"
#include "playback.h"

volatile bool runThreads = true;

int main(int argc, char **argv)
{
	PrintConsole	topScreen;
	PrintConsole	bottomScreen;
	int				fileMax;
	int				fileNum = 0;
	int				from = 0;
	Thread			watchdogThread;
	Handle			playbackFailEvent;
	struct watchdogInfo	watchdogInfoIn;
	struct errInfo_t	errInfo;
	struct playbackInfo_t playbackInfo;
	volatile int	error = 0;

	gfxInitDefault();
	sdmcInit();
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);

	svcCreateEvent(&playbackFailEvent, RESET_ONESHOT);
	errInfo.error = &error;
	errInfo.failEvent = &playbackFailEvent;
	errInfo.errstr = NULL;

	watchdogInfoIn.screen = &topScreen;
	watchdogInfoIn.errInfo = &errInfo;
	watchdogThread = threadCreate(playbackWatchdog,
			&watchdogInfoIn, 4 * 1024, 0x20, -2, true);

	playbackInfo.file = NULL;
	playbackInfo.errInfo = &errInfo;

	chdir(DEFAULT_DIR);
	chdir("MUSIC");
	if(listDir(from, MAX_LIST, 0) < 0)
	{
		err_print("Unable to list directory.");
		goto err;
	}

	fileMax = getNumberFiles();

	/**
	 * This allows for music to continue playing through the headphones whilst
	 * the 3DS is closed.
	 */
	aptSetSleepAllowed(false);

	while(aptMainLoop())
	{
		u32			kDown;
		u32			kHeld;
		static u64	mill = 0;

		hidScanInput();

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		kDown = hidKeysDown();
		kHeld = hidKeysHeld();

		consoleSelect(&bottomScreen);

		/* Exit ctrmus */
		if(kDown & KEY_START)
			break;

#ifdef DEBUG
		consoleSelect(&topScreen);
		printf("\rNum: %d, Max: %d, from: %d   ", fileNum, fileMax, from);
		consoleSelect(&bottomScreen);
#endif
		if(kDown)
			mill = osGetTime();

		if(kHeld & KEY_L)
		{
			/* Pause/Play */
			if(kDown & (KEY_R | KEY_UP))
			{
				if(isPlaying() == false)
					continue;

				consoleSelect(&topScreen);
				if(togglePlayback() == true)
					puts("Paused");
				else
					puts("Playing");

				continue;
			}

			/* Show controls */
			if(kDown & KEY_LEFT)
			{
				consoleSelect(&topScreen);
				showControls();
				continue;
			}

			/* Stop */
			if(kDown & KEY_B)
			{
				stopPlayback();
				changeFile(NULL, &playbackInfo);
				consoleSelect(&topScreen);
				puts("Stopped");
				continue;
			}
		}

		if((kDown & KEY_UP ||
					((kHeld & KEY_UP) && (osGetTime() - mill > 500))) &&
				fileNum > 0)
		{
			fileNum--;

			/* 26 is the maximum number of entries that can be printed */
			if(fileMax - fileNum > 26 && from != 0)
				from--;

			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");
		}

		if((kDown & KEY_DOWN ||
					((kHeld & KEY_DOWN) && (osGetTime() - mill > 500))) &&
				fileNum < fileMax)
		{
			fileNum++;

			if(fileNum >= MAX_LIST && fileMax - fileNum >= 0 &&
					from < fileMax - MAX_LIST)
				from++;

			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");
		}

		/*
		 * Pressing B goes up a folder, as well as pressing A or R when ".."
		 * is selected.
		 */
		if((kDown & KEY_B) ||
				((kDown & KEY_A) && (from == 0 && fileNum == 0)))
		{
			if(chdir("..") != 0)
				err_print("chdir");

			fileNum = 0;
			from = 0;
			fileMax = getNumberFiles();

			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");

			continue;
		}

		if(kDown & KEY_A)
		{
			int				audioFileNum = 0;
			DIR				*dp;
			struct dirent	*ep;

			dp = opendir(".");

			if(dp != NULL)
			{
				while((ep = readdir(dp)) != NULL)
				{
					if(audioFileNum == fileNum - 1)
						break;

					audioFileNum++;
				}

				if(ep->d_type == DT_DIR)
				{
					/* file not allocated yet, so no need to clear it */
					if(chdir(ep->d_name) != 0)
						err_print("chdir");

					fileNum = 0;
					from = 0;
					fileMax = getNumberFiles();
					if(listDir(from, MAX_LIST, fileNum) < 0)
						err_print("Unable to list directory.");

					closedir(dp);
					continue;
				}

				consoleSelect(&topScreen);
				changeFile(ep->d_name, &playbackInfo);
				consoleSelect(&bottomScreen);

				if(closedir(dp) != 0)
					err_print("Closing directory failed.");
			}
			else
				err_print("Unable to open directory.");
		}
	}
#ifdef DEBUG
		consoleSelect(&topScreen);
		printf("\rNum: %d, Max: %d, from: %d   ", fileNum, fileMax, from);
		consoleSelect(&bottomScreen);
#endif

out:
	puts("Exiting...");
	runThreads = false;
	svcSignalEvent(playbackFailEvent);
	changeFile(NULL, &playbackInfo);

	gfxExit();
	sdmcExit();
	return 0;

err:
	puts("A fatal error occurred. Press START to exit.");

	while(true)
	{
		u32 kDown;

		hidScanInput();
		kDown = hidKeysDown();

		if(kDown & KEY_START)
			break;
	}

	goto out;
}

static void showControls(void)
{
	printf("Button mappings:\n"
			"Pause: L+R or L+Up\n"
			"Stop: L+B\n"
			"A: Open File\n"
			"B: Go up folder\n"
			"Start: Exit\n");
}

void playbackWatchdog(void* infoIn)
{
	struct watchdogInfo* info = infoIn;

	while(runThreads)
	{
		svcWaitSynchronization(*info->errInfo->failEvent, U64_MAX);
		svcClearEvent(*info->errInfo->failEvent);
		consoleSelect(info->screen);

		if(*info->errInfo->error != 0)
		{
			printf("Error %d: %s", *info->errInfo->error,
					ctrmus_strerror(*info->errInfo->error));

			if(info->errInfo->errstr != NULL)
			{
				printf(" %s", info->errInfo->errstr);
				delete(info->errInfo->errstr);
			}

			printf("\n");
		}
	}

	return;
}

static int changeFile(const char* ep_file, struct playbackInfo_t* playbackInfo)
{
	s32 prio;
	static Thread thread = NULL;
	
	/**
	 * If music is playing, stop it. Only one playback thread should be playing
	 * at any time.
	 */
	if(thread != NULL)
	{
		/* Tell the thread to stop playback before we join it */
		stopPlayback();

		threadJoin(thread, U64_MAX);
		threadFree(thread);
		thread = NULL;

		/* free allocated file string */
		delete(playbackInfo->file);
	}

	if(ep_file == NULL || playbackInfo == NULL)
		return 0;

	playbackInfo->file = strdup(ep_file);
	printf("Playing: %s\n", playbackInfo->file);

	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	thread = threadCreate(playFile, playbackInfo, 32 * 1024, prio - 1, -2, false);

	return 0;
}

/**
 * List current directory.
 *
 * \param	from	First entry in directory to list.
 * \param	max		Maximum number of entries to list. Must be > 0.
 * \param	select	File to show as selected. Must be > 0.
 * \return			Number of entries listed or negative on error.
 */
int listDir(int from, int max, int select)
{
	DIR				*dp;
	struct dirent	*ep;
	int				fileNum = 0;
	int				listed = 0;
	char*			wd = getcwd(NULL, 0);

	if(wd == NULL)
		goto err;

	consoleClear();
	printf("Dir: %.33s\n", wd);

	if((dp = opendir(wd)) == NULL)
		goto err;

	if(from == 0)
	{
		printf("%c../\n", select == 0 ? '>' : ' ');
		listed++;
	}

	while((ep = readdir(dp)) != NULL)
	{
		fileNum++;

		if(fileNum <= from)
			continue;

		listed++;

		printf("%c%s%.37s%s\n",
				select == fileNum ? '>' : ' ',
				ep->d_type == DT_DIR ? "\x1b[34;1m" : "",
				ep->d_name,
				ep->d_type == DT_DIR ? "/\x1b[0m" : "");

		if(fileNum == max + from)
			break;
	}

	if(closedir(dp) != 0)
		goto err;

out:
	free(wd);
	return listed;

err:
	listed = -1;
	goto out;
}

/**
 * Get number of files in current working folder
 *
 * \return	Number of files in current working folder, -1 on failure with
 *			errno set.
 */
int getNumberFiles(void)
{
	DIR				*dp;
	struct dirent	*ep;
	int				ret = 0;

	if((dp = opendir(".")) == NULL)
		goto err;

	while((ep = readdir(dp)) != NULL)
		ret++;

	closedir(dp);

out:
	return ret;

err:
	ret = -1;
	goto out;
}

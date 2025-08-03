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
#include "file.h"
#include "main.h"
#include "playback.h"

volatile bool runThreads = true;

/**
 * Prints the current key mappings to stdio.
 */
static void showControls(void)
{
	printf("Button mappings:\n"
			"Pause: L+R or L+Up\n"
			"Stop: L+B\n"
			"A: Open File\n"
			"B: Go up folder\n"
			"Start: Exit\n"
			"Browse: Up, Down, Left or Right\n");
}

/**
 * Allows the playback thread to return any error messages that it may
 * encounter.
 *
 * \param	infoIn	Struct containing addresses of the event, the error code,
 *					and an optional error string.
 */
void playbackWatchdog(void* infoIn)
{
	struct watchdogInfo* info = infoIn;

	while(runThreads)
	{
		svcWaitSynchronization(*info->errInfo->failEvent, U64_MAX);
		svcClearEvent(*info->errInfo->failEvent);

		if(*info->errInfo->error > 0)
		{
			continue;
			consoleSelect(info->screen);
			printf("Error %d: %s\n", *info->errInfo->error,
					ctrmus_strerror(*info->errInfo->error));
		}
		else if (*info->errInfo->error == -1)
		{
			continue;
			/* Used to signify that playback has stopped.
			 * Not technically an error.
			 */
			consoleSelect(info->screen);
			puts("Stopped");
		}
	}

	return;
}

/**
 * Stop the currently playing file (if there is one) and play another file.
 *
 * \param	ep_file			File to play.
 * \param	playbackInfo	Information that the playback thread requires to
 *							play file.
 */
static int changeFile(const char* ep_file, struct playbackInfo_t* playbackInfo)
{
	s32 prio;
	static Thread thread = NULL;

	if(ep_file != NULL && getFileType(ep_file) == FILE_TYPE_ERROR)
	{
		*playbackInfo->errInfo->error = errno;
		svcSignalEvent(*playbackInfo->errInfo->failEvent);
		return -1;
	}

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
	}

	/* If file is NULL, then only thread termination was requested. */
	if(ep_file == NULL || playbackInfo == NULL)
		return 0;
	
	//playbackInfo->file = strdup(ep_file);
	if (memccpy(playbackInfo->file, ep_file, '\0', sizeof(playbackInfo->file)) == NULL)
	{
		puts("Error: File path too long\n");
		return -1;
	}

	printf("Playing: %s\n", playbackInfo->file);
	playbackInfo->samples_total = 0;
	playbackInfo->samples_played = 0;
	playbackInfo->samples_per_second = 0;

	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	thread = threadCreate(playFile, playbackInfo, 32 * 1024, prio - 1, -2, false);

	return 0;
}

static int cmpstringp(const void *p1, const void *p2)
{
	/* The actual arguments to this function are "pointers to
	   pointers to char", but strcmp(3) arguments are "pointers
	   to char", hence the following cast plus dereference */

	return strcasecmp(* (char * const *) p1, * (char * const *) p2);
}

/**
 * Store the list of files and folders in current director to an array.
 */
static int getDir(struct dirList_t* dirList)
{
	DIR				*dp;
	struct dirent	*ep;
	int				fileNum = 0;
	int				dirNum = 0;
	char*			wd = getcwd(NULL, 0);

	if(wd == NULL)
		goto out;

	/* Clear strings */
	for(int i = 0; i < dirList->dirNum; i++)
		free(dirList->directories[i]);

	for(int i = 0; i < dirList->fileNum; i++)
		free(dirList->files[i]);

	free(dirList->currentDir);

	if((dirList->currentDir = strdup(wd)) == NULL)
		puts("Failure");

	if((dp = opendir(wd)) == NULL)
		goto out;

	while((ep = readdir(dp)) != NULL)
	{
		if(ep->d_type == DT_DIR)
		{
			/* Add more space for another pointer to a dirent struct */
			dirList->directories = realloc(dirList->directories, (dirNum + 1) * sizeof(char*));

			if((dirList->directories[dirNum] = strdup(ep->d_name)) == NULL)
				puts("Failure");

			dirNum++;
			continue;
		}

		/* Add more space for another pointer to a dirent struct */
		dirList->files = realloc(dirList->files, (fileNum + 1) * sizeof(char*));

		if((dirList->files[fileNum] = strdup(ep->d_name)) == NULL)
			puts("Failure");

		fileNum++;
	}

	qsort(&dirList->files[0], fileNum, sizeof(char *), cmpstringp);
	qsort(&dirList->directories[0], dirNum, sizeof(char *), cmpstringp);

	dirList->dirNum = dirNum;
	dirList->fileNum = fileNum;

	if(closedir(dp) != 0)
		goto out;

out:
	free(wd);
	return fileNum + dirNum;
}

/**
 * List current directory.
 *
 * \param	from	First entry in directory to list.
 * \param	max		Maximum number of entries to list. Must be > 0.
 * \param	select	File to show as selected. Must be > 0.
 * \return			Number of entries listed or negative on error.
 */
static int listDir(int from, int max, int select, struct dirList_t dirList)
{
	int				fileNum = 0;
	int				listed = 0;

	printf("\033[0;0H");
	printf("Dir: %.33s\n", dirList.currentDir);

	if(from == 0)
	{
		printf("\33[2K%c../\n", select == 0 ? '>' : ' ');
		listed++;
		max--;
	}

	while(dirList.fileNum + dirList.dirNum > fileNum)
	{
		fileNum++;

		if(fileNum <= from)
			continue;

		listed++;

		if(dirList.dirNum >= fileNum)
		{
			printf("\33[2K%c\x1b[34;1m%.37s/\x1b[0m\n",
					select == fileNum ? '>' : ' ',
					dirList.directories[fileNum - 1]);

		}

		/* fileNum must be referring to a file instead of a directory. */
		if(dirList.dirNum < fileNum)
		{
			printf("\33[2K%c%.37s\n",
					select == fileNum ? '>' : ' ',
					dirList.files[fileNum - dirList.dirNum - 1]);

		}

		if(fileNum == max + from)
			break;
	}

	return listed;
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

int main(int argc, char **argv)
{
	PrintConsole	topScreenLog, topScreenInfo, bottomScreen;
	int			fileMax;
	int			fileNum = 0;
	int			from = 0;
	Thread			watchdogThread;
	Handle			playbackFailEvent;
	struct watchdogInfo	watchdogInfoIn;
	struct errInfo_t	errInfo;
	struct playbackInfo_t	playbackInfo = { 0 };
	volatile int		error = 0;
	struct dirList_t	dirList = { 0 };

	gfxInitDefault();
	consoleInit(GFX_TOP, &topScreenLog);
	consoleInit(GFX_TOP, &topScreenInfo);
	consoleInit(GFX_BOTTOM, &bottomScreen);

	/* Set console sizes. */
	consoleSetWindow(&topScreenLog, 1, 3, 50, 36);
	consoleSetWindow(&topScreenInfo, 1, 1, 50, 2);

	consoleSelect(&bottomScreen);

	svcCreateEvent(&playbackFailEvent, RESET_ONESHOT);
	errInfo.error = &error;
	errInfo.failEvent = &playbackFailEvent;

	watchdogInfoIn.screen = &topScreenLog;
	watchdogInfoIn.errInfo = &errInfo;
	watchdogThread = threadCreate(playbackWatchdog,
			&watchdogInfoIn, 4 * 1024, 0x20, -2, true);

	playbackInfo.errInfo = &errInfo;

	chdir(DEFAULT_DIR);
	chdir("MUSIC");

	/* TODO: Not actually possible to get less than 0 */
	if(getDir(&dirList) < 0)
	{
		puts("Unable to obtain directory information");
		goto err;
	}

	if(listDir(from, MAX_LIST, 0, dirList) < 0)
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

		gfxFlushBuffers();
		gspWaitForVBlank();
		gfxSwapBuffers();

		hidScanInput();
		kDown = hidKeysDown();
		kHeld = hidKeysHeld();

		consoleSelect(&bottomScreen);

		/* Exit ctrmus */
		if(kDown & KEY_START)
			break;

#ifdef DEBUG
		consoleSelect(&topScreenLog);
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

				consoleSelect(&topScreenLog);
				if(togglePlayback() == true)
					puts("Paused");
				else
					puts("Playing");

				continue;
			}

			/* Show controls */
			if(kDown & KEY_LEFT)
			{
				consoleSelect(&topScreenLog);
				showControls();
				continue;
			}

			/* Stop */
			if(kDown & KEY_B)
			{
				stopPlayback();

				/* Clear playback information. */
				consoleSelect(&topScreenInfo);
				consoleClear();
				consoleSelect(&topScreenLog);
				consoleClear();

				changeFile(NULL, &playbackInfo);

				/* If the playback thread is currently playing, it will now
				 * stop and tell the Watchdog thread to display "Stopped".
				 */
				continue;
			}
		}

		if((kDown & KEY_UP ||
					((kHeld & KEY_UP) && (osGetTime() - mill > 500))) &&
				fileNum > 0)
		{
			fileNum--;

			// one line taken up by cwd, other by ../
			if(fileMax - fileNum > MAX_LIST-2 && from != 0)
				from--;

			if(listDir(from, MAX_LIST, fileNum, dirList) < 0)
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

			if(listDir(from, MAX_LIST, fileNum, dirList) < 0)
				err_print("Unable to list directory.");
		}

		if((kDown & KEY_LEFT ||
					((kHeld & KEY_LEFT) && (osGetTime() - mill > 500))) &&
				fileNum > 0)
		{
			int skip = MAX_LIST / 2;

			if(fileNum < skip)
				skip = fileNum;

			fileNum -= skip;

			// one line taken up by cwd, other by ../
			if(fileMax - fileNum > MAX_LIST-2 && from != 0)
			{
				from -= skip;
				if(from < 0)
					from = 0;
			}

			if(listDir(from, MAX_LIST, fileNum, dirList) < 0)
				err_print("Unable to list directory.");
		}

		if((kDown & KEY_RIGHT ||
					((kHeld & KEY_RIGHT) && (osGetTime() - mill > 500))) &&
				fileNum < fileMax)
		{
			int skip = fileMax - fileNum;

			if(skip > MAX_LIST / 2)
				skip = MAX_LIST / 2;

			fileNum += skip;

			if(fileNum >= MAX_LIST && fileMax - fileNum >= 0 &&
					from < fileMax - MAX_LIST)
			{
				from += skip;
				if(from > fileMax - MAX_LIST)
					from = fileMax - MAX_LIST;
			}

			if(listDir(from, MAX_LIST, fileNum, dirList) < 0)
				err_print("Unable to list directory.");
		}

		/*
		 * Pressing B goes up a folder, as well as pressing A or R when ".."
		 * is selected.
		 */
		if((kDown & KEY_B) ||
				((kDown & KEY_A) && (from == 0 && fileNum == 0)))
		{
			chdir("..");
			consoleClear();
			fileMax = getDir(&dirList);

			fileNum = 0;
			from = 0;

			if(listDir(from, MAX_LIST, fileNum, dirList) < 0)
				err_print("Unable to list directory.");

			continue;
		}

		if(kDown & KEY_A)
		{
			if(dirList.dirNum >= fileNum)
			{
				chdir(dirList.directories[fileNum - 1]);
				consoleClear();
				fileMax = getDir(&dirList);
				fileNum = 0;
				from = 0;

				if(listDir(from, MAX_LIST, fileNum, dirList) < 0)
					err_print("Unable to list directory.");
				continue;
			}

			if(dirList.dirNum < fileNum)
			{
				consoleSelect(&topScreenInfo);
				consoleClear();
				consoleSelect(&topScreenLog);
				//consoleClear();

				changeFile(dirList.files[fileNum - dirList.dirNum - 1], &playbackInfo);
				error = 0;
				continue;
			}
		}

		if (kDown & KEY_ZR && fileNum < fileMax) {
			fileNum += 1;
			if(fileNum >= MAX_LIST && fileMax - fileNum >= 0 &&
					from < fileMax - MAX_LIST)
				from++;
			consoleSelect(&topScreenInfo);
			consoleClear();
			consoleSelect(&topScreenLog);
			//consoleClear();
			changeFile(dirList.files[fileNum - dirList.dirNum - 1], &playbackInfo);
			error = 0;
			consoleSelect(&bottomScreen);
			if(listDir(from, MAX_LIST, fileNum, dirList) < 0) err_print("Unable to list directory.");
			continue;
		}

		// don't go to ../
		if (kDown & KEY_ZL && fileNum > 1) {
			fileNum -= 1;
			if(fileMax - fileNum > MAX_LIST-2 && from != 0)
				from--;
			consoleSelect(&topScreenInfo);
			consoleClear();
			consoleSelect(&topScreenLog);
			//consoleClear();
			changeFile(dirList.files[fileNum - dirList.dirNum - 1], &playbackInfo);
			error = 0;
			consoleSelect(&bottomScreen);
			if(listDir(from, MAX_LIST, fileNum, dirList) < 0) err_print("Unable to list directory.");
			continue;
		}

		// play next song automatically
		if (error == -1) {
			if (fileNum >= fileMax) {
				error = 0;
				continue;
			}
			fileNum += 1;
			consoleSelect(&topScreenInfo);
			consoleClear();
			consoleSelect(&topScreenLog);
			//consoleClear();
			changeFile(dirList.files[fileNum - dirList.dirNum - 1], &playbackInfo);
			error = 0;
			consoleSelect(&bottomScreen);
			if(listDir(from, MAX_LIST, fileNum, dirList) < 0) err_print("Unable to list directory.");
			continue;
		}

		/* After 1000ms, update playback time. */
		while(osGetTime() - mill > 1000)
		{
			consoleSelect(&topScreenInfo);
			/* Reset cursor position and print status. */
			printf("\033[0;0H");

			/* Avoid divide by zero. */
			if(playbackInfo.samples_per_second == 0)
				break;

			{
				unsigned hr, min, sec;
				size_t seconds_played;

				seconds_played = playbackInfo.samples_played / playbackInfo.samples_per_second; 

				hr = (seconds_played/3600); 
				min = (seconds_played - (3600*hr))/60;
				sec = (seconds_played -(3600*hr)-(min*60));

				printf("%02d:%02d:%02d", hr, min, sec);
			}

			if(playbackInfo.samples_total != 0)
			{
				unsigned hr, min, sec;
				size_t seconds_total;

				seconds_total = playbackInfo.samples_total / playbackInfo.samples_per_second; 

				hr = (seconds_total/3600); 
				min = (seconds_total - (3600*hr))/60;
				sec = (seconds_total -(3600*hr)-(min*60));

				printf(" %02d:%02d:%02d", hr, min, sec);
			}

			break;
		}
	}

out:
	puts("Exiting...");
	runThreads = false;
	svcSignalEvent(playbackFailEvent);
	changeFile(NULL, &playbackInfo);

	gfxExit();
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


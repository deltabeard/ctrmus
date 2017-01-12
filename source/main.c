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
#include "main.h"
#include "playback.h"

int main(int argc, char **argv)
{
	PrintConsole	topScreen;
	PrintConsole	bottomScreen;
	int				fileMax;
	int				fileNum = 0;
	int				from = 0;

	gfxInitDefault();
	sdmcInit();
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);

	chdir(DEFAULT_DIR);
	chdir("MUSIC");
	if(listDir(from, MAX_LIST, 0) < 0)
	{
		err_print("Unable to list directory.");
		goto err;
	}

	fileMax = getNumberFiles();

	consoleSelect(&topScreen);
	puts("Log");
	consoleSelect(&bottomScreen);

	/**
	 * This allows for music to continue playing through the headphones whilst
	 * the 3DS is closed.
	 */
	aptSetSleepAllowed(false);

	while(aptMainLoop())
	{
		u32 kDown;
		u32 kHeld;
		static u64	mill = 0;

		hidScanInput();

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		kDown = hidKeysDown();
		kHeld = hidKeysHeld();

		if(kDown & KEY_START)
			break;

#ifdef DEBUG
		consoleSelect(&topScreen);
		printf("\rNum: %d, Max: %d, from: %d   ", fileNum, fileMax, from);
		consoleSelect(&bottomScreen);
#endif

		if((kHeld & KEY_UP) && fileNum > 0 && fileNum > 0)
		{
			/* Limit the speed of the selector */
			if(osGetTime() - mill < 100)
				continue;

			fileNum--;

			if(fileMax - fileNum >= from && from != 0)
				from--;

			mill = osGetTime();

			consoleClear();
			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");
		}

		if((kHeld & KEY_DOWN) && fileNum < fileMax && fileNum < fileMax)
		{
			if(osGetTime() - mill < 100)
				continue;

			fileNum++;

			if(fileNum >= MAX_LIST && fileMax - fileNum >= 0 &&
					from < fileMax - MAX_LIST)
				from++;

			mill = osGetTime();

			consoleClear();
			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");
		}

		/*
		 * Pressing B goes up a folder, as well as pressing A or R when ".."
		 * is selected.
		 */
		if((kDown & KEY_B) ||
				((kDown & (KEY_A | KEY_R)) && (from == 0 && fileNum == 0)))
		{
			if(chdir("..") != 0)
				err_print("chdir");

			fileNum = 0;
			from = 0;
			fileMax = getNumberFiles();

			consoleClear();
			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");

			continue;
		}

		if(kDown & (KEY_A | KEY_R))
		{
			int				audioFileNum = 0;
			DIR				*dp;
			struct dirent	*ep;
			char*			wd = getcwd(NULL, 0);

			if(wd == NULL)
			{
				err_print("wd");
				goto err;
			}

			dp = opendir(wd);

			if(dp != NULL)
			{
				char* file = NULL;

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
					consoleClear();
					fileMax = getNumberFiles();
					if(listDir(from, MAX_LIST, fileNum) < 0)
						err_print("Unable to list directory.");

					closedir(dp);
					free(wd);
					continue;
				}

				if(asprintf(&file, "%s%s", wd, ep->d_name) == -1)
				{
					err_print("Constructing file name failed.");
					file = NULL;
				}
				else
				{
					consoleSelect(&topScreen);
					playFile(file);
					consoleSelect(&bottomScreen);
				}

				free(file);
				free(wd);

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
	char*			wd = getcwd(NULL, 0);
	int				ret = 0;

	if(wd == NULL)
		goto err;

	if((dp = opendir(wd)) == NULL)
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

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
#include "flac.h"
#include "main.h"
#include "mp3.h"
#include "opus.h"
#include "wav.h"

/* Default folder */
#define DEFAULT_DIR		"sdmc:/"
/* Maximum number of lines that can be displayed */
#define	MAX_LIST		27

enum file_types {
	FILE_TYPE_ERROR = -1,
	FILE_TYPE_WAV,
	FILE_TYPE_FLAC,
	FILE_TYPE_OGG,
	FILE_TYPE_OPUS,
	FILE_TYPE_MP3
};

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
					switch(getFileType(file))
					{
						case FILE_TYPE_WAV:
							playWav(file);
							break;

						case FILE_TYPE_FLAC:
							playFlac(file);
							break;

						case FILE_TYPE_OPUS:
							playOpus(file);
							break;

						case FILE_TYPE_MP3:
							playMp3(file);
							break;

						default:
							consoleSelect(&bottomScreen);
							printf("Unsupported File type.\n");
					}
				}

				consoleSelect(&bottomScreen);

				free(file);
				free(wd);

				if(closedir(dp) != 0)
					err_print("Closing directory failed.");
			}
			else
				err_print("Unable to open directory.");
		}
	}

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

	printf("Dir: %.30s\n", wd);

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

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			File type, else negative.
 */
int getFileType(const char *file)
{
	FILE* ftest = fopen(file, "rb");
	int fileSig = 0;
	enum file_types file_type = FILE_TYPE_ERROR;

	if(ftest == NULL)
	{
		err_print("Opening file failed.");
		printf("file: %s\n", file);
		return -1;
	}

	if(fread(&fileSig, 4, 1, ftest) == 0)
	{
		err_print("Unable to read file.");
		fclose(ftest);
		return -1;
	}

	switch(fileSig)
	{
		// "RIFF"
		case 0x46464952:
			if(fseek(ftest, 4, SEEK_CUR) != 0)
			{
				err_print("Unable to seek.");
				break;
			}

			// "WAVE"
			// Check required as AVI file format also uses "RIFF".
			if(fread(&fileSig, 4, 1, ftest) == 0)
			{
				err_print("Unable to read potential WAV file.");
				break;
			}

			if(fileSig != 0x45564157)
				break;

			file_type = FILE_TYPE_WAV;
			printf("File type is WAV.");
			break;

		// "fLaC"
		case 0x43614c66:
			file_type = FILE_TYPE_FLAC;
			printf("File type is FLAC.");
			break;

		// "OggS"
		case 0x5367674f:
			if(isOpus(file) == 0)
			{
				printf("\nFile type is Opus.");
				file_type = FILE_TYPE_OPUS;
			}
			else
			{
				file_type = FILE_TYPE_OGG;
				printf("\nFile type is OGG.");
			}

			break;

		default:
			/*
			 * MP3 without ID3 tag, ID3v1 tag is at the end of file, or MP3
			 * with ID3 tag at the beginning  of the file.
			 */
			if((fileSig << 16) == 0xFBFF0000 || (fileSig << 8) == 0x33444900)
			{
				puts("File type is MP3.");
				file_type = FILE_TYPE_MP3;
				break;
			}

			printf("Unknown magic number: %#010x\n.", fileSig);
	}

	fclose(ftest);
	return file_type;
}

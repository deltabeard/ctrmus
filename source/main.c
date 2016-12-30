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

#include "all.h"
#include "flac.h"
#include "main.h"
#include "opus.h"
#include "wav.h"

/* Default folder */
#define TOP_FOLDER		"sdmc:/"
/* Maximum number of lines that can be displayed */
#define	MAX_LIST		28

enum file_types {
	FILE_TYPE_ERROR = -1,
	FILE_TYPE_WAV,
	FILE_TYPE_FLAC,
	FILE_TYPE_OGG,
	FILE_TYPE_OPUS
};

int main(int argc, char **argv)
{
	DIR				*dp;
	struct dirent	*ep;
	PrintConsole	topScreen;
	PrintConsole	bottomScreen;
	int				fileMax;
	int				fileNum = 0;

	gfxInitDefault();
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);

	if((fileMax = listDir(TOP_FOLDER, 0, MAX_LIST)) < 0)
	{
		err_print("Unable to list directory.");
		goto out;
	}

	consoleSelect(&topScreen);
	puts("Queue");

	/**
	 * This allows for music to continue playing through the headphones whilst
	 * the 3DS is closed.
	 */
	aptSetSleepAllowed(false);

	while(aptMainLoop())
	{
		u32 kDown;

		hidScanInput();

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		kDown = hidKeysDown();

		if(kDown & KEY_START)
			break;

		if(kDown & KEY_UP)
		{
			if(fileNum < fileMax)
				fileNum++;
		}

		if(kDown & KEY_DOWN)
		{
			if(fileNum > 0)
				fileNum--;
		}

		if(kDown & (KEY_DOWN | KEY_UP))
		{
			printf("\33[2K\rSelected file %d", fileNum);
		}

		if(kDown & (KEY_A | KEY_R))
		{
			int audioFileNum = 0;
			dp = opendir(TOP_FOLDER);
			char* file = NULL;

			if (dp != NULL)
			{
				while((ep = readdir(dp)) != NULL)
				{
					if(audioFileNum == fileNum)
						break;

					audioFileNum++;
				}

				if(asprintf(&file, "%s%s", AUDIO_FOLDER, ep->d_name) == -1)
				{
					err_print("Constructing file name failed.");
					file = NULL;
				}
				else
				{
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

						default:
							printf("Unsupported File type.\n");
					}
				}

				if(closedir(dp) != 0)
					err_print("Closing directory failed.");
			}
			else
				err_print("Unable to open directory.");

			free(file);
		}
	}

out:
	puts("Exiting...");

	gfxExit();
	return 0;
}

/**
 * List directory.
 *
 * \param	dir		Path of directory.
 * \param	from	First entry in directory to list.
 * \param	max		Maximum number of entries to list. Must be > 0.
 * \return			Number of entries listed or negative on error.
 */
int listDir(const char *dir, int from, int max)
{
	DIR				*dp;
	struct dirent	*ep;
	int				fileNum = 0;
	int				countChr = 0;
	int				listed = 0;

	if((dp = opendir(dir)) == NULL)
		goto err;

	/* Count number of occurrences of character in string. */
	for(int i = 0; i < strlen(dir); i++)
	{
		if(dir[i] == '/')
			countChr++;
	}

	/* There should always be one slash. So error out here. */
	if(countChr < 1)
	{
		errno = ENOMSG;
		goto err;
	}

	if(countChr > 1 && from == 0)
		puts("../");

	while((ep = readdir(dp)) != NULL)
	{
		fileNum++;

		if(fileNum < from)
			continue;

		if(fileNum + from == max)
			break;

		listed++;
		printf(" %.48s%s\n", ep->d_name, ep->d_type == DT_DIR ? "/" : "");
	}

	if(closedir(dp) != 0)
		goto err;

	return listed;

err:
	return -1;
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
	}

	fclose(ftest);
	return file_type;
}

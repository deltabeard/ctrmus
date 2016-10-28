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
#include <stdio.h>
#include <string.h>

#include "main.h"

#define BUFFER_SIZE 128 * 1024
#define AUDIO_FOLDER "sdmc:/audio/"

int main()
{
	DIR *dp;
	struct dirent *ep;
	u8 fileMax = 0;
	u8 fileNum = 1;

	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);

	if(R_FAILED(csndInit()))
	{
		printf("Error %d: Could not initialize CSND.", __LINE__);
		goto out;
	}

	puts("Scanning audio directory.");

	dp = opendir(AUDIO_FOLDER);
	if(dp != NULL)
	{
		while((ep = readdir(dp)) != NULL)
			printf("%d: %s\n", ++fileMax, ep->d_name);

		(void)closedir(dp);
	}
	else
	{
		puts("Couldn't open the directory");
		goto out;
	}

	if(fileMax == 0)
	{
		puts("Error: No files in audio folder.");
		goto out;
	}

	while(aptMainLoop())
	{
		u32 kDown;
		char file[128]; //TODO: Make this dynamic.

		hidScanInput();
		gspWaitForVBlank();
		kDown = hidKeysDown();

		if(kDown & KEY_START)
			break;

		if(kDown & KEY_UP && fileNum < fileMax)
		{
			fileNum++;
			printf("Selected file %d\n", fileNum);
		}

		if(kDown & KEY_DOWN && fileNum > 1)
		{
			fileNum--;
			printf("Selected file %d\n", fileNum);
		}

		if(kDown & KEY_A)
		{
			u8 audioFileNum = 0;

			dp = opendir(AUDIO_FOLDER);
			if (dp != NULL)
			{
				while((ep = readdir(dp)) != NULL)
				{
					audioFileNum++;
					if(audioFileNum == fileNum)
						break;
				}
				(void)closedir(dp);
				snprintf(file, sizeof(file), "%s%s", AUDIO_FOLDER, ep->d_name);
			}
			playWav(file);
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
	}

out:
	puts("Exiting...");

	csndExit();
	gfxExit();
	return 0;
}

/**
 * Plays a WAV file.
 *
 * \param	file	File location of WAV file.
 * \return			Zero if successful, else failure.
 */
int playWav(const char *wav)
{
	FILE	*file	= fopen(wav, "rb");
	char	header[45];
	u32		sample;
	u8		format;
	u8		channels;
	u32		bitness;
	u8*		buffer;
	u8*		left1 = NULL;
	u8*		left2 = NULL;
	u8*		right1 = NULL;
	u8*		right2 = NULL;
	off_t	bytesRead1;
	off_t	bytesRead2;
	off_t	size;

	if(file == NULL)
	{
		puts("Opening file failed.");
		return 1;
	}

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if(size > BUFFER_SIZE)
		size = BUFFER_SIZE;

	buffer = linearAlloc(size);

	if(fread(header, 1, 44, file) == 0)
	{
		puts("Unable to read WAV file.");
		goto out;
	}

	if(strncmp(header + 8, "WAVE", 4) == 0)
		puts("Valid WAV file.");
	else
	{
		puts("Invalid WAV file.");
		goto out;
	}

	/**
	 * http://www.topherlee.com/software/pcm-tut-wavformat.html and
	 * http://soundfile.sapp.org/doc/WaveFormat/ helped a lot.
	 */
	format = (header[19]<<8) + (header[20]);
	channels = (header[23]<<8) + (header[22]);
	sample = (header[27]<<24) + (header[26]<<16) + (header[25]<<8) +
		(header[24]);
	bitness = (header[35]<<8) + (header[34]);
	printf("Format: %s, Ch: %d, Sam: %lu, bit: %lu\n",
			format == 1 ? "PCM" : "Other", channels, sample, bitness);

	/* Prepare for stereo playing */
	if(channels == 2)
	{
		right1 = linearAlloc(size/2);
		right2 = linearAlloc(size/2);
		left1 = linearAlloc(size/2);
		left2 = linearAlloc(size/2);
	}
	else if(channels > 2)
	{
		puts("Unsupported number of channels.");
		goto out;
	}

	switch(bitness)
	{
		case 8:
			bitness = SOUND_FORMAT_8BIT;
			break;

		case 16:
			bitness = SOUND_FORMAT_16BIT;
			break;

		default:
			printf("Bitness of %lu unsupported.\n", bitness);
			goto out;
	}

	printf("Playing %s\n", wav);

	while((bytesRead1 = fread(buffer, 1, size, file)) > 0)
	{
		u8 status = 1;

		for(int i = 0, leftLoc = 0, rightLoc = 0; i < bytesRead1 && channels == 2; i++)
		{
			if(i%2 == 0)
			{
				left1[leftLoc++] = buffer[i];
			}
			else
			{
				right1[rightLoc++] = buffer[i];
			}

			printf("\rBuffering %d", i);
		}

		if(R_FAILED(GSPGPU_FlushDataCache(buffer, size)))
			puts("Flush failed.");

		if(R_FAILED(GSPGPU_FlushDataCache(left1, size/2)))
			puts("Flush failed.");

		if(R_FAILED(GSPGPU_FlushDataCache(right1, size/2)))
			puts("Flush failed.");

		while(status != 0)
		{
			u32 kDown;

			csndIsPlaying(8, &status);

			hidScanInput();
			kDown = hidKeysDown();

			if(kDown & KEY_B)
				goto out;
		}

		if(channels == 2)
		{
			csndPlaySound(8, bitness | SOUND_ONE_SHOT, sample, 1, -1,
					left1, NULL, bytesRead1 / 2);
			csndPlaySound(9, bitness | SOUND_ONE_SHOT, sample, 1, 1,
					right1, NULL, bytesRead1 / 2);
		}
		else if(csndPlaySound(8, bitness | SOUND_ONE_SHOT, sample, 1, 0,
					buffer, NULL, bytesRead1) != 0)
		{
			printf("Error %d.\n", __LINE__);
			goto out;
		}

		bytesRead2 = fread(buffer, 1, size, file);

		printf("Size of buffer: %d", size);

		for(int i = 0, leftLoc = 0, rightLoc = 0;
				i < bytesRead2 && channels > 1; i++)
		{
			if(i%2== 0)
			{
				left2[leftLoc++] = buffer[i];
			}
			else
			{
				right2[rightLoc++] = buffer[i];
			}
			printf("\rBuffering %d", i);
		}

		if(R_FAILED(GSPGPU_FlushDataCache(buffer, size)))
			puts("Flush failed.");

		if(R_FAILED(GSPGPU_FlushDataCache(left2, size/2)))
			puts("Flush failed.");

		if(R_FAILED(GSPGPU_FlushDataCache(right2, size/2)))
			puts("Flush failed.");

		status = 1;

		while(status != 0)
		{
			u32 kDown;

			csndIsPlaying(8, &status);

			hidScanInput();
			kDown = hidKeysDown();

			if(kDown & KEY_B)
				goto out;
		}

		if(bytesRead2 == 0)
			goto out;

		if(channels == 2)
		{
			csndPlaySound(8, bitness | SOUND_ONE_SHOT, sample, 1, -1,
					left2, NULL, bytesRead2 / 2);
			csndPlaySound(9, bitness | SOUND_ONE_SHOT, sample, 1, 1,
					right2, NULL, bytesRead2 / 2);
		}
		else if(csndPlaySound(8, bitness | SOUND_ONE_SHOT, sample, 1, 0,
					buffer, NULL, bytesRead2) != 0)
		{
			printf("Error %d.\n", __LINE__);
			goto out;
		}
	}

out:
	puts("Stopping playback.");

	csndExecCmds(true);
	CSND_SetPlayState(8, 0);
	CSND_SetPlayState(9, 0);
	if(R_FAILED(CSND_UpdateInfo(0)))
		printf("Failed to stop audio playback.\n");

	fclose(file);
	linearFree(buffer);
	linearFree(left1);
	linearFree(left2);
	linearFree(right1);
	linearFree(right2);
	return 0;
}

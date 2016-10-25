/**
 * ctrmus - 3DS Music Player
 * Copyright (C) 2016 Mahyar Koshkouei
 *
 * This program comes with ABSOLUTELY NO WARRANTY and is free software. You are
 * welcome to redistribute it under certain conditions; for details see the
 * LICENCE file.
 */

#include <3ds.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 1 * 1024 * 1024
#define AUDIO_FOLDER "sdmc:/audio/"

int playWav(const char *wav);

int main()
{
	DIR *dp;
	struct dirent *ep;
	u8 fileMax = 0;
	u8 fileNum = 0;

	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);

	puts("Initializing CSND...");

	if(R_FAILED(csndInit()))
	{
		printf("Error %d: Could not initialize CSND.", __LINE__);
		goto out;
	}
	else
		puts("CSND initialized.");

	dp = opendir("sdmc:/audio/");
	if (dp != NULL)
	{
		while(ep = readdir(dp))
			printf("%d: %s\n", ++fileMax, ep->d_name);

		(void)closedir(dp);
	}
	else
		puts("Couldn't open the directory");

	if(fileMax == 0)
	{
		puts("Error: No files in audio folder.");
		goto out;
	}

	while(aptMainLoop())
	{
		u32 kDown;
		u8 ret;
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

		if(kDown & KEY_X)
		{
			u8 audioFileNum = 0;

			dp = opendir(AUDIO_FOLDER);
			if (dp != NULL)
			{
				while(ep = readdir(dp))
				{
					audioFileNum++;
					if(audioFileNum == fileNum)
						break;
				}
				(void)closedir(dp);
				snprintf(file, sizeof(file), "%s%s", AUDIO_FOLDER, ep->d_name);
			}
			printf("Opening file %s\n", file);
			playWav(file); // No error checking. Terribad.
		}

		if(kDown & KEY_A && (ret = playWav("sdmc:/audio/audio.wav") != 0))
		{
			printf("Error ");
			switch(ret)
			{
				case 1:
					printf("%d: Audio file missing.\n", __LINE__);
					break;

				case 2:
					printf("%d: \"csnd\" init failed.\n", __LINE__);
					break;

				default:
					printf("%d: Unknown.\n", __LINE__);
					break;
			}
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
	FILE *file		= fopen(wav, "rb");
	u8* buffer1;
	u8* buffer2;
	off_t bytesRead1;
	off_t bytesRead2;
	off_t size;
	u8 chunk = 0;

	printf("Got to line %d\n", __LINE__);

	if(file == NULL)
		return 1;

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);

	printf("Got to line %d. Size: %d\n", __LINE__, size);
	if(size > BUFFER_SIZE)
		size = BUFFER_SIZE;

	printf("Got to line %d\n", __LINE__);

	buffer1 = linearAlloc(size);
	buffer2 = linearAlloc(size);

	printf("Got to line %d. Size: %d\n", __LINE__, size);

	while((bytesRead1 = fread(buffer1, 1, size, file)) > 0)
	{
		u8 status = 1;

		printf("Chunk %d", ++chunk);

		while(status != 0)
		{
			u32 kDown;

			csndIsPlaying(8, &status);

			hidScanInput();
			kDown = hidKeysDown();

			if(kDown & KEY_B)
				goto out;
		}

		if(R_FAILED(GSPGPU_FlushDataCache(buffer1, size)))
			puts("Flush failed.");

		if(csndPlaySound(8, SOUND_FORMAT_16BIT | SOUND_ONE_SHOT, 48000, 1, 0,
					buffer1, NULL, bytesRead1) != 0)
		{
			printf("Error %d.\n", __LINE__);
			goto out;
		}

		bytesRead2 = fread(buffer2, 1, size, file);

		if(R_FAILED(GSPGPU_FlushDataCache(buffer2, size)))
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

		printf("Chunk %d", ++chunk);

		if(csndPlaySound(8, SOUND_FORMAT_16BIT | SOUND_ONE_SHOT, 48000, 1, 0,
					buffer2, NULL, bytesRead2) != 0)
		{
			printf("Error %d.\n", __LINE__);
			goto out;
		}
	}

out:
	printf("Got to line %d\n", __LINE__);
	csndExecCmds(true);
	CSND_SetPlayState(8, 0);
	if(R_FAILED(CSND_UpdateInfo(0)))
		printf("Failed to stop audio playback.\n");

	fclose(file);
	linearFree(buffer1);
	linearFree(buffer2);
	return 0;
}

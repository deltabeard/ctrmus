/**
 * ctrmus - 3DS Music Player
 * Copyright (C) 2016 Mahyar Koshkouei
 *
 * This program comes with ABSOLUTELY NO WARRANTY and is free software. You are
 * welcome to redistribute it under certain conditions; for details see the
 * LICENCE file.
 */

#include <3ds.h>
#include <stdio.h>

#define BUFFER_SIZE 1131072

int playWav(const char *wav);

int main()
{
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

	/**
	 * Open File
	 * Read 16 kb from file
	 * Play 16 kb buffer
	 * Wait until finished playing
	 *  Read next 16 kb in the meantime
	 * Play next chunk
	 * Continue until end of file
	 */

	while(aptMainLoop())
	{
		u32 kDown;
		u8 ret;

		hidScanInput();
		gspWaitForVBlank();
		kDown = hidKeysDown();

		if(kDown & KEY_START)
			break;

		if(kDown & KEY_A && (ret = playWav("sdmc:/audio/audio3.wav") != 0))
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
	u8* buffer;
	off_t bytesRead;
	off_t size;

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
	buffer = linearAlloc(size);

	printf("Got to line %d. Size: %d\n", __LINE__, size);
	while((bytesRead = fread(buffer, 1, size, file)) > 0)
	{
		u8 status = 1;

		while(status != 0)
			csndIsPlaying(8, &status);

		printf("Got to line %d. Read %d bytes\n", __LINE__, bytesRead);

		if(csndPlaySound(8, SOUND_FORMAT_16BIT | SOUND_ONE_SHOT, 48000, 1, 0,
					buffer, buffer, size) != 0)
		{
			printf("Error %d.\n", __LINE__);
		}
	}

	printf("Got to line %d\n", __LINE__);
	linearFree(buffer);
	return 0;
}

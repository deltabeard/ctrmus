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
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

#define BUFFER_SIZE		16 * 1024
#define AUDIO_FOLDER	"sdmc:/MUSIC/"
#define CHANNEL			0x08

/* Adds extra debugging text */
#define DEBUG 0

/* From: http://stackoverflow.com/a/1644898 */
#define debug_print(fmt, ...) \
	do { if (DEBUG) fprintf(stderr, "%d:%s(): " fmt, __LINE__,\
			__func__, __VA_ARGS__); } while (0)

#define err_print(err) \
	do { fprintf(stderr, "\nError %d:%s(): %s %s\n", __LINE__, __func__, \
			err, strerror(errno)); } while (0)

int main(int argc, char **argv)
{
	DIR				*dp;
	struct dirent	*ep;
	PrintConsole	topScreen;
	PrintConsole	bottomScreen;
	u8 fileMax = 0;
	u8 fileNum = 1;

	gfxInitDefault();
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&topScreen);

	puts("Scanning audio directory.");

	dp = opendir(AUDIO_FOLDER);
	if(dp != NULL)
	{
		while((ep = readdir(dp)) != NULL)
			printf("%d: %s\n", ++fileMax, ep->d_name);

		if(closedir(dp) != 0)
			err_print("Closing directory failed.");
	}
	else
	{
		err_print("Opening directory failed.");
		goto out;
	}

	if(fileMax == 0)
	{
		err_print("No files in audio folder.");
		goto out;
	}

	consoleSelect(&bottomScreen);

	/**
	 * This allows for music to continue playing through the headphones whilst
	 * the 3DS is closed.
	 */
	aptSetSleepAllowed(false);

	while(aptMainLoop())
	{
		u32 kDown;
		char* file = NULL;

		hidScanInput();

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		kDown = hidKeysDown();

		if(kDown & KEY_START)
			break;

		if(kDown & KEY_UP && fileNum < fileMax)
		{
			fileNum++;
			printf("\rSelected file %d   ", fileNum);
		}

		if(kDown & KEY_DOWN && fileNum > 1)
		{
			fileNum--;
			printf("\rSelected file %d   ", fileNum);
		}

		if(kDown & (KEY_A | KEY_R))
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

				if(closedir(dp) != 0)
					err_print("Closing directory failed.");

				if(asprintf(&file, "%s%s", AUDIO_FOLDER, ep->d_name) == -1)
				{
					err_print("Constructing file name failed.");
					file = NULL;
				}
			}

			if(file == NULL)
				err_print("Opening file failed.");
			else
				playWav(file);

			free(file);
		}
	}

out:
	puts("Exiting...");

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
	u8		bitness;
	u32		byterate; // TODO: Not used.
	u32		blockalign;
	s16*	buffer1 = NULL;
	s16*	buffer2 = NULL;
	ndspWaveBuf waveBuf[2];
	bool playing = true;
	bool lastbuf = false;

	if(R_FAILED(ndspInit()))
	{
		err_print("Initialising ndsp failed.");
		goto out;
	}

	ndspSetOutputMode(NDSP_OUTPUT_STEREO);

	if(file == NULL)
	{
		err_print("Opening file failed.");
		goto out;
	}

	if(fread(header, 1, 44, file) == 0)
	{
		err_print("Unable to read WAV file.");
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
	byterate = (header[31]<<24) + (header[30]<<16) + (header[29]<<8) +
		(header[28]);
	blockalign = (header[33]<<8) + (header[32]);
	bitness = (header[35]<<8) + (header[34]);
	printf("Format: %s(%d), Ch: %d, Sam: %lu, bit: %d, BR: %lu, BA: %lu\n",
			format == 1 ? "PCM" : "Other", format, channels, sample, bitness,
			byterate, blockalign);

	if(channels > 2)
	{
		puts("Error: Invalid number of channels.");
		goto out;
	}

	/**
	 * Playing ADPCM, and 8 bit WAV files are disabled as they both sound like
	 * complete garbage.
	 */
	switch(bitness)
	{
		case 8:
			bitness = channels == 2 ? NDSP_FORMAT_STEREO_PCM8 : NDSP_FORMAT_MONO_PCM8;
			puts("8bit playback disabled.");
			goto out;

		case 16:
			bitness = channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16;
			break;

		default:
			printf("Bitness of %d unsupported.\n", bitness);
			goto out;
	}

	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	/* Polyphase sounds much better than linear or no interpolation */
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, sample);
	ndspChnSetFormat(CHANNEL, bitness);
	memset(waveBuf, 0, sizeof(waveBuf));

	buffer1 = (s16*) linearAlloc(BUFFER_SIZE);
	buffer2 = (s16*) linearAlloc(BUFFER_SIZE);

	fread(buffer1, 1, BUFFER_SIZE, file);
	fread(buffer2, 1, BUFFER_SIZE, file);
	waveBuf[0].nsamples = BUFFER_SIZE / blockalign;
	waveBuf[0].data_vaddr = &buffer1[0];
	waveBuf[1].nsamples = BUFFER_SIZE / blockalign;
	waveBuf[1].data_vaddr = &buffer2[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);

	printf("Playing %s\n", wav);

	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false)
	{}

	while(playing == false || ndspChnIsPlaying(CHANNEL) == true)
	{
		u32 kDown;

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		hidScanInput();
		kDown = hidKeysDown();

		if(kDown & KEY_B)
			break;

		if(kDown & KEY_A)
			playing = !playing;

		if(playing == false || lastbuf == true)
			continue;

		if(waveBuf[0].status == NDSP_WBUF_DONE)
		{
			size_t read = fread(buffer1, 1, BUFFER_SIZE, file);

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < BUFFER_SIZE)
				waveBuf[0].nsamples = read / blockalign;

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
		}

		if(waveBuf[1].status == NDSP_WBUF_DONE)
		{
			size_t read = fread(buffer2, 1, BUFFER_SIZE, file);

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < BUFFER_SIZE)
				waveBuf[1].nsamples = read / blockalign;

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);
		}

		DSP_FlushDataCache(buffer1, BUFFER_SIZE);
		DSP_FlushDataCache(buffer2, BUFFER_SIZE);

		// TODO: Remove this printf.
		printf("\rBuf0: %s, Buf1: %s.", waveBuf[0].status == NDSP_WBUF_QUEUED ? "Queued" : "Playing",
				waveBuf[1].status == NDSP_WBUF_QUEUED ? "Queued" : "Playing");
	}

	debug_print("Pos: %lx\n", ndspChnGetSamplePos(CHANNEL));
	debug_print("%s\n", "Before clear");

	ndspChnWaveBufClear(CHANNEL);

out:
	puts("Stopping playback.");

	ndspExit();
	fclose(file);
	linearFree(buffer1);
	linearFree(buffer2);
	return 0;
}

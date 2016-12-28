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
#include <opus/opusfile.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "opus.h"
#include "flac.h"

#define BUFFER_SIZE		(16 * 1024)
#define AUDIO_FOLDER	"sdmc:/MUSIC/"
#define CHANNEL			0x08

/* Adds extra debugging text */
#define DEBUG 0

#define err_print(err) \
	do { fprintf(stderr, "\nError %d:%s(): %s %s\n", __LINE__, __func__, \
			err, strerror(errno)); } while (0)

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
	u8				fileMax = 0;
	u8				fileNum = 1;

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

		hidScanInput();

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		kDown = hidKeysDown();

		if(kDown & KEY_START)
			break;

		if(kDown & KEY_UP)
		{
			printf("\33[2K\rSelected file %d",
					++fileNum > fileMax ? 1 : fileNum);
		}

		if(kDown & KEY_DOWN)
		{
			printf("\33[2K\rSelected file %d",
					--fileNum == 0 ? fileMax : fileNum);
		}

		if(fileNum == 0)
			fileNum = fileMax;
		else if(fileNum > fileMax)
			fileNum = 1;

		if(kDown & (KEY_A | KEY_R))
		{
			u8 audioFileNum = 0;
			dp = opendir(AUDIO_FOLDER);
			char* file = NULL;

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

				/**
				 * TODO: There is an issue with Unicode files here.
				 * Playing a flac file then a file with a name containing the
				 * character 'é' will cause the asprint to not work properly.
				 */
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

/**
 * Plays a WAV file.
 *
 * \param	file	File location of WAV file.
 * \return			Zero if successful, else failure.
 */
int playWav(const char *wav)
{
	FILE*	file	= fopen(wav, "rb");
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

	// TODO: Check if this is required.
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);

	if(file == NULL)
	{
		err_print("Opening file failed.");
		goto out;
	}

	/* TODO: No need to read the first number of bytes */
	if(fread(header, 1, 44, file) == 0)
	{
		err_print("Unable to read WAV file.");
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
			bitness = channels == 2 ? NDSP_FORMAT_STEREO_PCM8 :
				NDSP_FORMAT_MONO_PCM8;
			puts("8bit playback disabled.");
			goto out;

		case 16:
			bitness = channels == 2 ? NDSP_FORMAT_STEREO_PCM16 :
				NDSP_FORMAT_MONO_PCM16;
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
	waveBuf[0].nsamples = BUFFER_SIZE / blockalign;
	waveBuf[0].data_vaddr = &buffer1[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);

	fread(buffer2, 1, BUFFER_SIZE, file);
	waveBuf[1].nsamples = BUFFER_SIZE / blockalign;
	waveBuf[1].data_vaddr = &buffer2[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);

	printf("Playing %s\n", wav);

	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false);

	while(playing == false || ndspChnIsPlaying(CHANNEL) == true)
	{
		u32 kDown;
		/* Number of bytes read from file.
		 * Static only for the purposes of the printf debug at the bottom.
		 */
		static size_t read = 0;

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		hidScanInput();
		kDown = hidKeysDown();

		if(kDown & KEY_B)
			break;

		if(kDown & (KEY_A | KEY_R))
			playing = !playing;

		if(playing == false || lastbuf == true)
		{
			printf("\33[2K\rPaused");
			continue;
		}

		if(waveBuf[0].status == NDSP_WBUF_DONE)
		{
			read = fread(buffer1, 1, BUFFER_SIZE, file);

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
			read = fread(buffer2, 1, BUFFER_SIZE, file);

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
	}

	ndspChnWaveBufClear(CHANNEL);

out:
	puts("Stopping playback.");

	ndspExit();
	fclose(file);
	linearFree(buffer1);
	linearFree(buffer2);
	return 0;
}

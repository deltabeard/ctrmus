#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "all.h"
#include "wav.h"

#define BUFFER_SIZE	(16 * 1024)

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
#ifdef DEBUG
	printf("Format: %s(%d), Ch: %d, Sam: %lu, bit: %d, BR: %lu, BA: %lu\n",
			format == 1 ? "PCM" : "Other", format, channels, sample, bitness,
			byterate, blockalign);
#endif

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
		{
			playing = !playing;
			printf("\33[2K\r%s", playing == false ? "Paused" : "");
		}

		if(playing == false || lastbuf == true)
			continue;

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

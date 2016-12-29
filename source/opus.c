#include <3ds.h>
#include <opus/opusfile.h>
#include <stdlib.h>
#include <string.h>

#include "opus.h"

#define SAMPLES_TO_READ	(32 * 1024)
#define CHANNEL			0x08

int playOpus(const char* in)
{
	int				err = 0;
	int16_t*		buffer1 = linearAlloc(SAMPLES_TO_READ * sizeof(int16_t));
	int16_t*		buffer2 = linearAlloc(SAMPLES_TO_READ * sizeof(int16_t));
	ndspWaveBuf 	waveBuf[2];
	bool			playing = true;
	bool			lastbuf = false;
	OggOpusFile*	opusFile = op_open_file(in, &err);
	const OpusHead*	opusHead;
	int				link;

	if(err != 0)
	{
		printf("libopusfile failed with error %d.", err);
		return -1;
	}

	if(R_FAILED(ndspInit()))
	{
		printf("Initialising ndsp failed.");
		goto out;
	}

	if((link = op_current_link(opusFile)) < 0)
	{
		printf("Error getting current link: %d\n", link);
		goto out;
	}

	opusHead = op_head(opusFile, link);
	printf("\nRate: %lu\tChan: %d\n", opusHead->input_sample_rate,
			opusHead->channel_count);

	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, opusHead->input_sample_rate);
	ndspChnSetFormat(CHANNEL, NDSP_FORMAT_STEREO_PCM16);

	memset(waveBuf, 0, sizeof(waveBuf));
	waveBuf[0].nsamples =
		fillOpusBuffer(opusFile, SAMPLES_TO_READ, buffer1) / 2;
	waveBuf[0].data_vaddr = &buffer1[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);

	waveBuf[1].nsamples =
		fillOpusBuffer(opusFile, SAMPLES_TO_READ, buffer2) / 2;
	waveBuf[1].data_vaddr = &buffer2[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);

	printf("Playing %s\n", in);
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

		printf("\33[2K\r");

		if(waveBuf[0].status == NDSP_WBUF_DONE)
		{
			read = fillOpusBuffer(opusFile, SAMPLES_TO_READ, buffer1);

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < SAMPLES_TO_READ)
				waveBuf[0].nsamples = read / opusHead->channel_count;

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
		}

		if(waveBuf[1].status == NDSP_WBUF_DONE)
		{
			read = fillOpusBuffer(opusFile, SAMPLES_TO_READ, buffer2);

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < SAMPLES_TO_READ)
				waveBuf[1].nsamples = read / opusHead->channel_count;

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);
		}

		DSP_FlushDataCache(buffer1, SAMPLES_TO_READ * sizeof(s16));
		DSP_FlushDataCache(buffer2, SAMPLES_TO_READ * sizeof(s16));
	}

out:
	printf("\nStopping Opus playback.\n");
	ndspChnWaveBufClear(CHANNEL);
	ndspExit();
	linearFree(buffer1);
	linearFree(buffer2);
	op_free(opusFile);
	return 0;
}

/**
 * Fill a buffer with decoded samples.
 *
 * \param opusFile		OggOpusFile pointer.
 * \param samplesToRead	Number of samples to read in to buffer.
 * \param bufferOut		Pointer to output buffer.
 * \return				Number of samples read per channel.
 */
uint64_t fillOpusBuffer(OggOpusFile* opusFile, uint64_t samplesToRead,
		int16_t* bufferOut)
{
	uint64_t samplesRead = 0;

	while(samplesToRead > 0)
	{
		uint64_t samplesJustRead = op_read_stereo(opusFile, bufferOut,
				samplesToRead > 120*48*2 ? 120*48*2 : samplesToRead);

		if(samplesJustRead < 0)
		{
			printf("\nFatal error decoding Opus: %llu.", samplesJustRead);
			return 0;
		}
		else if(samplesJustRead == 0)
		{
			/* End of file reached. */
			break;
		}

		samplesRead += samplesJustRead * 2;
		samplesToRead -= samplesJustRead * 2;
		bufferOut += samplesJustRead * 2;
	}

	return samplesRead;
}

/**
 * Checks if the input file is Opus.
 *
 * \param in	Input file.
 * \return		0 if Opus file, else not or failure.
 */
int isOpus(const char* in)
{
	int err = 0;
	OggOpusFile* opusTest = op_test_file(in, &err);

	op_free(opusTest);
	return err;
}

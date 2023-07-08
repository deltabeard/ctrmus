#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DR_WAV_IMPLEMENTATION
#include <dr_libs/dr_wav.h>

#include "wav.h"
#include "playback.h"

static drwav wav;
static const size_t buffSize = 16 * 1024;

static int initWav(const char* file);
static uint32_t rateWav(void);
static uint8_t channelWav(void);
static uint64_t readWav(void* buffer);
static void exitWav(void);

/**
 * Set decoder parameters for WAV.
 *
 * \param	decoder Structure to store parameters.
 */
void setWav(struct decoder_fn* decoder)
{
	decoder->init = &initWav;
	decoder->rate = &rateWav;
	decoder->channels = &channelWav;
	decoder->buffSize = buffSize;
	decoder->decode = &readWav;
	decoder->exit = &exitWav;
}

/**
 * Initialise WAV playback.
 *
 * \param	file	Location of WAV file to play.
 * \return			0 on success, else failure.
 */
int initWav(const char* file)
{
	return !drwav_init_file(&wav, file, NULL);
}

/**
 * Get sampling rate of Wav file.
 *
 * \return	Sampling rate.
 */
uint32_t rateWav(void)
{
	return wav.sampleRate;
}

/**
 * Get number of channels of Wav file.
 *
 * \return	Number of channels for opened file.
 */
uint8_t channelWav(void)
{
	return wav.channels;
}

/**
 * Read part of open Wav file.
 *
 * \param buffer	Output.
 * \return			Samples read for each channel.
 */
uint64_t readWav(void* buffer)
{
	size_t buffSizeFrames;
	uint64_t samplesRead;

	buffSizeFrames = buffSize / (size_t)wav.channels;
	samplesRead = drwav_read_pcm_frames_s16(&wav, buffSizeFrames, buffer);
	samplesRead *= (uint64_t)wav.channels;
	return samplesRead;
}

/**
 * Free Wav file.
 */
void exitWav(void)
{
	drwav_uninit(&wav);
}

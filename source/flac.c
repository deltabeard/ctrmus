#define DR_FLAC_IMPLEMENTATION
#include <dr_libs/dr_flac.h>

#include "flac.h"
#include "playback.h"

static drflac*		pFlac;
static const size_t	buffSize = 16 * 1024;

static int initFlac(const char* file);
static uint32_t rateFlac(void);
static uint8_t channelFlac(void);
static uint64_t decodeFlac(void* buffer);
static void exitFlac(void);

/**
 * Set decoder parameters for flac.
 *
 * \param	decoder Structure to store parameters.
 */
void setFlac(struct decoder_fn* decoder)
{
	decoder->init = &initFlac;
	decoder->rate = &rateFlac;
	decoder->channels = &channelFlac;
	decoder->buffSize = buffSize;
	decoder->decode = &decodeFlac;
	decoder->exit = &exitFlac;
}

/**
 * Initialise Flac decoder.
 *
 * \param	file	Location of flac file to play.
 * \return			0 on success, else failure.
 */
int initFlac(const char* file)
{
	pFlac = drflac_open_file(file, NULL);

	return pFlac == NULL ? -1 : 0;
}

/**
 * Get sampling rate of Flac file.
 *
 * \return	Sampling rate.
 */
uint32_t rateFlac(void)
{
	return pFlac->sampleRate;
}

/**
 * Get number of channels of Flac file.
 *
 * \return	Number of channels for opened file.
 */
uint8_t channelFlac(void)
{
	return pFlac->channels;
}

/**
 * Decode part of open Flac file.
 *
 * \param buffer	Decoded output.
 * \return			Samples read for each channel.
 */
uint64_t decodeFlac(void* buffer)
{
	size_t buffSizeFrames;
	uint64_t samplesRead;

	buffSizeFrames = buffSize / (size_t)pFlac->channels;
	samplesRead = drflac_read_pcm_frames_s16(pFlac, buffSizeFrames, buffer);
	samplesRead *= (uint64_t)pFlac->channels;
	return samplesRead;
}

/**
 * Free Flac decoder.
 */
void exitFlac(void)
{
	drflac_close(pFlac);
}

/**
 * Checks if the input file is Flac
 *
 * \param in	Input file.
 * \return		0 if Flac file, else not or failure.
 */
int isFlac(const char* in)
{
	int err = -1;
	drflac* pFlac = drflac_open_file(in, NULL);

	if(pFlac != NULL)
		err = 0;

	drflac_close(pFlac);
	return err;
}

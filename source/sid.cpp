#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sidplay/player.h>

#include "playback.h"

static uint32_t		frequency = 44100;
static int			channels = SIDEMU_STEREO;
static int			selectedSong = 0;
static size_t		buffSize = 0.5*frequency*channels; // 0.5 seconds

// don't change anything below - only 16 bit/sigend PCM is supported my ctrmus
static int			sampleFormat = SIDEMU_SIGNED_PCM;
static int			bitsPerSample = SIDEMU_16BIT;

emuEngine	*myEmuEngine = NULL;
sidTune		*myTune = NULL;

extern "C" void setSid(struct decoder_fn* decoder);
extern "C" int initSid(const char* file);
extern "C" uint32_t rateSid(void);
extern "C" uint8_t channelSid(void);
extern "C" uint64_t readSid(void* buffer);
extern "C" void exitSid(void);

/**
 * Set decoder parameters for SID.
 *
 * \param	decoder Structure to store parameters.
 */
extern "C" void setSid(struct decoder_fn* decoder)
{
	decoder->init = &initSid;
	decoder->rate = &rateSid;
	decoder->channels = &channelSid;
	decoder->buffSize = buffSize;
	decoder->decode = &readSid;
	decoder->exit = &exitSid;
}

/**
 * Initialise SID playback.
 *
 * \param	file	Location of SID file to play.
 * \return			0 on success, else failure.
 */
int initSid(const char* file)
{
	// init emuEngine
	myEmuEngine = new emuEngine;
	if ( !myEmuEngine ) return -1;

	//configure emuEngine
	struct emuConfig myEmuConfig;
	myEmuEngine->getConfig(myEmuConfig);
	myEmuConfig.frequency = frequency;
	myEmuConfig.channels = channels;
	myEmuConfig.bitsPerSample = bitsPerSample;
	myEmuConfig.sampleFormat = sampleFormat;
	myEmuEngine->setConfig(myEmuConfig);

	// load the SID file
	myTune=new sidTune ( file );
	if ( !myTune ) return -1;

	// init emuEngine with sidTune
	if ( !sidEmuInitializeSong(*myEmuEngine,*myTune,selectedSong) ) return -1;

	return 0;
}

/**
 * Get sampling rate of SID file.
 *
 * \return	Sampling rate.
 */
uint32_t rateSid(void)
{
	return (frequency);
}

/**
 * Get number of channels of SID file.
 *
 * \return	Number of channels for opened file.
 */
uint8_t channelSid(void)
{
	return channels;
}

/**
 * Read part of open SID file.
 *
 * \param buffer	Output.
 * \return			Samples read for each channel.
 */
uint64_t readSid(void* buffer)
{
	sidEmuFillBuffer( *myEmuEngine, *myTune, buffer, buffSize*bitsPerSample/8 );
	if (myTune->getStatus()) return buffSize;
	return 0;
}

/**
 * Free Sid file.
 */
void exitSid(void)
{
	if(myTune)
		delete(myTune);
	if (myEmuEngine) {delete(myEmuEngine);}
}

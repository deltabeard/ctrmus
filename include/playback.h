#include <stdbool.h>
#include <limits.h>

#ifndef ctrmus_playback_h
#define ctrmus_playback_h

/* Channel to play music on */
#define CHANNEL	0x08

struct decoder_fn
{
	/**
	 * Set decoder parameters.
	 * \param	decoder Structure to store parameters.
	 * \return	0 on success, else failure.
	 */
	int (* init)(const char* file);

	/**
	 * Get sampling rate of file.
	 * \return	Sampling rate.
	 */
	uint32_t (* rate)(void);

	/**
	 * Get number of channels of file.
	 * \return	Number of channels for opened file.
	 */
	uint8_t (* channels)(void);

	/**
	 * Size of output buffer used in decode().
	 */
	size_t buffSize;

	/**
	 * Fill buffer with decoded samples.
	 * \param buffer	Output buffer to fill.
	 * \return		Samples read for each channel.
	 */
	uint64_t (* decode)(void*);

	/**
	 * Free codec resources.
	 */
	void (* exit)(void);

	/**
	 * Optional. Set to NULL if unavailable.
	 * Get number of samples in audio file.
	 */
	size_t (* getFileSamples)(void);
};

struct playbackInfo_t
{
	char file[PATH_MAX];
	struct errInfo_t *errInfo;

	/* If 0, then the duration of file is unavailable. */
	size_t samples_total;
	size_t samples_played;
	size_t samples_per_second;
};

/**
 * Pause or play current file.
 *
 * \return	True if paused.
 */
bool togglePlayback(void);

/**
 * Stops current playback. Playback thread should exit as a result.
 */
void stopPlayback(void);

/**
 * Returns whether music is playing or paused.
 */
bool isPlaying(void);

/**
 * Should only be called from a new thread only, and have only one playback
 * thread at time. This function has not been written for more than one
 * playback thread in mind.
 *
 * \param	infoIn	Playback information.
 */
void playFile(void* infoIn);

#endif

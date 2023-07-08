#include <stdint.h>
#include "playback.h"

/**
 * Set decoder parameters for SID.
 *
 * \param	decoder Structure to store parameters.
 */
void setSid(struct decoder_fn* decoder);

/**
 * Initialise SID playback.
 *
 * \param	file	Location of SID file to play.
 * \return			0 on success, else failure.
 */
int initSid(const char* file);

/**
 * Get sampling rate of SID file.
 *
 * \return	Sampling rate.
 */
uint32_t rateSid(void);

/**
 * Get number of channels of SID file.
 *
 * \return	Number of channels for opened file.
 */
uint8_t channelSid(void);

/**
 * Read part of open SID file.
 *
 * \param buffer	Output.
 * \return			Samples read for each channel.
 */
uint64_t readSid(void* buffer);

/**
 * Free SID file.
 */
void exitSid(void);

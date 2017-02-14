/**
 * ctrmus - 3DS Music Player
 * Copyright (C) 2016 Mahyar Koshkouei
 *
 * This program comes with ABSOLUTELY NO WARRANTY and is free software. You are
 * welcome to redistribute it under certain conditions; for details see the
 * LICENSE file.
 */

#ifndef ctrmus_main_h
#define ctrmus_main_h

/* Default folder */
#define DEFAULT_DIR		"sdmc:/"

/* Maximum number of lines that can be displayed */
#define	MAX_LIST		27

struct watchdogInfo
{
	PrintConsole*		screen;
	struct errInfo_t*	errInfo;
};

/**
 * Allows the playback thread to return any error messages that it may
 * encounter.
 *
 * \param	infoIn	Struct containing addresses of the event, the error code,
 *					and an optional error string.
 */
void playbackWatchdog(void* infoIn);

/**
 * Get number of files in current working folder
 *
 * \return	Number of files in current working folder, -1 on failure with
 *			errno set.
 */
int getNumberFiles(void);

#endif

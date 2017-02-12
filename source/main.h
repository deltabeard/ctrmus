/**
 * ctrmus - 3DS Music Player
 * Copyright (C) 2016 Mahyar Koshkouei
 *
 * This program comes with ABSOLUTELY NO WARRANTY and is free software. You are
 * welcome to redistribute it under certain conditions; for details see the
 * LICENSE file.
 */

/* Default folder */
#define DEFAULT_DIR		"sdmc:/"

/* Maximum number of lines that can be displayed */
#define	MAX_LIST		27

struct watchdogInfo
{
	PrintConsole*		screen;
	struct errInfo_t*	errInfo;
};

void playbackWatchdog(void* infoIn);

static void showControls(void);

static int changeFile(const char* ep_file, volatile int* error, Handle* failEvent);

/**
 * Get number of files in current working folder
 *
 * \return	Number of files in current working folder, -1 on failure with
 *			errno set.
 */
int getNumberFiles(void);

/**
 * List current directory.
 *
 * \param	from	First entry in directory to list.
 * \param	max		Maximum number of entries to list. Must be > 0.
 * \param	select	File to show as selected. Must be > 0.
 * \return			Number of entries listed or negative on error.
 */
int listDir(int from, int max, int select);

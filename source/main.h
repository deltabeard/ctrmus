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

enum sorting_algorithms {
	SORT_NONE,
	SORT_NAME_AZ,
	SORT_DATE_NEW,
	SORT_SIZE_SMALL,
};

static int obtainFoldersSizes(int *nbDirs, int *nbFiles);
static int obtainFolders(char** dirs, char** files, enum sorting_algorithms sort);

/**
 * Obtain array of files and directories in current directory.
 *
 * \param	dirs	Unallocated pointer to store allocated directory names.
 *					This must be freed after use.
 * \param	files	Unallocated pointer to store allocated file names.
 *					This must be freed after use.
 * \param	sort	Sorting algorithm to use.
 * \return			Number of entries in total or negative on error.
 */
static int obtainDir(char** *dirs_, char** *files_, int *num_dirs_, int *num_files_, enum sorting_algorithms sort);

/**
 * Free memory used by an array of strings.
 * Call this with dirs and files as parameters to free memory allocated by
 * obtainDir().
 */
static void freeDir(char** strs);

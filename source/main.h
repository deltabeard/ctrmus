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

/**
 * Obtain array of files and directories in current directory.
 *
 * Example:
 *
 *	{
 *	char**	dirlist = NULL;
 *	char**	filelist = NULL;
 *	int	files = 0;
 *	int	dirs = 0;
 *	enum sorting_algorithms sort = SORT_NAME_AZ;
 *
 *	obtainDir(&dirlist, &dirs, &filelist, &files, sort);
 *
 *  // Main code
 *
 *	freeDir(&dirlist, dirs);
 *	freeDir(&filelist, files);
 *	}
 *
 *
 * \param	dirlist		Pointer to store allocated directory names.
 *						This must be freed after use.
 * \param	dirs		Pointer to store number of directories in dirlist.
 * \param	filelist	Pointer to store allocated file names.
 *						This must be freed after use.
 * \param	files		Pointer to store number of directories in filelist.
 * \param	sort		Sorting algorithm to use.
 * \return				Number of entries in total or negative on error.
 */
static int obtainDir(char*** dirlist, int* dirs, char*** filelist, int* files,
		enum sorting_algorithms sort);

/**
 * Free memory used by an array of strings.
 * Call this with dirlist and filelist as parameters to free memory allocated by
 * obtainDir().
 *
 * \param strs	List to free.
 * \param size	Size of list.
 */
static void freeList(char*** strs, int size);

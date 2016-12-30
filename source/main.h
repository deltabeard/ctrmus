/**
 * ctrmus - 3DS Music Player
 * Copyright (C) 2016 Mahyar Koshkouei
 *
 * This program comes with ABSOLUTELY NO WARRANTY and is free software. You are
 * welcome to redistribute it under certain conditions; for details see the
 * LICENSE file.
 */

/**
 * List directory.
 *
 * \param	dir		Path of directory.
 * \param	from	First entry in directory to list.
 * \param	max		Maximum number of entries to list. Must be > 0.
 * \return			Number of entries listed or negative on error.
 */
int listDir(const char *dir, int from, int max);

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			File type, else negative.
 */
int getFileType(const char *file);

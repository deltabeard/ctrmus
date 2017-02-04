/**
 * ctrmus - 3DS Music Player
 * Copyright (C) 2016 Mahyar Koshkouei
 *
 * This program comes with ABSOLUTELY NO WARRANTY and is free software. You are
 * welcome to redistribute it under certain conditions; for details see the
 * LICENSE file.
 */

#include <3ds.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sf2d.h>
#include <sftd.h>

#include "all.h"
#include "main.h"
#include "sync.h"
#include "playback.h"

#define STACKSIZE (16 * 1024)

bool debug = false;

sftd_font* font;
int fontSize = 15;

// UI to PLAYER
bool run = true;
int nowPlaying = 0;

// PLAYER to UI
volatile float progress = 0;

int nbFolderNames = 1;
int nbListNames = 0;
char** foldernames = NULL;
char** listnames = NULL;

void listClicked(int hilit) {
	/*
	nowPlaying = hilit;
	progress = 0;
	*/
}

void folderClicked(int hilit) {
	char** newListNames = (char**)malloc((nbListNames+1)*sizeof(char*));
	memcpy(newListNames, listnames, nbListNames*sizeof(char*));
	newListNames[nbListNames] = strdup(foldernames[hilit]);
	if (nbListNames != 0) free(listnames);
	nbListNames++;
	listnames = newListNames;
}

void updateList(
	touchPosition* touchPad, touchPosition* oldTouchPad, touchPosition* orgTouchPad, bool touchPressed, bool touchWasPressed,
	int* hilit, float y, float* vy, void (*clicked)(int hilit),
	int cellSize, float inertia
) {
	if (touchPressed) {
		if (!touchWasPressed) {
			*hilit = (y+touchPad->py)/cellSize;
		} else {
			if (fabs((float)(touchPad->py-orgTouchPad->py))>15.0f) {
				// keep scrolling even if we come near the "real" original pos
				orgTouchPad->py = 256;
				*vy = oldTouchPad->py - touchPad->py;
			}
		}
	} else {
		if (touchWasPressed) { // keys were just released
			if (orgTouchPad->py != 256) { // we clicked, we didn't scroll
				(*clicked)(*hilit);
			}
		}
	}
}

char* basename(char* s) {
	int i = strlen(s);
	while (i!=-1 && s[i] != '/') i--;
	return s+i+1;
}

void f_player(void* arg) {
	while(run) {
		playFile("sdmc:/Music/03 - Rosalina.mp3");
	}
}

int countCharStars(char** t) {
	int n = 0;
	while (t[n]!=NULL) n++;
	return n;
}

int main(int argc, char** argv)
{
	sftd_init();
	sf2d_init();
	romfsInit();
	sdmcInit();

	FILE* file = fopen("ctrmus_debug","rb");
	if (file != NULL) {
		debug = true;
		fclose(file);
	};

	chdir(DEFAULT_DIR);
	chdir("Music");

	sf2d_set_clear_color(RGBA8(255,106,0, 255));
	sf2d_set_vblank_wait(0);
	font = sftd_load_font_file("romfs:/FreeSerif.ttf");

	aptSetSleepAllowed(false);
	svcCreateEvent(&event2, 0);

	Thread t2 = NULL;
	if (!debug) t2 = threadCreate(f_player, NULL, STACKSIZE, 0x18, -2, true);

	// WORKING VERSION
	int nbDirs;
	int nbFiles;
	obtainFoldersSizes(&nbDirs, &nbFiles);
	char** dirs = (char**)malloc(nbDirs*sizeof(char*));
	char** files = (char**)malloc(nbFiles*sizeof(char*));
	obtainFolders(dirs, files, SORT_NAME_AZ);
	// todo: put files and folders together
	foldernames = files;
	nbFolderNames = nbFiles;

	touchPosition oldTouchPad;
	touchPosition orgTouchPad;
	oldTouchPad.px = 0;
	oldTouchPad.py = 0;
	orgTouchPad = oldTouchPad;

	float yFolder = -100; // will go through a minmax, don't worry
	float vyFolder = 0;

	float yList = -100; // will go through a minmax, don't worry
	float vyList = 0;

	float inertia = 10;

	int cellSize = fontSize*2;

	int hilitFolder = -1;
	int hilitList = -1;

	float paneBorder = 160.0f;
	float paneBorderGoal = 160.0f;

	// may be loaded from a config file or something
	u32 bgColor = RGBA8(0,0,0,255);
	u32 lineColor = RGBA8(255,106,0,255);
	u32 textColor = RGBA8(255,255,255,255);
	u32 hlTextColor = RGBA8(255,0,0,255);

	int scheduleCount = 0;
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_START) break;

		if (scheduleCount++%4==0) svcSignalEvent(event2);

		// scroll using touchpad
		touchPosition touchPad;
		hidTouchRead(&touchPad);
		bool touchPressed = touchPad.px+touchPad.py != 0;
		bool touchWasPressed = oldTouchPad.px+oldTouchPad.py != 0;
		if (!touchWasPressed) orgTouchPad = touchPad;
		if (orgTouchPad.px < paneBorder) {
			if (touchPressed) paneBorderGoal = 320-60;
			updateList(
				&touchPad, &oldTouchPad, &orgTouchPad, touchPressed, touchWasPressed,
				&hilitList, yList, &vyList, listClicked,
				cellSize, inertia
			);
		} else {
			if (touchPressed) paneBorderGoal = 60;
			updateList(
				&touchPad, &oldTouchPad, &orgTouchPad, touchPressed, touchWasPressed,
				&hilitFolder, yFolder, &vyFolder, folderClicked,
				cellSize, inertia
			);
		}

		yList += vyList;
		vyList = vyList*inertia/(inertia+1);
		yFolder += vyFolder;
		vyFolder = vyFolder*inertia/(inertia+1);

		paneBorder = (paneBorder*inertia+paneBorderGoal*1)/(inertia+1);

		oldTouchPad = touchPad;

		// prevent y from going below 0 and above a certain value here
		yFolder = fmax(-10,fmin(cellSize*nbFolderNames-240,yFolder));
		yList = fmax(-10,fmin(cellSize*nbListNames-240,yList));

		gspWaitForVBlank();

		sf2d_start_frame(GFX_TOP, GFX_LEFT);
		{
			/*
			sftd_draw_textf(font, 0, fontSize*0, RGBA8(0,0,0,255), fontSize, "hilit Folder: %i", hilitFolder);
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "hilit List: %i", hilitList);
			sftd_draw_textf(font, 0, fontSize*2, RGBA8(0,0,0,255), fontSize, "folder number: %i", nbDirs);
			sftd_draw_textf(font, 0, fontSize*3, RGBA8(0,0,0,255), fontSize, "file number: %i", nbFiles);
			*/
		}
		sf2d_end_frame();

		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
		{
			// list entries
			sf2d_draw_rectangle(1, 0, paneBorder, 240, bgColor);
			for (int i = (yList+10)/cellSize; i < (240+yList)/cellSize && i<nbListNames; i++) {
				sf2d_draw_rectangle(1, fmax(0,cellSize*i-yList), paneBorder, 1, lineColor);
				sftd_draw_textf(font, 0+10, cellSize*i-yList, i==hilitList?hlTextColor:textColor, fontSize, basename(listnames[i]));
			}

			// folder entries
			sf2d_draw_rectangle(paneBorder, 0, 320-1-(int)paneBorder, 240, bgColor);
			for (int i = (yFolder+10)/cellSize; i < (240+yFolder)/cellSize && i<nbFolderNames; i++) {
				sf2d_draw_rectangle(paneBorder, fmax(0,cellSize*i-yFolder), 320-1-(int)paneBorder, 1, lineColor);
				sftd_draw_textf(font, paneBorder+10, cellSize*i-yFolder, i==hilitFolder?hlTextColor:textColor, fontSize, basename(foldernames[i]));
			}

			// TODO don't try to draw the text at index i if it doesn't exist (if nb is so low that lists don't fill the screen)

			sf2d_draw_rectangle(paneBorder, 0, 1, 240, RGBA8(255,255,255,255));

			// progress bar
			sf2d_draw_rectangle(0, 0, 320, 10, RGBA8(255,106,0,255));
			sf2d_draw_rectangle(20, 0, progress*(320-20*2), 9, RGBA8(0,0,0,255));
		}
		sf2d_end_frame();

		sf2d_swapbuffers();
	}

	for (int i=0; i<nbFolderNames; i++) free(foldernames[i]);
	free(foldernames);
	for (int i=0; i<nbListNames; i++) free(listnames[i]);
	free(listnames);

	// TODO kill playback
	run = false;
	svcSignalEvent(event1); // exit
	svcSignalEvent(event2); // stop waiting
	if (t2 != NULL) threadJoin(t2, 1000000);

	sftd_free_font(font);

	sdmcExit();
	romfsExit();
	sftd_fini();
	sf2d_fini();
	return 0;
}

/**
 * Function used for qsort(), to sort string in alphabetical order (A-Z).
 *
 * \param	p1	First string.
 * \param	p2	Second string.
 * \return		strcasecmp return value.
 */
static int sortName(const void *p1, const void *p2)
{
	return strcasecmp(*(char* const*)p1, *(char* const*)p2);
}

static int obtainFoldersSizes(int *nbDirs, int *nbFiles) {
	DIR*			dp;
	struct dirent*	ep;
	int				ret = -1;
	char*			wd = getcwd(NULL, 0);
	int				num_dirs = 0;
	int				num_files = 0;

	if(wd == NULL) goto err;
	if((dp = opendir(wd)) == NULL) goto err;

	while((ep = readdir(dp)) != NULL) {
		if(ep->d_type == DT_DIR) {
			num_dirs++;
		} else {
			num_files++;
		}
	}
	if(closedir(dp) != 0)
	goto err;

	ret = 0;
	*nbDirs = num_dirs;
	*nbFiles = num_files;

err:
	free(wd);
	return ret;
}
static int obtainFolders(char** dirs, char** files, enum sorting_algorithms sort) {
	DIR*			dp;
	struct dirent*	ep;
	int				ret = -1;
	char*			wd = getcwd(NULL, 0);
	int				num_dirs = 0;
	int				num_files = 0;

	if(wd == NULL)
		goto err;
	if((dp = opendir(wd)) == NULL)
		goto err;

	while((ep = readdir(dp)) != NULL)
	{
		char* temp = strdup(ep->d_name);
		if (temp == NULL) goto err;
		if(ep->d_type == DT_DIR)
		{
			dirs[num_dirs] = temp;
			num_dirs++;
		}
		else
		{
			files[num_files] = temp;
			num_files++;
		}
	}

	if(closedir(dp) != 0)
		goto err;

	if(sort == SORT_NAME_AZ)
	{
		qsort(dirs, num_dirs, sizeof(char*), sortName);
		qsort(files, num_files, sizeof(char*), sortName);
	}
	ret = 0;

err:
	free(wd);
	return ret;
}

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
static int obtainDir(char** *dirs_, char** *files_, int *num_dirs_, int *num_files_, enum sorting_algorithms sort)
{
	DIR*			dp;
	struct dirent*	ep;
	int				ret = -1;
	char*			wd = getcwd(NULL, 0);
	int				num_dirs = 0;
	int				num_files = 0;

	if(wd == NULL)
		goto err;

	if((dp = opendir(wd)) == NULL)
		goto err;

	char** dirs = NULL;
	char** files = NULL;

	while((ep = readdir(dp)) != NULL)
	{
		char* temp = strdup(ep->d_name);
		if (temp == NULL) goto err;
		if(ep->d_type == DT_DIR)
		{
			dirs = realloc(dirs, num_dirs * sizeof(char*));
			*(dirs + num_dirs) = strdup(ep->d_name);
			num_dirs++;
		}
		else
		{
			files = realloc(files, num_files * sizeof(char*));
			*(files + num_files) = strdup(ep->d_name);
			num_files++;
		}
	}

/*
	if(sort == SORT_NAME_AZ)
	{
		qsort(&dirs, num_dirs, sizeof(char*), sortName);
		qsort(&files, num_files, sizeof(char*), sortName);
	}
*/

	// NULL terminate arrays
	dirs = realloc(dirs, (num_dirs * sizeof(char*)) + 1);
	*(dirs + num_dirs + 1) = NULL;

	files = realloc(files, (num_files * sizeof(char*)) + 1);
	*(files + num_files + 1) = NULL;

	if(closedir(dp) != 0)
		goto err;

	ret = 0;
	*num_dirs_ = num_dirs;
	*num_files_ = num_files;
	*dirs_ = dirs;
	*files_ = files;

err:
	free(wd);
	return ret;
}

/**
 * Free memory used by an array of strings.
 * Call this with dirs and files as parameters to free memory allocated by
 * obtainDir().
 */
static void freeDir(char** strs)
{
	while(*strs != NULL)
	{
		free(*strs);
		strs++;
	}

	free(strs);
	strs = NULL;

	return;
}

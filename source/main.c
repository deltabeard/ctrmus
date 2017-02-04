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
#include "playback.h"
#include "sync.h"

#define STACKSIZE (16 * 1024)

sftd_font* font;
int fontSize = 15;

// UI to PLAYER
volatile bool run = true;
volatile int nowPlaying = 0;

// PLAYER to UI
volatile float progress = 0;

int nbFolderNames = 70;
int nbListNames = 20;
char** foldernames = NULL;
char** listnames = NULL;

void listClicked(int hilit) {
	/*
	nowPlaying = hilit;
	progress = 0;
	*/
}

void folderClicked(int hilit) {
	char* folderentry = foldernames[hilit];
	char** newListNames = (char**)malloc((nbListNames+1)*sizeof(char*));
	memcpy(newListNames, listnames, nbListNames*sizeof(char*));
	newListNames[nbListNames] = (char*)malloc((strlen(folderentry)+1)*sizeof(char));
	memcpy(newListNames[nbListNames], folderentry, strlen(folderentry)+1);
	nbListNames++;
	free(listnames);
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
	while(true) {
		playFile("sdmc:/Music/03 - Rosalina.mp3");
		playFile("sdmc:/Music/17 - F-Zero.mp3");
	}
}

int main(int argc, char** argv)
{
	sftd_init();
	sf2d_init();
	romfsInit();
	sdmcInit();

	sf2d_set_clear_color(RGBA8(255,106,0, 255));
	sf2d_set_vblank_wait(0);
	font = sftd_load_font_file("romfs:/FreeSerif.ttf");

	aptSetSleepAllowed(false);
	svcCreateEvent(&event2, 0);

	// UNCOMMENT TO GET SOUND IN BACKGROUND
	//Thread t2 = threadCreate(f_player, NULL, STACKSIZE, 0x18, -2, true);

	const char* baseFolderName = "filepath/foldername00";
	foldernames = (char**)malloc(nbFolderNames*sizeof(char*));
	for (int i=0; i<nbFolderNames; i++) {
		foldernames[i] = (char*)malloc((strlen(baseFolderName)+1)*sizeof(char));
		memcpy(foldernames[i], baseFolderName, strlen(baseFolderName));
		foldernames[i][strlen(baseFolderName)-1] = '0' + (i%10);
		foldernames[i][strlen(baseFolderName)-2] = '0' + (i/10%10);
		foldernames[i][strlen(baseFolderName)-0] = 0;
	}
	const char* baseListName = "filepath/listname00";
	listnames = (char**)malloc(nbListNames*sizeof(char*));
	for (int i=0; i<nbListNames; i++) {
		listnames[i] = (char*)malloc((strlen(baseListName)+1)*sizeof(char));
		memcpy(listnames[i], baseListName, strlen(baseListName));
		listnames[i][strlen(baseListName)-1] = '0' + (i%10);
		listnames[i][strlen(baseListName)-2] = '0' + (i/10%10);
		listnames[i][strlen(baseListName)-0] = 0;
	}

	touchPosition oldTouchPad;
	touchPosition orgTouchPad;
	oldTouchPad.px = 0;
	oldTouchPad.py = 0;
	orgTouchPad = oldTouchPad;

	float yFolder = -100; // will go through a minmax, don't worry'
	float vyFolder = 0;

	float yList = 0;
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
	while (run && aptMainLoop()) {
		if (scheduleCount++%4==0) svcSignalEvent(event2);

		hidScanInput();
		if (hidKeysDown() & KEY_START) run = false;

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
			// there was a segfault somewhere here at some point
			sftd_draw_textf(font, 0, fontSize*0, RGBA8(0,0,0,255), fontSize, "Now playing: %s", basename(listnames[nowPlaying]));
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "Full path: %s", listnames[nowPlaying]);
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "Full path: %s", listnames[nowPlaying]);
			*/
			sftd_draw_textf(font, 0, fontSize*0, RGBA8(0,0,0,255), fontSize, "hilit Folder: %i", hilitFolder);
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "hilit List: %i", hilitList);

			char* folderentry = foldernames[(int)fmax(0,hilitFolder)];
			char* test = malloc((strlen(folderentry)+1)*sizeof(char));
			memcpy(test, folderentry, strlen(folderentry)+1);
			sftd_draw_textf(font, 0, fontSize*2, RGBA8(0,0,0,255), fontSize, "hilit folder: %s", basename(test));
			free(test);
		}
		sf2d_end_frame();

		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
		{
			// list entries
			sf2d_draw_rectangle(1, 0, paneBorder, 240, bgColor);
			for (int i = (yList+10)/cellSize; i < (240+yList)/cellSize; i++) {
				sf2d_draw_rectangle(1, fmax(0,cellSize*i-yList), paneBorder, 1, lineColor);
				sftd_draw_textf(font, 0+10, cellSize*i-yList, i==hilitList?hlTextColor:textColor, fontSize, basename(listnames[i]));
				//sftd_draw_textf(font, 0+10, cellSize*i-yList, i==hilitList?hlTextColor:textColor, fontSize, "list%i", i);
			}

			// folder entries
			sf2d_draw_rectangle(paneBorder, 0, 320-1-(int)paneBorder, 240, bgColor);
			for (int i = (yFolder+10)/cellSize; i < (240+yFolder)/cellSize; i++) {
				sf2d_draw_rectangle(paneBorder, fmax(0,cellSize*i-yFolder), 320-1-(int)paneBorder, 1, lineColor);
				sftd_draw_textf(font, paneBorder+10, cellSize*i-yFolder, i==hilitFolder?hlTextColor:textColor, fontSize, basename(foldernames[i]));
				//sftd_draw_textf(font, paneBorder+10, cellSize*i-yFolder, i==hilitFolder?hlTextColor:textColor, fontSize, "folder%i", i);
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
static int obtainDir(char** dirs, char** files, enum sorting_algorithms sort)
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

	dirs = NULL;
	files = NULL;
	
	while((ep = readdir(dp)) != NULL)
	{
		if(ep->d_type == DT_DIR)
		{
			num_dirs++;
			dirs = realloc(dirs, num_dirs * sizeof(char*));

			if((*(dirs + num_dirs) = strdup(ep->d_name)) == NULL)
				goto err;
		}
		else
		{
			num_files++;
			files = realloc(files, num_files * sizeof(char*));

			if((*(files + num_files) = strdup(ep->d_name)) == NULL)
				goto err;
		}
	}

	if(sort == SORT_NAME_AZ)
	{
		qsort(&dirs, num_dirs, sizeof(char*), sortName);
		qsort(&files, num_files, sizeof(char*), sortName);
	}

	/* NULL terminate arrays */
	dirs = realloc(dirs, (num_dirs * sizeof(char*)) + 1);
	*(dirs + num_dirs + 1) = NULL;

	files = realloc(files, (num_files * sizeof(char*)) + 1);
	*(files + num_files + 1) = NULL;

	if(closedir(dp) != 0)
		goto err;

	ret = 0;

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

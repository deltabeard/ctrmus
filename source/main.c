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

void listClicked(int hilit) {
	/*
	nowPlaying = hilit;
	progress = 0;
	*/
}

void folderClicked(int hilit) {
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
				(*clicked)(hilit);
			}
		}
	}
}

char* basename(char* s) {
	int i = strlen(s);
	while (i!=-1 && s[i] != '/') i--;
	return s+i+1;
}

volatile int int1 = 0;
volatile int int2 = 0;

char** foldernames = NULL;
char** listnames = NULL;

void f_UI(void* arg) {
	const char* baseFolderName = "filepath/foldername00";
	int nbFolderNames = 70;
	/*
	foldernames = (char**)malloc(nbFolderNames*sizeof(char*));
	for (int i=0; i<nbFolderNames; i++) {
		foldernames[i] = (char*)malloc((strlen(baseFolderName)+1)*sizeof(char));
		memcpy(foldernames[i], baseFolderName, strlen(baseFolderName));
		foldernames[i][strlen(baseFolderName)-1] = '0' + (i%10);
		foldernames[i][strlen(baseFolderName)-2] = '0' + (i/10%10);
		foldernames[i][strlen(baseFolderName)-0] = 0;
	}
	*/
	const char* baseListName = "filepath/listname00";
	int nbListNames = 30;
	/*
	listnames = (char**)malloc(nbListNames*sizeof(char*));
	for (int i=0; i<nbListNames; i++) {
		listnames[i] = (char*)malloc((strlen(baseListName)+1)*sizeof(char));
		memcpy(listnames[i], baseListName, strlen(baseListName));
		listnames[i][strlen(baseListName)-1] = '0' + (i%10);
		listnames[i][strlen(baseListName)-2] = '0' + (i/10%10);
		listnames[i][strlen(baseListName)-0] = 0;
	}
	*/
	// TODO has to be freed somewhere

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

	while (run) {
		svcWaitSynchronization(event1, U64_MAX);
		svcClearEvent(event1);

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
			sftd_draw_textf(font, 0, 0, RGBA8(0,0,0,255), fontSize, "int2%i", int2);
			/*
			// there is a segfault somewhere here
			sftd_draw_textf(font, 0, fontSize*0, RGBA8(0,0,0,255), fontSize, "Now playing: %s", basename(listnames[nowPlaying]));
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "Full path: %s", listnames[nowPlaying]);
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "Full path: %s", listnames[nowPlaying]);
			*/
		}
		sf2d_end_frame();

		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
		{
			// list entries
			sf2d_draw_rectangle(1, 0, paneBorder, 240, bgColor);
			for (int i = (yList+10)/cellSize; i < (240+yList)/cellSize; i++) {
				sf2d_draw_rectangle(1, fmax(0,cellSize*i-yList), paneBorder, 1, lineColor);
				//sftd_draw_textf(font, 0+10, cellSize*i-yList, i==hilitList?hlTextColor:textColor, fontSize, basename(listnames[i]));
				sftd_draw_textf(font, 0+10, cellSize*i-yList, i==hilitList?hlTextColor:textColor, fontSize, "list%i", i);
			}

			// folder entries
			sf2d_draw_rectangle(paneBorder, 0, 320-1-(int)paneBorder, 240, bgColor);
			for (int i = (yFolder+10)/cellSize; i < (240+yFolder)/cellSize; i++) {
				sf2d_draw_rectangle(paneBorder, fmax(0,cellSize*i-yFolder), 320-1-(int)paneBorder, 1, lineColor);
				//sftd_draw_textf(font, paneBorder+10, cellSize*i-yFolder, i==hilitFolder?hlTextColor:textColor, fontSize, basename(foldernames[i]));
				sftd_draw_textf(font, paneBorder+10, cellSize*i-yFolder, i==hilitFolder?hlTextColor:textColor, fontSize, "folder%i", i);
			}

			sf2d_draw_rectangle(paneBorder, 0, 1, 240, RGBA8(255,255,255,255));

			// progress bar
			sf2d_draw_rectangle(0, 0, 320, 10, RGBA8(255,106,0,255));
			sf2d_draw_rectangle(20, 0, progress*(320-20*2), 9, RGBA8(0,0,0,255));
		}
		sf2d_end_frame();

		sf2d_swapbuffers();
	}
}

void f_player(void* arg) {
	while(true) {
		playFile("sdmc:/Music/03 - Rosalina.mp3");
		playFile("sdmc:/Music/17 - F-Zero.mp3");
	}
}

void threadFunction1(void* arg) {
	while (run) {
		svcWaitSynchronization(event1, U64_MAX);
		svcClearEvent(event1);
		int1++;
	}
}
void threadFunction2(void* arg) {
	while (run) {
		svcWaitSynchronization(event2, U64_MAX);
		svcClearEvent(event2);
		int2++;
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

	//f_UI(NULL);

	//f_player(NULL);

	//Thread t = threadCreate(f_UI, NULL, STACKSIZE, 0x18, -2, true);
	//f_player(NULL);

	//Thread t = threadCreate(f_player, NULL, STACKSIZE, 0x18, -2, true);
	//f_UI(NULL);

	/*
	while (run) {
		svcSleepThread(100000000);
	}
	*/

	svcCreateEvent(&event1, 0);
	svcCreateEvent(&event2, 0);

	Thread t1 = threadCreate(threadFunction1, &int1, STACKSIZE, 0x18, -2, true);
	//Thread t1 = threadCreate(f_UI, &int1, STACKSIZE, 0x18, -2, true);
	//Thread t2 = threadCreate(threadFunction2, &int2, STACKSIZE, 0x18, -2, true);
	Thread t2 = threadCreate(f_player, NULL, STACKSIZE, 0x18, -2, true);

	int scheduler_count = 0;
	while (run) {
		scheduler_count++;
		if (scheduler_count % 16 != 0) {
			svcSignalEvent(event1);
		} else {
			svcSignalEvent(event2);
		}
		gspWaitForVBlank();

		sf2d_start_frame(GFX_TOP, GFX_LEFT);
		{
			sftd_draw_textf(font, 0, fontSize*0, RGBA8(0,0,0,255), fontSize, "int1: %i", int1);
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "int2: %i", int2);
		}
		sf2d_end_frame();

		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
		{
		}
		sf2d_end_frame();

		sf2d_swapbuffers();
	}

	sftd_free_font(font);

	sdmcExit();
	romfsExit();
	sftd_fini();
	sf2d_fini();
	return 0;
}

/**
 * List current directory.
 *
 * \param	from	First entry in directory to list.
 * \param	max		Maximum number of entries to list. Must be > 0.
 * \param	select	File to show as selected. Must be > 0.
 * \return			Number of entries listed or negative on error.
 */
int listDir(int from, int max, int select)
{
	DIR				*dp;
	struct dirent	*ep;
	int				fileNum = 0;
	int				listed = 0;
	char*			wd = getcwd(NULL, 0);

	if(wd == NULL)
		goto err;

	consoleClear();
	printf("Dir: %.33s\n", wd);

	if((dp = opendir(wd)) == NULL)
		goto err;

	if(from == 0)
	{
		printf("%c../\n", select == 0 ? '>' : ' ');
		listed++;
	}

	while((ep = readdir(dp)) != NULL)
	{
		fileNum++;

		if(fileNum <= from)
			continue;

		listed++;

		printf("%c%s%.37s%s\n",
				select == fileNum ? '>' : ' ',
				ep->d_type == DT_DIR ? "\x1b[34;1m" : "",
				ep->d_name,
				ep->d_type == DT_DIR ? "/\x1b[0m" : "");

		if(fileNum == max + from)
			break;
	}

	if(closedir(dp) != 0)
		goto err;

out:
	free(wd);
	return listed;

err:
	listed = -1;
	goto out;
}

/**
 * Get number of files in current working folder
 *
 * \return	Number of files in current working folder, -1 on failure with
 *			errno set.
 */
int getNumberFiles(void)
{
	DIR				*dp;
	struct dirent	*ep;
	char*			wd = getcwd(NULL, 0);
	int				ret = 0;

	if(wd == NULL)
		goto err;

	if((dp = opendir(wd)) == NULL)
		goto err;

	while((ep = readdir(dp)) != NULL)
		ret++;

	closedir(dp);

out:
	return ret;

err:
	ret = -1;
	goto out;
}

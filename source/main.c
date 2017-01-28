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

#define STACKSIZE (16 * 1024)

// UI to PLAYER
volatile bool run = true;
volatile int nowPlaying = 0;

// PLAYER to UI
volatile float progress = 0;

void listClicked(int hilit) {
	nowPlaying = hilit;
	progress = 0;
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

void f_UI(void* arg) {
	const char* baseFolderName = "filepath/foldername00";
	int nbFolderNames = 70;
	char foldernames[nbFolderNames][strlen(baseFolderName)+1];
	for (int i=0; i<nbFolderNames; i++) {
		memcpy(foldernames[i], baseFolderName, strlen(baseFolderName));
		foldernames[i][strlen(baseFolderName)-1] = '0' + (i%10);
		foldernames[i][strlen(baseFolderName)-2] = '0' + (i/10%10);
		foldernames[i][strlen(baseFolderName)-0] = 0;
	}
	const char* baseListName = "filepath/listname00";
	int nbListNames = 30;
	char listnames[nbListNames][strlen(baseListName)+1];
	for (int i=0; i<nbListNames; i++) {
		memcpy(listnames[i], baseListName, strlen(baseListName));
		listnames[i][strlen(baseListName)-1] = '0' + (i%10);
		listnames[i][strlen(baseListName)-2] = '0' + (i/10%10);
		listnames[i][strlen(baseListName)-0] = 0;
	}

	sf2d_set_clear_color(RGBA8(255,106,0, 255));
	sf2d_set_vblank_wait(0);

	// Font loading
	//sftd_font* font = sftd_load_font_mem(FreeSerif_ttf, FreeSerif_ttf_size);
	sftd_font* font = sftd_load_font_file("romfs:/FreeSerif.ttf");

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

	int fontSize = 15;
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
		svcSleepThread(1);

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

		//gspWaitForVBlank();

		sf2d_start_frame(GFX_TOP, GFX_LEFT);
		{
			sftd_draw_textf(font, 0, fontSize*0, RGBA8(0,0,0,255), fontSize, "Now playing: %s", basename(listnames[nowPlaying]));
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "Full path: %s", listnames[nowPlaying]);
			sftd_draw_textf(font, 0, fontSize*1, RGBA8(0,0,0,255), fontSize, "Full path: %s", listnames[nowPlaying]);
		}
		sf2d_end_frame();

		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
		{
			// list entries
			sf2d_draw_rectangle(1, 0, paneBorder, 240, bgColor);
			for (int i = (yList+10)/cellSize; i < (240+yList)/cellSize; i++) {
				sf2d_draw_rectangle(1, fmax(0,cellSize*i-yList), paneBorder, 1, lineColor);
				sftd_draw_textf(font, 0+10, cellSize*i-yList, i==hilitList?hlTextColor:textColor, fontSize, basename(listnames[i]));
			}

			// folder entries
			sf2d_draw_rectangle(paneBorder, 0, 320-1-(int)paneBorder, 240, bgColor);
			for (int i = (yFolder+10)/cellSize; i < (240+yFolder)/cellSize; i++) {
				sf2d_draw_rectangle(paneBorder, fmax(0,cellSize*i-yFolder), 320-1-(int)paneBorder, 1, lineColor);
				sftd_draw_textf(font, paneBorder+10, cellSize*i-yFolder, i==hilitFolder?hlTextColor:textColor, fontSize, basename(foldernames[i]));
			}

			sf2d_draw_rectangle(paneBorder, 0, 1, 240, RGBA8(255,255,255,255));

			// progress bar
			sf2d_draw_rectangle(0, 0, 320, 10, RGBA8(255,106,0,255));
			sf2d_draw_rectangle(20, 0, progress*(320-20*2), 9, RGBA8(0,0,0,255));
		}
		sf2d_end_frame();

		sf2d_swapbuffers();
	}
	sftd_free_font(font);
}

void f_player(void* arg) {
	while(run) {
		playFile("sdmc:/Music/17 - F-Zero.mp3");
	}
}

int main(int argc, char **argv)
{
	sftd_init();
	sf2d_init();
	romfsInit();
	aptSetSleepAllowed(false);

	f_UI(NULL);
	//f_player(NULL);

	/*
	Thread t = threadCreate(f_UI, NULL, STACKSIZE, 0x18, -2, true);
	f_player(NULL);
	*/

	/*
	Thread t = threadCreate(f_player, NULL, STACKSIZE, 0x18, -2, true);
	f_UI(NULL);
	*/

	romfsExit();
	sftd_fini();
	sf2d_fini();
	return 0;
}

int old_main(int argc, char **argv)
{
	PrintConsole	topScreen;
	PrintConsole	bottomScreen;
	int				fileMax;
	int				fileNum = 0;
	int				from = 0;

	gfxInitDefault();
	sdmcInit();
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);

	chdir(DEFAULT_DIR);
	chdir("MUSIC");
	if(listDir(from, MAX_LIST, 0) < 0)
	{
		err_print("Unable to list directory.");
		goto err;
	}

	fileMax = getNumberFiles();

	consoleSelect(&topScreen);
	puts("Log");
	consoleSelect(&bottomScreen);

	/**
	 * This allows for music to continue playing through the headphones whilst
	 * the 3DS is closed.
	 */
	aptSetSleepAllowed(false);

	while(aptMainLoop())
	{
		u32 kDown;
		u32 kHeld;
		static u64	mill = 0;

		hidScanInput();

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		kDown = hidKeysDown();
		kHeld = hidKeysHeld();

		if(kDown & KEY_START)
			break;

#ifdef DEBUG
		consoleSelect(&topScreen);
		printf("\rNum: %d, Max: %d, from: %d   ", fileNum, fileMax, from);
		consoleSelect(&bottomScreen);
#endif
		if(kDown)
			mill = osGetTime();

		if((kDown & KEY_UP || ((kHeld & KEY_UP) &&
						osGetTime() - mill > 500)) &&
				fileNum > 0 && fileNum > 0)
		{
			fileNum--;

			/* 26 is the maximum number of entries that can be printed */
			if(fileMax - fileNum > 26 && from != 0)
				from--;

			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");
		}

		if((kDown & KEY_DOWN || ((kHeld & KEY_DOWN) &&
						osGetTime() - mill > 500)) &&
				fileNum < fileMax && fileNum < fileMax)
		{
			fileNum++;

			if(fileNum >= MAX_LIST && fileMax - fileNum >= 0 &&
					from < fileMax - MAX_LIST)
				from++;

			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");
		}

		/*
		 * Pressing B goes up a folder, as well as pressing A or R when ".."
		 * is selected.
		 */
		if((kDown & KEY_B) ||
				((kDown & (KEY_A | KEY_R)) && (from == 0 && fileNum == 0)))
		{
			if(chdir("..") != 0)
				err_print("chdir");

			fileNum = 0;
			from = 0;
			fileMax = getNumberFiles();

			if(listDir(from, MAX_LIST, fileNum) < 0)
				err_print("Unable to list directory.");

			continue;
		}

		if(kDown & (KEY_A | KEY_R))
		{
			int				audioFileNum = 0;
			DIR				*dp;
			struct dirent	*ep;
			char*			wd = getcwd(NULL, 0);

			if(wd == NULL)
			{
				err_print("wd");
				goto err;
			}

			dp = opendir(wd);

			if(dp != NULL)
			{
				char* file = NULL;

				while((ep = readdir(dp)) != NULL)
				{
					if(audioFileNum == fileNum - 1)
						break;

					audioFileNum++;
				}

				if(ep->d_type == DT_DIR)
				{
					/* file not allocated yet, so no need to clear it */
					if(chdir(ep->d_name) != 0)
						err_print("chdir");

					fileNum = 0;
					from = 0;
					fileMax = getNumberFiles();
					if(listDir(from, MAX_LIST, fileNum) < 0)
						err_print("Unable to list directory.");

					closedir(dp);
					free(wd);
					continue;
				}

				if(asprintf(&file, "%s%s", wd, ep->d_name) == -1)
				{
					err_print("Constructing file name failed.");
					file = NULL;
				}
				else
				{
					consoleSelect(&topScreen);
					playFile(file);
					consoleSelect(&bottomScreen);
				}

				free(file);
				free(wd);

				if(closedir(dp) != 0)
					err_print("Closing directory failed.");
			}
			else
				err_print("Unable to open directory.");
		}
	}
#ifdef DEBUG
		consoleSelect(&topScreen);
		printf("\rNum: %d, Max: %d, from: %d   ", fileNum, fileMax, from);
		consoleSelect(&bottomScreen);
#endif

out:
	puts("Exiting...");

	gfxExit();
	sdmcExit();
	return 0;

err:
	puts("A fatal error occurred. Press START to exit.");

	while(true)
	{
		u32 kDown;

		hidScanInput();
		kDown = hidKeysDown();

		if(kDown & KEY_START)
			break;
	}

	goto out;
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

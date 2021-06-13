/**
 * Main.c: Initialisation and user interface handling.
 * Copyright (c) 2021 Mahyar Koshkouei
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 */

#ifndef __3DS__
# define LV_USE_LOG 1
#endif

#ifdef _WIN32
# include <direct.h>
# include <dirent_port.h>
# define chdir _chdir
# define getcwd _getcwd
# define strcasecmp _stricmp
#else
# include <dirent.h>
# include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <noto_sans_14_common.h>
#include <lvgl.h>
#include <platform.h>
#include <playback.h>

#define BUF_PX_SIZE (GSP_SCREEN_WIDTH_TOP * 64)

struct ui_ctx {
	/* Opaque pointer for handling platform functions. */
	void *platform_ctx;

	/* Display objects used to select target screen for new objects. */
	lv_disp_t *lv_disp_top, *lv_disp_bot;

	/* LVGL function mutex. */
	platform_mutex_s *lv_mutex;

	/* Playback context. */
	playback_ctx_s *pbk;

	/* File list used in file browser. */
	lv_obj_t *filelist;
	/* Spinner showing load progress of file browser. */
	lv_obj_t *fileloadspinner;
	/* Toggle for adding file to playlist or playing selected file. */
	lv_obj_t *fileaddbtn;

	/* File list population synchronisation. */
	platform_atomic_s filelist_populating;
#define FP_POP_NO  0
#define FP_POP_YES 1
	platform_atomic_s filelist_finished;
#define FP_POPFIN_NO  0
#define FP_POPFIN_YES 1

	/* Playlist used in playlist tab. */
	lv_obj_t *playlist;
};

/* Context for file picker poulation thread. */
struct fp_comp {
	/* User interface context. */
	struct ui_ctx *u;

	/* List of files and folders to populate. */
	struct dirent **namelist;

	/* Width of buttons within the file picker to use. */
	lv_coord_t w;

	/* Number of entries in the namelist. */
	int entries;

	/* Entries in the namelist that have already been populated on to the
	 * file picker list. */
	int entries_added;
};

static bool quit = false;
static const char *compat_fileext[] = {
	"wav", "flac", "mp3", "mp2", "ogg", "opus"
};

static void show_error_msg(const char *msg, lv_disp_t *disp);
static void recreate_filepicker(void *p);

static void btnev_quit(lv_obj_t *btn, lv_event_t event)
{
	(void)btn;

	if (event != LV_EVENT_CLICKED)
		return;

	quit = true;
	return;
}

static void mboxen_del(lv_obj_t *mbox, lv_event_t event)
{
	lv_obj_t *bg;

	if (event != LV_EVENT_CLICKED)
		return;

	if (lv_msgbox_get_active_btn(mbox) == LV_BTNMATRIX_BTN_NONE)
		return;

	bg = lv_obj_get_parent(mbox);
	lv_obj_del_async(bg);
}

/**
 * Displays an error message box.
 * The application must continue to function correctly.
 */
static void show_error_msg(const char *msg, lv_disp_t *disp)
{
	static const char *btns[] = {"OK", ""};
	lv_obj_t *scr, *bg, *mbox1;
	lv_coord_t ver, hor;

	lv_disp_set_default(disp);
	scr = lv_scr_act();

	ver = lv_disp_get_ver_res(disp);
	hor = lv_disp_get_hor_res(disp);

	bg = lv_obj_create(scr, NULL);
	lv_obj_set_size(bg, hor, ver);
	lv_obj_set_style_local_border_width(bg, LV_OBJ_PART_MAIN,
					    LV_STATE_DEFAULT, 0);
	lv_obj_set_style_local_radius(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT,
				      0);
	lv_obj_set_style_local_bg_color(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT,
					LV_COLOR_BLACK);
	lv_obj_set_style_local_bg_opa(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT,
				      80);

	mbox1 = lv_msgbox_create(bg, NULL);
	lv_msgbox_set_text(mbox1, msg);
	lv_msgbox_add_btns(mbox1, btns);
	lv_obj_set_event_cb(mbox1, mboxen_del);
	lv_obj_set_width(mbox1, hor - 32);
	lv_obj_set_height(mbox1, ver - 32);
	lv_obj_align_mid(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_state(mbox1, LV_STATE_DISABLED);
}

static void btnev_nop(lv_obj_t *btn, lv_event_t event)
{
	return;
}

static void btnev_file(lv_obj_t *btn, lv_event_t event)
{
	struct ui_ctx *ui;
	lv_obj_t *label;
	char *filename;

	/* Only perform an action with the file is the button was clicked. */
	if (event != LV_EVENT_CLICKED)
		return;

	ui = lv_obj_get_user_data(btn);
	label = lv_obj_get_child(btn, NULL);
	filename = lv_label_get_text(label);

	if(lv_btn_get_state(ui->fileaddbtn) == LV_BTN_STATE_CHECKED_RELEASED)
	{
		/* Add file to playlist. */
		playlist_entry_s *entry;
		lv_obj_t *file_btn, *file_lbl;
		lv_obj_t *scrl;
		lv_coord_t w;

		scrl = lv_page_get_scrollable(ui->playlist);
		w = lv_obj_get_width(scrl);

		/* TODO: Add ui context to entry. */
		entry = playback_add_file(ui->pbk, filename);
		file_btn = lv_btn_create(ui->playlist, NULL);
		lv_page_glue_obj(file_btn, true);
		lv_btn_set_layout(file_btn, LV_LAYOUT_ROW_MID);
		lv_obj_set_width(file_btn, w);

		file_lbl = lv_label_create(file_btn, NULL);
		lv_label_set_text(file_lbl, filename);
		lv_label_set_long_mode(label, LV_LABEL_LONG_CROP);
		lv_obj_set_click(file_lbl, false);

		lv_obj_set_user_data(file_btn, entry);
		lv_theme_apply(file_btn, LV_THEME_LIST_BTN);

		return;
	}

	/* Play file (fileaddbtn is unchecked). */

	return;
}

static void btnev_chdir(lv_obj_t *btn, lv_event_t event)
{
	lv_obj_t *label;
	const char *dir;
	struct ui_ctx *ui;

	label = lv_obj_get_child(btn, NULL);
	dir = lv_label_get_text(label);
	ui = lv_obj_get_user_data(btn);

	if (event != LV_EVENT_CLICKED)
		return;

	if (chdir(dir) != 0)
	{
		char err_txt[256] = "";

		snprintf(err_txt, sizeof(err_txt),
			 "Changing directory to '%s' failed:\n"
			 "%s",
			 dir, strerror(errno));
		show_error_msg(err_txt, ui->lv_disp_bot);
		return;
	}

	lv_async_call(recreate_filepicker, ui);

	return;
}

static void btnev_updir(lv_obj_t *btn, lv_event_t event)
{
	struct ui_ctx *ui;

#ifdef _MSC_VER
	char buf[] = "C:\\";
#else
	char buf[32];
#endif

	ui = lv_obj_get_user_data(btn);

	if (event != LV_EVENT_CLICKED)
		return;

	/* Check if we are in the filesystem root directory. */
	if(getcwd(buf, sizeof(buf)) != NULL &&
#ifdef _MSC_VER
			buf[2] == '\\'
#else
			strcmp(buf, "/") == 0
#endif
		)
	{
		/* If already in the root directory, don't go up a directory. */
		return;
	}

	if (chdir("..") != 0)
	{
		char err_txt[256] = "";
		snprintf(err_txt, sizeof(err_txt),
			 "Unable to go up a directory:\n"
			 "%s",
			 strerror(errno));
		show_error_msg(err_txt, ui->lv_disp_bot);
		return;
	}

	lv_async_call(recreate_filepicker, ui);

	return;
}

static const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(dot == NULL)
	    return "";

    return dot + 1;
}

/**
 * Populates the file picker list using the given dirent namelist.
 *
 * \param p File picker thread context.
 */
static int filepicker_add_entries(void *p)
{
	struct fp_comp *c = p;
	struct ui_ctx *ui = c->u;
	lv_obj_t *list_btn = NULL, *img = NULL, *label = NULL;
	const uint32_t tlim = 10;
	uint32_t ticks_ms = platform_get_ticks();

	/* Whilst we're still in the same folder, keep populating the file

	 * * picker until all the items in the folder are rendered. */
	while (c->entries_added < c->entries &&
	       platform_atomic_get(&ui->filelist_populating) == FP_POP_YES)
	{
		lv_event_cb_t event_cb = NULL;
		lv_anim_value_t deg;
		const char *symbol = LV_SYMBOL_FILE;
		mutex_stat_e mtx_stat;

		mtx_stat = platform_try_lock_mutex(ui->lv_mutex);
		if (mtx_stat == LOCK_MUTEX_TIMEOUT)
		{
			/* If the lvgl mutex is locked, wait and try again. */
			platform_usleep(1);
			continue;
		}
		else if (mtx_stat == LOCK_MUTEX_ERROR)
		{
			/* This shouldn't happen, but if it does, exit this
			 * thread. */
			goto out;
		}

		/* Select the most appropriate button image for the current
		 * item being added to the file picker. */
		if (c->namelist[c->entries_added]->d_type == DT_DIR)
		{
			event_cb = btnev_chdir;
			symbol = LV_SYMBOL_DIRECTORY;
		}
		else if (c->namelist[c->entries_added]->d_type == DT_LNK)
		{
			event_cb = btnev_chdir;
			symbol = LV_SYMBOL_DIRECTORY;
		}
		else
		{
			const unsigned exts_n =
			    sizeof(compat_fileext) / sizeof(*compat_fileext);
			const char *ext = get_filename_ext(
			    c->namelist[c->entries_added]->d_name);

			for (unsigned ext_n = 0; ext_n < exts_n; ext_n++)
			{
				if (strcmp(ext, compat_fileext[ext_n]) != 0)
					continue;

				symbol = LV_SYMBOL_AUDIO;
				event_cb = btnev_file;
				break;
			}
		}

		/* Create file entry button. */
		list_btn = lv_btn_create(ui->filelist, list_btn);
		lv_page_glue_obj(list_btn, true);
		lv_btn_set_layout(list_btn, LV_LAYOUT_ROW_MID);
		lv_obj_set_width(list_btn, c->w);

		/* Set button image. */
		img = lv_img_create(list_btn, img);
		lv_img_set_src(img, symbol);
		lv_obj_set_click(img, false);

		label = lv_label_create(list_btn, label);
		lv_obj_set_width(label,
				 c->w - (lv_obj_get_width_margin(img) * 4));
		lv_label_set_text(label, c->namelist[c->entries_added]->d_name);
		lv_label_set_long_mode(label, LV_LABEL_LONG_CROP);
		lv_obj_set_click(label, false);

		lv_obj_set_event_cb(list_btn, event_cb);
		lv_obj_set_user_data(list_btn, ui);
		lv_theme_apply(list_btn, LV_THEME_LIST_BTN);
		//lv_group_add_obj(u->groups[SCREEN_OPEN_FILE], list_btn);

		/* Free memory allocated by scandir(); LVGL allocated memory
		 * internally when setting the button label. */
		free(c->namelist[c->entries_added]);

		/* Move to the next entry. */
		c->entries_added++;

		/* Set the spinner progress bar. */
		// deg = ((float)c->entries_added / (float)c->entries) * 360.0f;
		/* Optimisation of the above which removes the use of
		 * floating-point instructions.
		 * "361" makes the angle round up. */
		deg = (((c->entries_added * 1024U) / (c->entries)) * 361U) /
		      1024U;
		lv_spinner_set_arc_length(ui->fileloadspinner, deg);

		/* Unlock the mutex to give the UI a chance to render if it is
		 * waiting. */
		platform_unlock_mutex(ui->lv_mutex);

		/* Since the main thread sleeps for 1ms on failure to lock
		 * mutex, we sleep this thread for 2 seconds to give the main
		 * thread a chance to continue.
		 */
		if ((platform_get_ticks() - ticks_ms) >= tlim)
		{
			platform_usleep(4);
			ticks_ms = platform_get_ticks();
		}
	}

out:
	/* Signal to the main thread that populating the filer picker list has
	 * completed. */
	platform_atomic_set(&ui->filelist_finished, FP_POPFIN_YES);

	free(c->namelist);
	free(c);
	return 0;
}

/* Alphabetical sorting */
static int smartfilesort(const struct dirent **a, const struct dirent **b)
{
	/* Sort by folders first. */
	if((*a)->d_type == (*b)->d_type)
		return 0;

	if((*a)->d_type == DT_DIR)
		return -1;

	return 1;
}

static int sort_file(const void *in1, const void *in2)
{
	const struct dirent *const *d1 = in1;
	const struct dirent *const *d2 = in2;

#if 0
	if(isdigit((*d1)->d_name[0]) != 0 && isdigit((*d2)->d_name[0] != 0))
		return strverscmp((*d1)->d_name, (*d2)->d_name);
#endif

	return strcasecmp((*d1)->d_name, (*d2)->d_name);
}

static void recreate_filepicker(void *p)
{
	struct ui_ctx *ui = p;
	lv_obj_t *list_btn = NULL, *img = NULL, *label = NULL;
	uint32_t ticks;
	struct fp_comp *c;

	c = malloc(sizeof(struct fp_comp));
	if (c == NULL)
	{
		show_error_msg("Unable to allocate memory for file picker context",
			ui->lv_disp_bot);
		return;
	}

	/* If previous population of folder hasn't finish, signal to the thread
	 * to finish immediately. */
	while (platform_atomic_get(&ui->filelist_finished) == FP_POPFIN_NO)
	{
		platform_atomic_set(&ui->filelist_populating, FP_POP_NO);
		platform_usleep(1);
	}

	lv_obj_set_hidden(ui->fileloadspinner, true);
	lv_spinner_set_arc_length(ui->fileloadspinner, 0);

	c->entries = scandir(".", &c->namelist, NULL, smartfilesort);
	if (c->entries == -1)
	{
		char err_txt[512] = "";
		char buf[PATH_MAX];

		snprintf(err_txt, sizeof(err_txt),
			 "Unable to scan '%s':\n%s",
			 getcwd(buf, sizeof(buf)) != NULL ? buf : "directory",
			 strerror(errno));

		show_error_msg(err_txt, ui->lv_disp_bot);

		/* Attempt to recover by going to root directory. */
		(void) chdir("/");

		/* Don't clean file list on error. */
		return;
	}

	/* Clean file list */
	{
		lv_obj_t *scrl = lv_page_get_scrollable(ui->filelist);
		lv_fit_t scrl_fitl = lv_cont_get_fit_left(scrl);
		lv_fit_t scrl_fitr = lv_cont_get_fit_right(scrl);
		lv_fit_t scrl_fitt = lv_cont_get_fit_top(scrl);
		lv_fit_t scrl_fitb = lv_cont_get_fit_bottom(scrl);
		lv_layout_t scrl_layout = lv_cont_get_layout(scrl);

		/* Do not re-fit all the buttons on the removable of each
		 * button. */
		lv_cont_set_fit(scrl, LV_FIT_NONE);
		lv_cont_set_layout(scrl, LV_LAYOUT_OFF);
		lv_page_clean(ui->filelist);
		lv_cont_set_fit4(scrl, scrl_fitl, scrl_fitr, scrl_fitr,
				 scrl_fitb);
		lv_cont_set_layout(scrl, scrl_layout);
		c->w = lv_obj_get_width_fit(scrl);
	}

	ticks = platform_get_ticks();
	c->entries_added = 0;
	c->u = ui;

	/* Sort folders and files separately. */
	{
		int file_ent;

		for(file_ent = 0; file_ent < c->entries; file_ent++)
		{
			if(c->namelist[file_ent]->d_type == DT_DIR)
				continue;

			break;
		}

		qsort(c->namelist, file_ent, sizeof(c->namelist),
				sort_file);
		qsort(c->namelist + file_ent, c->entries - file_ent,
				sizeof(c->namelist), sort_file);
	}

	for (; c->entries_added < c->entries; c->entries_added++)
	{
		lv_event_cb_t event_cb = NULL;
		const char *symbol = LV_SYMBOL_FILE;
		const unsigned filelist_time_thresh = 10;

		/* Ignore "current directory" file. */
		if (strcmp(c->namelist[c->entries_added]->d_name, ".") == 0)
		{
			free(c->namelist[c->entries_added]);
			continue;
		}

		/* Ignore "up directory" file, since the 3DS does not generate
		 * this automatically. */
		if (strcmp(c->namelist[c->entries_added]->d_name, "..") == 0)
		{
			free(c->namelist[c->entries_added]);
			continue;
		}

		if (c->namelist[c->entries_added]->d_type == DT_DIR)
		{
			event_cb = btnev_chdir;
			symbol = LV_SYMBOL_DIRECTORY;
		}
		else if (c->namelist[c->entries_added]->d_type == DT_LNK)
		{
			event_cb = btnev_chdir;
			symbol = LV_SYMBOL_DIRECTORY;
		}
		else
		{
			const unsigned exts_n =
				sizeof(compat_fileext)/sizeof(*compat_fileext);
			const char *ext = get_filename_ext(
			    c->namelist[c->entries_added]->d_name);

			for(unsigned ext_n = 0; ext_n < exts_n; ext_n++)
			{
				if(strcmp(ext, compat_fileext[ext_n]) != 0)
					continue;

				symbol = LV_SYMBOL_AUDIO;
				event_cb = btnev_file;
				break;
			}
		}

		list_btn = lv_btn_create(ui->filelist, list_btn);
		lv_page_glue_obj(list_btn, true);
		lv_btn_set_layout(list_btn, LV_LAYOUT_ROW_MID);
		lv_obj_set_width(list_btn, c->w);

		img = lv_img_create(list_btn, img);
		lv_img_set_src(img, symbol);
		lv_obj_set_click(img, false);

		label = lv_label_create(list_btn, label);
		lv_obj_set_width(label, c->w - (lv_obj_get_width_margin(img) * 4));
		lv_label_set_text(label, c->namelist[c->entries_added]->d_name);
		lv_label_set_long_mode(label, LV_LABEL_LONG_CROP);
		lv_obj_set_click(label, false);

		lv_obj_set_event_cb(list_btn, event_cb);
		lv_obj_set_user_data(list_btn, ui);
		lv_theme_apply(list_btn, LV_THEME_LIST_BTN);
		// lv_group_add_obj(ui_ctx->groups[SCREEN_OPEN_FILE], list_btn);

		free(c->namelist[c->entries_added]);

		/* Always allow one button to be created before decided whether
		 * or not loading this folder is taking too long. */
		if ((platform_get_ticks() - ticks) >=
				filelist_time_thresh)
		{
			c->entries_added++;
			lv_obj_set_hidden(ui->fileloadspinner, false);
			platform_atomic_set(&ui->filelist_populating,
					    FP_POP_YES);
			platform_atomic_set(&ui->filelist_finished,
					    FP_POPFIN_NO);

			/* If loading the folder is taking too long, create a
			 * thread to load the remaining entries. */
			platform_create_thread(filepicker_add_entries, c);
			return;
		}
	}

	free(c->namelist);
	free(c);
}

static void create_top_ui(struct ui_ctx *ui)
{
	const char *lorem_ipsum = "Playback information will appear here.";
	lv_obj_t *label1, *top_cont;
	lv_coord_t ver, hor;

	/* Select bottom screen. */
	lv_disp_set_default(ui->lv_disp_top);
	ver = lv_disp_get_ver_res(ui->lv_disp_top);
	hor = lv_disp_get_hor_res(ui->lv_disp_top);

	top_cont = lv_cont_create(lv_scr_act(), NULL);
	label1 = lv_label_create(top_cont, NULL);
	lv_label_set_text(label1, lorem_ipsum);
	lv_label_set_long_mode(label1, LV_LABEL_LONG_BREAK);
	lv_cont_set_fit(top_cont, LV_FIT_NONE);
	lv_obj_set_size(top_cont, hor, ver);
	lv_cont_set_layout(top_cont, LV_LAYOUT_COLUMN_MID);
}

static void create_bottom_ui(struct ui_ctx *ui)
{
	lv_obj_t *tabview;
	lv_obj_t *tab_file, *tab_playlist, *tab_control, *tab_settings,
	    *tab_system;

	/* Select bottom screen. */
	lv_disp_set_default(ui->lv_disp_bot);

	/* Create tabview with main options. */
	tabview = lv_tabview_create(lv_scr_act(), NULL);

	tab_file = lv_tabview_add_tab(tabview, LV_SYMBOL_DIRECTORY);
	tab_playlist = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST);
	tab_control = lv_tabview_add_tab(tabview, LV_SYMBOL_PLAY);
	tab_settings = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);
	tab_system = lv_tabview_add_tab(tabview, LV_SYMBOL_POWER);

	/* Create file picker. */
	{
		lv_obj_t *toolbar;
		lv_coord_t cw = lv_obj_get_width(tab_file);
		lv_coord_t ch = lv_obj_get_height(tab_file);
		lv_coord_t toolbar_h = 32;
		lv_obj_t *file_scrl = lv_page_get_scrollable(tab_file);

		lv_cont_set_layout(file_scrl, LV_LAYOUT_ROW_TOP);

		ui->filelist = lv_page_create(tab_file, NULL);
		toolbar = lv_cont_create(tab_file, NULL);

		lv_obj_set_state(toolbar, LV_STATE_DISABLED);
		lv_cont_set_fit(toolbar, LV_FIT_NONE);
		lv_cont_set_layout(toolbar, LV_LAYOUT_COLUMN_LEFT);
		lv_obj_set_size(toolbar, toolbar_h, ch);
		lv_obj_set_style_local_pad_all(
			    toolbar, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
		lv_obj_set_style_local_pad_inner(toolbar,
				LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);

		/* Add buttons to toolbar. */
		{
			lv_obj_t *btn, *btn_lbl;
			btn = lv_btn_create(toolbar, NULL);
			btn_lbl = lv_label_create(btn, NULL);
			lv_label_set_text(btn_lbl, LV_SYMBOL_UP);
			lv_obj_set_size(btn, toolbar_h, toolbar_h);
			lv_obj_set_event_cb(btn, btnev_updir);
			lv_obj_set_user_data(btn, ui);

			ui->fileaddbtn = lv_btn_create(toolbar, NULL);
			btn_lbl = lv_label_create(ui->fileaddbtn, NULL);
			lv_label_set_text(btn_lbl, LV_SYMBOL_PLUS);
			lv_obj_set_size(ui->fileaddbtn, toolbar_h, toolbar_h);
			//lv_obj_set_event_cb(btn, btnev_updir);
			lv_btn_set_checkable(ui->fileaddbtn, true);
			lv_obj_set_user_data(ui->fileaddbtn, ui);

			ui->fileloadspinner = lv_spinner_create(toolbar, NULL);
			lv_obj_set_hidden(ui->fileloadspinner, true);
			lv_obj_set_style_local_pad_all(
			    ui->fileloadspinner, LV_SPINNER_PART_BG,
			    LV_STATE_DEFAULT, 4);
			lv_spinner_set_type(ui->fileloadspinner,
					    LV_SPINNER_TYPE_CONSTANT_ARC);
			lv_obj_set_size(ui->fileloadspinner, toolbar_h,
					toolbar_h);
			lv_spinner_set_arc_length(ui->fileloadspinner, 0);
		}

		lv_obj_set_size(ui->filelist, cw - toolbar_h, ch);
		lv_theme_apply(ui->filelist, LV_THEME_LIST);
		lv_page_set_scrl_layout(ui->filelist, LV_LAYOUT_COLUMN_MID);
		lv_page_set_scrollbar_mode(ui->filelist,
					   LV_SCROLLBAR_MODE_AUTO);

		/* Store target display in user data for the file list. */
		lv_obj_set_user_data(ui->filelist, ui);
		lv_async_call(recreate_filepicker, ui);
	}

	/* Populate playlist tab. */
	{
		lv_coord_t cw = lv_obj_get_width(tab_playlist);
		lv_coord_t ch = lv_obj_get_height(tab_playlist);

		ui->playlist = lv_page_create(tab_playlist, NULL);
		lv_obj_set_size(ui->playlist, cw, ch);
		lv_theme_apply(ui->playlist, LV_THEME_LIST);
		lv_page_set_scrl_layout(ui->playlist, LV_LAYOUT_COLUMN_MID);
		lv_page_set_scrollbar_mode(ui->playlist,
			LV_SCROLLBAR_MODE_AUTO);
		lv_obj_set_user_data(ui->playlist, ui);
	}

	/* Populate playlist control tab. */
	{
		lv_obj_t *prev, *next;
		lv_obj_t *play;
		lv_obj_t *lbl;
		lv_obj_t *scrl;

		scrl = lv_page_get_scrollable(tab_control);
		lv_cont_set_layout(scrl, LV_LAYOUT_ROW_MID);

		prev = lv_btn_create(scrl, NULL);
		lbl = lv_label_create(prev, NULL);
		lv_label_set_text(lbl, LV_SYMBOL_PREV);
		lv_obj_set_event_cb(prev, btnev_nop);
		lv_obj_set_user_data(prev, ui);

		play = lv_btn_create(scrl, NULL);
		lbl = lv_label_create(play, NULL);
		lv_label_set_text(lbl, LV_SYMBOL_PLAY);
		lv_obj_set_event_cb(play, btnev_nop);
		lv_obj_set_user_data(play, ui);

		next = lv_btn_create(scrl, NULL);
		lbl = lv_label_create(next, NULL);
		lv_label_set_text(lbl, LV_SYMBOL_NEXT);
		lv_obj_set_event_cb(next, btnev_nop);
		lv_obj_set_user_data(next, ui);
	}

	/* Populate settings tab. */
	{
		lv_obj_t *text;
		static const char *str = "There are no settings available to "
					 "configure.";

		text = lv_label_create(tab_settings, NULL);
		lv_label_set_text_static(text, str);
		lv_obj_align_mid(text, tab_settings, LV_ALIGN_CENTER,
			0, 0);
	}

	/* Populate system tab. */
	{
		lv_obj_t *quit_btn, *quit_lbl;
		quit_btn = lv_btn_create(tab_system, NULL);
		lv_obj_set_event_cb(quit_btn, btnev_quit);
		lv_obj_align(quit_btn, tab_system, LV_ALIGN_IN_TOP_MID, 0, 0);
		quit_lbl = lv_label_create(quit_btn, NULL);
		lv_label_set_text(quit_lbl, "Quit");
	}

	return;
}

static void print_cb(lv_log_level_t level, const char *file, uint32_t line,
	      const char *fn, const char *desc)
{
	const char *pri_str[] = {"TRACE", "INFO", "WARNING",
				 "ERROR", "USER", "NONE"};

	fprintf(stderr, "%s %s +%u %s(): %s\n", pri_str[level], file, line, fn,
		desc);
	return;
}

int main(int argc, char *argv[])
{
	platform_ctx_s *plat;
	struct ui_ctx ui = { 0 };
	int ret = EXIT_FAILURE;
	lv_disp_buf_t lv_disp_buf_top, lv_disp_buf_bot;
	lv_disp_drv_t lv_disp_drv_top, lv_disp_drv_bot;
	static lv_color_t top_buf[BUF_PX_SIZE];
	static lv_color_t bot_buf[BUF_PX_SIZE];

	(void)argc;
	(void)argv;

	plat = init_system();
	if (plat == NULL)
		goto err;

	/* Initialise LVGL. */
	lv_init();
#ifdef __DEBUG__
	lv_log_register_print_cb(print_cb);
#endif

	lv_disp_buf_init(&lv_disp_buf_top, top_buf, NULL, BUF_PX_SIZE);
	lv_disp_buf_init(&lv_disp_buf_bot, bot_buf, NULL, BUF_PX_SIZE);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_top.buffer = &lv_disp_buf_top;
	lv_disp_drv_top.flush_cb = flush_top_cb;
	lv_disp_drv_top.user_data = plat;
#ifdef __3DS__
	lv_disp_drv_top.rotated = LV_DISP_ROT_270;
	lv_disp_drv_top.sw_rotate = 1;
	lv_disp_drv_top.hor_res = GSP_SCREEN_WIDTH_TOP;
	lv_disp_drv_top.ver_res = GSP_SCREEN_HEIGHT_TOP;
#else
	lv_disp_drv_top.rotated = 0;
	lv_disp_drv_top.sw_rotate = 0;
	lv_disp_drv_top.hor_res = NATURAL_SCREEN_WIDTH_TOP;
	lv_disp_drv_top.ver_res = NATURAL_SCREEN_HEIGHT_TOP;
#endif
	ui.lv_disp_top = lv_disp_drv_register(&lv_disp_drv_top);

	lv_disp_drv_init(&lv_disp_drv_bot);
	lv_disp_drv_bot.buffer = &lv_disp_buf_bot;
	lv_disp_drv_bot.flush_cb = flush_bot_cb;
	lv_disp_drv_bot.user_data = plat;
#ifdef __3DS__
	lv_disp_drv_bot.rotated = LV_DISP_ROT_270;
	lv_disp_drv_bot.sw_rotate = 1;
	lv_disp_drv_bot.hor_res = GSP_SCREEN_WIDTH_BOT;
	lv_disp_drv_bot.ver_res = GSP_SCREEN_HEIGHT_BOT;
#else
	lv_disp_drv_bot.rotated = 0;
	lv_disp_drv_bot.sw_rotate = 0;
	lv_disp_drv_bot.hor_res = NATURAL_SCREEN_WIDTH_BOT;
	lv_disp_drv_bot.ver_res = NATURAL_SCREEN_HEIGHT_BOT;
#endif
	ui.lv_disp_bot = lv_disp_drv_register(&lv_disp_drv_bot);

	lv_disp_drv_init(&lv_disp_drv_top);
	lv_disp_drv_init(&lv_disp_drv_bot);

	/* Initialise UI input drivers. */
	lv_disp_set_default(ui.lv_disp_bot);
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = read_pointer;
	indev_drv.user_data = plat;
	lv_indev_drv_register(&indev_drv);

	ui.lv_mutex = platform_create_mutex();
	platform_atomic_set(&ui.filelist_populating, FP_POP_NO);
	platform_atomic_set(&ui.filelist_finished, FP_POPFIN_YES);

	create_top_ui(&ui);
	create_bottom_ui(&ui);

	ui.pbk = playback_init();
	/* TODO: Return an error message instead of exiting. */
	if(ui.pbk == NULL)
		goto err;

	while (exit_requested(plat) == 0 && quit == false)
	{
		if (platform_atomic_get(&ui.filelist_finished) == FP_POPFIN_YES)
			lv_obj_set_hidden(ui.fileloadspinner, true);

		handle_events(plat);
		lv_task_handler();
		render_present(plat);
	}

	exit_system(plat);
	playback_exit(ui.pbk);

	ret = EXIT_SUCCESS;

err:
	return ret;
}

/**
 * Platform.c: Platform specific functions.
 * Copyright (c) 2021 Mahyar Koshkouei
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 */

#include <lvgl.h>
#include <platform.h>

#if defined(__3DS__)
# include <3ds.h>

struct platform_ctx
{
	unsigned unused;
};

int exit_requested(platform_ctx_s *ctx)
{
	return !aptMainLoop();
}

platform_ctx_s *init_system(void)
{
	void *c = NULL;
	u16 width, height;

	/* This stops scandir() from working. */
	// consoleDebugInit(debugDevice_SVC);

	gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);

	c = c - 1;

out:
	return c;
}

void handle_events(platform_ctx_s *ctx)
{
	u32 kDown;

	/* Scan all the inputs. */
	hidScanInput();
	kDown = hidKeysDown();

	return;
}

void render_present(platform_ctx_s *ctx)
{
	gfxFlushBuffers();
	gfxScreenSwapBuffers(GFX_TOP, false);
	gfxScreenSwapBuffers(GFX_BOTTOM, false);

	/* Wait for VBlank */
	gspWaitForVBlank();
}

static void draw_pixels(lv_color_t *restrict dst, const lv_color_t *restrict src, const lv_area_t *area, u16 w)
{
	for (lv_coord_t y = area->y1; y <= area->y2; y++)
	{
		lv_color_t *dst_p = dst + (w * y) + area->x1;
		size_t len = (area->x2 - area->x1) + 1;
		size_t sz = len * sizeof(lv_color_t);

		memcpy(dst_p, src, sz);
		src += len;
	}
}

void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct platform_ctx *c = disp_drv->user_data;
	u16 w;
	lv_color_t *fb = (lv_color_t *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, NULL);
	draw_pixels(fb, color_p, area, w);
	lv_disp_flush_ready(disp_drv);
}

void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct platform_ctx *c = disp_drv->user_data;
	u16 w;
	lv_color_t *fb = (lv_color_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, NULL);
	draw_pixels(fb, color_p, area, w);
	lv_disp_flush_ready(disp_drv);
}

bool read_pointer(struct _lv_indev_drv_t *indev_drv,
			 lv_indev_data_t *data)
{
	static touchPosition touch = {0};
	u32 kRepeat;

	kRepeat = hidKeysHeld();

	if (kRepeat & KEY_TOUCH)
		hidTouchRead(&touch);

	data->point.x = touch.px;
	data->point.y = touch.py;
	data->state =
	    (kRepeat & KEY_TOUCH) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
	return false;
}

void exit_system(platform_ctx_s *ctx)
{
	gfxExit();
	ctx = NULL;
	return;
}

void platform_create_thread(platform_thread_fn fn, void *thread_data)
{
	const size_t stack_size = 4 * 1024;
	s32 prio = 0;

	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	threadCreate(fn, thread_data, stack_size, prio + 1, -2, true);
}

/* Functions for synchronization mechanisms. */
platform_mutex_s *platform_create_mutex(void)
{
	static LightLock locks[16];
	static int locks_used = 0;
	LightLock *lock;

	if (locks_used >= sizeof(locks) / sizeof(locks[0]))
		return NULL;

	LightLock_Init(&locks[locks_used]);
	return &locks[locks_used];
}

void platform_destroy_mutex(platform_mutex_s *mutex)
{
	return;
}

void platform_lock_mutex(platform_mutex_s *mutex)
{
	LightLock_Lock(mutex);
}

mutex_stat_e platform_try_lock_mutex(platform_mutex_s *mutex)
{
	return LightLock_TryLock(mutex);
}

void platform_unlock_mutex(platform_mutex_s *mutex)
{
	LightLock_Unlock(mutex);
}

/* Functions for atomic operations. */
int platform_atomic_get(platform_atomic_s *atomic)
{
	return __atomic_load_n(&atomic->value, __ATOMIC_SEQ_CST);
}

void platform_atomic_set(platform_atomic_s *atomic, int val)
{
	__atomic_store_n(&atomic->value, val, __ATOMIC_SEQ_CST);
}

void platform_usleep(unsigned ms)
{
	s64 ns = (u64)ms * 1024UL;
	svcSleepThread(ns);
}

uint64_t platform_get_ticks(void)
{
	return osGetTime();
}

#else
#include <SDL.h>

struct platform_ctx
{
	SDL_Window *win_top;
	SDL_Window *win_bot;
};

int exit_requested(platform_ctx_s *ctx)
{
	(void) ctx;
	return SDL_QuitRequested();
}

platform_ctx_s *init_system(void)
{
	struct platform_ctx *ctx;
	int win_top_x, win_top_y, win_top_border;

	ctx = SDL_calloc(1, sizeof(struct platform_ctx));
	SDL_assert_always(ctx != NULL);

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
	SDL_Init(SDL_INIT_EVERYTHING);

	ctx->win_top = SDL_CreateWindow("3DS Top Screen",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		NATURAL_SCREEN_WIDTH_TOP, NATURAL_SCREEN_HEIGHT_TOP, 0);
	SDL_assert_always(ctx->win_top != NULL);

	/* Align the bottom window to the top window. */
	SDL_GetWindowPosition(ctx->win_top, &win_top_x, &win_top_y);
	SDL_GetWindowBordersSize(ctx->win_top, &win_top_border, NULL, NULL, NULL);
	win_top_x += (GSP_SCREEN_HEIGHT_TOP - GSP_SCREEN_HEIGHT_BOT) / 2;
	win_top_y += GSP_SCREEN_WIDTH_TOP + win_top_border;

	ctx->win_bot = SDL_CreateWindow("3DS Bottom Screen", win_top_x, win_top_y,
				      NATURAL_SCREEN_WIDTH_BOT,
				      NATURAL_SCREEN_HEIGHT_BOT, 0);
	SDL_assert_always(ctx->win_bot != NULL);

	return ctx;
}

void handle_events(platform_ctx_s *ctx)
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
	{
	}
	return;
}

void render_present(platform_ctx_s *ctx)
{
	SDL_UpdateWindowSurface(ctx->win_top);
	SDL_UpdateWindowSurface(ctx->win_bot);
	SDL_Delay(5);
	return;
}

static void flush_cb(SDL_Surface *dst, const lv_area_t *area,
		     lv_color_t *color_p)
{
	SDL_Surface *src;
	SDL_Rect dstrect;

	dstrect.x = area->x1;
	dstrect.y = area->y1;
	dstrect.w = area->x2 - area->x1 + 1;
	dstrect.h = area->y2 - area->y1 + 1;
	src = SDL_CreateRGBSurfaceWithFormatFrom(
	    color_p, dstrect.w, dstrect.h, 16, dstrect.w * sizeof(lv_color_t),
	    SDL_PIXELFORMAT_RGB565);

	SDL_BlitSurface(src, NULL, dst, &dstrect);
	SDL_FreeSurface(src);
}

void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct platform_ctx *ctx = disp_drv->user_data;
	SDL_Surface *surf_top = SDL_GetWindowSurface(ctx->win_top);
	flush_cb(surf_top, area, color_p);
	lv_disp_flush_ready(disp_drv);
}

void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p)
{
	struct platform_ctx *ctx = disp_drv->user_data;
	SDL_Surface *surf_bot = SDL_GetWindowSurface(ctx->win_bot);
	flush_cb(surf_bot, area, color_p);
	lv_disp_flush_ready(disp_drv);
}

bool read_pointer(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
	struct platform_ctx *ctx = indev_drv->user_data;

	if (indev_drv->type == LV_INDEV_TYPE_POINTER)
	{
		int x, y;
		Uint32 btnmask;
		SDL_Window *win_active;
		Uint32 win_bot_id;
		Uint32 win_focused_id;

		win_active = SDL_GetMouseFocus();
		win_bot_id = SDL_GetWindowID(ctx->win_bot);
		win_focused_id = SDL_GetWindowID(win_active);

		/* Only accept touch input on bottom screen. */
		if (win_focused_id != win_bot_id)
			return false;

		btnmask = SDL_GetMouseState(&x, &y);

		data->point.x = x;
		data->point.y = y;
		data->state = (btnmask & SDL_BUTTON(SDL_BUTTON_LEFT))
				  ? LV_INDEV_STATE_PR
				  : LV_INDEV_STATE_REL;
	}
	else if (indev_drv->type == LV_INDEV_TYPE_KEYPAD)
	{
		const Uint8 *state = SDL_GetKeyboardState(NULL);
		SDL_Keymod mod = SDL_GetModState();

		if (state[SDL_SCANCODE_DOWN])
			data->key = LV_KEY_DOWN;
		else if (state[SDL_SCANCODE_UP])
			data->key = LV_KEY_UP;
		else if (state[SDL_SCANCODE_RIGHT])
			data->key = LV_KEY_RIGHT;
		else if (state[SDL_SCANCODE_LEFT])
			data->key = LV_KEY_LEFT;
		else if (state[SDL_SCANCODE_RETURN])
			data->key = LV_KEY_ENTER;
		else if (state[SDL_SCANCODE_BACKSPACE])
			data->key = LV_KEY_BACKSPACE;
		else if (state[SDL_SCANCODE_ESCAPE])
			data->key = LV_KEY_ESC;
		else if (state[SDL_SCANCODE_DELETE])
			data->key = LV_KEY_DEL;
		else if (state[SDL_SCANCODE_TAB] && (mod & KMOD_LSHIFT))
			data->key = LV_KEY_PREV;
		else if (state[SDL_SCANCODE_TAB])
			data->key = LV_KEY_NEXT;
		else if (state[SDL_SCANCODE_HOME])
			data->key = LV_KEY_HOME;
		else if (state[SDL_SCANCODE_END])
			data->key = LV_KEY_END;
		else
		{
			data->state = LV_INDEV_STATE_REL;
			return false;
		}

		data->state = LV_INDEV_STATE_PR;
	}
	return false;
}
void exit_system(platform_ctx_s *ctx)
{
	SDL_DestroyWindow(ctx->win_top);
	SDL_DestroyWindow(ctx->win_bot);
	SDL_Quit();

	return;
}

void platform_create_thread(platform_thread_fn fn, void *thread_data)
{
	char thread_name[32];
	SDL_Thread *thread;

	SDL_snprintf(thread_name, sizeof(thread_name), "Thread %p", fn);
	thread = SDL_CreateThread(fn, thread_name, thread_data);
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
	SDL_DetachThread(thread);
}

/* Functions for synchronization mechanisms. */
platform_mutex_s *platform_create_mutex(void)
{
	return SDL_CreateMutex();
}

void platform_destroy_mutex(platform_mutex_s *mutex)
{
	SDL_DestroyMutex(mutex);
}

void platform_lock_mutex(platform_mutex_s *mutex)
{
	SDL_LockMutex(mutex);
}

mutex_stat_e platform_try_lock_mutex(platform_mutex_s *mutex)
{
	int r;
	r = SDL_TryLockMutex(mutex);
	return (mutex_stat_e)r;
}

void platform_unlock_mutex(platform_mutex_s *mutex)
{
	SDL_UnlockMutex(mutex);
}

/* Functions for atomic operations. */
int platform_atomic_get(platform_atomic_s *atomic)
{
	return SDL_AtomicGet((SDL_atomic_t *)atomic);
}

void platform_atomic_set(platform_atomic_s *atomic, int val)
{
	SDL_AtomicSet((SDL_atomic_t *)atomic, val);
}

void platform_usleep(unsigned ms)
{
	SDL_Delay(ms);
}

uint64_t platform_get_ticks(void)
{
	return SDL_GetTicks();
}

#endif

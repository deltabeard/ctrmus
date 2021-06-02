#pragma once

#include <lvgl.h>

/* 3DS screens are portrait, so framebuffer must be rotated by 90 degrees. */
#ifndef GSP_SCREEN_WIDTH_TOP
#define GSP_SCREEN_WIDTH_TOP		240
#define GSP_SCREEN_WIDTH_BOT		240
#define GSP_SCREEN_HEIGHT_TOP		400
#define GSP_SCREEN_HEIGHT_BOT		320
#endif

#define NATURAL_SCREEN_WIDTH_TOP	GSP_SCREEN_HEIGHT_TOP
#define NATURAL_SCREEN_HEIGHT_TOP	GSP_SCREEN_WIDTH_BOT
#define NATURAL_SCREEN_WIDTH_BOT	GSP_SCREEN_HEIGHT_BOT
#define NATURAL_SCREEN_HEIGHT_BOT	GSP_SCREEN_WIDTH_BOT

#define SCREEN_PIXELS_TOP		(GSP_SCREEN_WIDTH_TOP * GSP_SCREEN_HEIGHT_TOP)
#define SCREEN_PIXELS_BOT		(GSP_SCREEN_WIDTH_BOT * GSP_SCREEN_HEIGHT_BOT)

/* Declerations for platform specific functions. */
/* Opaque platform context. */
typedef struct platform_ctx platform_ctx_s;

/* Thread function pointer. */
#ifdef __3DS__
typedef void (*platform_thread_fn)(void *thread_data);
#else
typedef int (*platform_thread_fn)(void *thread_data);
#endif

/* Mutex context. */
typedef void platform_mutex_s;

/* Atomic context. */
typedef struct { int value; } platform_atomic_s;

typedef enum
{
	LOCK_MUTEX_ERROR = -1,
	LOCK_MUTEX_SUCCESS = 0,
	LOCK_MUTEX_TIMEOUT = 1
} mutex_stat_e;

/**
 * Initialise platform.
 * \returns Opaque pointer to platform context, or NULL on error.
 */
platform_ctx_s *init_system(void);
void handle_events(platform_ctx_s *ctx);
void render_present(platform_ctx_s *ctx);
void flush_top_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p);
void flush_bot_cb(struct _disp_drv_t *disp_drv, const lv_area_t *area,
			 lv_color_t *color_p);
bool read_pointer(struct _lv_indev_drv_t *indev_drv,
			 lv_indev_data_t *data);
void exit_system(platform_ctx_s *ctx);
int exit_requested(platform_ctx_s *ctx);

/**
 * Create a detached thread.
 */
void platform_create_thread(platform_thread_fn fn, void *thread_data);

/* Functions for synchronization mechanisms. */
platform_mutex_s *platform_create_mutex(void);
void platform_destroy_mutex(platform_mutex_s *mutex);
void platform_lock_mutex(platform_mutex_s *mutex);
mutex_stat_e platform_try_lock_mutex(platform_mutex_s *mutex);
void platform_unlock_mutex(platform_mutex_s *mutex);

/* Functions for atomic operations. */
int platform_atomic_get(platform_atomic_s *atomic);
void platform_atomic_set(platform_atomic_s *atomic, int val);

void platform_usleep(unsigned ms);

uint64_t platform_get_ticks(void);

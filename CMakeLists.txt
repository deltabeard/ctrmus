cmake_minimum_required(VERSION 3.20)
project(ctrmus C)

set(CMAKE_C_STANDARD 99)

add_executable(ctrmus)
target_sources(ctrmus PRIVATE src/main.c src/platform.c src/playback.c)
target_include_directories(ctrmus PRIVATE inc)

# Add LVGL library
target_sources(ctrmus PRIVATE
		inc/lvgl/src/lv_core/lv_disp.c
		inc/lvgl/src/lv_core/lv_group.c
		inc/lvgl/src/lv_core/lv_indev.c
		inc/lvgl/src/lv_core/lv_obj.c
		inc/lvgl/src/lv_core/lv_refr.c
		inc/lvgl/src/lv_core/lv_style.c
		inc/lvgl/src/lv_draw/lv_draw_arc.c
		inc/lvgl/src/lv_draw/lv_draw_blend.c
		inc/lvgl/src/lv_draw/lv_draw_img.c
		inc/lvgl/src/lv_draw/lv_draw_label.c
		inc/lvgl/src/lv_draw/lv_draw_line.c
		inc/lvgl/src/lv_draw/lv_draw_mask.c
		inc/lvgl/src/lv_draw/lv_draw_rect.c
		inc/lvgl/src/lv_draw/lv_draw_triangle.c
		inc/lvgl/src/lv_draw/lv_img_buf.c
		inc/lvgl/src/lv_draw/lv_img_cache.c
		inc/lvgl/src/lv_draw/lv_img_decoder.c
		inc/lvgl/src/lv_font/lv_font.c
		inc/lvgl/src/lv_font/lv_font_fmt_txt.c
		inc/lvgl/src/lv_font/lv_font_loader.c
		inc/lvgl/src/lv_hal/lv_hal_disp.c
		inc/lvgl/src/lv_hal/lv_hal_indev.c
		inc/lvgl/src/lv_hal/lv_hal_tick.c
		inc/lvgl/src/lv_misc/lv_anim.c
		inc/lvgl/src/lv_misc/lv_area.c
		inc/lvgl/src/lv_misc/lv_async.c
		inc/lvgl/src/lv_misc/lv_bidi.c
		inc/lvgl/src/lv_misc/lv_color.c
		inc/lvgl/src/lv_misc/lv_debug.c
		inc/lvgl/src/lv_misc/lv_fs.c
		inc/lvgl/src/lv_misc/lv_gc.c
		inc/lvgl/src/lv_misc/lv_ll.c
		inc/lvgl/src/lv_misc/lv_log.c
		inc/lvgl/src/lv_misc/lv_math.c
		inc/lvgl/src/lv_misc/lv_mem.c
		inc/lvgl/src/lv_misc/lv_printf.c
		inc/lvgl/src/lv_misc/lv_task.c
		inc/lvgl/src/lv_misc/lv_templ.c
		inc/lvgl/src/lv_misc/lv_txt.c
		inc/lvgl/src/lv_misc/lv_txt_ap.c
		inc/lvgl/src/lv_misc/lv_utils.c
		inc/lvgl/src/lv_themes/lv_theme.c
		inc/lvgl/src/lv_themes/lv_theme_material.c
		inc/lvgl/src/lv_widgets/lv_arc.c
		inc/lvgl/src/lv_widgets/lv_bar.c
		inc/lvgl/src/lv_widgets/lv_btn.c
		inc/lvgl/src/lv_widgets/lv_btnmatrix.c
		inc/lvgl/src/lv_widgets/lv_calendar.c
		inc/lvgl/src/lv_widgets/lv_canvas.c
		inc/lvgl/src/lv_widgets/lv_chart.c
		inc/lvgl/src/lv_widgets/lv_checkbox.c
		inc/lvgl/src/lv_widgets/lv_cont.c
		inc/lvgl/src/lv_widgets/lv_cpicker.c
		inc/lvgl/src/lv_widgets/lv_dropdown.c
		inc/lvgl/src/lv_widgets/lv_gauge.c
		inc/lvgl/src/lv_widgets/lv_img.c
		inc/lvgl/src/lv_widgets/lv_imgbtn.c
		inc/lvgl/src/lv_widgets/lv_keyboard.c
		inc/lvgl/src/lv_widgets/lv_label.c
		inc/lvgl/src/lv_widgets/lv_led.c
		inc/lvgl/src/lv_widgets/lv_line.c
		inc/lvgl/src/lv_widgets/lv_linemeter.c
		inc/lvgl/src/lv_widgets/lv_list.c
		inc/lvgl/src/lv_widgets/lv_msgbox.c
		inc/lvgl/src/lv_widgets/lv_objmask.c
		inc/lvgl/src/lv_widgets/lv_objx_templ.c
		inc/lvgl/src/lv_widgets/lv_page.c
		inc/lvgl/src/lv_widgets/lv_roller.c
		inc/lvgl/src/lv_widgets/lv_slider.c
		inc/lvgl/src/lv_widgets/lv_spinbox.c
		inc/lvgl/src/lv_widgets/lv_spinner.c
		inc/lvgl/src/lv_widgets/lv_switch.c
		inc/lvgl/src/lv_widgets/lv_table.c
		inc/lvgl/src/lv_widgets/lv_tabview.c
		inc/lvgl/src/lv_widgets/lv_textarea.c
		inc/lvgl/src/lv_widgets/lv_tileview.c
		inc/lvgl/src/lv_widgets/lv_win.c)
target_include_directories(ctrmus PRIVATE inc/lvgl)

# All platforms other than the 3DS require SDL2
if(NOT DEFINED DEVKITARM)
	find_package(SDL2 CONFIG REQUIRED)
	target_link_libraries(ctrmus PRIVATE SDL2::SDL2 SDL2::SDL2main)
endif()

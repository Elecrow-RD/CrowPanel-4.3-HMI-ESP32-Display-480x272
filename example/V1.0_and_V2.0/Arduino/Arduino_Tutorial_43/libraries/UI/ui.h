// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.0
// LVGL VERSION: 8.3.3
// PROJECT: LVGL35

#ifndef _LVGL35_UI_H
#define _LVGL35_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"



void ui_event_BEGIN(lv_event_t * e);
extern lv_obj_t * ui_BEGIN;
extern lv_obj_t * ui_toppic;
extern lv_obj_t * ui_dowmpic;
extern lv_obj_t * ui_dowmblack;
extern lv_obj_t * ui_MENU;
void ui_event_meun(lv_event_t * e);
extern lv_obj_t * ui_menu;
void ui_event_touch(lv_event_t * e);
extern lv_obj_t * ui_touch;
extern lv_obj_t * ui_Bar;
extern lv_obj_t * ui_TOUCH;
void ui_event_jiaozhun(lv_event_t * e);
extern lv_obj_t * ui_jiaozhun;
void ui_event_ok(lv_event_t * e);
extern lv_obj_t * ui_ok;
extern lv_obj_t * ui_Label2;


extern lv_obj_t * ui_touch_calibrate;
//4.3
LV_IMG_DECLARE(ui_img_812486673);    // assets\wizee_logo_02_120x30.png
LV_IMG_DECLARE(ui_img_1405828474);    // assets\Wizee-ESP32-02-01.png
LV_IMG_DECLARE(ui_img_crowpanel_ui_bj_dis06043h_png);    // assets\480x272_R043.png
LV_IMG_DECLARE(ui_img_97665003);    // assets\005-icon_click_02.png
LV_IMG_DECLARE(ui_img_icon_click_1_png);    // assets\icon_click_1.png
LV_IMG_DECLARE(ui_img_1220821074);    // assets\005-icon_home_02.png
LV_IMG_DECLARE(ui_img_icon_home_1_png);    // assets\icon_home_1.png
LV_IMG_DECLARE(ui_img_bar_480_01_png);    // assets\bar_480_01.png
LV_IMG_DECLARE(ui_img_bar_480_02_png);    // assets\bar_480_02.png
LV_IMG_DECLARE(ui_img_553284475);    // assets\005-icon_return_02.png
LV_IMG_DECLARE(ui_img_1406806916);    // assets\003-icon_return_03.png



void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif

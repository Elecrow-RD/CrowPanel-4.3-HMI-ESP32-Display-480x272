#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

//UI
#include "ui.h"
#include <SPI.h>

SPIClass &spi = SPI;
int led = 0;

static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static lv_color_t *lvgl_draw_buf = nullptr;

#include <Arduino_GFX_Library.h>
#define TFT_BL 2

#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *lcd = create_default_Arduino_GFX();
#else

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 6000000 /* prefer_speed */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 480 /* bounce_buffer_size_px */
);

Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
    480 /* width */, 272 /* height */, bus, 0 /* rotation */, true /* auto_flush */);

#endif

#include "touch.h"

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    lcd->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    lcd->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    static uint32_t call_count = 0;
    call_count++;
    
    if (call_count % 50 == 0) {
        Serial.print("my_touchpad_read CALLED, count=");
        Serial.println(call_count);
    }
    
    ts.isrWake = true;
    if (ts.touched())
    {
        TS_Point p = ts.getPoint();
        int16_t x = map(p.x, 4000, 200, 0, 479);
        int16_t y = map(p.y, 200, 3600, 0, 271);
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

extern "C" void app_main()
{
    initArduino();

    Serial.begin(115200);
    delay(100);
    Serial.println("app_main start");

    lv_init();

    Serial.println("Calling lcd->begin()...");
    bool ok = lcd->begin();
    Serial.printf("lcd->begin() returned: %d\r\n", ok);
    if (!ok) {
        Serial.println("lcd->begin() failed, halt");
        while (1) {
            delay(1000);
        }
    }

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    touch_init();
    delay(300);

    pinMode(38, OUTPUT);
    digitalWrite(38, LOW);

    screenWidth = lcd->width();
    screenHeight = lcd->height();

    const size_t draw_buf_pixels = screenWidth * 40;
    lvgl_draw_buf = (lv_color_t *)heap_caps_malloc(draw_buf_pixels * sizeof(lv_color_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!lvgl_draw_buf) {
        Serial.println("draw buffer alloc failed");
        while (1) {
            delay(1000);
        }
    }
    lv_disp_draw_buf_init(&draw_buf, lvgl_draw_buf, NULL, draw_buf_pixels);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();

    Serial.println("Setup done");

    uint32_t last_tick_ms = millis();
    uint32_t last_touch_check = 0;

    while (1)
    {
        if (led == 1) {
            digitalWrite(38, HIGH);
        } else {
            digitalWrite(38, LOW);
        }

        uint32_t now = millis();
        lv_tick_inc(now - last_tick_ms);
        last_tick_ms = now;

        if (now - last_touch_check > 200) {
            last_touch_check = now;
            ts.isrWake = true;
            if (ts.touched()) {
                TS_Point p = ts.getPoint();
                int16_t x = map(p.x, 4000, 200, 0, 479);
                int16_t y = map(p.y, 200, 3600, 0, 271);
                // Serial.print("RAW TOUCH: x="); Serial.print(x); Serial.print(", y="); Serial.print(y); Serial.print(", z="); Serial.println(p.z);
            }
        }

        lv_timer_handler();
        delay(5);
    }
}

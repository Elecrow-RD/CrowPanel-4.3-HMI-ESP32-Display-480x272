#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

// Provides the screen and event code exported by SquareLine Studio.
#include "ui.h"
#include <SPI.h>

// Reuses Arduino's SPI controller for the XPT2046 touch interface.
SPIClass &spi = SPI;

// Stores the LED state requested by the SquareLine button callbacks.
int led = 0;

// Stores the width reported by the initialized display in pixels.
static uint32_t screenWidth;

// Stores the height reported by the initialized display in pixels.
static uint32_t screenHeight;

// Points to the LVGL partial render buffer allocated from external PSRAM.
static lv_color_t *lvgl_draw_buf = nullptr;

#include <Arduino_GFX_Library.h>
#define TFT_BL 2

/*---------------------------------------------------------------
 * RGB display hardware
 * Arduino GFX runs as an ESP-IDF component and drives the NV3047 panel.
 *--------------------------------------------------------------*/
#if defined(DISPLAY_DEV_KIT)
// Provides the display object used by every drawing and LVGL flush operation.
Arduino_GFX *lcd = create_default_Arduino_GFX();
#else

// Generates the 16-bit RGB data and synchronization signals for the LCD.
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 6000000 /* prefer_speed */, false /* useBigEndian */,
    // Eight scan lines in internal SRAM isolate RGB DMA from short PSRAM stalls
    // caused by touch SPI reads and LVGL rendering. 480 * 272 pixels is exactly
    // divisible by 480 * 8, as required by ESP-IDF.
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 3840 /* bounce_buffer_size_px */
);

// Exposes the 480 x 272 panel through the common Arduino GFX interface.
Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
    480 /* width */, 272 /* height */, bus, 0 /* rotation */, true /* auto_flush */);

#endif

#include "touch.h"

/**
 * @brief Constrain a mapped touch coordinate to the visible display range.
 *
 * Calibration and electrical noise can produce values just outside the panel.
 * Clamping prevents LVGL from receiving invalid pointer coordinates.
 *
 * @param value Coordinate to constrain.
 * @param min_value Lowest permitted coordinate.
 * @param max_value Highest permitted coordinate.
 * @return value when it is already within range.
 * @return min_value or max_value when value exceeds the corresponding limit.
 * @note The touch callback calls this after mapping every raw sample.
 */
static int16_t clamp_touch_coord(int16_t value, int16_t min_value, int16_t max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

// Return the middle value of five samples.  XPT2046 occasionally produces a
// single outlier while a finger is stationary; rejecting that outlier before
// mapping to pixels prevents visible one-pixel jumps.
static int16_t median5(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e)
{
    int16_t v[5] = {a, b, c, d, e};
    for (int i = 1; i < 5; ++i) {
        int16_t key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            --j;
        }
        v[j + 1] = key;
    }
    return v[2];
}

/**
 * @brief Copy an LVGL render area to the physical RGB display.
 *
 * LVGL supplies a rectangular RGB565 pixel map. The callback forwards that
 * rectangle to Arduino GFX and then releases LVGL to render the next area.
 *
 * @param disp LVGL display that requested the transfer.
 * @param area Inclusive screen coordinates of the rendered rectangle.
 * @param px_map RGB565 pixel data for the rectangle.
 * @return Nothing.
 * @note LVGL calls this function whenever a dirty display area is ready.
 */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    lcd->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    lv_display_flush_ready(disp);
}

/**
 * @brief Read, calibrate, smooth, and report an LVGL pointer sample.
 *
 * A first-order filter gives the previous sample three times the weight of the
 * newest sample while a press continues. The last stable coordinate is retained
 * on release so LVGL can close the gesture at a consistent position.
 *
 * @param indev LVGL input device requesting a sample.
 * @param data Output structure that receives pointer state and coordinates.
 * @return Nothing.
 * @note LVGL calls this function while processing its input timers.
 */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static bool last_pressed = false;
    static int16_t stable_x = 0;
    static int16_t stable_y = 0;
    static uint8_t release_count = 0;

    ts.isrWake = true;
    if (ts.touched())
    {
        int16_t raw_x[5], raw_y[5];
        for (int i = 0; i < 5; ++i) {
            TS_Point p = ts.getPoint();
            raw_x[i] = p.x;
            raw_y[i] = p.y;
        }
        int16_t x = clamp_touch_coord(map(median5(raw_x[0], raw_x[1], raw_x[2], raw_x[3], raw_x[4]), 4000, 200, 0, 479), 0, 479);
        int16_t y = clamp_touch_coord(map(median5(raw_y[0], raw_y[1], raw_y[2], raw_y[3], raw_y[4]), 200, 3600, 0, 271), 0, 271);

        if (last_pressed) {
            // Ignore one-pixel movement, otherwise apply a low-pass filter.
            if (abs(x - stable_x) > 1) stable_x = (stable_x * 3 + x) / 4;
            if (abs(y - stable_y) > 1) stable_y = (stable_y * 3 + y) / 4;
        } else {
            stable_x = x;
            stable_y = y;
        }

        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = stable_x;
        data->point.y = stable_y;
        last_pressed = true;
        release_count = 0;
    }
    else
    {
        // Require two consecutive released samples to avoid a brief ADC/IRQ
        // dropout ending a gesture and making the pointer visibly flicker.
        if (last_pressed && release_count < 2) {
            ++release_count;
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
            last_pressed = false;
        }
        data->point.x = stable_x;
        data->point.y = stable_y;
    }
}

/**
 * @brief Initialize Arduino services, hardware drivers, LVGL, and the UI.
 *
 * Arduino runs as an ESP-IDF component, so initArduino() prepares its runtime
 * before Arduino APIs are used. The function allocates a 40-line RGB565 render
 * buffer from PSRAM, registers display and touch callbacks, and then services
 * LVGL continuously inside the application task.
 *
 * @param None
 * @return Nothing.
 * @note ESP-IDF calls this function once as the application entry point.
 */
extern "C" void app_main()
{
    initArduino();

    Serial.begin(115200);
    delay(100);
    Serial.println("app_main start");

    lv_init();
    lv_tick_set_cb(millis);

    /* Stop on display initialization failure because later drawing would have
     * no valid destination and could hide the original hardware fault. */
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

    /* Forty pixel rows balance PSRAM use and transfer overhead. Both capability
     * flags are required because LVGL accesses the buffer as normal byte data. */
    const size_t draw_buf_pixels = screenWidth * 40;
    lvgl_draw_buf = (lv_color_t *)heap_caps_malloc(draw_buf_pixels * sizeof(lv_color_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!lvgl_draw_buf) {
        Serial.println("draw buffer alloc failed");
        while (1) {
            delay(1000);
        }
    }
    lv_display_t *disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, lvgl_draw_buf, NULL,
                           draw_buf_pixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Build and display the screen exported by SquareLine Studio.
    ui_init();

    Serial.println("Setup done");

    while (1)
    {
        // Apply the state selected by the on-screen on and off buttons.
        if (led == 1) {
            digitalWrite(38, HIGH);
        } else {
            digitalWrite(38, LOW);
        }

        lv_timer_handler();
        delay(5);
    }
}

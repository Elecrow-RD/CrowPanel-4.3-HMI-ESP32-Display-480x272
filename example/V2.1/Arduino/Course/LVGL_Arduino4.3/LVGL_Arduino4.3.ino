#include <lvgl.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include "ui.h"

/*---------------------------------------------------------------
 * 4.3-inch HMI peripheral pin assignments
 * These constants document the board wiring shared by related examples.
 *--------------------------------------------------------------*/
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12
#define SD_CS   10

#define I2S_DOUT      20
#define I2S_BCLK      35
#define I2S_LRC       19
#define LED    38

#define TFT_BL 2
#define GFX_BL DF_GFX_BL

/*---------------------------------------------------------------
 * RGB display hardware
 * Arduino GFX generates the RGB timing signals for the NV3047 panel.
 *--------------------------------------------------------------*/
#if defined(DISPLAY_DEV_KIT)
// Provides the display object used by every drawing and LVGL flush operation.
Arduino_GFX *lcd = create_default_Arduino_GFX();
#else

// Stores the state requested by the on-screen LED buttons.
int led;

// Reuses the board's standard SPI controller for touch communication.
SPIClass& spi = SPI;

/* The 480 x 272 NV3047 panel uses a typical 9 MHz pixel clock. The porch
 * and pulse values below define where each active line and frame begins. */

// Generates RGB control, data, and timing signals for the LCD panel.
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  // RGB control pins.
  40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
  
  // RGB data pins.
  45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
  5  /* G0 */, 6  /* G1 */, 7  /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
  8  /* B0 */, 3  /* B1 */, 46 /* B2 */, 9  /* B3 */, 1  /* B4 */,
  
  // Timing parameters from the NV3047 panel requirements.
  0  /* hsync_polarity */, 8  /* hsync_front_porch */, 4  /* hsync_pulse_width */, 43 /* hsync_back_porch */,
  0  /* vsync_polarity */, 8  /* vsync_front_porch */, 4  /* vsync_pulse_width */, 12 /* vsync_back_porch */,
  
  // Pixel clock and buffer settings.
  1  /* pclk_active_neg */, 9000000 /* prefer_speed: 9 MHz */, false /* useBigEndian */,
  0  /* de_idle_high */, 0  /* pclk_idle_high */, 480 /* bounce_buffer_size_px */
);

// Exposes the 480 x 272 RGB panel through the common Arduino GFX interface.
Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
  480 /* width */, 272 /* height */, bus, 0 /* rotation */, false /* auto_flush */
);

#endif

#include "touch.h"

/*---------------------------------------------------------------
 * LVGL display state
 * The dimensions come from the initialized display at runtime.
 *--------------------------------------------------------------*/
// Stores the width reported by the display driver in pixels.
static uint32_t screenWidth;

// Stores the height reported by the display driver in pixels.
static uint32_t screenHeight;

// Holds one-eighth of a frame for LVGL partial rendering in RGB565 format.
static lv_color_t disp_draw_buf[480 * 272 / 8];

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
 * @brief Translate touch-controller state into an LVGL pointer sample.
 *
 * Pressed samples provide the latest calibrated coordinate. A missing signal
 * or a release is reported explicitly so LVGL can finish the interaction.
 *
 * @param indev LVGL input device requesting a sample.
 * @param data Output structure that receives pointer state and coordinates.
 * @return Nothing.
 * @note LVGL calls this function while processing its input timers.
 */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      Serial.print("Data x :");
      Serial.println(touch_last_x);
      Serial.print("Data y :");
      Serial.println(touch_last_y);
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

/**
 * @brief Initialize the display, touch controller, LVGL, and generated UI.
 *
 * The initialization order matters: hardware is made ready before LVGL
 * registers callbacks, and the SquareLine screen is created only after the
 * display and input devices exist.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting...");

  /* GPIO 38 is controlled by the UI buttons. GPIO 0 is asserted manually as
   * the XPT2046 chip-select output used by the touch driver. */
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(0, OUTPUT); // XPT2046 chip select.

  /* millis() supplies LVGL's millisecond time base, allowing timers and input
   * processing to advance without a separate tick interrupt. */
  lv_init();
  lv_tick_set_cb(millis);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("Backlight ON");
#endif

  Serial.println("Init display...");
  if (!lcd->begin())
  {
    Serial.println("lcd->begin() FAILED!");
    while (1) delay(100);
  }
  Serial.println("lcd->begin() OK");

  lcd->setTextSize(2);
  lcd->fillScreen(0);

  touch_init();

  screenWidth = lcd->width();
  screenHeight = lcd->height();
  Serial.print("Screen: ");
  Serial.print(screenWidth);
  Serial.print("x");
  Serial.println(screenHeight);

  /* Partial rendering limits static RAM use. LVGL sends completed areas to
   * my_disp_flush(), which copies them into the RGB frame buffer. */
  lv_display_t *disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, disp_draw_buf, NULL, sizeof(disp_draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  // Build and display the screen exported by SquareLine Studio.
  ui_init();
  lv_timer_handler();

  Serial.println("Setup done");
}

/**
 * @brief Service LVGL and apply the LED state selected on screen.
 *
 * The 16 ms delay targets approximately 60 iterations per second, close to
 * the panel's intended refresh cadence while leaving time for background work.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop()
{
  lv_timer_handler();

  if(led == 1)
    digitalWrite(LED, HIGH);
  if(led == 0)
    digitalWrite(LED, LOW);

  delay(16);  // Approximately 60 frames per second.
}

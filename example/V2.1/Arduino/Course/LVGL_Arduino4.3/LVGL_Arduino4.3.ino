#include <lvgl.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include "ui.h"

// 4.3-inch SD pins
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

#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *lcd = create_default_Arduino_GFX();
#else

int led;
SPIClass& spi = SPI;

// ========== NV3047 480x272 timing parameters ==========
// DCLK: typical 9MHz (range 8-12MHz)
// timing: hsync_fp=8, hsync_pw=4, hsync_bp=43
//       vsync_fp=8, vsync_pw=4, vsync_bp=12

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  // RGB control pins
  40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
  
  // RGB data pins
  45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
  5  /* G0 */, 6  /* G1 */, 7  /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
  8  /* B0 */, 3  /* B1 */, 46 /* B2 */, 9  /* B3 */, 1  /* B4 */,
  
  // timing parameters（consistent with NV3047 datasheet）
  0  /* hsync_polarity */, 8  /* hsync_front_porch */, 4  /* hsync_pulse_width */, 43 /* hsync_back_porch */,
  0  /* vsync_polarity */, 8  /* vsync_front_porch */, 4  /* vsync_pulse_width */, 12 /* vsync_back_porch */,
  
  // PCLK settings
  1  /* pclk_active_neg */, 9000000 /* prefer_speed: 9MHz（NV3047 typical value）*/, false /* useBigEndian */,
  0  /* de_idle_high */, 0  /* pclk_idle_high */, 480 /* bounce_buffer_size_px */
);

Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
  480 /* width */, 272 /* height */, bus, 0 /* rotation */, false /* auto_flush */
);

#endif

#include "touch.h"

static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf[480 * 272 / 8];
static lv_disp_drv_t disp_drv;

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
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
      Serial.print("Data x :");
      Serial.println(touch_last_x);
      Serial.print("Data y :");
      Serial.println(touch_last_y);
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting...");
  
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(0, OUTPUT); // TOUCH-CS

  lv_init();

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

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 8);

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
  lv_timer_handler();
  
  Serial.println("Setup done");
}

void loop()
{
  lv_timer_handler();
  
  if(led == 1)
    digitalWrite(LED, HIGH);
  if(led == 0)
    digitalWrite(LED, LOW);
  
  delay(16);  // approx 60fps，matching NV3047 typical frame rate
}
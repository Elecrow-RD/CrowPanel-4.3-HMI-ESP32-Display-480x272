/*******************************************************************************
 * Touch libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 ******************************************************************************/

#define TOUCH_XPT2046
#define TOUCH_XPT2046_SCK 12
#define TOUCH_XPT2046_MISO 13
#define TOUCH_XPT2046_MOSI 11
#define TOUCH_XPT2046_CS 0
#define TOUCH_XPT2046_INT 36
#define TOUCH_XPT2046_ROTATION 0
#define TOUCH_MAP_X1 4000
#define TOUCH_MAP_X2 200
#define TOUCH_MAP_Y1 200
#define TOUCH_MAP_Y2 3600

int touch_last_x = 0, touch_last_y = 0;

#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>

XPT2046_Touchscreen ts(TOUCH_XPT2046_CS, TOUCH_XPT2046_INT);

void touch_init()
{
  SPI.begin(TOUCH_XPT2046_SCK, TOUCH_XPT2046_MISO, TOUCH_XPT2046_MOSI, TOUCH_XPT2046_CS);
  ts.begin(SPI);
  ts.setRotation(TOUCH_XPT2046_ROTATION);
}

bool touch_has_signal()
{
  return true;
}

bool touch_touched()
{
  ts.isrWake = true;   // 强制轮询，ESP-IDF 下 RGB 屏运行时中断不可靠
  if (ts.touched())
  {
    TS_Point p = ts.getPoint();
    touch_last_x = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, lcd->width() - 1);
    touch_last_y = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, lcd->height() - 1);
    return true;
  }
  return false;
}

bool touch_released()
{
  return true;
}

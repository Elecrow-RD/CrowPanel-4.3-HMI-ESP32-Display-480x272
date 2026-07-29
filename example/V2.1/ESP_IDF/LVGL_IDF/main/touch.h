/*---------------------------------------------------------------
 * XPT2046 touch configuration
 * The SPI pins and calibration endpoints match the 4.3-inch HMI board.
 * Reference libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 *--------------------------------------------------------------*/

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

// Stores the most recent calibrated display coordinates in pixels.
int touch_last_x = 0, touch_last_y = 0;

#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>

// Provides polled SPI access to the XPT2046 controller.
XPT2046_Touchscreen ts(TOUCH_XPT2046_CS, TOUCH_XPT2046_INT);

/**
 * @brief Initialize the SPI bus and XPT2046 touch controller.
 *
 * The existing SPI object is supplied explicitly because this project uses
 * Arduino libraries from inside an ESP-IDF application.
 *
 * @param None
 * @return Nothing.
 * @note app_main() calls this function after the display starts successfully.
 */
void touch_init()
{
  SPI.begin(TOUCH_XPT2046_SCK, TOUCH_XPT2046_MISO, TOUCH_XPT2046_MOSI, TOUCH_XPT2046_CS);
  ts.begin(SPI);
  ts.setRotation(TOUCH_XPT2046_ROTATION);
}

/**
 * @brief Indicate that the controller is available for polling.
 * @param None
 * @return true because the ESP-IDF integration polls the touch controller.
 * @note Input code may call this before touch_touched().
 */
bool touch_has_signal()
{
  return true;
}

/**
 * @brief Poll the XPT2046 and update calibrated display coordinates.
 *
 * Forced wake mode avoids relying on the interrupt while the RGB display is
 * active. Raw samples are mapped to the display dimensions reported by lcd.
 *
 * @param None
 * @return true when the screen is currently pressed.
 * @return false when the controller reports no press.
 * @note The input callback polls this function while LVGL services input.
 */
bool touch_touched()
{
  ts.isrWake = true; // Polling is more reliable while the RGB panel is active.
  if (ts.touched())
  {
    TS_Point p = ts.getPoint();
    touch_last_x = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, lcd->width() - 1);
    touch_last_y = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, lcd->height() - 1);
    return true;
  }
  return false;
}

/**
 * @brief Report that a non-pressed sample may be treated as released.
 * @param None
 * @return true because pressed state is determined by touch_touched().
 * @note Input code may call this after a sample reports no active press.
 */
bool touch_released()
{
  return true;
}

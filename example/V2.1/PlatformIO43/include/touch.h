/*******************************************************************************
 * Touch libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 ******************************************************************************/

/* uncomment for FT6X36 */
// #define TOUCH_FT6X36
// #define TOUCH_FT6X36_SCL 19
// #define TOUCH_FT6X36_SDA 18
// #define TOUCH_FT6X36_INT 39
// #define TOUCH_SWAP_XY
// #define TOUCH_MAP_X1 480
// #define TOUCH_MAP_X2 0
// #define TOUCH_MAP_Y1 0
// #define TOUCH_MAP_Y2 320

/* uncomment for GT911 */
// #define TOUCH_GT911
// #define TOUCH_GT911_SCL 20
// #define TOUCH_GT911_SDA 19
// #define TOUCH_GT911_INT -1
// #define TOUCH_GT911_RST 0
// #define TOUCH_GT911_ROTATION ROTATION_NORMAL
// #define TOUCH_MAP_X1 800
// #define TOUCH_MAP_X2 0
// #define TOUCH_MAP_Y1 480
// #define TOUCH_MAP_Y2 0

/* The 4.3-inch HMI uses the XPT2046 configuration below. */
 #define TOUCH_XPT2046
 #define TOUCH_XPT2046_SCK 12
 #define TOUCH_XPT2046_MISO 13
 #define TOUCH_XPT2046_MOSI 11
 #define TOUCH_XPT2046_CS 0
 #define TOUCH_XPT2046_INT 36
 #define TOUCH_XPT2046_ROTATION 0
 #define TOUCH_MAP_X1 4000
 #define TOUCH_MAP_X2 100
 #define TOUCH_MAP_Y1 100
 #define TOUCH_MAP_Y2 4000

// Stores the most recent calibrated screen coordinates in pixels.
int touch_last_x = 0, touch_last_y = 0;

#if defined(TOUCH_FT6X36)
#include <Wire.h>
#include <FT6X36.h>
FT6X36 ts(&Wire, TOUCH_FT6X36_INT);
bool touch_touched_flag = true, touch_released_flag = true;

#elif defined(TOUCH_GT911)
#include <Wire.h>
#include <TAMC_GT911.h>
TAMC_GT911 ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

#elif defined(TOUCH_XPT2046)
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
// Provides interrupt-assisted SPI access to the XPT2046 controller.
XPT2046_Touchscreen ts(TOUCH_XPT2046_CS, TOUCH_XPT2046_INT);

#endif

#if defined(TOUCH_FT6X36)
/**
 * @brief Convert an FT6X36 event into calibrated display coordinates.
 *
 * Only tap and drag events affect the pointer state. Axis selection follows
 * the configured screen orientation before coordinates are mapped to pixels.
 *
 * @param p Raw touch point supplied by the FT6X36 library.
 * @param e Touch event describing the current gesture phase.
 * @return None.
 * @note The FT6X36 library calls this handler while touch_has_signal() runs.
 */
void touch(TPoint p, TEvent e)
{
  if (e != TEvent::Tap && e != TEvent::DragStart && e != TEvent::DragMove && e != TEvent::DragEnd)
  {
    return;
  }
  // Select the source axis that corresponds to each display axis.
#if defined(TOUCH_SWAP_XY)
  touch_last_x = map(p.y, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 430);
  touch_last_y = map(p.x, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 272);
#else
  touch_last_x = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 430);
  touch_last_y = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 272);
#endif
  switch (e)
  {
  case TEvent::Tap:
    Serial.println("Tap");
    touch_touched_flag = true;
    touch_released_flag = true;
    break;
  case TEvent::DragStart:
    Serial.println("DragStart");
    touch_touched_flag = true;
    break;
  case TEvent::DragMove:
    Serial.println("DragMove");
    touch_touched_flag = true;
    break;
  case TEvent::DragEnd:
    Serial.println("DragEnd");
    touch_released_flag = true;
    break;
  default:
    Serial.println("UNKNOWN");
    break;
  }
}
#endif

/**
 * @brief Initialize the selected touch controller and communication bus.
 *
 * Each controller branch configures its required I2C or SPI bus before
 * setting event handling or rotation.
 *
 * @param None.
 * @return None.
 * @note setup() calls this function after the display hardware is ready.
 */
void touch_init()
{
#if defined(TOUCH_FT6X36)
  Wire.begin(TOUCH_FT6X36_SDA, TOUCH_FT6X36_SCL);
  ts.begin();
  ts.registerTouchHandler(touch);

#elif defined(TOUCH_GT911)
  Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
  ts.begin();
  ts.setRotation(TOUCH_GT911_ROTATION);

#elif defined(TOUCH_XPT2046)
  SPI.begin(TOUCH_XPT2046_SCK, TOUCH_XPT2046_MISO, TOUCH_XPT2046_MOSI, TOUCH_XPT2046_CS);
  ts.begin();
  ts.setRotation(TOUCH_XPT2046_ROTATION);

#endif
}

/**
 * @brief Check whether the selected controller may have new touch data.
 *
 * Event-driven controllers update pending flags here; XPT2046 uses its
 * interrupt indication to avoid an SPI read when no touch is present.
 *
 * @param None.
 * @return true when touch_touched() or touch_released() should be checked.
 * @return false when no input event is available.
 * @note The LVGL input callback calls this before reading touch state.
 */
bool touch_has_signal()
{
#if defined(TOUCH_FT6X36)
  ts.loop();
  return touch_touched_flag || touch_released_flag;

#elif defined(TOUCH_GT911)
  return true;

#elif defined(TOUCH_XPT2046)
  return ts.tirqTouched();

#else
  return false;
#endif
}

/**
 * @brief Read a press and map raw controller coordinates to screen pixels.
 *
 * Reversing a calibration endpoint pair also reverses that axis when required
 * by the panel mounting orientation.
 *
 * @param None.
 * @return true when the screen is currently pressed.
 * @return false when no press is active.
 * @note Call this after touch_has_signal() reports available activity.
 */
bool touch_touched()
{
#if defined(TOUCH_FT6X36)
  if (touch_touched_flag)
  {
    touch_touched_flag = false;
    return true;
  }
  else
  {
    return false;
  }

#elif defined(TOUCH_GT911)
  ts.read();
  if (ts.isTouched)
  {
#if defined(TOUCH_SWAP_XY)
    // Swap source axes when the touch controller is mounted rotated.
    touch_last_x = map(ts.points[0].y, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 430 - 1);
    touch_last_y = map(ts.points[0].x, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 272 - 1);
#else
    // Map the raw GT911 coordinates into the 4.3-inch screen coordinates.
    touch_last_x = map(ts.points[0].x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 430 - 1);
    touch_last_y = map(ts.points[0].y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 272 - 1);
#endif
    return true;
  }
  else
  {
    return false;
  }

#elif defined(TOUCH_XPT2046)
  if (ts.touched())
  {
    TS_Point p = ts.getPoint();
#if defined(TOUCH_SWAP_XY)
    // Swap source axes when the touch controller is mounted rotated.
    touch_last_x = map(p.y, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 430 - 1);
    touch_last_y = map(p.x, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 272 - 1);
#else
    // Map the raw XPT2046 ADC range into the 4.3-inch screen coordinates.
    touch_last_x = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, 430 - 1);
    touch_last_y = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, 272 - 1);
#endif
    return true;
  }
  else
  {
    return false;
  }

#else
  return false;
#endif
}

/**
 * @brief Report whether the current touch interaction has been released.
 *
 * @param None.
 * @return true when LVGL can report the pointer as released.
 * @return false when no FT6X36 release event is pending.
 * @note The LVGL input callback checks this after a signal without a press.
 */
bool touch_released()
{
#if defined(TOUCH_FT6X36)
  if (touch_released_flag)
  {
    touch_released_flag = false;
    return true;
  }
  else
  {
    return false;
  }

#elif defined(TOUCH_GT911)
  return true;

#elif defined(TOUCH_XPT2046)
  return true;

#else
  return false;
#endif
}

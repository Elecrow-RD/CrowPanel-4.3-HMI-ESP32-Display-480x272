#include "touch.h"

/**
 * @brief Start serial logging and initialize the touch controller.
 *
 * The controller must be initialized before the loop can request samples.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup() {
  Serial.begin( 115200 ); 
  touch_init();
}

/**
 * @brief Print the latest mapped coordinates while the screen is touched.
 *
 * A signal check avoids unnecessary controller reads when the interrupt line
 * indicates that no touch activity is available.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop() {
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      Serial.print( "Data x :" );
      Serial.println( touch_last_x );

      Serial.print( "Data y :" );
      Serial.println( touch_last_y );
    }
  }
}

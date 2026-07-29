/*---------------------------------------------------------------
 * LED hardware configuration
 * GPIO 38 drives the indicator LED used by this example.
 *--------------------------------------------------------------*/
#define D_PIN 38

/**
 * @brief Configure the serial port and indicator LED.
 *
 * The pin is configured as an output before the loop starts toggling it.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup() {
  Serial.begin(115200);
  pinMode(D_PIN, OUTPUT);
}

/**
 * @brief Blink the indicator LED at a one-second period.
 *
 * Each output state is held for 500 ms, producing equal on and off times.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop() {
  digitalWrite(D_PIN, HIGH);
  delay(500);
  digitalWrite(D_PIN, LOW);
  delay(500);
}

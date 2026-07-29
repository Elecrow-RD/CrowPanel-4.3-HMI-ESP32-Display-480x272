/*---------------------------------------------------------------
 * GPS serial bridge configuration
 * UART1 receives NMEA data from the Crowtail GPS module.
 *--------------------------------------------------------------*/
#define SERIAL_BAUD 9600

// Provides the UART1 connection to the GPS module.
HardwareSerial cardSerial(1);

// Temporarily stores one batch of bytes received from the GPS module.
unsigned char buffer[256];

// Tracks the number of valid bytes currently stored in buffer.
int count_1 = 0;

/**
 * @brief Clear the portion of the receive buffer used by the last batch.
 *
 * @param None
 * @return Nothing.
 * @note loop() calls this function after forwarding a GPS data batch to USB.
 */
void clearBufferArray()
{
  for (int i = 0; i < count_1; i++)
  {
    buffer[i] = 0;
  }
}

/**
 * @brief Configure USB serial and the GPS UART connection.
 *
 * UART1 uses GPIO 18 for RX and GPIO 17 for TX at the module's 9600-baud
 * default rate.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup() {
  Serial.begin( 115200 );
  cardSerial.begin(SERIAL_BAUD, SERIAL_8N1, 18, 17);
}

/**
 * @brief Forward data in both directions between the GPS module and USB.
 *
 * GPS bytes are collected in bounded batches before being written to the
 * serial monitor. Bytes entered in the monitor are sent back to the module.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop() {
  if (cardSerial.available())
  {
    while (cardSerial.available())
    {
      buffer[count_1++] = cardSerial.read();
      if (count_1 == 256) break;
    }
    Serial.write(buffer, count_1);
    clearBufferArray();
    count_1 = 0;
  }
  if (Serial.available())
    cardSerial.write(Serial.read());
}

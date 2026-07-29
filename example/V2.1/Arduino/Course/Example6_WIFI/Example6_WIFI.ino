#include <WiFi.h>

// Identifies the Wi-Fi network used by this public example.
const char *ssid = "elecrow888";

// Stores the password supplied when the station joins the network.
const char *password = "elecrow2014";

/**
 * @brief Connect the ESP32-S3 to Wi-Fi and print its assigned IP address.
 *
 * Setup waits until the station is connected so the final address is valid.
 * Automatic reconnection lets the station recover after a brief outage.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("connecting");
  }
  Serial.println("WiFi is connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // WiFi.disconnect();
}

/**
 * @brief Keep the sketch available for future network work.
 *
 * Wi-Fi maintenance runs in the background, so this example needs no polling.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop() {
}

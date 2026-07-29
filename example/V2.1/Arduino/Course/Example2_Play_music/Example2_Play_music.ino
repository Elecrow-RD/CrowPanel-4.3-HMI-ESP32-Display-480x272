#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "Audio.h"

// Maintains the MP3 decoder and I2S audio output state.
Audio audio;

/*---------------------------------------------------------------
 * Audio and SD card pin assignments
 * These GPIO connections match the 4.3-inch HMI board wiring.
 *--------------------------------------------------------------*/
#define I2S_DOUT      20
#define I2S_BCLK      35
#define I2S_LRC       19

#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12
#define SD_CS 10

/**
 * @brief Prepare the SD card and start MP3 playback over I2S.
 *
 * The example expects a file named 123.mp3 in the SD card root directory.
 * GPIO 2 is driven high after the audio path has been configured.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup() {
  Serial.begin( 9600 ); 
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  SPI.setFrequency(1000000);
  SD.begin(SD_CS);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // The library accepts volume levels from 0 to 21.
  audio.connecttoFS(SD, "/123.mp3");
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
}

/**
 * @brief Keep the MP3 decoder supplied with audio data.
 *
 * audio.loop() must run continuously or playback will pause and the decoder
 * buffers may underrun.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop() {
  audio.loop();
  // audio.stopSong(); // Enable this call when the lesson needs to stop playback.
}

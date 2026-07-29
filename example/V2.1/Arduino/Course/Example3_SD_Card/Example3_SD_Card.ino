#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

/*---------------------------------------------------------------
 * SD card SPI pin assignments
 * These GPIO connections match the 4.3-inch HMI board wiring.
 *--------------------------------------------------------------*/
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12
#define SD_CS 10

/**
 * @brief Initialize the serial port, SPI bus, and SD card.
 *
 * The result printed here lets the learner distinguish a successful mount
 * from a missing card or an incorrect SPI connection.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup() {
  Serial.begin(115200); 
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  delay(100);
  if (SD_init() == 1)
  {
    Serial.println("Card Mount Failed");
  }
  else
    Serial.println("Initialize SD Card successfully");
}

/**
 * @brief Leave the processor idle after the one-time SD card inspection.
 *
 * All observable work is performed by setup(), so no repeated action is
 * required for this example.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop() {
}

/**
 * @brief Mount the SD card and print its capacity and directory contents.
 *
 * @param None
 * @return 0 when the card is mounted and inspected successfully.
 * @return 1 when the card cannot be mounted or no card is detected.
 * @note setup() calls this function once after the SPI bus is ready.
 */
int SD_init()
{
  if (!SD.begin(SD_CS))
  {
    Serial.println("Card Mount Failed");
    return 1;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No TF card attached");
    return 1;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("TF Card Size: %lluMB\n", cardSize);
  listDir(SD, "/", 2);

  return 0;
}

/**
 * @brief Print files in a directory and optionally visit subdirectories.
 *
 * The levels argument limits recursion so a deeply nested card cannot keep
 * the lesson occupied indefinitely.
 *
 * @param fs File system that contains the directory.
 * @param dirname Directory path to inspect.
 * @param levels Maximum remaining subdirectory depth.
 * @return Nothing.
 * @note SD_init() calls this function after a successful card mount.
 */
void listDir(fs::FS & fs, const char *dirname, uint8_t levels)
{
  // Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    // Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {

      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("FILE: ");
      Serial.print(file.name());

      Serial.print("SIZE: ");
      Serial.println(file.size());
    }

    file = root.openNextFile();
  }
}

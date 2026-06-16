#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "Audio.h"

Audio audio;

// ========== 修改后的引脚定义 ==========
#define I2S_DOUT      20
#define I2S_BCLK      35   
#define I2S_LRC       19

#define SD_MOSI       11
#define SD_MISO       13
#define SD_SCK        12
#define SD_CS         10

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }
    delay(500);
    
    Serial.println("\n========================================");
    Serial.println("ESP32-S3 Audio Test - Fixed Version");
    Serial.println("========================================");
    
    // SD 卡初始化
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    SPI.setFrequency(1000000);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("ERROR: SD Card Mount Failed!");
        return;
    }
    Serial.println("SD Card OK");
    
    if (!SD.exists("/123.mp3")) {
        Serial.println("ERROR: /123.mp3 not found!");
        return;
    }
    Serial.println("File found");
    
    // I2S 音频初始化
    Serial.printf("BCLK -> GPIO %d\n", I2S_BCLK);
    Serial.printf("LRC  -> GPIO %d\n", I2S_LRC);
    Serial.printf("DOUT -> GPIO %d\n", I2S_DOUT);
    
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(21);
    audio.connecttoFS(SD, "/123.mp3");
    
    Serial.println("Audio started!");
}

void loop() {
    audio.loop();
}

// 调试回调
void audio_info(const char *info) {
    Serial.print("[INFO] "); Serial.println(info);
}
void audio_id3data(const char *info) {
    Serial.print("[ID3]  "); Serial.println(info);
}
void audio_eof_mp3(const char *info) {
    Serial.print("[EOF]  "); Serial.println(info);
}
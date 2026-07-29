#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include <BLECharacteristic.h>

/*---------------------------------------------------------------
 * BLE server objects and identifiers
 * The pointers retain access to the objects created during setup().
 *--------------------------------------------------------------*/
BLEAdvertising* pAdvertising = NULL;
BLEServer* pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic* pCharacteristic = NULL;

#define bleServerName "ESP32SPI-BLE"
#define SERVICE_UUID "6479571c-2e6d-4b34-abe9-c35116712345"
#define CHARACTERISTIC_UUID "826f072d-f87c-4ae6-a416-6ffdcaa02d73"

// Records whether a central device currently has an active connection.
bool connected_state = false;

/*---------------------------------------------------------------
 * BLE connection callbacks
 * The server updates shared connection state when a central connects.
 *--------------------------------------------------------------*/
class MyServerCallbacks: public BLEServerCallbacks
{
    /**
     * @brief Record that a BLE central has connected.
     * @param pServer Server that accepted the connection.
     * @return Nothing.
     * @note The BLE stack calls this callback after a connection is established.
     */
    void onConnect(BLEServer *pServer)
    {
      connected_state = true;
    }

    /**
     * @brief Record that the BLE central has disconnected.
     * @param pServer Server whose connection was closed.
     * @return Nothing.
     * @note The BLE stack calls this callback after a disconnection.
     */
    void onDisconnect(BLEServer *pServer)
    {
      connected_state = false;
    }
};

/**
 * @brief Create a readable, writable, and notifiable BLE service.
 *
 * Advertising includes the service UUID so a phone or BLE scanner can find
 * the board and read the initial characteristic value.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function once after startup or reset.
 */
void setup() {
  Serial.begin(115200);
  BLEDevice::init(bleServerName);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->setValue("ELECROW");

  /* Start advertising only after the service and characteristic have been
   * configured, ensuring that scanners see a complete GATT definition. */
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  pService->start();
  // pAdvertising->stop();
  // pService->stop();
}

/**
 * @brief Leave BLE processing to the ESP32 BLE stack.
 *
 * No polling is required because connection changes are delivered through
 * MyServerCallbacks.
 *
 * @param None
 * @return Nothing.
 * @note Arduino calls this function repeatedly after setup() finishes.
 */
void loop() {
}

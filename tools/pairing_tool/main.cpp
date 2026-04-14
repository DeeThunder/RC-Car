#include <Arduino.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>

void setup() {
  Serial.begin(115200);
  
  // Initialize Bluetooth controller
  if (!btStart()) {
    Serial.println("Failed to start controller");
    return;
  }

  // Initialize Bluedroid stack (required to get address)
  if (esp_bluedroid_init() != ESP_OK) {
    Serial.println("Failed to init bluedroid");
    return;
  }
  if (esp_bluedroid_enable() != ESP_OK) {
    Serial.println("Failed to enable bluedroid");
    return;
  }

  const uint8_t* address = esp_bt_dev_get_address();
  if (address == NULL) {
    Serial.println("Error: Bluetooth address is NULL!");
    return;
  }

  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
          address[0], address[1], address[2], address[3], address[4], address[5]);
  
  Serial.println("\n--- PAIRING INFO ---");
  Serial.print("Your ESP32 Bluetooth MAC: ");
  Serial.println(macStr);
  Serial.println("--------------------\n");
}

void loop() {
  // Do nothing
}

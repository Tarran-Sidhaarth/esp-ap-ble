#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// WiFi AP Configuration
const char* AP_SSID = "ESP32_Network";
const char* AP_PASSWORD = "password123";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// Global BLE objects
BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic *pCharacteristic = NULL;

// Buffer for serial data
String serialBuffer = "";

// Callback for Characteristic
class CharacteristicCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue();
        
        if (value.length() > 0) {
            String receivedMessage = String(value.c_str());
            Serial.println(receivedMessage);
        }
    }
};

// WiFi AP Task for Core 0
void wifiApTask(void * pvParameters) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    
    if(WiFi.softAP(AP_SSID, AP_PASSWORD)) {
        for(;;) {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    } else {
        vTaskDelete(NULL);
    }
}

// BLE Task for Core 1
void bleTask(void * pvParameters) {
    BLEDevice::init("MyESP32");
    
    pServer = BLEDevice::createServer();
    
    pService = pServer->createService(SERVICE_UUID);
    
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );

    pCharacteristic->setValue("Ready");
    
    pCharacteristic->setCallbacks(new CharacteristicCallback());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    for(;;) {
        // Check for serial data
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') {
                // End of line, update BLE characteristic
                if (serialBuffer.length() > 0) {
                    pCharacteristic->setValue(serialBuffer.c_str());
                    serialBuffer = "";
                }
            } else {
                // Add character to buffer
                serialBuffer += c;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    
    xTaskCreatePinnedToCore(
        wifiApTask,
        "WiFi AP Task",
        10000,
        NULL,
        1,
        NULL,
        0
    );
    
    xTaskCreatePinnedToCore(
        bleTask,
        "BLE Task",
        10000,
        NULL,
        1,
        NULL,
        1
    );
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}

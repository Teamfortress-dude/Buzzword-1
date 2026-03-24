#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Servo.h>
#include <esp_now.h>
#include <WiFi.h>

// Pin definitions for ESP32-C3 SuperMini
#define SDA_PIN     5
#define SCL_PIN     6
#define SERVO_PIN   10

// ================== CONFIGURATION ==================
const float TARGET_ALTITUDE   = 100.0;   // Meters
const int   SAFE_ANGLE        = 0;       
const int   EJECT_ANGLE       = 90;      
// ===================================================

Adafruit_BMP280 bmp;
Servo ejectionServo;

typedef struct message_struct {
  char message[32];
} message_struct;

message_struct incomingData;

float groundPressure = 0.0;
bool  armed = false;
bool  ejected = false;
bool  takeoffReceived = false;

void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  if (strcmp(incomingData.message, "TAKEOFF") == 0 && !takeoffReceived) {
    takeoffReceived = true;
    Serial.println("TAKEOFF SIGNAL RECEIVED - ARMING SYSTEM!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // MAC
  WiFi.mode(WIFI_STA);
  Serial.print("ESP32-C3 MAC Address: ");
  Serial.println(WiFi.macAddress());

  // ESP-NOW setup
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while(1) delay(10);
  }
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("ESP-NOW receiver ready - waiting for takeoff signal");


  ejectionServo.attach(SERVO_PIN);
  ejectionServo.write(SAFE_ANGLE);

  
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bmp.begin(0x76)) {               
    Serial.println("BMP280 not found!");
    while(1) delay(10);
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  Serial.println("BMP280 ready - WAITING FOR TAKEOFF SIGNAL...");

  
  while (!takeoffReceived) {
    Serial.println("Waiting for takeoff signal from igniter board...");
    delay(500);
  }


  groundPressure = bmp.readPressure() / 100.0F;
  armed = true;

  Serial.println("=== SYSTEM ARMED AT LAUNCH ===");
  Serial.print("Target ejection altitude: ");
  Serial.print(TARGET_ALTITUDE);
  Serial.println(" m");
}

void loop() {
  if (!armed || ejected) {
    delay(100);
    return;
  }

  float altitude = bmp.readAltitude(groundPressure);

  Serial.print("Altitude: ");
  Serial.print(altitude, 1);
  Serial.println(" m");

  if (altitude >= TARGET_ALTITUDE) {
    Serial.println("*** EJECTION TRIGGERED ***");
    ejectionServo.write(EJECT_ANGLE);
    ejected = true;
    Serial.println("Servo moved to ejection position.");
  }

  delay(100);   
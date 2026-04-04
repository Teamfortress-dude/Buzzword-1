// ============================================================
//  CO2 EJECTION SYSTEM — FLIGHT CODE
//  ESP32-C3 Super Mini
//
//  FIXES FROM ORIGINAL:
//    1. Changed Servo.h -> ESP32Servo.h (compatible with ESP32 core 3.x)
//    2. Corrected servo angles: ARMED = 90deg, EJECT = 0deg
//    3. Added 8-second timed gate — altitude must hold for full
//       gate duration before servo fires
//    4. Ground pressure captured BEFORE waiting for TAKEOFF signal
//       so baseline is always taken at ground level
//    5. Fixed missing closing brace on loop()
//    6. Target altitude corrected to 450m
// ============================================================

#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <ESP32Servo.h>
#include <esp_now.h>
#include <WiFi.h>

// ── Pin definitions ──────────────────────────────────────────
#define SDA_PIN    8
#define SCL_PIN    9
#define SERVO_PIN  10

// ── Configuration ────────────────────────────────────────────
const float TARGET_ALTITUDE    = 450.0;   // metres — ejection altitude
const int   ARMED_ANGLE        = 90;      // servo position while armed
const int   EJECT_ANGLE        = 0;       // servo position on ejection
const unsigned long GATE_MS    = 8000;    // timed gate — ms above altitude before fire

// ── Objects ──────────────────────────────────────────────────
Adafruit_BMP280 bmp;
Servo ejectionServo;

typedef struct message_struct {
  char message[32];
} message_struct;

message_struct incomingData;

// ── State ────────────────────────────────────────────────────
float groundPressure     = 0.0;
bool  armed              = false;
bool  ejected            = false;
bool  takeoffReceived    = false;
bool  gateOpen           = false;
unsigned long gateStart  = 0;

// ── ESP-NOW callback ─────────────────────────────────────────
void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  if (strcmp(incomingData.message, "TAKEOFF") == 0 && !takeoffReceived) {
    takeoffReceived = true;
    Serial.println("TAKEOFF SIGNAL RECEIVED - ARMING SYSTEM!");
  }
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(100);

  // ── Print MAC address ────────────────────────────────────
  WiFi.mode(WIFI_STA);
  Serial.print("ESP32-C3 MAC Address: ");
  Serial.println(WiFi.macAddress());

  // ── Servo init ───────────────────────────────────────────
  ejectionServo.attach(SERVO_PIN);
  ejectionServo.write(ARMED_ANGLE);
  Serial.print("Servo initialised at ");
  Serial.print(ARMED_ANGLE);
  Serial.println("deg (armed)");

  // ── BMP280 init ──────────────────────────────────────────
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found! Check wiring.");
    while (1) delay(10);
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  // ── Capture ground pressure BEFORE waiting for signal ────
  // Average 10 readings for a stable baseline
  Serial.println("Capturing ground pressure baseline...");
  float pressureSum = 0;
  for (int i = 0; i < 10; i++) {
    pressureSum += bmp.readPressure() / 100.0F;
    delay(100);
  }
  groundPressure = pressureSum / 10.0;
  Serial.print("Ground pressure locked: ");
  Serial.print(groundPressure, 2);
  Serial.println(" hPa");

  // ── ESP-NOW init ─────────────────────────────────────────
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (1) delay(10);
  }
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("ESP-NOW ready - waiting for TAKEOFF signal...");

  // ── Wait for TAKEOFF signal from igniter board ───────────
  while (!takeoffReceived) {
    Serial.println("Waiting for takeoff signal...");
    delay(500);
  }

  armed = true;

  Serial.println("=== SYSTEM ARMED AT LAUNCH ===");
  Serial.print("Target ejection altitude : ");
  Serial.print(TARGET_ALTITUDE);
  Serial.println(" m");
  Serial.print("Timed gate               : ");
  Serial.print(GATE_MS / 1000);
  Serial.println(" s");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  if (!armed || ejected) {
    delay(100);
    return;
  }

  float altitude = bmp.readAltitude(groundPressure);

  Serial.print("Altitude: ");
  Serial.print(altitude, 1);
  Serial.print(" m");

  // ── Altitude threshold check ──────────────────────────────
  if (altitude >= TARGET_ALTITUDE) {

    if (!gateOpen) {
      // First time crossing threshold — open the gate
      gateOpen  = true;
      gateStart = millis();
      Serial.print("  << GATE OPENED >>");
    }

    unsigned long elapsed   = millis() - gateStart;
    unsigned long remaining = (elapsed >= GATE_MS) ? 0 : (GATE_MS - elapsed);
    Serial.print("  Gate: ");
    Serial.print(remaining / 1000.0, 1);
    Serial.print("s remaining");

    // ── Fire if gate has passed ───────────────────────────
    if (elapsed >= GATE_MS) {
      Serial.println();
      Serial.println("*** EJECTION TRIGGERED — GATE PASSED ***");
      ejectionServo.write(EJECT_ANGLE);
      ejected = true;
      Serial.print("Servo moved to ");
      Serial.print(EJECT_ANGLE);
      Serial.println("deg — Ejection complete.");
      return;
    }

  } else {
    // Dropped back below threshold — reset gate
    if (gateOpen) {
      Serial.print("  << GATE RESET — altitude lost >>");
      gateOpen = false;
      gateStart = 0;
    }
  }

  Serial.println();
  delay(100);   // 10 Hz loop
}

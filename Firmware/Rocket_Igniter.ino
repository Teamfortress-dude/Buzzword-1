// ============================================================
//  ROCKET IGNITER — FLIGHT CODE
//  Send TAKEOFF via ESP-NOW to ejection board, then fire relay
//
//  FIXES FROM ORIGINAL:
//    1. sendTakeoff() is now called BEFORE digitalWrite(RELAY_PIN, HIGH)
//       so the ejection board locks its ground pressure baseline
//       before the rocket moves
//    2. Small delay added after TAKEOFF signal to give ejection
//       board time to process before ignition
//    3. Both launch paths (countdown + instant) updated consistently
// ============================================================

#include <BluetoothSerial.h>
#include <esp_now.h>
#include <WiFi.h>

// ── Configuration ─────────────────────────────────────────────
const int RELAY_PIN     = 5;   // Relay for nichrome wire
const int BUZZER_PIN    = 4;   // Passive buzzer
const int LAUNCH_BUTTON = 0;   // Boot button

const char* bluetoothName = "RocketIgniter";

// !! IMPORTANT: Replace with the actual MAC address of your
//    ejection board ESP32-C3. Flash the ejection code first,
//    read the MAC from Serial Monitor on boot, then fill in here.
uint8_t c3MacAddress[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};

// ── Objects & state ───────────────────────────────────────────
BluetoothSerial SerialBT;

typedef struct message_struct {
  char message[32];
} message_struct;

message_struct outgoingData;

bool countdownActive  = false;
bool launchTriggered  = false;
unsigned long countdownStart = 0;

// ── Send TAKEOFF signal via ESP-NOW ───────────────────────────
void sendTakeoff() {
  strcpy(outgoingData.message, "TAKEOFF");
  esp_now_send(c3MacAddress, (uint8_t*)&outgoingData, sizeof(outgoingData));
  Serial.println("TAKEOFF sent to ejection board");
  SerialBT.println("TAKEOFF sent to ejection board");
}

// ── Fire sequence: signal first, THEN ignite ─────────────────
void fireIgniter() {
  // 1. Tell ejection board we are launching (locks ground pressure)
  sendTakeoff();
  delay(200);   // brief pause — gives C3 time to receive and arm

  // 2. Fire the nichrome relay
  Serial.println("Firing igniter...");
  SerialBT.println("Firing igniter...");
  digitalWrite(RELAY_PIN, HIGH);
  delay(1000);
  digitalWrite(RELAY_PIN, LOW);

  launchTriggered = true;
  Serial.println("Igniter fired. Launch complete.");
  SerialBT.println("Igniter fired. Launch complete.");
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  pinMode(LAUNCH_BUTTON, INPUT_PULLUP);

  // Bluetooth
  SerialBT.begin(bluetoothName);
  Serial.println("\nBluetooth started! Device: " + String(bluetoothName));
  Serial.println("Commands: START  |  CANCEL");

  // ESP-NOW
  WiFi.mode(WIFI_STA);
  Serial.print("Igniter MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (1) delay(10);
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, c3MacAddress, 6);
  esp_now_add_peer(&peerInfo);

  Serial.println("=== IGNITER READY ===");
}

// ─────────────────────────────────────────────────────────────
void loop() {

  // ── Read command from USB Serial or Bluetooth ─────────────
  String cmd = "";
  if (Serial.available()) {
    cmd = Serial.readStringUntil('\n');
  } else if (SerialBT.available()) {
    cmd = SerialBT.readStringUntil('\n');
  }

  if (cmd.length() > 0) {
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "START" && !countdownActive && !launchTriggered) {
      countdownActive = true;
      countdownStart  = millis();
      Serial.println("10s COUNTDOWN STARTED");
      SerialBT.println("10s COUNTDOWN STARTED");
    }
    else if (cmd == "CANCEL" && countdownActive) {
      countdownActive = false;
      noTone(BUZZER_PIN);
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("COUNTDOWN CANCELLED");
      SerialBT.println("COUNTDOWN CANCELLED");
    }
  }

  // ── Countdown logic ───────────────────────────────────────
  if (countdownActive) {
    unsigned long elapsed = (millis() - countdownStart) / 1000;

    if (elapsed < 10) {
      int remaining = 10 - (int)elapsed;
      Serial.print("Countdown: ");
      Serial.println(remaining);
      SerialBT.print("Countdown: ");
      SerialBT.println(remaining);

      tone(BUZZER_PIN, 1200, 300);
      delay(400);

    } else {
      // Countdown complete
      Serial.println("*** LAUNCHING ***");
      SerialBT.println("*** LAUNCHING ***");
      noTone(BUZZER_PIN);
      countdownActive = false;
      fireIgniter();
    }
  }

  // ── Physical button — instant launch ─────────────────────
  if (digitalRead(LAUNCH_BUTTON) == LOW && !launchTriggered && !countdownActive) {
    delay(50);   // debounce
    if (digitalRead(LAUNCH_BUTTON) == LOW) {
      Serial.println("*** INSTANT LAUNCH ***");
      SerialBT.println("*** INSTANT LAUNCH ***");
      fireIgniter();
    }
  }

  delay(10);
}

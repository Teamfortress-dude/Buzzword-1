

#include <BluetoothSerial.h>
#include <esp_now.h>
#include <WiFi.h>

// ================== CONFIGURATION ==================
const int RELAY_PIN     = 5;      // Relay for nichrome
const int BUZZER_PIN    = 4;      // Passive buzzer (change if needed)
const int LAUNCH_BUTTON = 0;      // Boot button


const char* bluetoothName = "RocketIgniter";


uint8_t c3MacAddress[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
// ===================================================

BluetoothSerial SerialBT;
typedef struct message_struct {
  char message[32];
} message_struct;

message_struct outgoingData;

bool countdownActive = false;
bool launchTriggered = false;
unsigned long countdownStart = 0;

void sendTakeoff() {
  strcpy(outgoingData.message, "TAKEOFF");
  esp_now_send(c3MacAddress, (uint8_t*)&outgoingData, sizeof(outgoingData));
  Serial.println("TAKEOFF sent to C3");
  SerialBT.println("TAKEOFF sent to C3");   
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  pinMode(LAUNCH_BUTTON, INPUT_PULLUP);

  // Bluetooth setup
  SerialBT.begin(bluetoothName);   
  Serial.println("\nBluetooth started! Device name: " + String(bluetoothName));
  Serial.println("Pair from your laptop and open a serial terminal.");

  // ESP-NOW for C3
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  }
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, c3MacAddress, 6);
  esp_now_add_peer(&peerInfo);

  Serial.println("=== IGNITER READY (Bluetooth + ESP-NOW) ===");
  Serial.println("Commands: START  or  CANCEL");
}

void loop() {
 
  String cmd = "";
  if (Serial.available()) {
    cmd = Serial.readStringUntil('\n');
  } else if (SerialBT.available()) {
    cmd = SerialBT.readStringUntil('\n');
  }

  if (cmd.length() > 0) {
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "START" && !countdownActive) {
      countdownActive = true;
      countdownStart = millis();
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

 
  if (countdownActive) {
    unsigned long elapsed = (millis() - countdownStart) / 1000;

    if (elapsed < 10) {
      int remaining = 10 - elapsed;
      Serial.print("Countdown: ");
      Serial.println(remaining);
      SerialBT.print("Countdown: ");
      SerialBT.println(remaining);

      tone(BUZZER_PIN, 1200, 300);  
      delay(400);
    } else {
      Serial.println("*** LAUNCHING ***");
      SerialBT.println("*** LAUNCHING ***");
      noTone(BUZZER_PIN);

      digitalWrite(RELAY_PIN, HIGH);
      sendTakeoff();
      delay(1000);
      digitalWrite(RELAY_PIN, LOW);

      countdownActive = false;
      launchTriggered = true;
    }
  }

  
  if (digitalRead(LAUNCH_BUTTON) == LOW && !launchTriggered && !countdownActive) {
    delay(50);
    if (digitalRead(LAUNCH_BUTTON) == LOW) {
      Serial.println("\n*** INSTANT LAUNCH ***");
      SerialBT.println("\n*** INSTANT LAUNCH ***");
      digitalWrite(RELAY_PIN, HIGH);
      sendTakeoff();
      delay(1000);
      digitalWrite(RELAY_PIN, LOW);
      launchTriggered = true;
    }
  }

  delay(10);
}
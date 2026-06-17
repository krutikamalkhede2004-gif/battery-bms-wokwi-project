#define BLYNK_TEMPLATE_ID "TMPL3Pbfnjxf-"
#define BLYNK_TEMPLATE_NAME "Battery BMS"
#define BLYNK_AUTH_TOKEN "3ZyBZ58foN4hv377igZM27JD15d32uMb"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

#define CELL1_PIN 34
#define CELL2_PIN 35
#define CELL3_PIN 32
#define CELL4_PIN 33

#define GREEN_LED  18
#define YELLOW_LED 19
#define RED_LED    5
#define BLUE_LED   17

#define BUZZER_PIN 16
#define RELAY_PIN  25

#define MIN_VOLTAGE          3.0
#define MAX_VOLTAGE          4.2
#define MINOR_IMBAL          5.0
#define CRITICAL_IMBAL       15.0
#define FLUCTUATION_LIMIT    0.3
#define FROZEN_LIMIT         5

#define READ_INTERVAL        1000
#define LCD_INTERVAL         500
#define BUZZER_INTERVAL      500
#define RELAY_DELAY          2000
#define INIT_DURATION        2000
#define SCREEN_INTERVAL      3000
#define WIFI_CHECK_INTERVAL  5000
#define BLYNK_RUN_INTERVAL   100

unsigned long lastReadTime   = 0;
unsigned long lastLCDTime    = 0;
unsigned long lastBuzzerTime = 0;
unsigned long lastRelayTime  = 0;
unsigned long lastScreenTime = 0;
unsigned long startTime      = 0;
unsigned long lastWifiCheck  = 0;
unsigned long lastBlynkRun   = 0;

bool initDone      = false;
bool relayOpen     = false;
bool buzzerState   = false;
bool lastRelayOpen = false;

int  currentScreen = 0;
bool faultOverride = false;
bool lastFaultOverride = false;

float prevCell[4] = {3.7, 3.7, 3.7, 3.7};

int frozenCount[4] = {0, 0, 0, 0};
bool cellFault[4] = {false, false, false, false};

enum SystemMode { NORMAL, DEGRADED, FAILSAFE, SHUTDOWN };
SystemMode currentMode = NORMAL;
SystemMode lastMode = NORMAL;

struct FaultLog {
  unsigned long timestamp;
  char faultName[20];
};
FaultLog faultHistory[10];
int faultLogIndex = 0;
int faultLogCount = 0;

bool wifiConnected = false;
bool blynkConnected = false;
int wifiRSSI = 0;

char prevHealthState[25] = "";
char prevModeStr[12] = "";
bool prevRelayOpen = false;

struct TelemetryEvent {
  float cell[4];
  float imbalance;
  char healthState[25];
  char modeStr[12];
  bool relayOpen;
};
#define QUEUE_SIZE 10
TelemetryEvent eventQueue[QUEUE_SIZE];
int queueHead = 0;
int queueCount = 0;

float g_cell[4]      = {0, 0, 0, 0};
float g_packVoltage  = 0;
float g_average      = 0;
float g_imbalance    = 0;
int   g_weakest      = 1;
int   g_strongest    = 1;
bool  g_relayOpen    = false;
char  g_healthState[25] = "INITIALIZING";
char  g_warningMsg[25]  = "PLEASE WAIT";
char  g_modeStr[12] = "NORMAL";

float readVoltage(int pin) {
  float raw = analogRead(pin) * (3.3 / 4095.0);
  return 2.5 + (raw / 3.3) * (4.2 - 2.5);
}

bool isSensorAnomaly(float v) {
  return (v < 2.0 || v > 4.5);
}

bool isFluctuating(float current, float previous) {
  return abs(current - previous) > FLUCTUATION_LIMIT;
}

void logFault(const char* name) {
  faultHistory[faultLogIndex].timestamp = millis();
  strncpy(faultHistory[faultLogIndex].faultName, name, 19);
  faultHistory[faultLogIndex].faultName[19] = '\0';
  faultLogIndex = (faultLogIndex + 1) % 10;
  if (faultLogCount < 10) faultLogCount++;
}

// ══════════════════════════════════
// Task 6: Operator Recommendation
// ══════════════════════════════════
void getRecommendation(char* outMsg) {
  if (strcmp(g_healthState, "PACK FAILURE") == 0) {
    strcpy(outMsg, "Stop use! Replace weak cell.");
  }
  else if (strcmp(g_healthState, "SENSOR ERROR") == 0) {
    strcpy(outMsg, "Check sensor wiring now.");
  }
  else if (strcmp(g_healthState, "OVERVOLTAGE") == 0) {
    strcpy(outMsg, "Disconnect charger immediately.");
  }
  else if (strcmp(g_healthState, "CRITICAL IMBAL") == 0) {
    strcpy(outMsg, "Balance cells before next use.");
  }
  else if (strcmp(g_healthState, "FLUCTUATION") == 0) {
    strcpy(outMsg, "Monitor closely, check contacts.");
  }
  else if (strcmp(g_healthState, "MINOR IMBAL") == 0) {
    strcpy(outMsg, "Schedule routine balancing soon.");
  }
  else {
    strcpy(outMsg, "System healthy. No action needed.");
  }
}

// ══════════════════════════════════
// Task 6: Get Last Fault from Log
// ══════════════════════════════════
void getLastFault(char* outMsg) {
  if (faultLogCount == 0) {
    strcpy(outMsg, "No faults logged");
    return;
  }
  int lastIdx = (faultLogIndex - 1 + 10) % 10;
  snprintf(outMsg, 40, "[%lums] %s",
           faultHistory[lastIdx].timestamp,
           faultHistory[lastIdx].faultName);
}

void sendTelemetry() {
  if (!blynkConnected) {
    if (queueCount < QUEUE_SIZE) {
      int idx = (queueHead + queueCount) % QUEUE_SIZE;
      for (int i = 0; i < 4; i++) eventQueue[idx].cell[i] = g_cell[i];
      eventQueue[idx].imbalance = g_imbalance;
      strcpy(eventQueue[idx].healthState, g_healthState);
      strcpy(eventQueue[idx].modeStr, g_modeStr);
      eventQueue[idx].relayOpen = g_relayOpen;
      queueCount++;
      Serial.println("[Telemetry] Queued (offline)");
    }
    return;
  }

  char recommendation[40];
  char lastFault[40];
  getRecommendation(recommendation);
  getLastFault(lastFault);

  Blynk.virtualWrite(V0, g_cell[0]);
  Blynk.virtualWrite(V1, g_cell[1]);
  Blynk.virtualWrite(V2, g_cell[2]);
  Blynk.virtualWrite(V3, g_cell[3]);
  Blynk.virtualWrite(V4, g_packVoltage);
  Blynk.virtualWrite(V5, g_average);
  Blynk.virtualWrite(V6, g_imbalance);
  Blynk.virtualWrite(V7, g_healthState);
  Blynk.virtualWrite(V8, g_modeStr);
  Blynk.virtualWrite(V9, g_relayOpen ? "CUT OFF" : "NORMAL");
  Blynk.virtualWrite(V10, lastFault);
  Blynk.virtualWrite(V11, recommendation);

  Serial.println("[Telemetry] Sent to Blynk");
}

void syncQueuedEvents() {
  if (queueCount == 0) return;
  Serial.print("[Telemetry] Syncing ");
  Serial.print(queueCount);
  Serial.println(" queued events...");

  while (queueCount > 0) {
    int idx = queueHead;
    Blynk.virtualWrite(V0, eventQueue[idx].cell[0]);
    Blynk.virtualWrite(V1, eventQueue[idx].cell[1]);
    Blynk.virtualWrite(V2, eventQueue[idx].cell[2]);
    Blynk.virtualWrite(V3, eventQueue[idx].cell[3]);
    Blynk.virtualWrite(V6, eventQueue[idx].imbalance);
    Blynk.virtualWrite(V7, eventQueue[idx].healthState);
    Blynk.virtualWrite(V8, eventQueue[idx].modeStr);
    Blynk.virtualWrite(V9, eventQueue[idx].relayOpen ? "CUT OFF" : "NORMAL");
    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount--;
  }
  Serial.println("[Telemetry] Sync complete");
}

void checkConnection() {
  bool wasConnected = wifiConnected;
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  wifiRSSI = wifiConnected ? WiFi.RSSI() : -999;

  if (!wifiConnected) {
    Serial.println("[WiFi] Disconnected! Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    blynkConnected = false;
  }

  if (wifiConnected && !wasConnected) {
    Serial.println("[WiFi] Reconnected!");
  }

  if (wifiConnected && !Blynk.connected()) {
    Serial.println("[Blynk] Attempting connection...");
    Blynk.connect(2000);
  }

  bool wasBlynkConnected = blynkConnected;
  blynkConnected = Blynk.connected();

  if (blynkConnected && !wasBlynkConnected) {
    Serial.println("[Blynk] Connected! Syncing queue...");
    syncQueuedEvents();
  }

  if (!blynkConnected && wasBlynkConnected) {
    Serial.println("[Blynk] Disconnected!");
  }
}

void renderLCD() {
  switch (currentScreen) {

    case 0:
      lcd.setCursor(0, 0);
      lcd.print("-- Cell Voltages -- ");
      lcd.setCursor(0, 1);
      lcd.print("C1:");
      lcd.print(g_cell[0], 2);
      lcd.print("V C2:");
      lcd.print(g_cell[1], 2);
      lcd.print("V");
      lcd.setCursor(0, 2);
      lcd.print("C3:");
      lcd.print(g_cell[2], 2);
      lcd.print("V C4:");
      lcd.print(g_cell[3], 2);
      lcd.print("V");
      lcd.setCursor(0, 3);
      lcd.print("Screen 1/5          ");
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("-  Pack Analytics - ");
      lcd.setCursor(0, 1);
      lcd.print("Pack V:");
      lcd.print(g_packVoltage, 2);
      lcd.print("V      ");
      lcd.setCursor(0, 2);
      lcd.print("Avg  V:");
      lcd.print(g_average, 2);
      lcd.print("V      ");
      lcd.setCursor(0, 3);
      lcd.print("Imbal :");
      lcd.print(g_imbalance, 1);
      lcd.print("%  Scr 2/5  ");
      break;

    case 2:
      lcd.setCursor(0, 0);
      lcd.print("-- Health Status -- ");
      lcd.setCursor(0, 1);
      lcd.print("Status:");
      lcd.print(g_healthState);
      lcd.print("        ");
      lcd.setCursor(0, 2);
      lcd.print("Weak   Cell: C");
      lcd.print(g_weakest);
      lcd.print("      ");
      lcd.setCursor(0, 3);
      lcd.print("Strong Cell: C");
      lcd.print(g_strongest);
      lcd.print("  3/5 ");
      break;

    case 3:
      lcd.setCursor(0, 0);
      lcd.print("--  Protection  -- ");
      lcd.setCursor(0, 1);
      lcd.print("Relay  :");
      lcd.print(g_relayOpen ? "CUT OFF " : "NORMAL  ");
      lcd.setCursor(0, 2);
      lcd.print("Warning:");
      lcd.print(g_warningMsg);
      lcd.print("        ");
      lcd.setCursor(0, 3);
      lcd.print("Screen 4/5          ");
      break;

    case 4:
      lcd.setCursor(0, 0);
      lcd.print("-- System/Cloud  -- ");
      lcd.setCursor(0, 1);
      lcd.print("Mode: ");
      lcd.print(g_modeStr);
      lcd.print("       ");
      lcd.setCursor(0, 2);
      lcd.print("WiFi:");
      lcd.print(wifiConnected ? "OK  " : "FAIL");
      lcd.print(" RSSI:");
      lcd.print(wifiRSSI);
      lcd.setCursor(0, 3);
      lcd.print("Cloud:");
      lcd.print(blynkConnected ? "ON " : "OFF");
      lcd.print(" Q:");
      lcd.print(queueCount);
      lcd.print("     ");
      break;

    case 99:
      lcd.setCursor(0, 0);
      lcd.print("!! FAULT DETECTED !!");
      lcd.setCursor(0, 1);
      lcd.print("Status:");
      lcd.print(g_healthState);
      lcd.print("        ");
      lcd.setCursor(0, 2);
      lcd.print("Mode  :");
      lcd.print(g_modeStr);
      lcd.print("        ");
      lcd.setCursor(0, 3);
      lcd.print("Relay:");
      lcd.print(g_relayOpen ? "CUT OFF    " : "NORMAL     ");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(GREEN_LED,  OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED,    OUTPUT);
  pinMode(BLUE_LED,   OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN,  OUTPUT);

  digitalWrite(GREEN_LED,  LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED,    LOW);
  digitalWrite(BLUE_LED,   LOW);
  digitalWrite(RELAY_PIN,  HIGH);
  noTone(BUZZER_PIN);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Battery Engine    ");
  lcd.setCursor(0, 1);
  lcd.print("  Initializing...   ");
  lcd.setCursor(0, 2);
  lcd.print("  Task 1-6          ");
  lcd.setCursor(0, 3);
  lcd.print("  Connecting WiFi.. ");

  WiFi.begin(ssid, pass);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());

    Blynk.config(auth);
    Blynk.connect(5000);

    if (Blynk.connected()) {
      Serial.println("[Blynk] Connected successfully!");
      blynkConnected = true;
    } else {
      Serial.println("[Blynk] Connection failed, will retry in loop");
    }
  } else {
    Serial.println("\n[WiFi] Failed to connect, will retry in loop");
    Blynk.config(auth);
  }

  wifiConnected = (WiFi.status() == WL_CONNECTED);
  startTime = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastBlynkRun >= BLYNK_RUN_INTERVAL) {
    lastBlynkRun = now;
    if (Blynk.connected()) Blynk.run();
  }

  if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    checkConnection();
  }

  if (!initDone) {
    if (now - startTime >= INIT_DURATION) {
      lcd.clear();
      initDone = true;
      lastScreenTime = now;
    }
    return;
  }

  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;

    int rawADC[4];
    rawADC[0] = analogRead(CELL1_PIN);
    rawADC[1] = analogRead(CELL2_PIN);
    rawADC[2] = analogRead(CELL3_PIN);
    rawADC[3] = analogRead(CELL4_PIN);

    g_cell[0] = readVoltage(CELL1_PIN);
    g_cell[1] = readVoltage(CELL2_PIN);
    g_cell[2] = readVoltage(CELL3_PIN);
    g_cell[3] = readVoltage(CELL4_PIN);

    g_packVoltage = 0;
    for (int i = 0; i < 4; i++) g_packVoltage += g_cell[i];
    g_average = g_packVoltage / 4.0;

    float highest = g_cell[0], lowest = g_cell[0];
    g_strongest = 1; g_weakest = 1;

    for (int i = 1; i < 4; i++) {
      if (g_cell[i] > highest) { highest = g_cell[i]; g_strongest = i + 1; }
      if (g_cell[i] < lowest)  { lowest  = g_cell[i]; g_weakest   = i + 1; }
    }

    g_imbalance = 0;
    if (g_average > 0)
      g_imbalance = ((highest - lowest) / g_average) * 100.0;

    bool anomaly     = false;
    bool fluctuation = false;
    bool overvoltage = false;
    bool weakCell    = false;
    bool sensorDisconnected = false;
    bool frozenADC          = false;

    for (int i = 0; i < 4; i++) {
      cellFault[i] = false;

      if (rawADC[i] == 0) {
        sensorDisconnected = true;
        cellFault[i] = true;
        logFault("SENSOR DISCONNECT");
      }

      if (abs(g_cell[i] - prevCell[i]) < 0.001) {
        frozenCount[i]++;
        if (frozenCount[i] > FROZEN_LIMIT) {
          frozenADC = true;
          cellFault[i] = true;
          logFault("FROZEN ADC");
        }
      } else {
        frozenCount[i] = 0;
      }

      if (isSensorAnomaly(g_cell[i])) {
        anomaly = true;
        cellFault[i] = true;
      }
      if (isFluctuating(g_cell[i], prevCell[i])) fluctuation = true;
      if (g_cell[i] > MAX_VOLTAGE) overvoltage = true;
      if (g_cell[i] < MIN_VOLTAGE) weakCell    = true;

      prevCell[i] = g_cell[i];
    }

    bool dangerState = false;

    digitalWrite(GREEN_LED,  LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED,    LOW);
    digitalWrite(BLUE_LED,   LOW);

    if (anomaly || sensorDisconnected) {
      strcpy(g_healthState, "SENSOR ERROR");
      strcpy(g_warningMsg,  "CHECK SENSOR!");
      digitalWrite(RED_LED, HIGH);
      dangerState = true;
    }
    else if (overvoltage) {
      strcpy(g_healthState, "OVERVOLTAGE");
      strcpy(g_warningMsg,  "OVERVOLT!");
      digitalWrite(RED_LED, HIGH);
      dangerState = true;
    }
    else if (weakCell) {
      strcpy(g_healthState, "PACK FAILURE");
      strcpy(g_warningMsg,  "WEAK CELL!");
      digitalWrite(BLUE_LED, HIGH);
      dangerState = true;
    }
    else if (g_imbalance > CRITICAL_IMBAL) {
      strcpy(g_healthState, "CRITICAL IMBAL");
      strcpy(g_warningMsg,  "CRITICAL!");
      digitalWrite(RED_LED, HIGH);
      dangerState = true;
    }
    else if (fluctuation || frozenADC) {
      strcpy(g_healthState, "FLUCTUATION");
      strcpy(g_warningMsg,  frozenADC ? "FROZEN ADC!" : "VOLT UNSTABLE!");
      digitalWrite(YELLOW_LED, HIGH);
      dangerState = false;
    }
    else if (g_imbalance > MINOR_IMBAL) {
      strcpy(g_healthState, "MINOR IMBAL");
      strcpy(g_warningMsg,  "MONITOR PACK");
      digitalWrite(YELLOW_LED, HIGH);
      dangerState = false;
    }
    else {
      strcpy(g_healthState, "HEALTHY");
      strcpy(g_warningMsg,  "ALL NORMAL");
      digitalWrite(GREEN_LED, HIGH);
      dangerState = false;
    }

    if (dangerState != lastRelayOpen) {
      if (now - lastRelayTime >= RELAY_DELAY) {
        relayOpen      = dangerState;
        lastRelayOpen  = dangerState;
        g_relayOpen    = dangerState;
        lastRelayTime  = now;
        digitalWrite(RELAY_PIN, relayOpen ? LOW : HIGH);
      }
    }

    bool actualRelayState = (digitalRead(RELAY_PIN) == LOW);
    bool relayMismatch = (relayOpen != actualRelayState);
    if (relayMismatch) {
      logFault("RELAY MISMATCH");
    }

    lastMode = currentMode;
    int faultCount = 0;
    for (int i = 0; i < 4; i++) if (cellFault[i]) faultCount++;

    if (sensorDisconnected || relayMismatch || faultCount >= 3) {
      currentMode = SHUTDOWN;
    }
    else if (overvoltage || weakCell || g_imbalance > CRITICAL_IMBAL) {
      currentMode = FAILSAFE;
    }
    else if (faultCount > 0 || fluctuation || g_imbalance > MINOR_IMBAL) {
      currentMode = DEGRADED;
    }
    else {
      currentMode = NORMAL;
    }

    if (currentMode != lastMode) {
      switch (currentMode) {
        case NORMAL:   strcpy(g_modeStr, "NORMAL");   logFault("MODE->NORMAL");   break;
        case DEGRADED: strcpy(g_modeStr, "DEGRADED"); logFault("MODE->DEGRADED"); break;
        case FAILSAFE: strcpy(g_modeStr, "FAILSAFE"); logFault("MODE->FAILSAFE"); break;
        case SHUTDOWN: strcpy(g_modeStr, "SHUTDOWN"); logFault("MODE->SHUTDOWN"); break;
      }
    }

    faultOverride = dangerState;

    bool stateChanged = (strcmp(g_healthState, prevHealthState) != 0) ||
                         (strcmp(g_modeStr, prevModeStr) != 0) ||
                         (g_relayOpen != prevRelayOpen) ||
                         dangerState;

    if (stateChanged) {
      sendTelemetry();
      strcpy(prevHealthState, g_healthState);
      strcpy(prevModeStr, g_modeStr);
      prevRelayOpen = g_relayOpen;
    }

    Serial.println("================================");
    Serial.print("Cell1 = ");        Serial.println(g_cell[0], 2);
    Serial.print("Cell2 = ");        Serial.println(g_cell[1], 2);
    Serial.print("Cell3 = ");        Serial.println(g_cell[2], 2);
    Serial.print("Cell4 = ");        Serial.println(g_cell[3], 2);
    Serial.print("Pack Voltage = "); Serial.println(g_packVoltage, 2);
    Serial.print("Average = ");      Serial.println(g_average, 2);
    Serial.print("Strongest = C");   Serial.println(g_strongest);
    Serial.print("Weakest   = C");   Serial.println(g_weakest);
    Serial.print("Imbalance = ");    Serial.println(g_imbalance, 2);
    Serial.print("Health = ");       Serial.println(g_healthState);
    Serial.print("Warning = ");      Serial.println(g_warningMsg);
    Serial.print("Relay = ");        Serial.println(relayOpen ? "CUT OFF" : "NORMAL");
    Serial.print("System Mode = ");  Serial.println(g_modeStr);
    Serial.print("WiFi RSSI = ");    Serial.println(wifiRSSI);
    Serial.print("Blynk Connected = "); Serial.println(blynkConnected ? "YES" : "NO");
    Serial.print("Queued Events = "); Serial.println(queueCount);

    Serial.println("---- Fault Log ----");
    for (int i = 0; i < faultLogCount; i++) {
      Serial.print("[");
      Serial.print(faultHistory[i].timestamp);
      Serial.print("ms] ");
      Serial.println(faultHistory[i].faultName);
    }
  }

  if (relayOpen) {
    if (now - lastBuzzerTime >= BUZZER_INTERVAL) {
      buzzerState = !buzzerState;
      buzzerState ? tone(BUZZER_PIN, 1000) : noTone(BUZZER_PIN);
      lastBuzzerTime = now;
    }
  } else {
    noTone(BUZZER_PIN);
    buzzerState = false;
  }

  if (faultOverride && !lastFaultOverride) {
    currentScreen      = 99;
    lastFaultOverride  = true;
    lcd.clear();
  }

  if (!faultOverride && lastFaultOverride) {
    currentScreen     = 0;
    lastFaultOverride = false;
    lastScreenTime    = now;
    lcd.clear();
  }

  if (!faultOverride) {
    if (now - lastScreenTime >= SCREEN_INTERVAL) {
      currentScreen  = (currentScreen + 1) % 5;
      lastScreenTime = now;
      lcd.clear();
    }
  }

  if (now - lastLCDTime >= LCD_INTERVAL) {
    lastLCDTime = now;
    renderLCD();
  }
}

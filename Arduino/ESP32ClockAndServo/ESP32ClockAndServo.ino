#include <Wire.h>
#include "RTClib.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- SERVO ----------
const int servoPin = 18;
const int freq = 50;
const int resolution = 16;

int stopUs = 1500;

// CCW (toward button)
int pressUs = 1150;   // stronger side
// CW (back)
int backUs  = 1890;   // weaker than 1900 because CW was too fast

int travelMs = 60;
int holdMs   = 15000;
int settleMs = 200;

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
const uint8_t OLED_ADDR = 0x3C; // change to 0x3D if needed

// ---------- RTC ----------
RTC_DS3231 rtc;

// ---------- Wi-Fi AP + Web ----------
const char* ssid = "ESP32-Clock";
const char* pass = "12345678";
WebServer server(80);

// ---------- Alarm ----------
const int ALARM_INT_PIN = 27;          
volatile bool alarmFiredFlag = false;

void IRAM_ATTR onAlarmISR() {
  alarmFiredFlag = true;
}

void clearAndDisableAllAlarms() {
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF);
}

void setDailyAlarm1(int h, int m, int s) {
  rtc.disableAlarm(1);
  rtc.clearAlarm(1);

  DateTime now = rtc.now();
  DateTime alarmTime(now.year(), now.month(), now.day(), h, m, s);

  // Fires when H:M:S match (once per day).
  rtc.setAlarm1(alarmTime, DS3231_A1_Hour);

  // Turn off square wave, enable INT mode for alarms
  rtc.writeSqwPinMode(DS3231_OFF);

  // Clear any stale flag
  rtc.clearAlarm(1);
}


void disableAlarm1() {
  rtc.disableAlarm(1);
  rtc.clearAlarm(1);
}

// ---------- OLED draw ----------
void drawTime(DateTime now) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0,0);
  display.printf("%04d-%02d-%02d", now.year(), now.month(), now.day());

  display.setTextSize(2);
  display.setCursor(0,16);
  display.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  display.display();
}

// ---------- Web UI ----------
String pageHtml() {
  return R"rawliteral(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <style>
    body{font-family:sans-serif;max-width:420px;margin:20px auto;padding:0 12px}
    h2{margin-top:0}
    .card{border:1px solid #ddd;border-radius:12px;padding:14px;margin:12px 0}
    label{display:block;margin:8px 0 4px}
    input{width:100%;font-size:16px;padding:8px}
    button{margin-top:10px;width:100%;padding:10px;font-size:16px}
  </style>
</head>
<body>
  <h2>ESP32 Clock Setup</h2>

  <div class="card">
    <h3>Set Date & Time</h3>
    <form action="/setTime">
      <label>Date</label>
      <input type="date" name="d" required>
      <label>Time</label>
      <input type="time" name="t" step="1" required>
      <button type="submit">Set Time</button>
    </form>
  </div>

  <div class="card">
    <h3>Daily Alarm</h3>
    <form action="/setAlarm">
      <label>Alarm time</label>
      <input type="time" name="a" step="1">
      <button type="submit">Set Daily Alarm</button>
    </form>
    <form action="/alarmOff">
      <button type="submit">Disable Alarm</button>
    </form>
  </div>

  <div class="card">
    <h3>Status</h3>
    <a href="/status">View current time</a>
  </div>
</body>
</html>
)rawliteral";
}

void handleRoot() {
  server.send(200, "text/html", pageHtml());
}

void handleStatus() {
  DateTime now = rtc.now();
  char buf[64];
  snprintf(buf, sizeof(buf),
           "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  server.send(200, "text/plain", buf);
}

void handleSetTime() {
  if (!server.hasArg("d") || !server.hasArg("t")) {
    server.send(400, "text/plain", "Missing date or time");
    return;
  }
  String d = server.arg("d"); // YYYY-MM-DD
  String t = server.arg("t"); // HH:MM:SS

  int Y,M,Dd,h,m,s;
  sscanf(d.c_str(), "%d-%d-%d", &Y,&M,&Dd);
  sscanf(t.c_str(), "%d:%d:%d", &h,&m,&s);

  rtc.adjust(DateTime(Y,M,Dd,h,m,s));
  server.send(200, "text/plain", "Time updated!");
}

void handleSetAlarm() {
  if (!server.hasArg("a") || server.arg("a").length() == 0) {
    server.send(400, "text/plain", "Missing alarm time");
    return;
  }

  String a = server.arg("a"); // might be HH:MM or HH:MM:SS
  int h=0, m=0, s=0;

  int n = sscanf(a.c_str(), "%d:%d:%d", &h, &m, &s);
  if (n < 2) {   // not even HH:MM
    server.send(400, "text/plain", "Bad alarm format (use HH:MM or HH:MM:SS)");
    return;
  }
  if (n == 2) s = 0; // default seconds if missing

  setDailyAlarm1(h, m, s);
  server.send(200, "text/plain", "Daily alarm set!");
}

void handleAlarmOff() {
  disableAlarm1();
  server.send(200, "text/plain", "Alarm disabled.");
}

int usToDuty(int us) {
  return (int)((us / 20000.0) * ((1 << resolution) - 1));
}

void writeUs(int us) {
  us = constrain(us, 500, 2500);
  ledcWrite(servoPin, usToDuty(us));
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21,22);

  // Servo
  ledcAttach(servoPin, freq, resolution);
  writeUs(stopUs);

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while(1);
  }

  if (!rtc.begin()) {
    while(1);
  }

  clearAndDisableAllAlarms();

  pinMode(ALARM_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ALARM_INT_PIN), onAlarmISR, FALLING);

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, pass);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/setTime", handleSetTime);
  server.on("/setAlarm", handleSetAlarm);
  server.on("/alarmOff", handleAlarmOff);
  server.begin();
}


void pressAndReturn() {
  writeUs(backUs);
  delay(travelMs);
  
  writeUs(stopUs);
  delay(holdMs);

  writeUs(pressUs);
  delay(travelMs);

  writeUs(stopUs);
  delay(settleMs);
}

void loop() {
  server.handleClient();

  static unsigned long last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    drawTime(rtc.now());
  }

  if (alarmFiredFlag) {
    alarmFiredFlag = false;
    rtc.clearAlarm(1);
    // Alarm action here
    Serial.println("ALARM!");
    pressAndReturn();
  }
}

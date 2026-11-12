#include <Arduino.h>
#include <Adafruit_TMP117.h>
#include <LEAmDNS.h>
#include <SdFat.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <time.h>

#include "dashboard.h"
#include "wifi_credentials.h"

namespace {
constexpr uint32_t kSampleMs = 1000;
constexpr uint32_t kFlushMs = 5000;
constexpr uint32_t kNtpRetryMs = 60000;
constexpr uint32_t kWifiRetryMs = 30000;
constexpr size_t kHistorySize = 300;
constexpr time_t kValidTime = 1704067200;
constexpr char kHost[] = "thermal-test";

struct Sample {
  uint32_t ms;
  float t0;
  float t1;
};

Adafruit_TMP117 sensor0, sensor1;
bool sensor0Ready = false, sensor1Ready = false;
bool sdReady = false, mdnsReady = false, wifiConnected = false;
uint32_t lastSample = 0, lastFlush = 0, lastNtpAttempt = 0;
uint32_t lastWifiAttempt = 0;
bool ntpAttempted = false;
Sample history[kHistorySize];
size_t historyCount = 0, historyNext = 0;
SdFs sd;
FsFile logFile;
char logPath[24] = "";
WebServer server(80);

bool clockValid() { return time(nullptr) >= kValidTime; }

void addNumber(String &json, float value) {
  json += isfinite(value) ? String(value, 4) : String("null");
}

float readSensor(Adafruit_TMP117 &sensor, bool ready) {
  if (!ready) return NAN;
  sensors_event_t event;
  sensor.getEvent(&event);
  return event.temperature;
}

void stopLogging(const char *reason) {
  Serial.println(reason);
  logFile.close();
  sdReady = false;
}

void startLogging() {
  SPI.setRX(16);
  SPI.setSCK(18);
  SPI.setTX(19);
  if (!sd.begin(SdSpiConfig(17, SHARED_SPI, SD_SCK_MHZ(10), &SPI))) {
    Serial.println("SD card unavailable; logging disabled");
    return;
  }
  for (unsigned i = 0; i < 10000; ++i) {
    snprintf(logPath, sizeof(logPath), "/LOG%04u.CSV", i);
    if (!sd.exists(logPath)) break;
    logPath[0] = '\0';
  }
  if (!logPath[0]) {
    Serial.println("No unused SD log filename; logging disabled");
    return;
  }
  logFile = sd.open(logPath, O_WRONLY | O_CREAT | O_EXCL);
  if (!logFile) {
    Serial.println("Could not create SD log; logging disabled");
    logPath[0] = '\0';
    return;
  }
  const char header[] = "elapsed_ms,unix_time_utc,external_temp_c,internal_temp_c\n";
  if (logFile.write(reinterpret_cast<const uint8_t *>(header), sizeof(header) - 1) != sizeof(header) - 1) {
    stopLogging("SD log header write failed");
    return;
  }
  sdReady = true;
  Serial.printf("Logging to %s\n", logPath);
}

void recordSample() {
  Sample s{millis(), readSensor(sensor0, sensor0Ready), readSensor(sensor1, sensor1Ready)};
  history[historyNext] = s;
  historyNext = (historyNext + 1) % kHistorySize;
  if (historyCount < kHistorySize) ++historyCount;
  Serial.printf("External (I2C0): %.4f C, internal (I2C1): %.4f C\n", s.t0, s.t1);

  if (!sdReady) return;
  char row[100], utc[24] = "", t0[24] = "", t1[24] = "";
  if (clockValid()) snprintf(utc, sizeof(utc), "%lu", static_cast<unsigned long>(time(nullptr)));
  if (isfinite(s.t0)) snprintf(t0, sizeof(t0), "%.4f", s.t0);
  if (isfinite(s.t1)) snprintf(t1, sizeof(t1), "%.4f", s.t1);
  int len = snprintf(row, sizeof(row), "%lu,%s,%s,%s\n", static_cast<unsigned long>(s.ms), utc, t0, t1);
  if (len < 0 || static_cast<size_t>(len) >= sizeof(row) ||
      logFile.write(reinterpret_cast<const uint8_t *>(row), len) != static_cast<size_t>(len)) {
    stopLogging("SD write failed; logging stopped");
  }
}

void handleStatus() {
  String json;
  json.reserve(16000);
  json = "{\"t0\":";
  size_t latest = (historyNext + kHistorySize - 1) % kHistorySize;
  addNumber(json, historyCount ? history[latest].t0 : NAN);
  json += ",\"t1\":";
  addNumber(json, historyCount ? history[latest].t1 : NAN);
  json += ",\"sd\":\"";
  json += sdReady ? "recording" : "unavailable";
  json += "\",\"clock\":\"";
  json += clockValid() ? "synchronized (UTC)" : "not synchronized";
  json += "\",\"log\":\"";
  if (sdReady) json += logPath;
  json += "\",\"history\":[";
  size_t oldest = (historyNext + kHistorySize - historyCount) % kHistorySize;
  for (size_t i = 0; i < historyCount; ++i) {
    const Sample &s = history[(oldest + i) % kHistorySize];
    if (i) json += ',';
    json += '[';
    json += s.ms;
    json += ',';
    addNumber(json, s.t0);
    json += ',';
    addNumber(json, s.t1);
    json += ']';
  }
  json += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void updateNetwork() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wifiConnected) {
    Serial.print("Wi-Fi connected. Dashboard: http://");
    Serial.println(WiFi.localIP());
    mdnsReady = MDNS.begin(kHost);
    if (mdnsReady) Serial.println("mDNS: http://thermal-test.local");
    ntpAttempted = false;
  } else if (!connected && wifiConnected) {
    Serial.println("Wi-Fi disconnected; retrying");
    if (mdnsReady) MDNS.end();
    mdnsReady = false;
  }
  wifiConnected = connected;
  if (!connected && millis() - lastWifiAttempt >= kWifiRetryMs) {
    lastWifiAttempt = millis();
    WiFi.beginNoBlock(WIFI_SSID, WIFI_PASSWORD);
  }
  if (connected && !clockValid() && (!ntpAttempted || millis() - lastNtpAttempt >= kNtpRetryMs)) {
    lastNtpAttempt = millis();
    ntpAttempted = true;
    NTP.begin("pool.ntp.org", "time.nist.gov");
  }
  if (mdnsReady) MDNS.update();
}
} // namespace

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && millis() - start < 3000) delay(10);

  Wire.setSDA(20);
  Wire.setSCL(21);
  Wire.begin();
  Wire1.setSDA(14);
  Wire1.setSCL(15);
  Wire1.begin();
  sensor0Ready = sensor0.begin(TMP117_I2CADDR_DEFAULT, &Wire, 0);
  sensor1Ready = sensor1.begin(TMP117_I2CADDR_DEFAULT, &Wire1, 1);
  if (!sensor0Ready) Serial.println("External TMP117 unavailable on I2C0 (GP20/GP21)");
  if (!sensor1Ready) Serial.println("Internal TMP117 unavailable on I2C1 (GP14/GP15)");

  startLogging();
  recordSample();
  lastSample = lastFlush = millis();
  server.on("/", []() { server.send_P(200, "text/html", kDashboard); });
  server.on("/api/status", handleStatus);
  server.begin();
  WiFi.mode(WIFI_STA);
  WiFi.beginNoBlock(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
  Serial.println("Connecting to Wi-Fi...");
}

void loop() {
  updateNetwork();
  server.handleClient();
  uint32_t now = millis();
  if (now - lastSample >= kSampleMs) {
    lastSample = now;
    recordSample();
  }
  if (sdReady && now - lastFlush >= kFlushMs) {
    lastFlush = now;
    if (!logFile.sync()) stopLogging("SD sync failed; logging stopped");
  }
}

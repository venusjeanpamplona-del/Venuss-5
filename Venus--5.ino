#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <DHT.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <time.h>

// ================= WIFI SETTINGS =================
// Replace these placeholders with your NEW credentials.
#define WIFI_SSID       "kim"
#define WIFI_PASSWORD   "12345678"

// ================= FIREBASE SETTINGS =================
#define API_KEY         "AIzaSyD69UthfpO3blXr-bkYJ4f-vK9CiZDSsR8"
#define USER_EMAIL      "venusjeanpamplona@gmail.com"
#define USER_PASSWORD   "Benus0987"


#define DATABASE_URL \
  "https://venusjean-253b5-default-rtdb.europe-west1.firebasedatabase.app/"

// ================= HARDWARE =================
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define SD_CS 5

#define STUDENT_ID "Venus Pamplona"

String studentRoot() { return String("/students/") + STUDENT_ID; }
String studentPath(const String &relativePath) { return studentRoot() + "/" + relativePath; }
String studentQueuePath() { return studentPath("queue"); }

// SPI pins: SCK=18, MISO=19, MOSI=23, CS=5

// ================= TIMING =================
const unsigned long SAMPLE_INTERVAL_MS = 10000;
const unsigned long WIFI_RETRY_MS = 10000;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long NTP_RETRY_MS = 30000;
const unsigned long QUEUE_RETRY_INTERVAL_MS = 3000;
const int MAX_UPLOADS_PER_CYCLE = 3;

// Philippines UTC+8
const long GMT_OFFSET_SEC = 8L * 60L * 60L;
const int DAYLIGHT_OFFSET_SEC = 0;

// ================= FIREBASE OBJECTS =================
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
RealtimeDatabase Database;

// ================= SENSOR / STATE =================
DHT dht(DHT_PIN, DHT_TYPE);

bool sdReady = false;
bool wifiAttemptStarted = false;
bool timeWasSynced = false;
bool wasWiFiConnected = false;
bool syncMessageShown = false;

unsigned long lastSampleMs = 0;
unsigned long lastWiFiAttemptMs = 0;
unsigned long wifiAttemptStartMs = 0;
unsigned long lastNtpAttemptMs = 0;
unsigned long lastQueueRetryMs = 0;

uint32_t queueSequence = 1;

// Fallback clock works while ESP32 remains powered.
// After a power loss, accurate time requires NTP or an RTC.
time_t fallbackBaseEpoch = 0;
unsigned long fallbackBaseMillis = 0;

// ====================================================
// TIME HELPERS
// ====================================================

bool isValidEpoch(time_t epoch) {
  return epoch >= 1704067200; // 2024-01-01
}

String dateFromEpoch(time_t epoch) {
  struct tm timeinfo;
  localtime_r(&epoch, &timeinfo);
  char buf[16];
  strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
  return String(buf);
}

String timeFromEpoch(time_t epoch) {
  struct tm timeinfo;
  localtime_r(&epoch, &timeinfo);
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

String timestampFromEpoch(time_t epoch) {
  return dateFromEpoch(epoch) + " " + timeFromEpoch(epoch);
}

bool getCurrentEpoch(time_t &epoch) {
  time_t now = time(nullptr);
  if (isValidEpoch(now)) {
    epoch = now;
    return true;
  }

  if (isValidEpoch(fallbackBaseEpoch)) {
    unsigned long elapsed = (millis() - fallbackBaseMillis) / 1000UL;
    epoch = fallbackBaseEpoch + elapsed;
    return true;
  }
  return false;
}

// ====================================================
// SAVE / LOAD LAST TIME
// ====================================================

void saveLastEpoch(time_t epoch) {
  if (!sdReady || !isValidEpoch(epoch)) return;

  File f = SD.open(studentPath("last_epoch.txt"), FILE_WRITE);
  if (!f) {
    Serial.println("[SD ERROR] Cannot save last timestamp");
    return;
  }
  f.seek(0);
  f.print((long)epoch);
  f.print('\n');
  f.flush();
  f.close();
}

time_t loadLastEpoch() {
  if (!sdReady || !SD.exists(studentPath("last_epoch.txt"))) return 0;

  File f = SD.open(studentPath("last_epoch.txt"), FILE_READ);
  if (!f) return 0;
  String s = f.readStringUntil('\n');
  f.close();

  time_t epoch = (time_t)s.toInt();
  return isValidEpoch(epoch) ? epoch : 0;
}

// ====================================================
// PERMANENT HISTORY: CSV + VALID HTML
// ====================================================

// Create permanent history files if they do not exist.
void initializeHistoryFiles() {
  if (!sdReady) return;

  if (!SD.exists(studentPath("readings.csv"))) {
    File f = SD.open(studentPath("readings.csv"), FILE_WRITE);
    if (f) {
      f.println("Date,Time,Temperature_C,Humidity_percent");
      f.close();
    } else {
      Serial.println("[SD ERROR] Cannot create /readings.csv");
    }
  }

  if (!SD.exists(studentPath("readings.html"))) {
    File f = SD.open(studentPath("readings.html"), FILE_WRITE);
    if (f) {
      f.println("<!doctype html><html lang='en'><head><meta charset='UTF-8'>");
      f.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
      f.println("<title>ESP32 Sensor Readings</title>");
      f.println("<style>");
      f.println("body{font-family:Arial,sans-serif;margin:16px;background:#f4f6f8;color:#222}");
      f.println("h2{font-size:1.25rem}p{color:#555}");
      f.println(".wrap{overflow-x:auto;background:white;border-radius:8px;padding:8px}");
      f.println("table{border-collapse:collapse;width:100%;min-width:430px}");
      f.println("th,td{padding:10px;border:1px solid #ddd;text-align:left}");
      f.println("th{background:#1769aa;color:white}tr:nth-child(even){background:#f2f6fa}");
      f.println("</style></head><body><h2>ESP32 Sensor Readings</h2>");
      f.println("<p>Temperature and humidity history saved on the SD card.</p>");
      f.println("<div class='wrap'><table><thead><tr><th>Date</th><th>Time</th><th>Temperature (&deg;C)</th><th>Humidity (%)</th></tr></thead><tbody>");
      f.println("</tbody></table></div></body></html>");
      f.close();
    } else {
      Serial.println("[SD ERROR] Cannot create /readings.html");
    }
  }
}

// Add one sample to permanent CSV history, then rebuild HTML from all CSV rows.
// Rebuilding through a temporary file keeps the HTML valid after many readings.
bool appendReadingToHistory(time_t epoch, float temperature, float humidity) {
  if (!sdReady || !isValidEpoch(epoch)) return false;

  String date = dateFromEpoch(epoch);
  String clock = timeFromEpoch(epoch);

  File csv = SD.open(studentPath("readings.csv"), FILE_APPEND);
  if (!csv) {
    Serial.println("[SD ERROR] Cannot append /readings.csv");
    return false;
  }
  csv.printf("%s,%s,%.2f,%.2f\n",
             date.c_str(), clock.c_str(), temperature, humidity);
  csv.flush();
  csv.close();

  File input = SD.open(studentPath("readings.csv"), FILE_READ);
  if (!input) {
    Serial.println("[SD ERROR] Cannot read /readings.csv to rebuild HTML");
    return false;
  }

  String tempPath = studentPath("readings.tmp");
  SD.remove(tempPath);
  File output = SD.open(tempPath, FILE_WRITE);
  if (!output) {
    input.close();
    Serial.println("[SD ERROR] Cannot create temporary HTML file");
    return false;
  }

  output.println("<!doctype html><html lang='en'><head><meta charset='UTF-8'>");
  output.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  output.println("<title>ESP32 Sensor Readings</title>");
  output.println("<style>");
  output.println("body{font-family:Arial,sans-serif;margin:16px;background:#f4f6f8;color:#222}");
  output.println("h2{font-size:1.25rem}p{color:#555}.wrap{overflow-x:auto;background:white;border-radius:8px;padding:8px}");
  output.println("table{border-collapse:collapse;width:100%;min-width:430px}");
  output.println("th,td{padding:10px;border:1px solid #ddd;text-align:left}");
  output.println("th{background:#1769aa;color:white}tr:nth-child(even){background:#f2f6fa}");
  output.println("</style></head><body><h2>ESP32 Sensor Readings</h2>");
  output.println("<p>Temperature and humidity history saved on the SD card.</p>");
  output.println("<div class='wrap'><table><thead><tr><th>Date</th><th>Time</th><th>Temperature (&deg;C)</th><th>Humidity (%)</th></tr></thead><tbody>");

  // Skip CSV header, then convert each data row into an HTML table row.
  input.readStringUntil('\n');
  while (input.available()) {
    String line = input.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    int c3 = line.indexOf(',', c2 + 1);
    if (c1 < 0 || c2 < 0 || c3 < 0) continue;

    String d = line.substring(0, c1);
    String t = line.substring(c1 + 1, c2);
    String temp = line.substring(c2 + 1, c3);
    String hum = line.substring(c3 + 1);

    output.print("<tr><td>");
    output.print(d);
    output.print("</td><td>");
    output.print(t);
    output.print("</td><td>");
    output.print(temp);
    output.print("</td><td>");
    output.print(hum);
    output.println("</td></tr>");
  }

  output.println("</tbody></table></div></body></html>");
  output.flush();
  output.close();
  input.close();

  // Replace the old HTML only after the new version is fully written.
  SD.remove(studentPath("readings.html"));
  if (!SD.rename(tempPath, studentPath("readings.html"))) {
    Serial.println("[SD ERROR] Could not replace /readings.html");
    return false;
  }
  return true;
}

// ====================================================
// SD QUEUE HELPERS
// ====================================================

String queueFileName(uint32_t id) {
  char name[32];
  snprintf(name, sizeof(name), "Q%08lu.txt", (unsigned long)id);
  return studentQueuePath() + "/" + String(name);
}

bool isQueueFile(const String &name) {
  return name.startsWith(studentQueuePath() + "/Q") && name.endsWith(".txt");
}

void loadQueueSequence() {
  if (!sdReady) return;

  if (SD.exists(studentPath("seq.txt"))) {
    File f = SD.open(studentPath("seq.txt"), FILE_READ);
    if (f) {
      uint32_t stored = (uint32_t)f.readStringUntil('\n').toInt();
      f.close();
      if (stored > 0) queueSequence = stored;
    }
  }

  while (SD.exists(queueFileName(queueSequence))) queueSequence++;
}

void saveQueueSequence() {
  if (!sdReady) return;
  File f = SD.open(studentPath("seq.txt"), FILE_WRITE);
  if (!f) return;
  f.seek(0);
  f.print((unsigned long)queueSequence);
  f.print('\n');
  f.flush();
  f.close();
}

bool saveReadingToQueue(time_t epoch, float temperature, float humidity) {
  if (!sdReady) {
    Serial.println("[SD ERROR] SD card is not ready");
    return false;
  }

  String filename = queueFileName(queueSequence);
  while (SD.exists(filename)) {
    queueSequence++;
    filename = queueFileName(queueSequence);
  }

  File f = SD.open(filename, FILE_WRITE);
  if (!f) {
    Serial.println("[SD ERROR] Cannot create queue file");
    return false;
  }

  f.printf("%ld,%.2f,%.2f\n", (long)epoch, temperature, humidity);
  f.flush();
  f.close();

  if (!SD.exists(filename)) {
    Serial.println("[SD ERROR] Queue file was not created");
    return false;
  }

  queueSequence++;
  saveQueueSequence();
  Serial.printf("[SD] Data saved to SD card: %s\n", filename.c_str());
  return true;
}

bool parseQueueFile(const String &filename, time_t &epoch,
                    float &temperature, float &humidity) {
  File f = SD.open(filename, FILE_READ);
  if (!f) return false;

  String line = f.readStringUntil('\n');
  f.close();
  line.trim();

  int comma1 = line.indexOf(',');
  int comma2 = line.indexOf(',', comma1 + 1);
  if (comma1 < 1 || comma2 < 0) return false;

  epoch = (time_t)line.substring(0, comma1).toInt();
  temperature = line.substring(comma1 + 1, comma2).toFloat();
  humidity = line.substring(comma2 + 1).toFloat();

  return isValidEpoch(epoch) && !isnan(temperature) && !isnan(humidity);
}

String normalizeQueueName(String name) {
  if (name.startsWith(studentQueuePath() + "/")) return name;
  while (name.startsWith("/")) name.remove(0, 1);
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  return studentQueuePath() + "/" + name;
}

String oldestQueueFile() {
  if (!sdReady) return "";

  File dir = SD.open(studentQueuePath());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return "";
  }

  String oldest = "";
  File entry = dir.openNextFile();
  while (entry) {
    String name = normalizeQueueName(String(entry.name()));
    bool isFile = !entry.isDirectory();
    entry.close();

    if (isFile && isQueueFile(name) && (oldest == "" || name < oldest)) {
      oldest = name;
    }
    entry = dir.openNextFile();
  }
  dir.close();
  return oldest;
}

int countPendingFiles() {
  if (!sdReady) return 0;

  int count = 0;
  File dir = SD.open(studentQueuePath());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 0;
  }

  File entry = dir.openNextFile();
  while (entry) {
    String name = normalizeQueueName(String(entry.name()));
    bool isFile = !entry.isDirectory();
    entry.close();
    if (isFile && isQueueFile(name)) count++;
    entry = dir.openNextFile();
  }
  dir.close();
  return count;
}

// ====================================================
// FIREBASE UPLOAD
// ====================================================

bool uploadReading(time_t epoch, float temperature, float humidity) {
  if (WiFi.status() != WL_CONNECTED || !app.ready()) return false;

  String path = "/ESP32_Data/" + dateFromEpoch(epoch) + "/" + timeFromEpoch(epoch);
  String json = "{\"temperature\":" + String(temperature, 2) +
                ",\"humidity\":" + String(humidity, 2) + "}";

  bool success = Database.set<object_t>(aClient, path, object_t(json));

  if (!success) {
    Serial.printf("[Firebase ERROR] Code: %d | %s\n",
                  aClient.lastError().code(),
                  aClient.lastError().message().c_str());
    return false;
  }
  return true;
}

// ====================================================
// UPLOAD SAVED OFFLINE RECORDS
// ====================================================

void uploadPendingQueue() {
  if (!sdReady || WiFi.status() != WL_CONNECTED || !app.ready()) {
    syncMessageShown = false;
    return;
  }

  if (millis() - lastQueueRetryMs < QUEUE_RETRY_INTERVAL_MS) return;
  lastQueueRetryMs = millis();

  int pending = countPendingFiles();
  if (pending == 0) {
    syncMessageShown = false;
    return;
  }

  if (!syncMessageShown) {
    Serial.printf("[SYNC] Pending records found on SD card: %d\n", pending);
    Serial.println("[SYNC] Uploading saved records to Firebase...");
    syncMessageShown = true;
  }

  for (int i = 0; i < MAX_UPLOADS_PER_CYCLE; i++) {
    String filename = oldestQueueFile();
    if (filename == "") break;

    time_t epoch;
    float temperature, humidity;
    if (!parseQueueFile(filename, epoch, temperature, humidity)) {
      Serial.printf("[SD ERROR] Invalid queue file; kept: %s\n", filename.c_str());
      return;
    }

    String timestamp = timestampFromEpoch(epoch);
    Serial.printf("[SYNC] %s | Temperature: %.1f C | Humidity: %.1f %%\n",
                  timestamp.c_str(), temperature, humidity);

    if (!uploadReading(epoch, temperature, humidity)) {
      Serial.println("[SYNC] Upload failed. Record remains on SD.");
      syncMessageShown = false;
      return;
    }

    Serial.println("[Firebase] Upload successful");

    if (SD.remove(filename)) {
      Serial.printf("[SD] Uploaded record deleted: %s\n", filename.c_str());
      Serial.printf("[SD] Pending records: %d\n", countPendingFiles());
    } else {
      Serial.println("[SD ERROR] Upload succeeded, but file could not be deleted.");
      return;
    }
  }

  if (countPendingFiles() == 0) {
    Serial.println("[SYNC] All pending records uploaded successfully.");
    syncMessageShown = false;
  }
}

// ====================================================
// WIFI
// ====================================================

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  Serial.println("[Wi-Fi] Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiAttemptStarted = true;
  wifiAttemptStartMs = millis();
  lastWiFiAttemptMs = millis();
}

void maintainWiFi() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected) {
    wifiAttemptStarted = false;

    if (!wasWiFiConnected) {
      Serial.println("[Wi-Fi] WIFI CONNECTED");
      Serial.print("[Wi-Fi] IP address: ");
      Serial.println(WiFi.localIP());

      int pending = countPendingFiles();
      if (pending > 0) {
        Serial.printf("[SD] Pending records waiting for upload: %d", pending);
        Serial.println("[SYNC] Will upload saved SD records when Firebase is ready.");
      }

      wasWiFiConnected = true;
    }
    return;
  }

  if (wasWiFiConnected) {
    Serial.println("[Wi-Fi] WIFI DISCONNECTED");
    Serial.println("[SD] Data will be saved to SD card until Wi-Fi reconnects.");
    wasWiFiConnected = false;
    syncMessageShown = false;
  }

  // Do not repeatedly restart an active connection attempt.
  if (wifiAttemptStarted &&
      millis() - wifiAttemptStartMs < WIFI_CONNECT_TIMEOUT_MS) {
    return;
  }

  if (millis() - lastWiFiAttemptMs >= WIFI_RETRY_MS) {
    Serial.println("[Wi-Fi] Reconnecting...");
    lastWiFiAttemptMs = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiAttemptStarted = true;
    wifiAttemptStartMs = millis();
  }
}

// ====================================================
// NTP TIME SYNC (print success only once)
// ====================================================

void trySyncNTP() {
  if (timeWasSynced || WiFi.status() != WL_CONNECTED) return;
  if (lastNtpAttemptMs != 0 &&
      millis() - lastNtpAttemptMs < NTP_RETRY_MS) return;

  lastNtpAttemptMs = millis();
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC,
             "pool.ntp.org", "time.google.com");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    time_t now = time(nullptr);
    if (isValidEpoch(now)) {
      timeWasSynced = true;
      fallbackBaseEpoch = now;
      fallbackBaseMillis = millis();
      saveLastEpoch(now);
      Serial.printf("[TIME] NTP synchronized: %s\n",
                    timestampFromEpoch(now).c_str());
    }
  } else {
    Serial.println("[TIME] NTP not available. Using fallback time if possible.");
  }
}

// ====================================================
// DHT11 SAMPLING
// ====================================================

void sampleSensor() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT11 ERROR] Failed to read sensor. Check wiring.");
    return;
  }

  time_t epoch;
  if (!getCurrentEpoch(epoch)) {
    Serial.printf("[NO TIME] Temperature: %.1f C | Humidity: %.1f %%\n",
                  temperature, humidity);
    Serial.println("[TIME ERROR] No valid time yet; reading was not saved.");
    return;
  }

  String timestamp = timestampFromEpoch(epoch);
  Serial.printf("[%s] Temperature: %.1f C | Humidity: %.1f %%\n",
                timestamp.c_str(), temperature, humidity);

  // Save every valid sample to permanent history, even when offline.
  if (sdReady) {
    if (!appendReadingToHistory(epoch, temperature, humidity)) {
      Serial.println("[SD ERROR] Permanent history could not be updated.");
    }
  }

  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool firebaseReady = wifiConnected && app.ready();

  // Keep older pending records in order. Queue this reading behind them.
  int pending = countPendingFiles();
  if (pending > 0) {
    if (!wifiConnected) {
      Serial.println("[Wi-Fi] WIFI DISCONNECTED. Data will be saved to SD card.");
    } else if (!firebaseReady) {
      Serial.println("[Firebase] Not ready. Data will be saved to SD card.");
    }

    if (saveReadingToQueue(epoch, temperature, humidity)) {
      saveLastEpoch(epoch);
      Serial.printf("[SD] Pending records: %d", countPendingFiles());
    } else {
      Serial.println("[SD ERROR] Reading could not be saved to SD card.");
    }
    return;
  }

  // Upload directly if both Wi-Fi and Firebase are ready.
  if (firebaseReady) {
    if (uploadReading(epoch, temperature, humidity)) {
      saveLastEpoch(epoch);
      return; // Keep normal online output quiet.
    }

    Serial.println("[Firebase] Upload failed. Data will be saved to SD card.");
  } else if (!wifiConnected) {
    Serial.println("[Wi-Fi] WIFI DISCONNECTED. Data will be saved to SD card.");
  } else {
    Serial.println("[Firebase] Not ready. Data will be saved to SD card.");
  }

  if (saveReadingToQueue(epoch, temperature, humidity)) {
    saveLastEpoch(epoch);
    Serial.printf("[SD] Pending records: %d", countPendingFiles());
  } else {
    Serial.println("[SD ERROR] Reading could not be saved to SD card.");
  }
}

// ====================================================
// SETUP
// ====================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("====================================");
  Serial.println(" ACTIVITY 5: ESP32 + DHT11 + Firebase");
  Serial.printf("[SD] Student ID: %s\n", STUDENT_ID);
  Serial.println("====================================");

  dht.begin();
  Serial.println("[DHT11] Sensor initialized");

  SPI.begin(18, 19, 23, SD_CS);

  if (SD.begin(SD_CS, SPI)) {
    sdReady = true;
    Serial.println("[SD] Card initialized successfully");

    if (!SD.exists("/students") && !SD.mkdir("/students")) {
      sdReady = false;
      Serial.println("[SD ERROR] Could not create /students folder");
    }
    if (sdReady && !SD.exists(studentRoot()) && !SD.mkdir(studentRoot())) {
      sdReady = false;
      Serial.printf("[SD ERROR] Could not create student folder: %s\n", studentRoot().c_str());
    }
    if (sdReady && !SD.exists(studentQueuePath()) && !SD.mkdir(studentQueuePath())) {
      sdReady = false;
      Serial.printf("[SD ERROR] Could not create queue folder: %s\n", studentQueuePath().c_str());
    }

    if (sdReady) {
      loadQueueSequence();
      initializeHistoryFiles();

      Serial.printf("[SD] Pending records at startup: %d\n", countPendingFiles());

      time_t lastEpoch = loadLastEpoch();
      if (isValidEpoch(lastEpoch)) {
        fallbackBaseEpoch = lastEpoch;
        fallbackBaseMillis = millis();
        Serial.printf("[TIME] Loaded last saved time: %s\n",
                      timestampFromEpoch(lastEpoch).c_str());
      }

      Serial.printf("[SD] Student folder: %s\n", studentRoot().c_str());
      Serial.printf("[SD] History files: %s/readings.html and %s/readings.csv\n", studentRoot().c_str(), studentRoot().c_str());
    }
  } else {
    sdReady = false;
    Serial.println("[SD ERROR] Card initialization failed. Check wiring and FAT32 format.");
  }

  // Prototype setting. Certificate validation is safer for production.
  ssl_client.setInsecure();

  startWiFi();

  Serial.println("[Firebase] Initializing...");
  initializeApp(aClient, app, getAuth(user_auth));
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  Serial.println("[SYSTEM] Setup complete");
}

// ====================================================
// LOOP
// ====================================================

void loop() {
  app.loop();
  maintainWiFi();
  trySyncNTP();

  // Refresh fallback clock if system time becomes valid.
  time_t now = time(nullptr);
  if (isValidEpoch(now) && !timeWasSynced) {
    timeWasSynced = true;
    fallbackBaseEpoch = now;
    fallbackBaseMillis = millis();
    saveLastEpoch(now);
  }

  if (millis() - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = millis();
    sampleSensor();
  }

  uploadPendingQueue();
  delay(5);
}

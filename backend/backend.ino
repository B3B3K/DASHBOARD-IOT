// sensors
#include "SoftI2C.h"
#include "BMP180.h"
#include "TSL2561.h"
#include "MPU6050.h"
#include "MQ135.h"
#include "api.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// Pin definitions
#define MQ135_PIN  4
#define SDA_PIN    2
#define SCL_PIN    1

// STA WiFi credentials
const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";

// MGM Weather
const int MGM_MERKEZ_ID = 94101;  // Kocaeli

// Create software I2C instance
SoftI2C i2c(SDA_PIN, SCL_PIN, 10);

// Create sensor instances
BMP180 bmp180(i2c);
TSL2561 tsl2561(i2c);
MPU6050 mpu6050(i2c);
MQ135 mq135(MQ135_PIN);

// Create MGM Weather instance
MGMWeather mgm;

// Create Web Server
AsyncWebServer server(80);

// Dynamic Web Page's Path that changeable for devs
const char* PAGE_PATH = "/index.html";

// Default index page
const char DEFAULT_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<body>
<h1>ESP32 Sensor Hub</h1>
<p>Upload new page at <a href="/upload">/upload</a></p>
</body>
</html>
)rawliteral";

// Static Web Page that uploads new webpage
const char UPLOAD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<body>
<h2>ESP32 Page Uploader</h2>
<p>Upload HTML file to replace current page</p>

<form id="uploadForm" enctype="multipart/form-data">
  <input type="file" id="fileInput" accept=".html,.htm"><br><br>
  <input type="submit" value="UPLOAD">
</form>

<div id="status"></div>

<script>
document.getElementById('uploadForm').onsubmit = async (e) => {
  e.preventDefault();
  const file = document.getElementById('fileInput').files[0];
  if(!file) {
    document.getElementById('status').innerHTML = 'Select a file first';
    return;
  }
  
  document.getElementById('status').innerHTML = 'Uploading...';
  
  const fd = new FormData();
  fd.append('file', file, 'index.html');
  
  try {
    const res = await fetch('/upload', { method:'POST', body:fd });
    const txt = await res.text();
    if(res.ok) {
      document.getElementById('status').innerHTML = 'Uploaded! Reloading...';
      setTimeout(()=> location.href='/', 1000);
    } else {
      document.getElementById('status').innerHTML = 'Error: ' + txt;
    }
  } catch(e) {
    document.getElementById('status').innerHTML = 'Network error: ' + e.message;
  }
};
</script>
</body>
</html>
)rawliteral";

void writeProgmemToFS(const char* path, const char* progmemData) {
  File f = LittleFS.open(path, "w");
  if (!f) { Serial.println("[FS] Failed to open file for writing"); return; }
  f.print(FPSTR(progmemData));
  f.close();
  Serial.printf("[FS] Written %s\n", path);
}

void printFSInfo() {
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  Serial.printf("[FS] Total: %u B  Used: %u B  Free: %u B\n", total, used, total - used);
}

// ── Sensor cache ────────────────────────────────────────────────────────────
struct SensorCache {
  // BMP180
  float   temperature;
  long    pressure;
  float   altitude;
  bool    bmpOk;
  // TSL2561
  unsigned long lux;
  bool    tslOk;
  // MPU6050
  float   ax, ay, az;
  float   gx, gy, gz;
  bool    mpuOk;
  // MQ135
  uint16_t mq135Raw;
  float    mq135Ppm;
  // MGM Weather (yeni)
  WeatherData mgmData;
  // meta
  unsigned long updatedAt;   // millis() of last successful read
} cache;

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 2000;   // ms
bool mgmFetched = false;  // Boot'ta 1 kez fetch yapmak için

// Build JSON from cache – no heap alloc, stack buffer only
String buildApiJson() {
  char buf[1024];
  snprintf(buf, sizeof(buf),
    "{"
      "\"timestamp\":%lu,"
      "\"bmp180\":{"
        "\"ok\":%s,"
        "\"temperature_c\":%.1f,"
        "\"pressure_pa\":%ld,"
        "\"altitude_m\":%.1f"
      "},"
      "\"tsl2561\":{"
        "\"ok\":%s,"
        "\"lux\":%lu"
      "},"
      "\"mpu6050\":{"
        "\"ok\":%s,"
        "\"accel_g\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
        "\"gyro_dps\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}"
      "},"
      "\"mq135\":{"
        "\"raw\":%u,"
        "\"ppm\":%.1f"
      "},"
      "\"mgm\":{"
        "\"valid\":%s,"
        "\"aktuelBasinc\":%.1f,"
        "\"denizeIndirgenmisBasinc\":%.1f,"
        "\"sicaklik\":%.1f,"
        "\"nem\":%.0f,"
        "\"ruzgarHiz\":%.1f,"
        "\"ruzgarYon\":%d,"
        "\"veriZamani\":\"%s\","
        "\"denizVeriZamani\":\"%s\""
      "}"
    "}",
    cache.updatedAt,
    cache.bmpOk ? "true" : "false",
    cache.temperature, cache.pressure, cache.altitude,
    cache.tslOk ? "true" : "false",
    cache.lux,
    cache.mpuOk ? "true" : "false",
    cache.ax, cache.ay, cache.az,
    cache.gx, cache.gy, cache.gz,
    cache.mq135Raw, cache.mq135Ppm,
    cache.mgmData.valid ? "true" : "false",
    cache.mgmData.aktuelBasinc,
    cache.mgmData.denizeIndirgenmisBasinc,
    cache.mgmData.sicaklik,
    cache.mgmData.nem,
    cache.mgmData.ruzgarHiz,
    cache.mgmData.ruzgarYon,
    cache.mgmData.veriZamani.c_str(),
    cache.mgmData.denizVeriZamani.c_str()
  );
  return String(buf);
}

void readSensors() {
  BMP180Data bmp = bmp180.read();
  cache.bmpOk = bmp.valid;
  if (bmp.valid) {
    cache.temperature = bmp.temperature;
    cache.pressure    = (long)bmp.pressure;
    cache.altitude    = bmp.altitude;
    Serial.printf("Temp: %.1f°C, Press: %ld Pa, Alt: %.1fm\n",
                  bmp.temperature, (long)bmp.pressure, bmp.altitude);
  }

  TSL2561Data tsl = tsl2561.read();
  cache.tslOk = tsl.valid;
  if (tsl.valid) {
    cache.lux = tsl.lux;
    Serial.printf("Lux: %lu\n", tsl.lux);
  }

  MPU6050Data mpu = mpu6050.read();
  cache.mpuOk = mpu.valid;
  if (mpu.valid) {
    cache.ax = mpu.ax; cache.ay = mpu.ay; cache.az = mpu.az;
    cache.gx = mpu.gx; cache.gy = mpu.gy; cache.gz = mpu.gz;
    Serial.printf("Accel: %.2f, %.2f, %.2f g\n", mpu.ax, mpu.ay, mpu.az);
    Serial.printf("Gyro: %.1f, %.1f, %.1f °/s\n", mpu.gx, mpu.gy, mpu.gz);
  }

  cache.mq135Raw = mq135.readRaw();
  cache.mq135Ppm = mq135.readConcentration();
  Serial.printf("MQ135 Raw: %u, PPM: %.1f\n", cache.mq135Raw, cache.mq135Ppm);

  cache.updatedAt = millis();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize software I2C
    i2c.init();
    
    // Initialize sensors with error checking
    if (!bmp180.begin()) {
        Serial.println("BMP180 initialization FAILED!");
    } else {
        Serial.println("BMP180 initialized");
    }
    
    if (!tsl2561.begin()) {
        Serial.println("TSL2561 initialization FAILED!");
    } else {
        Serial.println("TSL2561 initialized");
    }
    
    if (!mpu6050.begin()) {
        Serial.println("MPU6050 initialization FAILED!");
    } else {
        Serial.println("MPU6050 initialized");
    }
    
    // Calibrate MQ135
    Serial.println("Calibrating MQ135...");
    mq135.calibrate(100);
    Serial.printf("MQ135 R0 = %.2f\n", mq135.getR0());
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
      Serial.println("[FS] LittleFS mount FAILED");
      while (true) delay(1000);
    }
    Serial.println("[FS] LittleFS mounted OK");
    printFSInfo();
    
    // Write default index if not exists
    if (!LittleFS.exists(PAGE_PATH)) {
      Serial.println("[FS] index.html not found – writing default");
      writeProgmemToFS(PAGE_PATH, DEFAULT_INDEX);
    }
    
    // Connect WiFi
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
    Serial.printf("\n[WiFi] Connected!  IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Initialize MGM Weather (EEPROM'dan yükler)
    mgm.setDebug(true);
    mgm.begin(1024);
    
    // EEPROM'dan veriyi cache'e yükle
    cache.mgmData = mgm.getLastWeatherData();
    if (cache.mgmData.valid) {
      Serial.println("[MGM] EEPROM'dan veri yüklendi:");
      Serial.printf("  Basinc: %.1f hPa\n", cache.mgmData.denizeIndirgenmisBasinc);
      Serial.printf("  Sicaklik: %.1f°C\n", cache.mgmData.sicaklik);
      Serial.printf("  Olcum zamani: %s\n", cache.mgmData.veriZamani.c_str());
    } else {
      Serial.println("[MGM] EEPROM'da veri yok, yeni veri alinacak");
    }
    
    // Her boot'ta 1 kez yeni veri al (3 saat kontrolü olmadan)
    Serial.println("[MGM] Boot'ta yeni veri aliniyor...");
    if (mgm.fetchAndSaveWeather(MGM_MERKEZ_ID)) {
      cache.mgmData = mgm.getLastWeatherData();
      Serial.println("[MGM] Veri guncellendi!");
    } else {
      Serial.println("[MGM] Veri alinamadi, EEPROM'daki eski veri kullaniliyor");
    }
    
    // Server routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      if (LittleFS.exists(PAGE_PATH)) {
        req->send(LittleFS, PAGE_PATH, "text/html");
      } else {
        req->send(200, "text/html", FPSTR(DEFAULT_INDEX));
      }
    });
    
    server.on("/upload", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send_P(200, "text/html", UPLOAD_HTML);
    });
    
    server.on("/upload", HTTP_POST,
      [](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "Upload complete");
      },
      [](AsyncWebServerRequest* req,
         const String& filename,
         size_t index,
         uint8_t* data,
         size_t len,
         bool final) {
        static File uploadFile;
        
        if (index == 0) {
          if (uploadFile) uploadFile.close();
          if (LittleFS.exists(PAGE_PATH)) {
            LittleFS.remove(PAGE_PATH);
          }
          uploadFile = LittleFS.open(PAGE_PATH, "w");
          if (!uploadFile) {
            Serial.println("[Upload] ERROR: cannot open file for writing");
            return;
          }
        }
        
        if (uploadFile && len) {
          uploadFile.write(data, len);
        }
        
        if (final) {
          if (uploadFile) {
            uploadFile.close();
            Serial.printf("[Upload] Done. %u bytes written to %s\n", index + len, PAGE_PATH);
            printFSInfo();
          }
        }
      }
    );
    
    // /api – JSON sensor data (cache refreshed every 2 s in loop())
    server.on("/api", HTTP_GET, [](AsyncWebServerRequest* req) {
      AsyncWebServerResponse* res =
        req->beginResponse(200, "application/json", buildApiJson());
      res->addHeader("Cache-Control", "no-store");
      res->addHeader("Access-Control-Allow-Origin", "*");
      req->send(res);
    });

    server.onNotFound([](AsyncWebServerRequest* req) {
      req->send(404, "text/plain", "Not found: " + req->url());
    });
    
    server.begin();
    Serial.println("[HTTP] Server started");
    Serial.printf("[HTTP] Main page : http://%s/\n",      WiFi.localIP().toString().c_str());
    Serial.printf("[HTTP] Upload page: http://%s/upload\n", WiFi.localIP().toString().c_str());
    Serial.printf("[HTTP] API        : http://%s/api\n",    WiFi.localIP().toString().c_str());
}

void loop() {
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    readSensors();
  }
}

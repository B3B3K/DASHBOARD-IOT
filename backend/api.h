#ifndef api_h
#define api_h

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// EEPROM adresleri
#define EEPROM_MAGIC_ADDR       0
#define EEPROM_TIMESTAMP_ADDR   4
#define EEPROM_WEATHER_ADDR     12

// WeatherData, MGMWeather sınıfından ÖNCE tanımlanmalı
struct WeatherData {
  float aktuelBasinc;
  float denizeIndirgenmisBasinc;
  float sicaklik;
  float hissedilenSicaklik;
  float nem;
  float ruzgarHiz;
  int ruzgarYon;
  float gorus;
  int kapalilik;
  float karYukseklik;
  float yagis00Now;
  float yagis10Dk;
  float yagis1Saat;
  float yagis24Saat;
  String veriZamani;
  String denizVeriZamani;
  int istNo;
  String hadiseKodu;
  unsigned long timestamp;
  bool valid;
};

class MGMWeather {
  public:
    MGMWeather();
    bool begin(int eepromSize = 512);
    bool fetchAndSaveWeather(int merkezId);
    WeatherData getLastWeatherData();
    bool shouldFetch();
    void setDebug(bool enable);
    void setMerkezId(int merkezId);

  private:
    struct WeatherDataStruct {
      float aktuelBasinc;
      float denizeIndirgenmisBasinc;
      float sicaklik;
      float hissedilenSicaklik;
      float nem;
      float ruzgarHiz;
      int ruzgarYon;
      float gorus;
      int kapalilik;
      float karYukseklik;
      float yagis00Now;
      float yagis10Dk;
      float yagis1Saat;
      float yagis24Saat;
      char veriZamani[32];
      char denizVeriZamani[32];
      int istNo;
      char hadiseKodu[8];
    };

    WeatherDataStruct _lastData;
    bool _debug;
    int _merkezId;
    unsigned long _eepromTimestamp;

    bool _isDataValid();
    void _saveToEEPROM();
    bool _loadFromEEPROM();
    bool _parseAndSave(String payload);   // ← EKSİKTİ
    void _addRequestHeaders(HTTPClient &http);
    unsigned long _parseIsoTimestamp(const char* isoString);
};

#endif
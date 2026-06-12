#include "api.h"

MGMWeather::MGMWeather() {
  _debug = false;
  _merkezId = 94101;
  _eepromTimestamp = 0;
  memset(&_lastData, 0, sizeof(_lastData));
}

bool MGMWeather::begin(int eepromSize) {
  EEPROM.begin(eepromSize);
  bool loaded = _loadFromEEPROM();
  if (_debug) {
    Serial.println("[MGM] EEPROM baslatildi");
    if (loaded) {
      Serial.print("[MGM] EEPROM'dan yuklendi, son zaman: ");
      Serial.println(_eepromTimestamp);
    } else {
      Serial.println("[MGM] Yeni baslangic, EEPROM'da veri yok");
    }
  }
  return loaded;
}

void MGMWeather::_addRequestHeaders(HTTPClient &http) {
  http.addHeader("Host", "servis.mgm.gov.tr");
  http.addHeader("Sec-Ch-Ua-Platform", "\"Windows\"");
  http.addHeader("Accept-Language", "tr-TR,tr;q=0.9");
  http.addHeader("Accept", "application/json, text/plain, */*");
  http.addHeader("Sec-Ch-Ua", "\"Not-A.Brand\";v=\"24\", \"Chromium\";v=\"146\"");
  http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
  http.addHeader("Sec-Ch-Ua-Mobile", "?0");
  http.addHeader("Origin", "https://www.mgm.gov.tr");
  http.addHeader("Sec-Fetch-Site", "same-site");
  http.addHeader("Sec-Fetch-Mode", "cors");
  http.addHeader("Sec-Fetch-Dest", "empty");
  http.addHeader("Referer", "https://www.mgm.gov.tr/");
  http.addHeader("Accept-Encoding", "gzip, deflate, br");
  http.addHeader("Priority", "u=1, i");
  http.addHeader("Connection", "keep-alive");
}

// ISO 8601 timestamp'i Unix timestamp'e çevir
// "2026-06-11T00:59:00.000Z" -> 1749603540
unsigned long MGMWeather::_parseIsoTimestamp(const char* isoString) {
  // Basit parsing: YYYY-MM-DDTHH:MM:SS
  int year, month, day, hour, minute, second;
  
  if (sscanf(isoString, "%d-%d-%dT%d:%d:%d", 
             &year, &month, &day, &hour, &minute, &second) == 6) {
    // 1 Ocak 2023'ten itibaren saniye cinsinden (RTC yoksa karşılaştırma için yeterli)
    // Basit bir dönüşüm: gün * 86400 + saat * 3600 + dakika * 60 + saniye
    // Not: Ay ve yıl farkı için tam dönüşüm yapılmıyor, sadece aynı gün içinde karşılaştırma için
    
    // Referans olarak 2026-06-01 00:00:00 baz alalım
    if (year == 2026 && month == 6) {
      int dayOfMonth = day - 1; // 1 Haziran = 0. gün
      return (unsigned long)(dayOfMonth * 86400UL + hour * 3600UL + minute * 60UL + second);
    }
  }
  
  // Parse edilemezse 0 döndür
  return 0;
}

bool MGMWeather::shouldFetch() {
  if (!_isDataValid()) {
    if (_debug) Serial.println("[MGM] EEPROM'da veri yok, yeni veri alinmali");
    return true;
  }
  
  // EEPROM'dan okunan zaman damgasını kullan
  unsigned long lastFetch = _eepromTimestamp;
  unsigned long threeHours = 3 * 60 * 60; // 10800 saniye
  
  // Şu anki zamanı API'den gelen veriZamani ile karşılaştıramayız,
  // sadece son alınan verinin üzerinden 3 saat geçti mi kontrol et
  // Bunun için EEPROM'a kaydettiğimiz zamanı kullanıyoruz
  
  if (_debug) {
    Serial.print("[MGM] Son alim zamani (EEPROM): ");
    Serial.println(lastFetch);
    Serial.print("[MGM] 3 saat = ");
    Serial.print(threeHours);
    Serial.println(" saniye");
  }
  
  // Not: Burada gerçek "şu an" zamanını bilmiyoruz, 
  // sadece EEPROM'daki verinin ne kadar eski olduğunu bilemeyiz.
  // Bu yüzden her boot'ta 1 kez fetch yapılması daha güvenli.
  // Alternatif: WiFi üzerinden NTP zamanı al
  
  if (_debug) {
    Serial.println("[MGM] UYARI: RTC/NTP olmadigi icin her boot'ta 1 kez veri alinacak");
    Serial.println("[MGM] 3 saat kontrolu icin NTP veya RTC gerekli");
  }
  
  // RTC yoksa her boot'ta fetch yap
  return true;
}

bool MGMWeather::fetchAndSaveWeather(int merkezId) {
  _merkezId = merkezId;
  
  if (WiFi.status() != WL_CONNECTED) {
    if (_debug) Serial.println("[MGM] WiFi bagli degil!");
    return false;
  }
  
  HTTPClient http;
  String url = "https://servis.mgm.gov.tr/web/sondurumlar?merkezid=" + String(_merkezId);
  
  if (_debug) {
    Serial.println("[MGM] ========== HTTP ISTEGI ==========");
    Serial.print("GET ");
    Serial.println(url);
  }
  
  http.begin(url);
  http.setTimeout(15000);
  _addRequestHeaders(http);
  
  int httpCode = http.GET();
  bool success = false;
  
  if (_debug) {
    Serial.print("[MGM] HTTP Kodu: ");
    Serial.println(httpCode);
  }
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    if (_debug) {
      Serial.println("[MGM] HTTP 200 OK");
      Serial.print("[MGM] Yanit: ");
      Serial.println(payload);
    }
    
    success = _parseAndSave(payload);
  } else {
    if (_debug) {
      Serial.print("[MGM] HTTP Hata: ");
      Serial.println(httpCode);
    }
  }
  
  http.end();
  return success;
}

bool MGMWeather::_parseAndSave(String payload) {
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    if (_debug) {
      Serial.print("[MGM] JSON Parse Hatasi: ");
      Serial.println(error.c_str());
    }
    return false;
  }
  
  JsonObject data = doc[0];
  
  if (data.isNull()) {
    if (_debug) Serial.println("[MGM] JSON'da veri yok");
    return false;
  }
  
  // Tüm alanları oku
  _lastData.aktuelBasinc = data["aktuelBasinc"] | 0.0;
  _lastData.denizeIndirgenmisBasinc = data["denizeIndirgenmisBasinc"] | 0.0;
  _lastData.sicaklik = data["sicaklik"] | 0.0;
  _lastData.hissedilenSicaklik = data["hissedilenSicaklik"] | 0.0;
  _lastData.nem = data["nem"] | 0.0;
  _lastData.ruzgarHiz = data["ruzgarHiz"] | 0.0;
  _lastData.ruzgarYon = data["ruzgarYon"] | 0;
  _lastData.gorus = data["gorus"] | 0.0;
  _lastData.kapalilik = data["kapalilik"] | 0;
  _lastData.karYukseklik = data["karYukseklik"] | 0.0;
  _lastData.yagis00Now = data["yagis00Now"] | 0.0;
  _lastData.yagis10Dk = data["yagis10Dk"] | 0.0;
  _lastData.yagis1Saat = data["yagis1Saat"] | 0.0;
  _lastData.yagis24Saat = data["yagis24Saat"] | 0.0;
  _lastData.istNo = data["istNo"] | 0;
  
  // String'leri kopyala
  String vZamani = data["veriZamani"] | "";
  strncpy(_lastData.veriZamani, vZamani.c_str(), 31);
  _lastData.veriZamani[31] = '\0';
  
  String dzZamani = data["denizVeriZamani"] | "";
  strncpy(_lastData.denizVeriZamani, dzZamani.c_str(), 31);
  _lastData.denizVeriZamani[31] = '\0';
  
  String hKodu = data["hadiseKodu"] | "";
  strncpy(_lastData.hadiseKodu, hKodu.c_str(), 7);
  _lastData.hadiseKodu[7] = '\0';
  
  // EEPROM timestamp'ini veriZamani'ndan al
  _eepromTimestamp = _parseIsoTimestamp(_lastData.veriZamani);
  
  _saveToEEPROM();
  
  if (_debug) {
    Serial.println("[MGM] ========== PARSELENEN VERI ==========");
    Serial.print("aktuelBasinc: ");
    Serial.println(_lastData.aktuelBasinc);
    Serial.print("denizeIndirgenmisBasinc: ");
    Serial.println(_lastData.denizeIndirgenmisBasinc);
    Serial.print("sicaklik: ");
    Serial.println(_lastData.sicaklik);
    Serial.print("veriZamani: ");
    Serial.println(_lastData.veriZamani);
    Serial.print("denizVeriZamani: ");
    Serial.println(_lastData.denizVeriZamani);
    Serial.print("EEPROM Timestamp: ");
    Serial.println(_eepromTimestamp);
    Serial.println("===========================================");
  }
  
  return true;
}

bool MGMWeather::_isDataValid() {
  return (_lastData.istNo != 0);
}

void MGMWeather::_saveToEEPROM() {
  unsigned long magic = 0x4D474D; // "MGM"
  EEPROM.put(EEPROM_MAGIC_ADDR, magic);
  EEPROM.put(EEPROM_TIMESTAMP_ADDR, _eepromTimestamp);
  EEPROM.put(EEPROM_WEATHER_ADDR, _lastData);
  EEPROM.commit();
  
  if (_debug) {
    Serial.println("[MGM] EEPROM'a kaydedildi");
    Serial.print("  Zaman damgasi: ");
    Serial.println(_eepromTimestamp);
  }
}

bool MGMWeather::_loadFromEEPROM() {
  unsigned long magic = 0;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);
  
  if (magic != 0x4D474D) {
    if (_debug) Serial.println("[MGM] EEPROM'da gecerli veri yok");
    return false;
  }
  
  EEPROM.get(EEPROM_TIMESTAMP_ADDR, _eepromTimestamp);
  EEPROM.get(EEPROM_WEATHER_ADDR, _lastData);
  
  if (_debug) {
    Serial.println("[MGM] EEPROM'dan yuklendi");
    Serial.print("  Zaman damgasi: ");
    Serial.println(_eepromTimestamp);
    Serial.print("  veriZamani: ");
    Serial.println(_lastData.veriZamani);
  }
  
  return true;
}

WeatherData MGMWeather::getLastWeatherData() {
  WeatherData result;
  
  result.aktuelBasinc = _lastData.aktuelBasinc;
  result.denizeIndirgenmisBasinc = _lastData.denizeIndirgenmisBasinc;
  result.sicaklik = _lastData.sicaklik;
  result.hissedilenSicaklik = _lastData.hissedilenSicaklik;
  result.nem = _lastData.nem;
  result.ruzgarHiz = _lastData.ruzgarHiz;
  result.ruzgarYon = _lastData.ruzgarYon;
  result.gorus = _lastData.gorus;
  result.kapalilik = _lastData.kapalilik;
  result.karYukseklik = _lastData.karYukseklik;
  result.yagis00Now = _lastData.yagis00Now;
  result.yagis10Dk = _lastData.yagis10Dk;
  result.yagis1Saat = _lastData.yagis1Saat;
  result.yagis24Saat = _lastData.yagis24Saat;
  result.veriZamani = String(_lastData.veriZamani);
  result.denizVeriZamani = String(_lastData.denizVeriZamani);
  result.istNo = _lastData.istNo;
  result.hadiseKodu = String(_lastData.hadiseKodu);
  result.timestamp = _eepromTimestamp;
  result.valid = _isDataValid();
  
  return result;
}

void MGMWeather::setDebug(bool enable) {
  _debug = enable;
}

void MGMWeather::setMerkezId(int merkezId) {
  _merkezId = merkezId;
}
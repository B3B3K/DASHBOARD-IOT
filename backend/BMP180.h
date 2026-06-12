#ifndef BMP180_H
#define BMP180_H

#include <Arduino.h>
#include "SoftI2C.h"

#define BMP180_ADDR 0x77

struct BMP180Data {
    float temperature;   // °C
    int32_t pressure;    // Pa
    float altitude;      // metres (deniz seviyesi basıncına göre)
    bool valid;
};

class BMP180 {
private:
    SoftI2C* i2c;
    struct CalData {
        int16_t AC1, AC2, AC3;
        uint16_t AC4, AC5, AC6;
        int16_t B1, B2, MB, MC, MD;
    } cal;
    
    bool readCalibration();
    int32_t readTempRaw(int32_t& B5_out);
    int32_t readPressure(int32_t B5);

public:
    BMP180(SoftI2C& i2c_ref);
    bool begin();
    // seaLevelPressure_hPa: API'dan gelen denizeIndirgenmisBasinc (hPa)
    // Varsayılan: 1013.25 hPa (standart atmosfer)
    BMP180Data read(float seaLevelPressure_hPa = 1013.25f);
    bool isConnected();
};

#endif

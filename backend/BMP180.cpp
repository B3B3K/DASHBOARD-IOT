#include "BMP180.h"
#include <math.h>

BMP180::BMP180(SoftI2C& i2c_ref) : i2c(&i2c_ref) {}

bool BMP180::begin() {
    uint8_t chip_id = 0;
    if (!i2c->readReg(BMP180_ADDR, 0xD0, &chip_id, 1)) return false;
    if (chip_id != 0x55) return false;
    return readCalibration();
}

bool BMP180::readCalibration() {
    uint8_t buf[22];
    if (!i2c->readReg(BMP180_ADDR, 0xAA, buf, 22)) return false;
    
    cal.AC1 = (buf[0] << 8) | buf[1];
    cal.AC2 = (buf[2] << 8) | buf[3];
    cal.AC3 = (buf[4] << 8) | buf[5];
    cal.AC4 = (buf[6] << 8) | buf[7];
    cal.AC5 = (buf[8] << 8) | buf[9];
    cal.AC6 = (buf[10] << 8) | buf[11];
    cal.B1  = (buf[12] << 8) | buf[13];
    cal.B2  = (buf[14] << 8) | buf[15];
    cal.MB  = (buf[16] << 8) | buf[17];
    cal.MC  = (buf[18] << 8) | buf[19];
    cal.MD  = (buf[20] << 8) | buf[21];
    
    return true;
}

int32_t BMP180::readTempRaw(int32_t& B5_out) {
    if (!i2c->writeReg(BMP180_ADDR, 0xF4, 0x2E)) return 0;
    delay(5);
    
    uint8_t buf[2];
    if (!i2c->readReg(BMP180_ADDR, 0xF6, buf, 2)) return 0;
    
    int32_t UT = ((int32_t)buf[0] << 8) | buf[1];
    
    int32_t X1 = ((UT - (int32_t)cal.AC6) * (int32_t)cal.AC5) >> 15;
    int32_t X2 = ((int32_t)cal.MC << 11) / (X1 + (int32_t)cal.MD);
    B5_out = X1 + X2;
    int32_t temp = (B5_out + 8) >> 4;
    
    return temp;
}

int32_t BMP180::readPressure(int32_t B5) {
    const uint8_t OSS = 3;
    
    uint8_t cmd = 0x34 | (OSS << 6);
    if (!i2c->writeReg(BMP180_ADDR, 0xF4, cmd)) return 0;
    
    switch(OSS) {
        case 0: delay(5); break;
        case 1: delay(8); break;
        case 2: delay(14); break;
        case 3: delay(26); break;
        default: delay(26); break;
    }
    
    uint8_t buf[3];
    if (!i2c->readReg(BMP180_ADDR, 0xF6, buf, 3)) return 0;
    
    int32_t UP = (((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | buf[2]) >> (8 - OSS);
    
    int64_t X1, X2, X3, B3, B6;
    uint64_t B4, B7;
    
    B6 = B5 - 4000;
    
    X1 = ((int64_t)cal.B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = ((int64_t)cal.AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = ((((int64_t)cal.AC1 * 4) + X3) << OSS) + 2;
    B3 >>= 2;
    
    X1 = ((int64_t)cal.AC3 * B6) >> 13;
    X2 = ((int64_t)cal.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = ((uint64_t)cal.AC4 * (uint64_t)(X3 + 32768)) >> 15;
    
    B7 = ((uint64_t)UP - (uint64_t)B3) * (50000ULL >> OSS);
    
    int64_t p;
    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p = p + ((X1 + X2 + 3791) >> 4);
    
    return (int32_t)p;
}

// seaLevelPressure_hPa: API'dan gelen "denizeIndirgenmisBasinc" değeri (hPa cinsinden, örn: 1013.6)
// Değer verilmezse standart atmosfer basıncı (1013.25 hPa) kullanılır
BMP180Data BMP180::read(float seaLevelPressure_hPa) {
    BMP180Data data = {0, 0, 0, false};
    
    if (!isConnected()) return data;
    
    int32_t B5;
    int32_t raw_temp = readTempRaw(B5);
    if (raw_temp == 0) return data;
    
    data.temperature = raw_temp / 10.0f;
    data.temperature -= 10; // TO DO: DYNAMIC ADJUST
    
    int32_t pressure = readPressure(B5);
    if (pressure == 0) return data;
    
    data.pressure = pressure;

    // API'dan gelen hPa değerini Pa'ya çevir (1 hPa = 100 Pa)
    float seaLevelPressure_Pa = seaLevelPressure_hPa * 100.0f;

    // Sensörden okunan basınç makul aralıkta mı kontrol et
    if (pressure > 30000 && pressure < 110000) {
        data.altitude = 44330.0f * (1.0f - powf((float)pressure / seaLevelPressure_Pa, 0.190295f));
    }

    data.valid = true;
    
    return data;
}

bool BMP180::isConnected() {
    return i2c->probe(BMP180_ADDR);
}

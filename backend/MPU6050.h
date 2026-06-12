#ifndef MPU6050_H
#define MPU6050_H

#include <Arduino.h>
#include "SoftI2C.h"

#define MPU6050_ADDR 0x68

struct MPU6050Data {
    float ax, ay, az;   // acceleration (g)
    float gx, gy, gz;   // angular velocity (°/s)
    float temp;         // temperature (°C)
    bool valid;
};

class MPU6050 {
private:
    SoftI2C* i2c;
    uint8_t addr;
    
    int16_t readRaw16(uint8_t reg);

public:
    MPU6050(SoftI2C& i2c_ref, uint8_t address = MPU6050_ADDR);
    bool begin();
    MPU6050Data read();
    bool isConnected();
};

#endif
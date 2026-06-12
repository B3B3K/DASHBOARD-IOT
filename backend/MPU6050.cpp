#include "MPU6050.h"

MPU6050::MPU6050(SoftI2C& i2c_ref, uint8_t address) : i2c(&i2c_ref) {
    addr = address;
}

int16_t MPU6050::readRaw16(uint8_t reg) {
    uint8_t buf[2];
    if (!i2c->readReg(addr, reg, buf, 2)) return 0;
    return (int16_t)((buf[0] << 8) | buf[1]);
}

bool MPU6050::begin() {
    // Check WHO_AM_I
    uint8_t who = 0;
    if (!i2c->readReg(addr, 0x75, &who, 1)) return false;
    if (who != 0x68 && who != 0x72 && who != 0x70) return false;
    
    // Wake up device
    i2c->writeReg(addr, 0x6B, 0x00);
    delay(100);
    
    // Configure
    i2c->writeReg(addr, 0x1A, 0x03);  // DLPF
    i2c->writeReg(addr, 0x1B, 0x00);  // Gyro ±250°/s
    i2c->writeReg(addr, 0x1C, 0x00);  // Accel ±2g
    
    return true;
}

MPU6050Data MPU6050::read() {
    MPU6050Data data = {0, 0, 0, 0, 0, 0, 0, false};
    
    if (!isConnected()) return data;
    
    uint8_t buf[14];
    if (!i2c->readReg(addr, 0x3B, buf, 14)) return data;
    
    int16_t ax_raw = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t ay_raw = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t az_raw = (int16_t)((buf[4] << 8) | buf[5]);
    int16_t t_raw  = (int16_t)((buf[6] << 8) | buf[7]);
    int16_t gx_raw = (int16_t)((buf[8] << 8) | buf[9]);
    int16_t gy_raw = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t gz_raw = (int16_t)((buf[12] << 8) | buf[13]);
    
    data.ax = ax_raw / 16384.0f;
    data.ay = ay_raw / 16384.0f;
    data.az = az_raw / 16384.0f;
    data.gx = gx_raw / 131.0f;
    data.gy = gy_raw / 131.0f;
    data.gz = gz_raw / 131.0f;
    data.temp = t_raw / 340.0f + 36.53f;
    data.valid = true;
    
    return data;
}

bool MPU6050::isConnected() {
    return i2c->probe(addr);
}
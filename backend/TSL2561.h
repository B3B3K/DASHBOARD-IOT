#ifndef TSL2561_H
#define TSL2561_H

#include <Arduino.h>
#include "SoftI2C.h"

#define TSL2561_ADDR 0x39

struct TSL2561Data {
    uint16_t ch0;        // Broadband channel
    uint16_t ch1;        // Infrared channel
    uint32_t lux;        // Calculated lux
    bool valid;
};

class TSL2561 {
private:
    SoftI2C* i2c;
    uint8_t gain;        // 0x00 = 1x, 0x10 = 16x
    uint8_t integration; // 0x00 = 13.7ms, 0x01 = 101ms, 0x02 = 402ms
    
    void writeReg(uint8_t reg, uint8_t value);
    uint8_t readReg(uint8_t reg);
    void readChannels();

public:
    TSL2561(SoftI2C& i2c_ref);
    bool begin(uint8_t gain = 0x10, uint8_t integration = 0x01);
    TSL2561Data read();
    void powerOn();
    void powerOff();
    bool isConnected();
};

#endif
#include "TSL2561.h"

TSL2561::TSL2561(SoftI2C& i2c_ref) : i2c(&i2c_ref) {
    gain = 0x10;        // 16x gain default
    integration = 0x01; // 101ms default
}

void TSL2561::writeReg(uint8_t reg, uint8_t value) {
    i2c->writeReg(TSL2561_ADDR, reg | 0x80, value);
}

uint8_t TSL2561::readReg(uint8_t reg) {
    uint8_t val = 0;
    i2c->readReg(TSL2561_ADDR, reg | 0x80, &val, 1);
    return val;
}

bool TSL2561::begin(uint8_t gain_set, uint8_t integration_set) {
    gain = gain_set;
    integration = integration_set;
    
    // Check part ID
    uint8_t id = readReg(0x8A);
    uint8_t part = (id >> 4) & 0x0F;
    if (part != 0x05 && part != 0x04) return false;
    
    powerOn();
    // Set gain and integration
    writeReg(0x81, gain | integration);
    delay(integration == 0x02 ? 402 : (integration == 0x01 ? 101 : 14));
    
    return true;
}

void TSL2561::powerOn() {
    writeReg(0x80, 0x03);
    delay(10);
}

void TSL2561::powerOff() {
    writeReg(0x80, 0x00);
}

TSL2561Data TSL2561::read() {
    TSL2561Data data = {0, 0, 0, false};
    
    if (!isConnected()) return data;
    
    uint8_t buf[2];
    
    // Read CH0
    i2c->readReg(TSL2561_ADDR, 0xAC | 0x80, buf, 2);
    data.ch0 = (uint16_t)((buf[1] << 8) | buf[0]);
    
    // Read CH1
    i2c->readReg(TSL2561_ADDR, 0xAE | 0x80, buf, 2);
    data.ch1 = (uint16_t)((buf[1] << 8) | buf[0]);
    
    // Calculate lux
    if (data.ch0 != 0) {
        uint32_t ratio = ((uint32_t)data.ch1 << 10) / data.ch0;
        
        if (ratio <= 0x0040)
            data.lux = (uint32_t)(0.0304f * data.ch0 - 0.0272f * data.ch1);
        else if (ratio <= 0x0080)
            data.lux = (uint32_t)(0.0325f * data.ch0 - 0.0440f * data.ch1);
        else if (ratio <= 0x00C0)
            data.lux = (uint32_t)(0.0351f * data.ch0 - 0.0544f * data.ch1);
        else if (ratio <= 0x0100)
            data.lux = (uint32_t)(0.0381f * data.ch0 - 0.0624f * data.ch1);
        else if (ratio <= 0x0138)
            data.lux = (uint32_t)(0.0224f * data.ch0 - 0.0310f * data.ch1);
        else if (ratio <= 0x019A)
            data.lux = (uint32_t)(0.0128f * data.ch0 - 0.0153f * data.ch1);
        else if (ratio <= 0x029A)
            data.lux = (uint32_t)(0.00146f * data.ch0 - 0.00112f * data.ch1);
    }
    
    data.valid = true;
    return data;
}

bool TSL2561::isConnected() {
    return i2c->probe(TSL2561_ADDR);
}
#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <Arduino.h>

class SoftI2C {
private:
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint32_t delay_us;

    void sda_high() { pinMode(sda_pin, INPUT); }
    void sda_low()  { pinMode(sda_pin, OUTPUT); digitalWrite(sda_pin, LOW); }
    void scl_high() { pinMode(scl_pin, INPUT); }
    void scl_low()  { pinMode(scl_pin, OUTPUT); digitalWrite(scl_pin, LOW); }
    bool sda_read() { pinMode(sda_pin, INPUT); return digitalRead(sda_pin); }
    void delay_usec() { delayMicroseconds(delay_us); }

public:
    SoftI2C(uint8_t sda, uint8_t scl, uint32_t delay_us = 5);
    void init();
    void start();
    void stop();
    bool writeByte(uint8_t byte);
    uint8_t readByte(bool send_ack);
    bool probe(uint8_t addr);
    bool writeReg(uint8_t addr, uint8_t reg, uint8_t val);
    bool readReg(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len);
};

#endif
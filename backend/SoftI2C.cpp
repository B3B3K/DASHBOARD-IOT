#include "SoftI2C.h"

SoftI2C::SoftI2C(uint8_t sda, uint8_t scl, uint32_t delay_us) {
    this->sda_pin = sda;
    this->scl_pin = scl;
    this->delay_us = delay_us;
}

void SoftI2C::init() {
    sda_high();
    scl_high();
    delay_usec();
}

void SoftI2C::start() {
    sda_high();
    scl_high();
    delay_usec();
    sda_low();
    delay_usec();
    scl_low();
    delay_usec();
}

void SoftI2C::stop() {
    sda_low();
    scl_high();
    delay_usec();
    sda_high();
    delay_usec();
}

bool SoftI2C::writeByte(uint8_t byte) {
    for (int8_t i = 7; i >= 0; i--) {
        if (byte & (1 << i)) sda_high();
        else sda_low();
        delay_usec();
        scl_high();
        delay_usec();
        scl_low();
        delay_usec();
    }
    sda_high();
    delay_usec();
    scl_high();
    delay_usec();
    bool ack = !sda_read();
    scl_low();
    delay_usec();
    return ack;
}

uint8_t SoftI2C::readByte(bool send_ack) {
    uint8_t byte = 0;
    sda_high();
    for (int8_t i = 7; i >= 0; i--) {
        scl_high();
        delay_usec();
        if (sda_read()) byte |= (1 << i);
        scl_low();
        delay_usec();
    }
    if (send_ack) sda_low();
    else sda_high();
    delay_usec();
    scl_high();
    delay_usec();
    scl_low();
    delay_usec();
    sda_high();
    return byte;
}

bool SoftI2C::probe(uint8_t addr) {
    start();
    bool ack = writeByte((addr << 1) | 0x00);
    stop();
    return ack;
}

bool SoftI2C::writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
    start();
    if (!writeByte((addr << 1) | 0x00)) { stop(); return false; }
    writeByte(reg);
    writeByte(val);
    stop();
    return true;
}

bool SoftI2C::readReg(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
    start();
    if (!writeByte((addr << 1) | 0x00)) { stop(); return false; }
    writeByte(reg);
    start();
    if (!writeByte((addr << 1) | 0x01)) { stop(); return false; }
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = readByte(i < (len - 1));
    }
    stop();
    return true;
}
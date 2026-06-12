#ifndef MQ135_H
#define MQ135_H

#include <Arduino.h>

class MQ135 {
private:
    uint8_t pin;
    float r0;           // Sensor resistance in clean air
    float rl;           // Load resistance (10kΩ typical)
    
    float getResistance(float voltage);
    
public:
    MQ135(uint8_t analog_pin, float load_resistance = 10.0f);
    void calibrate(float samples = 100);
    uint16_t readRaw();
    float readResistance();
    float readConcentration();
    float getR0() { return r0; }
    void setR0(float new_r0) { r0 = new_r0; }
};

#endif

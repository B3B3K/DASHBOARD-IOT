#include "MQ135.h"

MQ135::MQ135(uint8_t analog_pin, float load_resistance) {
    pin = analog_pin;
    rl = load_resistance;
    r0 = 1.0f;  // Will be set by calibration
}

uint16_t MQ135::readRaw() {
    return (uint16_t)analogRead(pin);
}

float MQ135::getResistance(float voltage) {
    // Vout = Vcc * (RL / (RL + RS))
    // RS = RL * (Vcc/Vout - 1)
    float vcc = 3.3f;  // ESP32 ADC reference
    float rs = rl * ((vcc / voltage) - 1.0f);
    return rs;
}

float MQ135::readResistance() {
    int adc = analogRead(pin);
    float voltage = adc * (3.3f / 4095.0f);
    return getResistance(voltage);
}

void MQ135::calibrate(float samples) {
    float sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += readResistance();
        delay(50);
    }
    float rs = sum / samples;
    // In clean air, RS/R0 should be ~3.6 for MQ135
    r0 = rs / 3.6f;
}

float MQ135::readConcentration() {
    float rs = readResistance();
    float ratio = rs / r0;
    
    // MQ135 response curve: PPM = 10 ^ ((log10(ratio) - 0.77) / -0.47)
    // Approximate formula for CO2

    float ppm = 5000.0f * pow(ratio, -1.972f);
    
    return ppm;
}

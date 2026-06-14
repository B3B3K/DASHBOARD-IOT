# DASHBOARD-IOT

A real-time IoT monitoring dashboard built with C++ backend and HTML5 frontend. Displays live sensor data from multiple environmental and motion sensors.

## Overview

This project integrates multiple sensors with an ESP32-C3 microcontroller to create a comprehensive environmental monitoring system. The frontend dashboard provides real-time visualization of sensor readings through interactive gauges and indicators.

## Hardware Stack

- **ESP32-C3-OLED**: Main microcontroller with Wi-Fi and BLE connectivity
- **BMP180**: Digital pressure sensor for weather monitoring and altitude measurement
- **MQ-135**: Air quality sensor detecting NH3, CO, NOx, and other pollutants
- **TSL-2561**: Light intensity sensor with full-spectrum detection
- **MPU6050**: 6-axis accelerometer/gyroscope for motion detection

## Architecture

### Frontend (`index.html`)
- Responsive web dashboard with real-time data updates
- Custom SVG gauges and visualizations
- Sections for:
  - Live sensor statistics (pressure, temperature, motion)
  - Light intensity gauge with gradient display
  - Gas level water-fill visualization
  - Accelerometer axis indicators
  - Hardware specifications display

### Backend (`backend/` directory)
- Modular C++ sensor drivers for each hardware component
- I2C communication via custom SoftI2C implementation
- REST API endpoint serving JSON sensor data
- Real-time data aggregation and processing

## Key Features

- Real-time sensor data visualization
- Status indicators with warning/danger thresholds
- Responsive design with fallback states
- API-driven frontend updates
- Modular driver architecture

## Sensors Data

The API provides:
- Air quality (PPM)
- Light intensity (LUX)
- Pressure and temperature
- Motion acceleration (X, Y, Z axes in G-force)

## API Endpoint

`GET /api` - Returns JSON with all sensor readings in real-time.

## Deployment

1. Flash backend code to ESP32-C3
2. Serve frontend files via built-in web server
3. Access dashboard at microcontroller's IP address

## Usage

The dashboard automatically fetches sensor data every 60 seconds and updates all visualizations. Color indicators change based on configured thresholds for each sensor type.

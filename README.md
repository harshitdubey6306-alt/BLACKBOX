# Predictive Maintenance for Industrial Motors 🛠️

**Blackbox Hackathon - 2nd Place Winner ** 🥈

This repository contains the firmware and web dashboard code for an IoT-based predictive maintenance system. Designed to monitor industrial CNC motors, this system detects early signs of equipment failure through changes in vibration, current, and temperature to warn operators before a major failure occurs. 

## Core Features
* **Real-Time Telemetry:** Continuous monitoring of motor health using a physical sensor array.
* **Dual-Display Monitoring:** Live sensor readings are displayed simultaneously on a physical OLED screen and a local web dashboard.
* **Automated Alerting Pipeline:** Integrates the Telegram Bot API for instant diagnostic messages and uses MacroDroid webhooks to push critical SMS alerts to mobile devices.
* **Physical Hazard Failsafes:** Local LEDs change color and an active buzzer triggers immediately when mechanical parameters exceed safe operational thresholds.

## Hardware Architecture
* **Microcontroller:** ESP32
* **Sensors:** MPU6050 Accelerometer, Current Sensor, Temperature Sensor
* **Actuators/Outputs:** OLED Display, Active Buzzer, Status LEDs

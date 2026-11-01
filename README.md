Predictive Maintenance for Industrial Motors 🛠️Blackbox Hackathon - 2nd Place Winner🥈This repository contains the firmware and web dashboard code for an IoT-based predictive maintenance system. Designed to monitor industrial CNC motors, this system detects early signs of equipment failure through changes in vibration, current, and temperature to warn operators before a major failure occurs.   
Core Features 
Real-Time Telemetry: Continuous monitoring of motor health using a physical sensor array.
Dual-Display Monitoring: Live sensor readings are displayed simultaneously on a physical OLED screen and a local web dashboard.
Automated Alerting Pipeline: Integrates the Telegram Bot API for instant diagnostic messages and uses MacroDroid webhooks to push critical SMS alerts to mobile devices.
Physical Hazard Fail safes: Local LEDs change color and an active buzzer triggers immediately when parameters exceed safe operational thresholds.
Hardware Architecture
Microcontroller: ESP32 (handles sensor acquisition, logic, and asynchronous web server hosting)
Sensors: MPU6050 Accelerometer (vibration and anomaly detection)Current Sensor Temperature Sensor 
Actuators/Outputs:OLED Display Active Buzzer Status LEDs Test 
Subject: CNC Motor 
Software Stack
Firmware: C/C++ (Arduino Framework) 
Web Dashboard: HTML/CSS/JS (served from the ESP32)
Integrations: Telegram Bot API (HTTP POST requests), MacroDroid (Webhook to SMS bridging)
Setup and Installation 
Hardware Assembly: Wire the MPU6050, current sensor, temperature sensor, OLED, LEDs, and buzzer to the ESP32 GPIO pins as defined in the source code .
Dependencies: Install the required sensor and display libraries in your IDE (e.g., Adafruit MPU6050, Adafruit SSD1306, UniversalTelegramBot).
Network Configuration:Update the WIFI_SSID and WIFI_PASSWORD variables. Insert your TELEGRAM_BOT_TOKEN and target CHAT_ID.Configure the MacroDroid webhook URL for SMS fallback.
Deployment: Flash the code to the ESP32. Open the Serial Monitor to obtain the ESP32's local IP address, and enter it into a browser on the same local network to access the live dashboard.

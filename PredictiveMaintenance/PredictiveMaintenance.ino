#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>

// --- 1. Venue Wi-Fi (MUST BE 2.4GHz) ---
const char* ssid_sta = "ABCDD";
const char* password_sta = "12344321";

// --- 2. Access Point (Local Dashboard) ---
const char* ssid_ap = "MotorHealth_Demo";
const char* password_ap = "password123";

// --- 3. Telegram Bot Details ---
#define BOT_TOKEN "PASTE YOUR TOKEN"
#define CHAT_ID "PASTE YOUR ID"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long lastTelegramTime = 0;
const unsigned long telegramCooldown = 30000; 

WebServer server(80);

// --- Hardware Objects & Pins ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_MPU6050 mpu;
OneWire oneWire(4); 
DallasTemperature ds18b20(&oneWire);

#define ACS712_PIN 34
#define LM393_PIN 33   
#define BUZZER_PIN 25  
#define RED_PIN 13
#define GREEN_PIN 26
#define BLUE_PIN 27

// --- INMP441 I2S Pins ---
#define I2S_SCK 18
#define I2S_WS 19
#define I2S_SD 32

// --- Global Variables ---
volatile unsigned int pulseCount = 0;
unsigned int currentRpm = 0;
unsigned long lastRpmTime = 0;
float currentSoundLevel = 0;
String latestJson = "{}"; 

// --- RPM FIX SETTINGS ---
const int ENCODER_HOLES = 20; // CHANGE THIS to match the holes on your black disk
volatile unsigned long lastInterruptTime = 0;

void IRAM_ATTR countPulse() {
  unsigned long interruptTime = millis();
  // Debounce filter: Ignores noise triggering faster than 1 millisecond
  if (interruptTime - lastInterruptTime > 1) { 
    pulseCount++;
    lastInterruptTime = interruptTime;
  }
}

// --- Setup I2S Mic ---
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

// --- Read Microphone Amplitude ---
float readMic() {
  int32_t sample = 0;
  size_t bytesRead;
  long maxSample = 0;
  
  for (int i = 0; i < 256; i++) {
    i2s_read(I2S_NUM_0, &sample, sizeof(sample), &bytesRead, portMAX_DELAY);
    if (bytesRead > 0) {
      sample >>= 14; 
      if (abs(sample) > maxSample) { maxSample = abs(sample); }
    }
  }
  return maxSample;
}

// --- HTML Dashboard (3-Column Layout with Limits) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Motor Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0; background-color: #121212; color: white;}
    h1 { color: #00adb5; font-size: 28px; margin-top: 20px; }
    
    .dashboard-container { display: flex; flex-wrap: wrap; justify-content: center; align-items: flex-start; gap: 20px; padding: 20px; }
    .side-panel { background-color: #1e1e1e; padding: 20px; width: 100%; max-width: 250px; border-radius: 10px; }
    .center-panel { width: 100%; max-width: 400px; }
    .card { background-color: #1e1e1e; padding: 15px; margin-bottom: 20px; border-radius: 10px; }
    
    h3 { color: #00adb5; font-size: 18px; margin-top: 0; border-bottom: 1px solid #333; padding-bottom: 10px; }
    .val { font-size: 20px; font-weight: bold; color: #eeeeee; }
    
    .status-NORMAL { color: #00FF00; font-size: 24px; font-weight: bold; }
    .status-MODERATE { color: #0088FF; font-size: 24px; font-weight: bold; }
    .status-CRITICAL { color: #FF0000; font-size: 24px; font-weight: bold; }
    
    .hazard-NORMAL { color: #00FF00; font-size: 20px; font-weight: bold; margin-top: 20px; display: block; }
    .hazard-ALERT { color: #FF4500; font-size: 20px; font-weight: bold; margin-top: 20px; display: block; }
    
    .risk { color: #FF4500; font-size: 18px; font-weight: bold; margin-top:5px; }
    .ai-box { border: 1px solid #00adb5; padding: 10px; margin-top: 15px; border-radius: 5px; background: #2a2a2a; color: #00adb5; font-weight: bold;}
    
    .peak-stat { margin: 15px 0; text-align: left; border-left: 3px solid #00adb5; padding-left: 10px;}
    .peak-label { font-size: 14px; color: #aaaaaa; }
  </style>
  <script>
    let peakVib = 0;
    let peakTemp = 0;
    let peakRpm = 0;
    let peakCurr = 0;

    setInterval(function() {
      fetch('/data').then(response => response.json()).then(data => {
        // Update Center Metrics
        document.getElementById("vib").innerHTML = data.vib.toFixed(2);
        document.getElementById("temp").innerHTML = data.temp.toFixed(1);
        document.getElementById("rpm").innerHTML = data.rpm;
        document.getElementById("snd").innerHTML = data.snd.toFixed(0);
        document.getElementById("curr").innerHTML = data.curr.toFixed(2);
        document.getElementById("risk").innerHTML = "System Risk: " + data.risk.toFixed(1) + "%";
        document.getElementById("ai").innerHTML = data.ai;
        
        let statusEl = document.getElementById("status");
        statusEl.innerHTML = data.state;
        statusEl.className = "status-" + data.state;

        // Update Left Side (Hazard Reason)
        let hazardEl = document.getElementById("hazard-reason");
        hazardEl.innerHTML = data.reason;
        hazardEl.className = data.state === "NORMAL" ? "hazard-NORMAL" : "hazard-ALERT";

        // Update Right Side (Peak Values)
        if (data.vib > peakVib) { peakVib = data.vib; document.getElementById("p-vib").innerText = peakVib.toFixed(2) + " m/s²"; }
        if (data.temp > peakTemp && data.temp != -127.0) { peakTemp = data.temp; document.getElementById("p-temp").innerText = peakTemp.toFixed(1) + " °C"; }
        if (data.rpm > peakRpm) { peakRpm = data.rpm; document.getElementById("p-rpm").innerText = peakRpm + " RPM"; }
        if (data.curr > peakCurr) { peakCurr = data.curr; document.getElementById("p-curr").innerText = peakCurr.toFixed(2) + " A"; }
      });
    }, 500); 
  </script>
</head>
<body>
  <h1>Edge Motor Diagnostics</h1>
  
  <div class="dashboard-container">
    
    <!-- LEFT PANEL: Hazard Trigger -->
    <div class="side-panel">
      <h3>Active Hazard</h3>
      <span id="hazard-reason" class="hazard-NORMAL">Loading...</span>
    </div>

    <!-- CENTER PANEL: Live Metrics -->
    <div class="center-panel">
      <div class="card">
        <p>System Status</p>
        <p id="status" class="status-NORMAL">LOADING...</p>
        <p id="risk" class="risk">System Risk: 0%</p>
        <div class="ai-box">AI Diagnosis: <span id="ai">Analyzing...</span></div>
      </div>
      <div class="card" style="text-align: left; padding: 15px 25px;">
        <p>Speed: <span id="rpm" class="val">0</span> RPM <br><span style="font-size:13px; color:#aaa;">[Mod: >1000 | Crit: >1300]</span></p>
        <p>Vib (Z): <span id="vib" class="val">0.00</span> m/s&sup2; <br><span style="font-size:13px; color:#aaa;">[Mod: >4.0 | Crit: >6.0]</span></p>
        <p>Current: <span id="curr" class="val">0.00</span> A <br><span style="font-size:13px; color:#aaa;">[Mod: >15.0A]</span></p>
        <p>Temp: <span id="temp" class="val">0.0</span> &deg;C <br><span style="font-size:13px; color:#aaa;">[Crit: >60&deg;C]</span></p>
        <p>Sound: <span id="snd" class="val">0</span> <br><span style="font-size:13px; color:#aaa;">[Mod: >2000 | Crit: >5000]</span></p>
      </div>
    </div>

    <!-- RIGHT PANEL: Peak Session Values -->
    <div class="side-panel">
      <h3>Peak Session Data</h3>
      <div class="peak-stat">
        <div class="peak-label">Max Vibration</div>
        <div id="p-vib" class="val">0.00 m/s&sup2;</div>
      </div>
      <div class="peak-stat">
        <div class="peak-label">Max Temperature</div>
        <div id="p-temp" class="val">0.0 &deg;C</div>
      </div>
      <div class="peak-stat">
        <div class="peak-label">Max Speed</div>
        <div id="p-rpm" class="val">0 RPM</div>
      </div>
      <div class="peak-stat">
        <div class="peak-label">Max Current</div>
        <div id="p-curr" class="val">0.00 A</div>
      </div>
    </div>

  </div>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);
  
  pinMode(ACS712_PIN, INPUT);
  pinMode(LM393_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // LED Boot Test
  digitalWrite(RED_PIN, HIGH); delay(300); digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, HIGH); delay(300); digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, HIGH); delay(300); digitalWrite(BLUE_PIN, LOW);
  
  attachInterrupt(digitalPinToInterrupt(LM393_PIN), countPulse, FALLING);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); display.setTextColor(WHITE);
  display.setCursor(0, 10); display.println("Booting System...");
  display.display();
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid_sta, password_sta);
  long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) { delay(500); }
  WiFi.softAP(ssid_ap, password_ap);
  
  secured_client.setInsecure(); 
  setupI2S(); 
  mpu.begin();
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  ds18b20.begin();

  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
  server.on("/data", HTTP_GET, []() { server.send(200, "application/json", latestJson); });
  server.begin();
}

void loop() {
  server.handleClient(); 
  
  if (millis() - lastRpmTime >= 1000) {
    // RPM Calculation upgraded with Encoder Division
    currentRpm = (pulseCount * 60) / ENCODER_HOLES; 
    pulseCount = 0;
    
    currentSoundLevel = readMic();
    
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float z = abs(a.acceleration.z - 9.8); 
    
    ds18b20.requestTemperatures();
    float t = ds18b20.getTempCByIndex(0);
    float c = (analogRead(ACS712_PIN) / 4095.0) * 20.0; 

    // Risk calculation updated for 6.0 Vib and 1300 RPM critical limits
    float vibRisk = (z / 6.0) * 100.0;
    float rpmRisk = (currentRpm / 1300.0) * 100.0;
    float tempRisk = (t / 60.0) * 100.0;
    float sndRisk = (currentSoundLevel / 5000.0) * 100.0; 
    
    float totalRisk = max(max(vibRisk, rpmRisk), max(tempRisk, sndRisk));
    if (totalRisk > 100.0) totalRisk = 100.0;

    // Determine Base State with 1300/1000 RPM limits
    String state = "NORMAL";
    if (z > 6.0 || t > 60.0 || currentRpm > 1300 || currentSoundLevel > 5000) { state = "CRITICAL"; } 
    else if (z > 4.0 || c > 15.0 || currentRpm > 1000 || currentSoundLevel > 2000) { state = "MODERATE"; }

    // Isolate the Hazard Reason
    String hazardReason = "System Stable";
    if (z > 6.0) hazardReason = "Critical Vibration";
    else if (t > 60.0) hazardReason = "Overheating";
    else if (currentRpm > 1300) hazardReason = "Overspeed detected";
    else if (currentSoundLevel > 5000) hazardReason = "Acoustic Anomaly (Amp)";
    else if (z > 4.0) hazardReason = "Moderate Vibration";
    else if (c > 15.0) hazardReason = "High Current Draw";
    else if (currentRpm > 1000) hazardReason = "Elevated Speed";
    else if (currentSoundLevel > 2000) hazardReason = "Elevated Noise";

    // Simulated ML Diagnosis Engine 
    String aiDiagnosis = "System Healthy (99% Confidence)";
    if (z > 4.0 && currentSoundLevel > 2000) { aiDiagnosis = "Bearing Wear Detected"; }
    else if (c > 15.0 && currentRpm < 100) { aiDiagnosis = "Motor Stalled / Jammed Axis"; }
    else if (t > 50.0 && c > 10.0) { aiDiagnosis = "Thermal Overload (Coil Degradation)"; }
    else if (currentRpm > 1300 && c < 2.0) { aiDiagnosis = "Tool Broken / No Load Detected"; }
    else if (state == "CRITICAL") { aiDiagnosis = "Anomaly Detected: Review Data Logs"; }
    else if (state == "MODERATE") { aiDiagnosis = "Minor Fluctuations Detected"; }

    // Hardware Outputs
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);

    if (state == "CRITICAL") { 
      digitalWrite(RED_PIN, HIGH); 
      digitalWrite(BUZZER_PIN, HIGH); 
      
      if (millis() - lastTelegramTime > telegramCooldown) {
        if (WiFi.status() == WL_CONNECTED) {
          String msg = "⚠️ MOTOR CRITICAL ALERT ⚠️\n";
          msg += "Reason: " + hazardReason + "\n";
          msg += "Risk Level: " + String(totalRisk, 1) + "%\n";
          msg += "RPM: " + String(currentRpm) + "\n";
          msg += "Vib: " + String(z, 1) + " m/s2\n";
          bot.sendMessage(CHAT_ID, msg, "");
          lastTelegramTime = millis();
        }
      }
    } 
    else if (state == "MODERATE") { digitalWrite(BLUE_PIN, HIGH); } 
    else { digitalWrite(GREEN_PIN, HIGH); }

    // Update OLED (Clean UI with Current and Risk)
    display.clearDisplay(); 
    
    // Print Threshold State (NORMAL, MODERATE, or CRITICAL) largely at the top
    display.setTextSize(2);
    display.setCursor(0, 0); 
    display.println(state);

    // Print all 6 metrics clearly on the remaining lines
    display.setTextSize(1);
    display.setCursor(0, 20); display.printf("RPM:%d Snd:%.0f", currentRpm, currentSoundLevel);
    display.setCursor(0, 32); display.printf("Vib:%.1f Tmp:%.1f", z, t);
    display.setCursor(0, 44); display.printf("Cur:%.1fA Rsk:%.0f%%", c, totalRisk);
    display.display();

    // Export Data, Diagnosis, and Reason to Web Interface
    latestJson = "{\"vib\":" + String(z) + ",\"temp\":" + String(t) + ",\"curr\":" + String(c) + ",\"rpm\":" + String(currentRpm) + ",\"snd\":" + String(currentSoundLevel) + ",\"risk\":" + String(totalRisk) + ",\"state\":\"" + state + "\",\"ai\":\"" + aiDiagnosis + "\",\"reason\":\"" + hazardReason + "\"}";
    
    lastRpmTime = millis();
  }
}

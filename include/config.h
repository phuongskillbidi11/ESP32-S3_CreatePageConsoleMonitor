#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi Configuration
// const char* WIFI_SSID = "I-Soft";
// const char* WIFI_PASSWORD = "i-soft@2023";
const char* WIFI_SSID = "Galaxy M12 A5AB";
const char* WIFI_PASSWORD = "88888888";

// WebSocket Configuration
const int WEBSOCKET_PORT = 80;
const char* WEBSOCKET_PATH = "/ws";

// Input Pin Configuration (7 inputs)
const int INPUT_PINS[7] = {4, 5, 13, 14, 15, 16, 17};
const int NUM_INPUTS = 7;

// Output Pin Configuration (7 outputs)
const int OUTPUT_PINS[7] = {18, 19, 21, 22, 23, 25, 26};
const int NUM_OUTPUTS = 7;

// LoRa Configuration Pins
const int LORA_CS = 5;
const int LORA_RST = 14;
const int LORA_IRQ = 2;

// System Monitor Configuration
const unsigned long MONITOR_INTERVAL = 2000; // 2 seconds
const unsigned long WEBSOCKET_UPDATE_INTERVAL = 5000; // 5 seconds

// Temperature sensor configuration
const float TEMP_OFFSET = -5.0; // Offset for temperature calibration

// Input pin modes
const int INPUT_MODE = INPUT_PULLUP; // Use internal pullup resistors

// Debug configuration
const bool DEBUG_MODE = true;
const int SERIAL_BAUD_RATE = 115200;

// Reset counter storage in RTC memory
RTC_DATA_ATTR int reset_counter = 0;

// Structure for input status
struct InputStatus {
  int pin;
  bool state;
  String stateStr;
};

// Structure for output status
struct OutputStatus {
  int pin;
  bool state;
  String stateStr;
};

// Structure for LoRa configuration
struct LoRaConfig {
  long frequency = 433E6;          // Frequency in Hz (433MHz)
  int spreadingFactor = 7;         // Spreading factor (6-12)
  long bandwidth = 125E3;          // Bandwidth in Hz (125kHz)
  int codingRate = 5;              // Coding rate (5-8)
  long preambleLength = 8;         // Preamble length
  int syncWord = 0x12;             // Sync word (0x12 is default)
  int txPower = 17;                // TX power in dBm (2-20)
};

// Structure for system status
struct SystemStatus {
  int resetCount;
  InputStatus inputs[NUM_INPUTS];
  OutputStatus outputs[NUM_OUTPUTS];
  size_t freeHeap;
  size_t freePsram;
  float temperature;
  String ipAddress;
  unsigned long uptime;
};

#endif
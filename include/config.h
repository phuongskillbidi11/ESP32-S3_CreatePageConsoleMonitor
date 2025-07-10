#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi Configuration
// const char* WIFI_SSID = "I-Soft";
// const char* WIFI_PASSWORD = "i-soft@2023";
// const char* WIFI_SSID = "Galaxy M12 A5AB";
// const char* WIFI_PASSWORD = "88888888";

const char* WIFI_SSID = "Heo vang 2019";
const char* WIFI_PASSWORD = "Hungthinh210315";

// WebSocket Configuration
const int WEBSOCKET_PORT = 80;
const char* WEBSOCKET_PATH = "/ws";

// Input Pin Configuration
const int INPUT_PINS[7] = {5, 6, 7, 37, 38, 39, 40};
const int NUM_INPUTS = 7;

// Output Pin Configuration
const int OUTPUT_PINS[5] = {21, 22, 23, 25, 26};
const int NUM_OUTPUTS = 5;

// LoRa E32 Configuration Pins
#define E32_M0_PIN    15
#define E32_M1_PIN    16
#define E32_TX_PIN    17
#define E32_RX_PIN    18
#define E32_AUX_PIN   -1

// System Monitor Configuration
const unsigned long MONITOR_INTERVAL = 2000;
const unsigned long WEBSOCKET_UPDATE_INTERVAL = 5000;

// Temperature sensor configuration
const float TEMP_OFFSET = -5.0;

// Input pin modes
const int INPUT_MODE = INPUT_PULLUP;

// Debug configuration
const bool DEBUG_MODE = true;
const int SERIAL_BAUD_RATE = 115200;

// Reset counter storage in RTC memory
RTC_DATA_ATTR int reset_counter = 0;

// Counter configuration
struct CounterConfig {
  int pin;
  unsigned long delayFilter; // Delay filter in milliseconds
  unsigned long count;
  bool lastState;
  unsigned long lastDebounceTime;
};

// Admin credentials
struct AdminCredentials {
  String username = "admin";
  String password = "admin123";
};

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

// Structure for LoRa E32 configuration
struct LoRaE32Config {
  bool initialized = false;
  String moduleInfo = "";
  String configInfo = "";
  uint8_t addh = 0;
  uint8_t addl = 0;
  uint8_t chan = 0;
  String frequency = "";
  String airDataRateStr = "";
  String uartBaudRateStr = "";
  String transmissionPowerStr = "";
  String parityBit = "";
  String wirelessWakeupTimeStr = "";
  String fecStr = "";
  String fixedTransmissionStr = "";
  String ioDriveModeStr = "";
  uint8_t uartParity = 0;
  uint8_t uartBaudRate = 0b011;
  uint8_t airDataRate = 0b010;
  uint8_t fixedTransmission = 0;
  uint8_t ioDriveMode = 1;
  uint8_t wirelessWakeupTime = 0;
  uint8_t fec = 1;
  uint8_t transmissionPower = 0b11;
  uint8_t operatingMode = 0;
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
  LoRaE32Config loraE32;
  int planDisplay = 0;
  CounterConfig counters[4];
  bool adminMode = false;
  AdminCredentials adminCredentials;
};

// Function declarations
void saveLoRaConfig();
void loadLoRaConfig();
void setLoRaConfig(LoRaE32Config config);
void setLoRaOperatingMode(uint8_t mode);
void saveCounterConfig();
void loadCounterConfig();
void saveAdminCredentials();
void loadAdminCredentials();

#endif
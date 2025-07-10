#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "LoRa_E32.h"
#include "config.h"

// Global variables
AsyncWebServer server(WEBSOCKET_PORT);
AsyncWebSocket ws(WEBSOCKET_PATH);
SystemStatus systemStatus;
unsigned long lastMonitorTime = 0;
unsigned long lastWebSocketTime = 0;
unsigned long bootTime = 0;
unsigned long lastAdminActivity = 0;
const unsigned long ADMIN_TIMEOUT = 30 * 60 * 1000; // 30 minutes

// LoRa E32 instance
LoRa_E32 e32ttl100(&Serial1, E32_AUX_PIN, E32_M0_PIN, E32_M1_PIN);

// Forward declarations
void initInputs();
void initOutputs();
void initWiFi();
void initLittleFS();
void initWebSocket();
void initLoRaE32();
void updateSystemStatus();
void sendSystemStatus();
void sendDebugMessage(const String& message);
float getTemperature();
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void systemMonitorTask(void *pvParameters);
void webSocketTask(void *pvParameters);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void saveIOConfig();
void loadIOConfig();
void saveLoRaConfig();
void loadLoRaConfig();
void setLoRaConfig(LoRaE32Config config);
void setLoRaOperatingMode(uint8_t mode);
void clearTerminal();
void controlOutput(int pin, bool state);
void printModuleInformation(struct ModuleInformation moduleInformation);
void printParameters(struct Configuration configuration);
//dashboard functions
void saveCounterConfig();
void loadCounterConfig();
void saveAdminCredentials();
void loadAdminCredentials();
void sendCounterStatus();
void updateCounterStatus();
void checkAdminTimeout();

// Initialize input pins
void initInputs() {
  for (int i = 0; i < NUM_INPUTS; i++) {
    pinMode(INPUT_PINS[i], INPUT_MODE);
    systemStatus.inputs[i].pin = INPUT_PINS[i];
    systemStatus.inputs[i].state = false;
    systemStatus.inputs[i].stateStr = "LOW";
  }
  Serial.println("Input pins initialized");
}

// Initialize output pins
void initOutputs() {
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    pinMode(OUTPUT_PINS[i], OUTPUT);
    systemStatus.outputs[i].pin = OUTPUT_PINS[i];
    systemStatus.outputs[i].state = false;
    systemStatus.outputs[i].stateStr = "LOW";
    digitalWrite(OUTPUT_PINS[i], LOW);
  }
  Serial.println("Output pins initialized");
}

// Initialize WiFi connection
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  systemStatus.ipAddress = WiFi.localIP().toString();
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");
  loadIOConfig();
  loadLoRaConfig();
  loadCounterConfig();
  loadAdminCredentials();
}

// Initialize LoRa E32
void initLoRaE32() {
  Serial.println("=== Initializing LoRa E32 ===");
  
  // Thiết lập chân M0, M1 là OUTPUT
  pinMode(E32_M0_PIN, OUTPUT);
  pinMode(E32_M1_PIN, OUTPUT);
  
  // Khởi tạo Serial1 với các chân đã định nghĩa
  Serial1.begin(9600, SERIAL_8N1, E32_RX_PIN, E32_TX_PIN);
  
  // Khởi tạo LoRa E32
  e32ttl100.begin();
  
  // Đặt chế độ mặc định là Normal Mode
  setLoRaOperatingMode(0); // Normal mode
  
  delay(1000); // Tăng delay để module ổn định
  
  Serial.println("Reading module information...");
  
  // Đọc thông tin module với error handling
  ResponseStructContainer c;
  c = e32ttl100.getModuleInformation();
  
  if (c.status.code == 1 && c.data != nullptr) {
    ModuleInformation mi = *(ModuleInformation*)c.data;
    printModuleInformation(mi);
    
    // Lưu thông tin module vào system status
    systemStatus.loraE32.moduleInfo = "HEAD: " + String(mi.HEAD, HEX) + 
                                      ", Freq: " + String(mi.frequency, HEX) + 
                                      ", Ver: " + String(mi.version, HEX) + 
                                      ", Features: " + String(mi.features, HEX);
    systemStatus.loraE32.initialized = true;
    
    sendDebugMessage("LoRa E32 module information read successfully");
  } else {
    Serial.print("Error reading module information: ");
    Serial.println(c.status.getResponseDescription());
    sendDebugMessage("Error reading LoRa E32 module information: " + String(c.status.getResponseDescription()));
    systemStatus.loraE32.initialized = false;
  }
  
  if (c.data != nullptr) {
    c.close();
  }
  
  delay(1000);
  
  // Chỉ đọc configuration nếu module đã được khởi tạo thành công
  if (systemStatus.loraE32.initialized) {
    Serial.println("Reading current configuration...");
    
    // Đọc cấu hình hiện tại
    ResponseStructContainer configContainer;
    configContainer = e32ttl100.getConfiguration();
    
    if (configContainer.status.code == 1 && configContainer.data != nullptr) {
      Configuration configuration = *(Configuration*)configContainer.data;
      Serial.println("Configuration read successfully!");
      
      printParameters(configuration);
      
      // Lưu cấu hình vào system status
      systemStatus.loraE32.addh = configuration.ADDH;
      systemStatus.loraE32.addl = configuration.ADDL;
      systemStatus.loraE32.chan = configuration.CHAN;
      systemStatus.loraE32.uartParity = configuration.SPED.uartParity;
      systemStatus.loraE32.uartBaudRate = configuration.SPED.uartBaudRate;
      systemStatus.loraE32.airDataRate = configuration.SPED.airDataRate;
      systemStatus.loraE32.fixedTransmission = configuration.OPTION.fixedTransmission;
      systemStatus.loraE32.ioDriveMode = configuration.OPTION.ioDriveMode;
      systemStatus.loraE32.wirelessWakeupTime = configuration.OPTION.wirelessWakeupTime;
      systemStatus.loraE32.fec = configuration.OPTION.fec;
      systemStatus.loraE32.transmissionPower = configuration.OPTION.transmissionPower;
      systemStatus.loraE32.frequency = configuration.getChannelDescription();
      systemStatus.loraE32.parityBit = configuration.SPED.getUARTParityDescription();
      systemStatus.loraE32.airDataRateStr = configuration.SPED.getAirDataRate();
      systemStatus.loraE32.uartBaudRateStr = configuration.SPED.getUARTBaudRate();
      systemStatus.loraE32.fixedTransmissionStr = configuration.OPTION.getFixedTransmissionDescription();
      systemStatus.loraE32.ioDriveModeStr = configuration.OPTION.getIODroveModeDescription();
      systemStatus.loraE32.wirelessWakeupTimeStr = configuration.OPTION.getWirelessWakeUPTimeDescription();
      systemStatus.loraE32.fecStr = configuration.OPTION.getFECDescription();
      systemStatus.loraE32.transmissionPowerStr = configuration.OPTION.getTransmissionPowerDescription();
      
      sendDebugMessage("LoRa E32 configuration read successfully");
    } else {
      Serial.print("Error reading configuration: ");
      Serial.println(configContainer.status.getResponseDescription());
      sendDebugMessage("Error reading LoRa E32 configuration: " + String(configContainer.status.getResponseDescription()));
    }
    
    if (configContainer.data != nullptr) {
      configContainer.close();
    }
  }
  
  Serial.println("=== LoRa E32 Initialization Complete ===");
}


// Save LoRa configuration to JSON
void saveLoRaConfig() {
  JSONVar config;
  JSONVar loraE32;
  loraE32["addh"] = systemStatus.loraE32.addh;
  loraE32["addl"] = systemStatus.loraE32.addl;
  loraE32["chan"] = systemStatus.loraE32.chan;
  loraE32["uartParity"] = systemStatus.loraE32.uartParity;
  loraE32["uartBaudRate"] = systemStatus.loraE32.uartBaudRate;
  loraE32["airDataRate"] = systemStatus.loraE32.airDataRate;
  loraE32["fixedTransmission"] = systemStatus.loraE32.fixedTransmission;
  loraE32["ioDriveMode"] = systemStatus.loraE32.ioDriveMode;
  loraE32["wirelessWakeupTime"] = systemStatus.loraE32.wirelessWakeupTime;
  loraE32["fec"] = systemStatus.loraE32.fec;
  loraE32["transmissionPower"] = systemStatus.loraE32.transmissionPower;
  loraE32["operatingMode"] = systemStatus.loraE32.operatingMode;
  config["loraE32"] = loraE32;

  String jsonString = JSON.stringify(config);
  
  File file = LittleFS.open("/lora_config.json", "w");
  if (file) {
    file.print(jsonString);
    file.close();
    Serial.println("LoRa E32 configuration saved");
    sendDebugMessage("LoRa E32 configuration saved to LittleFS");
  } else {
    Serial.println("Failed to save LoRa E32 configuration");
    sendDebugMessage("Failed to save LoRa E32 configuration");
  }
}

// Load LoRa configuration from JSON
void loadLoRaConfig() {
  if (LittleFS.exists("/lora_config.json")) {
    File file = LittleFS.open("/lora_config.json", "r");
    if (file) {
      String jsonString = file.readString();
      file.close();
      
      JSONVar config = JSON.parse(jsonString);
      
      if (JSON.typeof(config) == "undefined") {
        Serial.println("Failed to parse LoRa E32 configuration");
        sendDebugMessage("Failed to parse LoRa E32 configuration");
        return;
      }
      
      if (config.hasOwnProperty("loraE32")) {
        JSONVar loraE32 = config["loraE32"];
        systemStatus.loraE32.addh = (int)loraE32["addh"];
        systemStatus.loraE32.addl = (int)loraE32["addl"];
        systemStatus.loraE32.chan = (int)loraE32["chan"];
        systemStatus.loraE32.uartParity = (int)loraE32["uartParity"];
        systemStatus.loraE32.uartBaudRate = (int)loraE32["uartBaudRate"];
        systemStatus.loraE32.airDataRate = (int)loraE32["airDataRate"];
        systemStatus.loraE32.fixedTransmission = (int)loraE32["fixedTransmission"];
        systemStatus.loraE32.ioDriveMode = (int)loraE32["ioDriveMode"];
        systemStatus.loraE32.wirelessWakeupTime = (int)loraE32["wirelessWakeupTime"];
        systemStatus.loraE32.fec = (int)loraE32["fec"];
        systemStatus.loraE32.transmissionPower = (int)loraE32["transmissionPower"];
        systemStatus.loraE32.operatingMode = (int)loraE32["operatingMode"];
        
        // Áp dụng cấu hình LoRa
        setLoRaConfig(systemStatus.loraE32);
        // Áp dụng chế độ hoạt động
        setLoRaOperatingMode(systemStatus.loraE32.operatingMode);
        
        Serial.println("LoRa E32 configuration loaded");
        sendDebugMessage("LoRa E32 configuration loaded from LittleFS");
      }
    }
  }
}

// Set LoRa E32 configuration
void setLoRaConfig(LoRaE32Config config) {
  if (!systemStatus.loraE32.initialized) {
    sendDebugMessage("LoRa E32 not initialized, cannot set configuration");
    return;
  }
  
  // Lưu chế độ hoạt động hiện tại
  uint8_t previousMode = systemStatus.loraE32.operatingMode;

  // Chuyển module về chế độ Sleep để cấu hình
  setLoRaOperatingMode(3); // MODE_3_PROGRAM
  delay(500); // Tăng delay

  Configuration loraConfig;
  loraConfig.HEAD = 0xC0; // For WRITE_CFG_PWR_DWN_SAVE
  loraConfig.ADDH = config.addh;
  loraConfig.ADDL = config.addl;
  loraConfig.CHAN = config.chan;
  loraConfig.SPED.uartParity = config.uartParity;
  loraConfig.SPED.uartBaudRate = config.uartBaudRate;
  loraConfig.SPED.airDataRate = config.airDataRate;
  loraConfig.OPTION.fixedTransmission = config.fixedTransmission;
  loraConfig.OPTION.ioDriveMode = config.ioDriveMode;
  loraConfig.OPTION.wirelessWakeupTime = config.wirelessWakeupTime;
  loraConfig.OPTION.fec = config.fec;
  loraConfig.OPTION.transmissionPower = config.transmissionPower;

  ResponseStatus rs = e32ttl100.setConfiguration(loraConfig, WRITE_CFG_PWR_DWN_SAVE);
  if (rs.code == SUCCESS) {
    Serial.println("LoRa E32 configuration set successfully");
    sendDebugMessage("LoRa E32 configuration set successfully");
    
    // Cập nhật trạng thái hệ thống
    systemStatus.loraE32.addh = config.addh;
    systemStatus.loraE32.addl = config.addl;
    systemStatus.loraE32.chan = config.chan;
    systemStatus.loraE32.uartParity = config.uartParity;
    systemStatus.loraE32.uartBaudRate = config.uartBaudRate;
    systemStatus.loraE32.airDataRate = config.airDataRate;
    systemStatus.loraE32.fixedTransmission = config.fixedTransmission;
    systemStatus.loraE32.ioDriveMode = config.ioDriveMode;
    systemStatus.loraE32.wirelessWakeupTime = config.wirelessWakeupTime;
    systemStatus.loraE32.fec = config.fec;
    systemStatus.loraE32.transmissionPower = config.transmissionPower;
    systemStatus.loraE32.frequency = loraConfig.getChannelDescription();
    systemStatus.loraE32.parityBit = loraConfig.SPED.getUARTParityDescription();
    systemStatus.loraE32.airDataRateStr = loraConfig.SPED.getAirDataRate();
    systemStatus.loraE32.uartBaudRateStr = loraConfig.SPED.getUARTBaudRate();
    systemStatus.loraE32.fixedTransmissionStr = loraConfig.OPTION.getFixedTransmissionDescription();
    systemStatus.loraE32.ioDriveModeStr = loraConfig.OPTION.getIODroveModeDescription();
    systemStatus.loraE32.wirelessWakeupTimeStr = loraConfig.OPTION.getWirelessWakeUPTimeDescription();
    systemStatus.loraE32.fecStr = loraConfig.OPTION.getFECDescription();
    systemStatus.loraE32.transmissionPowerStr = loraConfig.OPTION.getTransmissionPowerDescription();
    
    // Lưu cấu hình vào LittleFS
    saveLoRaConfig();
    
    delay(500);
    
    // Đọc lại cấu hình để cập nhật trạng thái
    ResponseStructContainer configContainer = e32ttl100.getConfiguration();
    if (configContainer.status.code == 1 && configContainer.data != nullptr) {
      Configuration newConfig = *(Configuration*)configContainer.data;
      printParameters(newConfig);
      systemStatus.loraE32.frequency = newConfig.getChannelDescription();
      systemStatus.loraE32.parityBit = newConfig.SPED.getUARTParityDescription();
      systemStatus.loraE32.airDataRateStr = newConfig.SPED.getAirDataRate();
      systemStatus.loraE32.uartBaudRateStr = newConfig.SPED.getUARTBaudRate();
      systemStatus.loraE32.fixedTransmissionStr = newConfig.OPTION.getFixedTransmissionDescription();
      systemStatus.loraE32.ioDriveModeStr = newConfig.OPTION.getIODroveModeDescription();
      systemStatus.loraE32.wirelessWakeupTimeStr = newConfig.OPTION.getWirelessWakeUPTimeDescription();
      systemStatus.loraE32.fecStr = newConfig.OPTION.getFECDescription();
      systemStatus.loraE32.transmissionPowerStr = newConfig.OPTION.getTransmissionPowerDescription();
    } else {
      Serial.print("Error reading configuration: ");
      Serial.println(configContainer.status.getResponseDescription());
      sendDebugMessage("Error reading LoRa E32 configuration: " + String(configContainer.status.getResponseDescription()));
    }
    
    if (configContainer.data != nullptr) {
      configContainer.close();
    }
  } else {
    Serial.print("Error setting configuration: ");
    Serial.println(rs.getResponseDescription());
    sendDebugMessage("Error setting LoRa E32 configuration: " + String(rs.getResponseDescription()));
  }

  // Khôi phục chế độ hoạt động trước đó
  setLoRaOperatingMode(previousMode);
  delay(500);
}


// Set LoRa operating mode
void setLoRaOperatingMode(uint8_t mode) {
  switch (mode) {
    case 0: // Normal Mode (M0=0, M1=0)
      digitalWrite(E32_M0_PIN, LOW);
      digitalWrite(E32_M1_PIN, LOW);
      e32ttl100.setMode(MODE_0_NORMAL);
      systemStatus.loraE32.operatingMode = 0;
      sendDebugMessage("LoRa E32 set to Normal Mode");
      break;
    case 1: // Wake-Up Mode (M0=1, M1=0)
      digitalWrite(E32_M0_PIN, HIGH);
      digitalWrite(E32_M1_PIN, LOW);
      e32ttl100.setMode(MODE_1_WAKE_UP);
      systemStatus.loraE32.operatingMode = 1;
      sendDebugMessage("LoRa E32 set to Wake-Up Mode");
      break;
    case 2: // Power-Saving Mode (M0=0, M1=1)
      digitalWrite(E32_M0_PIN, LOW);
      digitalWrite(E32_M1_PIN, HIGH);
      e32ttl100.setMode(MODE_2_POWER_SAVING);
      systemStatus.loraE32.operatingMode = 2;
      sendDebugMessage("LoRa E32 set to Power-Saving Mode");
      break;
    case 3: // Sleep Mode (M0=1, M1=1)
      digitalWrite(E32_M0_PIN, HIGH);
      digitalWrite(E32_M1_PIN, HIGH);
      e32ttl100.setMode(MODE_3_PROGRAM);
      systemStatus.loraE32.operatingMode = 3;
      sendDebugMessage("LoRa E32 set to Sleep Mode");
      break;
    default:
      sendDebugMessage("Invalid LoRa E32 operating mode");
      return;
  }
  saveLoRaConfig(); // Lưu chế độ vào LittleFS
  delay(100); // Đợi module ổn định
}

// Print module information
void printModuleInformation(struct ModuleInformation moduleInformation) {
  Serial.println("======== MODULE INFORMATION ========");
  Serial.print("HEAD: ");
  Serial.print(moduleInformation.HEAD, HEX);
  Serial.print(" ");
  Serial.print(moduleInformation.HEAD, BIN);
  Serial.print(" ");
  Serial.println(moduleInformation.HEAD, DEC);
  
  Serial.print("Frequency: ");
  Serial.println(moduleInformation.frequency, HEX);
  
  Serial.print("Version: ");
  Serial.println(moduleInformation.version, HEX);
  
  Serial.print("Features: ");
  Serial.println(moduleInformation.features, HEX);
  
  Serial.println("===================================");
}

// Print configuration parameters
void printParameters(struct Configuration configuration) {
  Serial.println("======== LORA E32 CONFIGURATION ========");
  
  Serial.print("HEAD: ");
  Serial.print(configuration.HEAD, BIN);
  Serial.print(" (");
  Serial.print(configuration.HEAD, DEC);
  Serial.print(") 0x");
  Serial.println(configuration.HEAD, HEX);
  
  Serial.println();
  
  Serial.print("High Address (ADDH): ");
  Serial.print(configuration.ADDH, DEC);
  Serial.print(" (0x");
  Serial.print(configuration.ADDH, HEX);
  Serial.print(", ");
  Serial.print(configuration.ADDH, BIN);
  Serial.println(")");
  
  Serial.print("Low Address (ADDL): ");
  Serial.print(configuration.ADDL, DEC);
  Serial.print(" (0x");
  Serial.print(configuration.ADDL, HEX);
  Serial.print(", ");
  Serial.print(configuration.ADDL, BIN);
  Serial.println(")");
  
  Serial.print("Channel (CHAN): ");
  Serial.print(configuration.CHAN, DEC);
  Serial.print(" → ");
  Serial.println(configuration.getChannelDescription());
  
  Serial.println();
  Serial.println("=== SPEED CONFIGURATION (SPED) ===");
  
  Serial.print("UART Parity: ");
  Serial.print(configuration.SPED.uartParity, BIN);
  Serial.print(" → ");
  Serial.println(configuration.SPED.getUARTParityDescription());
  
  Serial.print("UART Baud Rate: ");
  Serial.print(configuration.SPED.uartBaudRate, BIN);
  Serial.print(" → ");
  Serial.println(configuration.SPED.getUARTBaudRate());
  
  Serial.print("Air Data Rate: ");
  Serial.print(configuration.SPED.airDataRate, BIN);
  Serial.print(" → ");
  Serial.println(configuration.SPED.getAirDataRate());
  
  Serial.println();
  Serial.println("=== OPTIONS ===");
  
  Serial.print("Fixed Transmission: ");
  Serial.print(configuration.OPTION.fixedTransmission, BIN);
  Serial.print(" → ");
  Serial.println(configuration.OPTION.getFixedTransmissionDescription());
  
  Serial.print("IO Drive Mode: ");
  Serial.print(configuration.OPTION.ioDriveMode, BIN);
  Serial.print(" → ");
  Serial.println(configuration.OPTION.getIODroveModeDescription());
  
  Serial.print("Wireless Wakeup Time: ");
  Serial.print(configuration.OPTION.wirelessWakeupTime, BIN);
  Serial.print(" → ");
  Serial.println(configuration.OPTION.getWirelessWakeUPTimeDescription());
  
  Serial.print("FEC (Forward Error Correction): ");
  Serial.print(configuration.OPTION.fec, BIN);
  Serial.print(" → ");
  Serial.println(configuration.OPTION.getFECDescription());
  
  Serial.print("Transmission Power: ");
  Serial.print(configuration.OPTION.transmissionPower, BIN);
  Serial.print(" → ");
  Serial.println(configuration.OPTION.getTransmissionPowerDescription());
  
  Serial.println("===================================");
}

// Save I/O configuration to JSON
void saveIOConfig() {
  JSONVar config;
  
  // Save output states
  JSONVar outputsArray;
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    JSONVar outputObj;
    outputObj["pin"] = systemStatus.outputs[i].pin;
    outputObj["state"] = systemStatus.outputs[i].state;
    outputsArray[i] = outputObj;
  }
  config["outputs"] = outputsArray;
  
  String jsonString = JSON.stringify(config);
  
  File file = LittleFS.open("/io_config.json", "w");
  if (file) {
    file.print(jsonString);
    file.close();
    Serial.println("I/O configuration saved");
  } else {
    Serial.println("Failed to save I/O configuration");
  }
}

// Load I/O configuration from JSON
void loadIOConfig() {
  if (LittleFS.exists("/io_config.json")) {
    File file = LittleFS.open("/io_config.json", "r");
    if (file) {
      String jsonString = file.readString();
      file.close();
      
      JSONVar config = JSON.parse(jsonString);
      
      if (JSON.typeof(config) == "undefined") {
        Serial.println("Failed to parse I/O configuration");
        return;
      }
      
      // Load output states
      if (config.hasOwnProperty("outputs")) {
        JSONVar outputsArray = config["outputs"];
        for (int i = 0; i < NUM_OUTPUTS; i++) {
          if (outputsArray.hasOwnProperty(String(i))) {
            JSONVar outputObj = outputsArray[i];
            int pin = (int)outputObj["pin"];
            bool state = (bool)outputObj["state"];
            
            digitalWrite(pin, state ? HIGH : LOW);
            systemStatus.outputs[i].state = state;
            systemStatus.outputs[i].stateStr = state ? "HIGH" : "LOW";
          }
        }
      }
      
      Serial.println("I/O configuration loaded");
    }
  }
}
// Save counter configuration to JSON
void saveCounterConfig() {
  JSONVar config;
  config["planDisplay"] = systemStatus.planDisplay;
  JSONVar countersArray;
  for (int i = 0; i < 4; i++) {
    JSONVar counterObj;
    counterObj["pin"] = systemStatus.counters[i].pin;
    counterObj["delayFilter"] = systemStatus.counters[i].delayFilter;
    counterObj["count"] = systemStatus.counters[i].count;
    countersArray[i] = counterObj;
  }
  config["counters"] = countersArray;

  String jsonString = JSON.stringify(config);
  
  File file = LittleFS.open("/counter_config.json", "w");
  if (file) {
    file.print(jsonString);
    file.close();
    Serial.println("Counter configuration saved");
    sendDebugMessage("Counter configuration saved to LittleFS");
  } else {
    Serial.println("Failed to save counter configuration");
    sendDebugMessage("Failed to save counter configuration");
  }
}
// Load counter configuration from JSON
void loadCounterConfig() {
  if (LittleFS.exists("/counter_config.json")) {
    File file = LittleFS.open("/counter_config.json", "r");
    if (file) {
      String jsonString = file.readString();
      file.close();
      
      JSONVar config = JSON.parse(jsonString);
      
      if (JSON.typeof(config) == "undefined") {
        Serial.println("Failed to parse counter configuration");
        sendDebugMessage("Failed to parse counter configuration");
        return;
      }
      
      systemStatus.planDisplay = (int)config["planDisplay"];
      JSONVar countersArray = config["counters"];
      for (int i = 0; i < 4; i++) {
        if (countersArray.hasOwnProperty(String(i))) {
          JSONVar counterObj = countersArray[i];
          systemStatus.counters[i].pin = (int)counterObj["pin"];
          systemStatus.counters[i].delayFilter = (int)counterObj["delayFilter"];
          systemStatus.counters[i].count = (int)counterObj["count"];
          systemStatus.counters[i].lastState = digitalRead(systemStatus.counters[i].pin);
          systemStatus.counters[i].lastDebounceTime = 0;
        }
      }
      
      Serial.println("Counter configuration loaded");
      sendDebugMessage("Counter configuration loaded from LittleFS");
    }
  } else {
    // Default configuration
    systemStatus.counters[0].pin = 5;
    systemStatus.counters[1].pin = 6;
    systemStatus.counters[2].pin = 7;
    systemStatus.counters[3].pin = 37;
    for (int i = 0; i < 4; i++) {
      systemStatus.counters[i].delayFilter = 50;
      systemStatus.counters[i].count = 0;
      systemStatus.counters[i].lastState = digitalRead(systemStatus.counters[i].pin);
      systemStatus.counters[i].lastDebounceTime = 0;
    }
    saveCounterConfig();
  }
}

// Save admin credentials to JSON
void saveAdminCredentials() {
  JSONVar config;
  config["username"] = systemStatus.adminCredentials.username;
  config["password"] = systemStatus.adminCredentials.password; // Should be hashed in production

  String jsonString = JSON.stringify(config);
  
  File file = LittleFS.open("/admin_credentials.json", "w");
  if (file) {
    file.print(jsonString);
    file.close();
    Serial.println("Admin credentials saved");
    sendDebugMessage("Admin credentials saved to LittleFS");
  } else {
    Serial.println("Failed to save admin credentials");
    sendDebugMessage("Failed to save admin credentials");
  }
}
// Load admin credentials from JSON
void loadAdminCredentials() {
  if (LittleFS.exists("/admin_credentials.json")) {
    File file = LittleFS.open("/admin_credentials.json", "r");
    if (file) {
      String jsonString = file.readString();
      file.close();
      
      JSONVar config = JSON.parse(jsonString);
      
      if (JSON.typeof(config) == "undefined") {
        Serial.println("Failed to parse admin credentials");
        sendDebugMessage("Failed to parse admin credentials");
        return;
      }
      
      systemStatus.adminCredentials.username = (const char*)config["username"];
      systemStatus.adminCredentials.password = (const char*)config["password"];
      
      Serial.println("Admin credentials loaded");
      sendDebugMessage("Admin credentials loaded from LittleFS");
    }
  } else {
    saveAdminCredentials();
  }
}
// Send counter status via WebSocket
void sendCounterStatus() {
  if (ws.count() > 0) {
    JSONVar response;
    response["action"] = "counter_status";
    response["planDisplay"] = systemStatus.planDisplay;
    JSONVar countersArray;
    for (int i = 0; i < 4; i++) {
      JSONVar counterObj;
      counterObj["pin"] = systemStatus.counters[i].pin;
      counterObj["delayFilter"] = systemStatus.counters[i].delayFilter;
      counterObj["count"] = systemStatus.counters[i].count;
      countersArray[i] = counterObj;
    }
    response["counters"] = countersArray;

    String jsonString = JSON.stringify(response);
    ws.textAll(jsonString);

    if (DEBUG_MODE) {
      Serial.println("Counter status sent to WebSocket");
    }
  }
}

// Update counter status with debounce
void updateCounterStatus() {
  unsigned long currentTime = millis();
  bool statusChanged = false;

  for (int i = 0; i < 4; i++) {
    bool currentState = digitalRead(systemStatus.counters[i].pin);
    
    if (currentState != systemStatus.counters[i].lastState) {
      systemStatus.counters[i].lastDebounceTime = currentTime;
    }

    if ((currentTime - systemStatus.counters[i].lastDebounceTime) > systemStatus.counters[i].delayFilter) {
      if (currentState != systemStatus.counters[i].lastState && currentState == LOW) { // Falling edge
        systemStatus.counters[i].count++;
        statusChanged = true;
        String message = String("Counter ") + (i+1) + " (Pin " + systemStatus.counters[i].pin + ") incremented to " + systemStatus.counters[i].count;
        sendDebugMessage(message);
      }
      systemStatus.counters[i].lastState = currentState;
    }
  }

  if (statusChanged) {
    saveCounterConfig();
    sendCounterStatus();
  }
}

// Check admin session timeout
void checkAdminTimeout() {
  if (systemStatus.adminMode && (millis() - lastAdminActivity) > ADMIN_TIMEOUT) {
    systemStatus.adminMode = false;
    sendDebugMessage("Admin session timed out");
  }
}
// Control output pin
void controlOutput(int pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
  
  // Update system status
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    if (OUTPUT_PINS[i] == pin) {
      systemStatus.outputs[i].state = state;
      systemStatus.outputs[i].stateStr = state ? "HIGH" : "LOW";
      break;
    }
  }
  
  // Save configuration
  saveIOConfig();
  
  Serial.printf("Output pin %d set to %s\n", pin, state ? "HIGH" : "LOW");
}

// Send debug message via WebSocket
void sendDebugMessage(const String& message) {
  if (ws.count() > 0) {
    JSONVar response;
    response["action"] = "debug";
    response["message"] = message;
    String jsonString = JSON.stringify(response);
    ws.textAll(jsonString);
    if (DEBUG_MODE) {
      Serial.println("Debug message sent to WebSocket: " + message);
    }
  }
}

// Send system status via WebSocket
void sendSystemStatus() {
  if (ws.count() > 0) {
    JSONVar response;
    response["action"] = "system_status";
    response["reset_count"] = systemStatus.resetCount;
    response["free_heap"] = (int)systemStatus.freeHeap;
    response["free_psram"] = (int)systemStatus.freePsram;
    response["temperature"] = systemStatus.temperature;
    response["ip_address"] = systemStatus.ipAddress;
    response["uptime"] = (int)systemStatus.uptime;

    // Add input states
    JSONVar inputsArray;
    for (int i = 0; i < NUM_INPUTS; i++) {
      JSONVar inputObj;
      inputObj["pin"] = systemStatus.inputs[i].pin;
      inputObj["state"] = systemStatus.inputs[i].stateStr;
      inputsArray[i] = inputObj;
    }
    response["inputs"] = inputsArray;

    // Add output states
    JSONVar outputsArray;
    for (int i = 0; i < NUM_OUTPUTS; i++) {
      JSONVar outputObj;
      outputObj["pin"] = systemStatus.outputs[i].pin;
      outputObj["state"] = systemStatus.outputs[i].stateStr;
      outputsArray[i] = outputObj;
    }
    response["outputs"] = outputsArray;

    // Add LoRa E32 information
    JSONVar loraE32Obj;
    loraE32Obj["initialized"] = systemStatus.loraE32.initialized;
    loraE32Obj["moduleInfo"] = systemStatus.loraE32.moduleInfo;
    loraE32Obj["addh"] = systemStatus.loraE32.addh;
    loraE32Obj["addl"] = systemStatus.loraE32.addl;
    loraE32Obj["chan"] = systemStatus.loraE32.chan;
    loraE32Obj["frequency"] = systemStatus.loraE32.frequency;
    loraE32Obj["airDataRate"] = systemStatus.loraE32.airDataRateStr;
    loraE32Obj["uartBaudRate"] = systemStatus.loraE32.uartBaudRateStr;
    loraE32Obj["transmissionPower"] = systemStatus.loraE32.transmissionPowerStr;
    loraE32Obj["parityBit"] = systemStatus.loraE32.parityBit;
    loraE32Obj["wirelessWakeupTime"] = systemStatus.loraE32.wirelessWakeupTimeStr;
    loraE32Obj["fec"] = systemStatus.loraE32.fecStr;
    loraE32Obj["fixedTransmission"] = systemStatus.loraE32.fixedTransmissionStr;
    loraE32Obj["ioDriveMode"] = systemStatus.loraE32.ioDriveModeStr;
    loraE32Obj["operatingMode"] = systemStatus.loraE32.operatingMode;
    response["loraE32"] = loraE32Obj;

    String jsonString = JSON.stringify(response);
    ws.textAll(jsonString);

    if (DEBUG_MODE) {
      Serial.println("Status sent to WebSocket");
    }
  }
}

// Handle WebSocket messages
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    
    JSONVar json = JSON.parse(message);
    
    if (JSON.typeof(json) == "undefined") {
      Serial.println("Invalid JSON received");
      sendDebugMessage("Invalid JSON received");
      return;
    }
    
    String action = JSON.stringify(json["action"]);
    action.replace("\"", "");
    
    if (action == "control_output") {
      int pin = (int)json["pin"];
      bool state = (bool)json["state"];
      controlOutput(pin, state);
    }
    else if (action == "get_lora_e32_config" || action == "get_counter_config") {
      sendSystemStatus();
      sendCounterStatus();
    }
    else if (action == "refresh_lora_e32") {
      initLoRaE32();
      sendSystemStatus();
    }
    else if (action == "set_lora_e32_config") {
      LoRaE32Config config;
      config.addh = (int)json["addh"];
      config.addl = (int)json["addl"];
      config.chan = (int)json["chan"];
      config.uartParity = (int)json["uartParity"];
      config.uartBaudRate = (int)json["uartBaudRate"];
      config.airDataRate = (int)json["airDataRate"];
      config.fixedTransmission = (int)json["fixedTransmission"];
      config.ioDriveMode = (int)json["ioDriveMode"];
      config.wirelessWakeupTime = (int)json["wirelessWakeupTime"];
      config.fec = (int)json["fec"];
      config.transmissionPower = (int)json["transmissionPower"];
      config.operatingMode = systemStatus.loraE32.operatingMode;
      
      setLoRaConfig(config);
      sendSystemStatus();
    }
    else if (action == "set_lora_operating_mode") {
      uint8_t mode = (int)json["mode"];
      setLoRaOperatingMode(mode);
      sendSystemStatus();
    }
    else if (action == "set_counter_config") {
      systemStatus.planDisplay = (int)json["planDisplay"];
      JSONVar countersArray = json["counters"];
      for (int i = 0; i < 4; i++) {
        systemStatus.counters[i].pin = (int)countersArray[i]["pin"];
        systemStatus.counters[i].delayFilter = (int)countersArray[i]["delayFilter"];
        systemStatus.counters[i].lastState = digitalRead(systemStatus.counters[i].pin);
        systemStatus.counters[i].lastDebounceTime = 0;
      }
      saveCounterConfig();
      sendCounterStatus();
    }
    else if (action == "admin_login") {
      String username = JSON.stringify(json["username"]);
      String password = JSON.stringify(json["password"]);
      username.replace("\"", "");
      password.replace("\"", "");
      
      JSONVar response;
      response["action"] = "login_result";
      if (username == systemStatus.adminCredentials.username && password == systemStatus.adminCredentials.password) {
        systemStatus.adminMode = true;
        lastAdminActivity = millis();
        response["success"] = true;
        sendDebugMessage("Admin login successful");
      } else {
        response["success"] = false;
        sendDebugMessage("Admin login failed");
      }
      ws.textAll(JSON.stringify(response));
    }
    else if (action == "change_admin_credentials" && systemStatus.adminMode) {
      String newUsername = JSON.stringify(json["username"]);
      String newPassword = JSON.stringify(json["password"]);
      newUsername.replace("\"", "");
      newPassword.replace("\"", "");
      
      systemStatus.adminCredentials.username = newUsername;
      systemStatus.adminCredentials.password = newPassword;
      saveAdminCredentials();
      sendDebugMessage("Admin credentials updated");
    }
  }
}

// WebSocket event handler
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      sendSystemStatus();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Initialize WebSocket
void initWebSocket() {
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/dashboard.html", "text/html");
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (systemStatus.adminMode) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(403, "text/plain", "Access denied. Please login as admin.");
    }
  });
  server.serveStatic("/", LittleFS, "/");
  server.begin();
  Serial.println("WebSocket server started");
}

// Get ESP32 temperature
float getTemperature() {
  return temperatureRead() + TEMP_OFFSET;
}

// Clear terminal screen
void clearTerminal() {
  Serial.print("\033[2J\033[H");
}

// Update system status
void updateSystemStatus() {
  static SystemStatus lastStatus;
  bool statusChanged = false;
  
  clearTerminal();
  
  systemStatus.resetCount = reset_counter;

  // Check input state changes
  for (int i = 0; i < NUM_INPUTS; i++) {
    bool currentState = digitalRead(INPUT_PINS[i]);
    if (systemStatus.inputs[i].state != currentState) {
      systemStatus.inputs[i].state = currentState;
      systemStatus.inputs[i].stateStr = currentState ? "HIGH" : "LOW";
      statusChanged = true;
      String message = String("Input pin ") + INPUT_PINS[i] + " changed to " + (currentState ? "HIGH" : "LOW");
      sendDebugMessage(message);
    }
  }

  updateCounterStatus(); // Update counter status

  size_t newFreeHeap = ESP.getFreeHeap();
  size_t newFreePsram = ESP.getFreePsram();
  
  if (abs((int)(systemStatus.freeHeap - newFreeHeap)) > 1024) {
    systemStatus.freeHeap = newFreeHeap;
    statusChanged = true;
  }
  
  if (abs((int)(systemStatus.freePsram - newFreePsram)) > 1024) {
    systemStatus.freePsram = newFreePsram;
    statusChanged = true;
  }

  float newTemp = getTemperature();
  if (abs(systemStatus.temperature - newTemp) > 0.5) {
    systemStatus.temperature = newTemp;
    statusChanged = true;
  }

  systemStatus.uptime = millis() - bootTime;

  checkAdminTimeout();

  // Send system status message
  String statusMessage = "=== System Status ===\n";
  statusMessage += String("Reset Count: ") + systemStatus.resetCount + "\n";
  statusMessage += "Inputs: ";
  for (int i = 0; i < NUM_INPUTS; i++) {
    statusMessage += String("Pin ") + systemStatus.inputs[i].pin + ": " + systemStatus.inputs[i].stateStr + " ";
  }
  statusMessage += "\nOutputs: ";
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    statusMessage += String("Pin ") + systemStatus.outputs[i].pin + ": " + systemStatus.outputs[i].stateStr + " ";
  }
  statusMessage += "\nCounters: ";
  for (int i = 0; i < 4; i++) {
    statusMessage += String("Pin ") + systemStatus.counters[i].pin + ": " + systemStatus.counters[i].count + " ";
  }
  statusMessage += String("\nFree Heap: ") + systemStatus.freeHeap + " bytes\n";
  statusMessage += String("Free PSRAM: ") + systemStatus.freePsram + " bytes\n";
  statusMessage += String("Temperature: ") + String(systemStatus.temperature, 2) + " °C\n";
  statusMessage += String("IP Address: ") + systemStatus.ipAddress + "\n";
  statusMessage += String("Uptime: ") + systemStatus.uptime + " ms\n";
  statusMessage += String("LoRa E32 Initialized: ") + (systemStatus.loraE32.initialized ? "YES" : "NO") + "\n";
  statusMessage += String("LoRa E32 Operating Mode: ") + (systemStatus.loraE32.operatingMode == 0 ? "Normal" : 
                                                           systemStatus.loraE32.operatingMode == 1 ? "Wake-Up" : 
                                                           systemStatus.loraE32.operatingMode == 2 ? "Power-Saving" : "Sleep") + "\n";
  statusMessage += String("Admin Mode: ") + (systemStatus.adminMode ? "Active" : "Inactive") + "\n";
  statusMessage += "====================";
  sendDebugMessage(statusMessage);

  if (statusChanged) {
    sendSystemStatus();
  }
}

// System monitor task
void systemMonitorTask(void *pvParameters) {
  const TickType_t monitorPeriod = pdMS_TO_TICKS(MONITOR_INTERVAL);

  while (1) {
    updateSystemStatus();
    vTaskDelay(monitorPeriod);
  }
}

// WebSocket task
void webSocketTask(void *pvParameters) {
  const TickType_t webSocketPeriod = pdMS_TO_TICKS(WEBSOCKET_UPDATE_INTERVAL);

  while (1) {
    ws.cleanupClients();
    vTaskDelay(webSocketPeriod);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("ESP32-S3 System Monitor Starting...");

  reset_counter++;
  bootTime = millis();

  initInputs();
  initOutputs();
  initWiFi();
  initLittleFS();
  initLoRaE32();
  initWebSocket();
  sendDebugMessage("WebSocket server started");

  systemStatus.resetCount = reset_counter;
  systemStatus.ipAddress = WiFi.localIP().toString();

  xTaskCreatePinnedToCore(
    systemMonitorTask,
    "SystemMonitor",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    webSocketTask,
    "WebSocketTask",
    4096,
    NULL,
    1,
    NULL,
    1
  );

  sendDebugMessage("System initialization complete!");
  sendDebugMessage("Tasks created and running on Core 1");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
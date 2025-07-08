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
void clearTerminal();
void controlOutput(int pin, bool state);
void printModuleInformation(struct ModuleInformation moduleInformation);
void printParameters(struct Configuration configuration);

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
}

// Initialize LoRa E32
void initLoRaE32() {
  Serial.println("=== Initializing LoRa E32 ===");
  
  // Khởi tạo Serial1 với các chân đã định nghĩa
  Serial1.begin(9600, SERIAL_8N1, E32_RX_PIN, E32_TX_PIN);
  
  // Khởi tạo LoRa E32
  e32ttl100.begin();
  
  // Load saved configuration if available
  loadLoRaConfig();
  
  Serial.println("Reading module information...");
  
  // Đọc thông tin module
  ResponseStructContainer c;
  c = e32ttl100.getModuleInformation();
  
  if (c.status.code == 1) {
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
  }
  c.close();
  
  delay(500);
  
  Serial.println("Reading current configuration...");
  
  // Đọc cấu hình hiện tại
  ResponseStructContainer configContainer;
  configContainer = e32ttl100.getConfiguration();
  
  if (configContainer.status.code == 1) {
    Configuration configuration = *(Configuration*)configContainer.data;
    Serial.println("Configuration read successfully!");
    
    printParameters(configuration);
    
    // lưu cấu hình vào system status
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
  
  configContainer.close();
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
        
        // Apply loaded configuration to LoRa module
        setLoRaConfig(systemStatus.loraE32);
        
        Serial.println("LoRa E32 configuration loaded");
        sendDebugMessage("LoRa E32 configuration loaded from LittleFS");
      }
    }
  }
}

// Set LoRa E32 configuration
void setLoRaConfig(LoRaE32Config config) {
  e32ttl100.setMode(MODE_3_PROGRAM);
  delay(100);

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
    
    // Update system status
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
    
    // Save configuration to LittleFS
    saveLoRaConfig();
    
    // Read back configuration to update system status
    ResponseStructContainer configContainer = e32ttl100.getConfiguration();
    if (configContainer.status.code == 1) {
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
      configContainer.close();
    } else {
      Serial.print("Error reading configuration: ");
      Serial.println(configContainer.status.getResponseDescription());
      sendDebugMessage("Error reading LoRa E32 configuration: " + String(configContainer.status.getResponseDescription()));
    }
  } else {
    Serial.print("Error setting configuration: ");
    Serial.println(rs.getResponseDescription());
    sendDebugMessage("Error setting LoRa E32 configuration: " + String(rs.getResponseDescription()));
  }

  e32ttl100.setMode(MODE_0_NORMAL);
  delay(100);
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
    else if (action == "get_lora_e32_config") {
      // Send current LoRa E32 configuration
      sendSystemStatus();
    }
    else if (action == "refresh_lora_e32") {
      // Refresh LoRa E32 configuration
      initLoRaE32();
      sendSystemStatus();
    }
    else if (action == "set_lora_e32_config") {
      // Set LoRa E32 configuration
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
      
      setLoRaConfig(config);
      sendSystemStatus();
    }
  }
}

// WebSocket event handler
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      // Send initial system status
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

  // Serve static files from LittleFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
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
  statusMessage += String("\nFree Heap: ") + systemStatus.freeHeap + " bytes\n";
  statusMessage += String("Free PSRAM: ") + systemStatus.freePsram + " bytes\n";
  statusMessage += String("Temperature: ") + String(systemStatus.temperature, 2) + " °C\n";
  statusMessage += String("IP Address: ") + systemStatus.ipAddress + "\n";
  statusMessage += String("Uptime: ") + systemStatus.uptime + " ms\n";
  statusMessage += String("LoRa E32 Initialized: ") + (systemStatus.loraE32.initialized ? "YES" : "NO") + "\n";
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
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("ESP32-S3 System Monitor Starting...");

  // Increment reset counter
  reset_counter++;
  bootTime = millis();

  // Initialize system components
  initInputs();
  initOutputs();
  initWiFi();
  initLittleFS();
  
  // Load I/O configuration
  loadIOConfig();
  
  // Initialize LoRa E32
  initLoRaE32();
  
  // Initialize WebSocket
  initWebSocket();
  sendDebugMessage("WebSocket server started");

  // Initialize system status
  systemStatus.resetCount = reset_counter;
  systemStatus.ipAddress = WiFi.localIP().toString();

  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(
    systemMonitorTask,    // Task function
    "SystemMonitor",      // Task name
    4096,                 // Stack size
    NULL,                 // Task parameters
    2,                    // Priority
    NULL,                 // Task handle
    1                     // Core ID
  );

  xTaskCreatePinnedToCore(
    webSocketTask,        // Task function
    "WebSocketTask",      // Task name
    4096,                 // Stack size
    NULL,                 // Task parameters
    1,                    // Priority
    NULL,                 // Task handle
    1                     // Core ID
  );

  sendDebugMessage("System initialization complete!");
  sendDebugMessage("Tasks created and running on Core 1");
}

void loop() {
  // Main loop is kept minimal to prevent blocking
  vTaskDelay(pdMS_TO_TICKS(1000));
}
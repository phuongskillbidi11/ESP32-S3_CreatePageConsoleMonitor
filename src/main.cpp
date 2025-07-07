#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <LoRa.h>
#include "config.h"

// Global variables
AsyncWebServer server(WEBSOCKET_PORT);
AsyncWebSocket ws(WEBSOCKET_PATH);
SystemStatus systemStatus;
LoRaConfig loraConfig;
unsigned long lastMonitorTime = 0;
unsigned long lastWebSocketTime = 0;
unsigned long bootTime = 0;



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

// Initialize LoRa
void initLoRa() {
  // Set LoRa pins
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
  
  if (!LoRa.begin(loraConfig.frequency)) {
    Serial.println("Starting LoRa failed!");
    return;
  }
  
  // Set LoRa parameters
  LoRa.setSpreadingFactor(loraConfig.spreadingFactor);
  LoRa.setSignalBandwidth(loraConfig.bandwidth);
  LoRa.setCodingRate4(loraConfig.codingRate);
  LoRa.setPreambleLength(loraConfig.preambleLength);
  LoRa.setSyncWord(loraConfig.syncWord);
  LoRa.setTxPower(loraConfig.txPower);
  
  Serial.println("LoRa initialized successfully");
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

// Save LoRa configuration to JSON
void saveLoRaConfig() {
  JSONVar config;
  config["frequency"] = loraConfig.frequency;
  config["spreadingFactor"] = loraConfig.spreadingFactor;
  config["bandwidth"] = loraConfig.bandwidth;
  config["codingRate"] = loraConfig.codingRate;
  config["preambleLength"] = loraConfig.preambleLength;
  config["syncWord"] = loraConfig.syncWord;
  config["txPower"] = loraConfig.txPower;
  
  String jsonString = JSON.stringify(config);
  
  File file = LittleFS.open("/lora_config.json", "w");
  if (file) {
    file.print(jsonString);
    file.close();
    Serial.println("LoRa configuration saved");
  } else {
    Serial.println("Failed to save LoRa configuration");
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
        Serial.println("Failed to parse LoRa configuration");
        return;
      }
      
      // Load LoRa parameters
      if (config.hasOwnProperty("frequency")) loraConfig.frequency = (long)config["frequency"];
      if (config.hasOwnProperty("spreadingFactor")) loraConfig.spreadingFactor = (int)config["spreadingFactor"];
      if (config.hasOwnProperty("bandwidth")) loraConfig.bandwidth = (long)config["bandwidth"];
      if (config.hasOwnProperty("codingRate")) loraConfig.codingRate = (int)config["codingRate"];
      if (config.hasOwnProperty("preambleLength")) loraConfig.preambleLength = (long)config["preambleLength"];
      if (config.hasOwnProperty("syncWord")) loraConfig.syncWord = (int)config["syncWord"];
      if (config.hasOwnProperty("txPower")) loraConfig.txPower = (int)config["txPower"];
      
      Serial.println("LoRa configuration loaded");
    }
  }
}

// Clear terminal screen
void clearTerminal() {
  Serial.print("\033[2J\033[H");
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
      return;
    }
    
    String action = JSON.stringify(json["action"]);
    action.replace("\"", "");
    
    if (action == "control_output") {
      int pin = (int)json["pin"];
      bool state = (bool)json["state"];
      controlOutput(pin, state);
    }
    else if (action == "save_lora_config") {
      // Update LoRa configuration
      if (json.hasOwnProperty("frequency")) loraConfig.frequency = (long)json["frequency"];
      if (json.hasOwnProperty("spreadingFactor")) loraConfig.spreadingFactor = (int)json["spreadingFactor"];
      if (json.hasOwnProperty("bandwidth")) loraConfig.bandwidth = (long)json["bandwidth"];
      if (json.hasOwnProperty("codingRate")) loraConfig.codingRate = (int)json["codingRate"];
      if (json.hasOwnProperty("preambleLength")) loraConfig.preambleLength = (long)json["preambleLength"];
      if (json.hasOwnProperty("syncWord")) loraConfig.syncWord = (int)json["syncWord"];
      if (json.hasOwnProperty("txPower")) loraConfig.txPower = (int)json["txPower"];
      
      saveLoRaConfig();
      
      // Reinitialize LoRa with new settings
      LoRa.end();
      delay(100);
      initLoRa();
      
      // Send confirmation
      JSONVar response;
      response["action"] = "lora_config_saved";
      response["success"] = true;
      String jsonString = JSON.stringify(response);
      ws.textAll(jsonString);
    }
    else if (action == "get_lora_config") {
      // Send current LoRa configuration
      JSONVar response;
      response["action"] = "lora_config_data";
      response["frequency"] = loraConfig.frequency;
      response["spreadingFactor"] = loraConfig.spreadingFactor;
      response["bandwidth"] = loraConfig.bandwidth;
      response["codingRate"] = loraConfig.codingRate;
      response["preambleLength"] = loraConfig.preambleLength;
      response["syncWord"] = loraConfig.syncWord;
      response["txPower"] = loraConfig.txPower;
      
      String jsonString = JSON.stringify(response);
      ws.textAll(jsonString);
    }
  }
}
// Trong phần khai báo hàm
void sendDebugMessage(const String& message);

// Gửi thông báo debug qua WebSocket
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

    String jsonString = JSON.stringify(response);
    ws.textAll(jsonString);

    if (DEBUG_MODE) {
      Serial.println("Status sent to WebSocket");
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

// Update system status
void updateSystemStatus() {
  static SystemStatus lastStatus;
  bool statusChanged = false;
  
  clearTerminal();
  
  systemStatus.resetCount = reset_counter;

  // Gửi thông báo thay đổi trạng thái đầu vào
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

  // Gửi thông báo trạng thái hệ thống
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

// Function declarations
void initInputs();
void initOutputs();
void initWiFi();
void initLittleFS();
void initWebSocket();
void initLoRa();
void updateSystemStatus();
void sendSystemStatus();
void monitorInputs();
float getTemperature();
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void systemMonitorTask(void *pvParameters);
void webSocketTask(void *pvParameters);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void saveIOConfig();
void loadIOConfig();
void saveLoRaConfig();
void loadLoRaConfig();
void clearTerminal();
void controlOutput(int pin, bool state);

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
  
  // Load configurations
  loadIOConfig();
  loadLoRaConfig();
  
  // Initialize LoRa and WebSocket
  initLoRa();
  if (!LoRa.begin(loraConfig.frequency)) {
    sendDebugMessage("Starting LoRa failed!");
  } else {
    sendDebugMessage("LoRa initialized successfully");
  }
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
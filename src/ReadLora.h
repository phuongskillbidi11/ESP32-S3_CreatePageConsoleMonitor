/*
 * LoRa E32-TTL-100 Configuration Reader
 * ESP32 S3 N16R8 with LoRa E32 module
 * 
 * Sơ đồ đấu dây:
 * E32 M0  → ESP32 chân 15
 * E32 M1  → ESP32 chân 16  
 * E32 TX  → ESP32 chân 18 (RX của Serial1)
 * E32 RX  → ESP32 chân 17 (TX của Serial1)
 * E32 VCC → 3.3V
 * E32 GND → GND
 * AUX     → Không kết nối
 */

#include "Arduino.h"
#include "LoRa_E32.h"

// Định nghĩa các chân kết nối
#define E32_M0_PIN    15
#define E32_M1_PIN    16
#define E32_TX_PIN    17  // TX của ESP32 → RX của E32
#define E32_RX_PIN    18  // RX của ESP32 → TX của E32
#define E32_AUX_PIN   -1  // Không sử dụng AUX

// Khởi tạo LoRa E32 với Serial1 và các chân điều khiển
LoRa_E32 e32ttl100(&Serial1, E32_AUX_PIN, E32_M0_PIN, E32_M1_PIN);

// Hàm in thông tin cấu hình
void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== ESP32 S3 LoRa E32 Configuration Reader ===");
    Serial.println("Khởi tạo Serial1 và LoRa E32...");
    
    // Khởi tạo Serial1 với các chân đã định nghĩa
    Serial1.begin(9600, SERIAL_8N1, E32_RX_PIN, E32_TX_PIN);
    
    // Khởi tạo LoRa E32
    e32ttl100.begin();
    
    Serial.println("Đang đọc thông tin module...");
    
    // Đọc thông tin module
    ResponseStructContainer c;
    c = e32ttl100.getModuleInformation();
    
    if (c.status.code == 1) {
        ModuleInformation mi = *(ModuleInformation*)c.data;
        printModuleInformation(mi);
    } else {
        Serial.print("Lỗi khi đọc thông tin module: ");
        Serial.println(c.status.getResponseDescription());
    }
    c.close();
    
    delay(500);
    
    Serial.println("Đang đọc cấu hình hiện tại...");
    
    // Đọc cấu hình hiện tại
    ResponseStructContainer configContainer;
    configContainer = e32ttl100.getConfiguration();
    
    if (configContainer.status.code == 1) {
        Configuration configuration = *(Configuration*)configContainer.data;
        Serial.println("Đọc cấu hình thành công!");
        Serial.print("Trạng thái: ");
        Serial.println(configContainer.status.getResponseDescription());
        
        printParameters(configuration);
    } else {
        Serial.print("Lỗi khi đọc cấu hình: ");
        Serial.println(configContainer.status.getResponseDescription());
        Serial.print("Mã lỗi: ");
        Serial.println(configContainer.status.code);
    }
    
    configContainer.close();
}

void loop() {
    // Không cần làm gì trong loop
    delay(10000);
}

void printModuleInformation(struct ModuleInformation moduleInformation) {
    Serial.println("======== THÔNG TIN MODULE ========");
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

void printParameters(struct Configuration configuration) {
    Serial.println("======== CẤU HÌNH LORA E32 ========");
    
    Serial.print("HEAD: ");
    Serial.print(configuration.HEAD, BIN);
    Serial.print(" (");
    Serial.print(configuration.HEAD, DEC);
    Serial.print(") 0x");
    Serial.println(configuration.HEAD, HEX);
    
    Serial.println();
    
    Serial.print("Địa chỉ cao (ADDH): ");
    Serial.print(configuration.ADDH, DEC);
    Serial.print(" (0x");
    Serial.print(configuration.ADDH, HEX);
    Serial.print(", ");
    Serial.print(configuration.ADDH, BIN);
    Serial.println(")");
    
    Serial.print("Địa chỉ thấp (ADDL): ");
    Serial.print(configuration.ADDL, DEC);
    Serial.print(" (0x");
    Serial.print(configuration.ADDL, HEX);
    Serial.print(", ");
    Serial.print(configuration.ADDL, BIN);
    Serial.println(")");
    
    Serial.print("Kênh (CHAN): ");
    Serial.print(configuration.CHAN, DEC);
    Serial.print(" → ");
    Serial.println(configuration.getChannelDescription());
    
    Serial.println();
    Serial.println("=== CẤU HÌNH TỐC ĐỘ (SPED) ===");
    
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
    Serial.println("=== TÙY CHỌN (OPTION) ===");
    
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
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
    
    Serial.println("=== ESP32 S3 LoRa E32 Configuration Setter ===");
    Serial.println("Khởi tạo Serial1 và LoRa E32...");
    
    // Khởi tạo Serial1 với các chân đã định nghĩa
    Serial1.begin(9600, SERIAL_8N1, E32_RX_PIN, E32_TX_PIN);
    
    // Khởi tạo LoRa E32
    e32ttl100.begin();
    
    // Thiết lập chế độ cấu hình (MODE_3_PROGRAM)
    e32ttl100.setMode(MODE_3_PROGRAM);
    delay(100);
    
    // Thiết lập cấu hình mới
    Serial.println("Đang thiết lập cấu hình mới...");
    
    Configuration configuration;
    configuration.HEAD = 0xC0;  // Đặt HEAD cho lưu cấu hình
    configuration.ADDH = 0x00;  // Địa chỉ cao
    configuration.ADDL = 0x01;  // Địa chỉ thấp
    configuration.CHAN = 23;    // Kênh 23 (410MHz + 23 = 433MHz)
    
    // Cấu hình tốc độ (SPED)
    configuration.SPED.uartParity = 0b00;  // 8N1 (No parity)
    configuration.SPED.uartBaudRate = 0b011;  // 9600 baud
    configuration.SPED.airDataRate = 0b010;   // 2.4kbps
    
    // Tùy chọn (OPTION)
    configuration.OPTION.fixedTransmission = 0b0;  // Truyền trong suốt
    configuration.OPTION.ioDriveMode = 0b1;       // Push-pull
    configuration.OPTION.wirelessWakeupTime = 0b000;  // 250ms
    configuration.OPTION.fec = 0b1;               // FEC bật
    configuration.OPTION.transmissionPower = 0b11; // 20dBm (tùy thuộc vào phiên bản module)
    
    // Gửi cấu hình đến module
    ResponseStatus rs = e32ttl100.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == SUCCESS) {
        Serial.println("Thiết lập cấu hình thành công!");
        Serial.print("Trạng thái: ");
        Serial.println(rs.getResponseDescription());
        
        // Đọc lại cấu hình để xác nhận
        Serial.println("Đang đọc lại cấu hình để xác nhận...");
        ResponseStructContainer configContainer = e32ttl100.getConfiguration();
        if (configContainer.status.code == SUCCESS) {
            Configuration newConfig = *(Configuration*)configContainer.data;
            printParameters(newConfig);
            configContainer.close();
        } else {
            Serial.print("Lỗi khi đọc lại cấu hình: ");
            Serial.println(configContainer.status.getResponseDescription());
        }
    } else {
        Serial.print("Lỗi khi thiết lập cấu hình: ");
        Serial.println(rs.getResponseDescription());
    }
    
    // Đặt lại chế độ bình thường
    e32ttl100.setMode(MODE_0_NORMAL);
    delay(100);
    
    // Đọc thông tin module
    Serial.println("Đang đọc thông tin module...");
    ResponseStructContainer c = e32ttl100.getModuleInformation();
    if (c.status.code == SUCCESS) {
        ModuleInformation mi = *(ModuleInformation*)c.data;
        printModuleInformation(mi);
        c.close();
    } else {
        Serial.print("Lỗi khi đọc thông tin module: ");
        Serial.println(c.status.getResponseDescription());
    }
}

void loop() {
    // Không cần làm gì trong loop
    delay(10000);
}

void printModuleInformation(struct ModuleInformation moduleInformation) {
    Serial.println("======== THÔNG TIN MODULE ========");
    Serial.print("HEAD: 0x");
    Serial.println(moduleInformation.HEAD, HEX);
    Serial.print("Frequency: 0x");
    Serial.println(moduleInformation.frequency, HEX);
    Serial.print("Version: 0x");
    Serial.println(moduleInformation.version, HEX);
    Serial.print("Features: 0x");
    Serial.println(moduleInformation.features, HEX);
    Serial.println("===================================");
}

void printParameters(struct Configuration configuration) {
    Serial.println("======== CẤU HÌNH LORA E32 ========");
    
    Serial.print("HEAD: 0x");
    Serial.println(configuration.HEAD, HEX);
    
    Serial.print("Địa chỉ cao (ADDH): 0x");
    Serial.println(configuration.ADDH, HEX);
    
    Serial.print("Địa chỉ thấp (ADDL): 0x");
    Serial.println(configuration.ADDL, HEX);
    
    Serial.print("Kênh (CHAN): ");
    Serial.print(configuration.CHAN, DEC);
    Serial.print(" → ");
    Serial.println(configuration.getChannelDescription());
    
    Serial.println("=== CẤU HÌNH TỐC ĐỘ (SPED) ===");
    Serial.print("UART Parity: ");
    Serial.println(configuration.SPED.getUARTParityDescription());
    Serial.print("UART Baud Rate: ");
    Serial.println(configuration.SPED.getUARTBaudRate());
    Serial.print("Air Data Rate: ");
    Serial.println(configuration.SPED.getAirDataRate());
    
    Serial.println("=== TÙY CHỌN (OPTION) ===");
    Serial.print("Fixed Transmission: ");
    Serial.println(configuration.OPTION.getFixedTransmissionDescription());
    Serial.print("IO Drive Mode: ");
    Serial.println(configuration.OPTION.getIODroveModeDescription());
    Serial.print("Wireless Wakeup Time: ");
    Serial.println(configuration.OPTION.getWirelessWakeUPTimeDescription());
    Serial.print("FEC: ");
    Serial.println(configuration.OPTION.getFECDescription());
    Serial.print("Transmission Power: ");
    Serial.println(configuration.OPTION.getTransmissionPowerDescription());
    
    Serial.println("===================================");
}
#include <Arduino.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>

// Định nghĩa các chân GPIO
const int buttonPins[] = {37, 38, 39, 40};
const int numButtons = 4;

// Biến đếm sản phẩm cho mỗi nút
int productCount[numButtons] = {0, 0, 0, 0};

// Biến debounce
unsigned long lastDebounceTime[numButtons] = {0, 0, 0, 0};
bool lastButtonState[numButtons] = {HIGH, HIGH, HIGH, HIGH};
bool buttonState[numButtons] = {HIGH, HIGH, HIGH, HIGH};

// Thời gian debounce filter (milliseconds)
const unsigned long delayFilter = 50;

// Tên file lưu trữ
const char* dataFile = "/product_count.json";

void saveData() {
  // Tạo JSON object
  JSONVar doc;
  
  // Thêm dữ liệu vào JSON
  for (int i = 0; i < numButtons; i++) {
    String buttonKey = "button_" + String(buttonPins[i]);
    doc[buttonKey] = productCount[i];
  }
  
  // Mở file để ghi
  File file = LittleFS.open(dataFile, "w");
  if (!file) {
    Serial.println("Lỗi mở file để ghi");
    return;
  }
  
  // Ghi JSON vào file
  String jsonString = JSON.stringify(doc);
  file.print(jsonString);
  file.close();
  
  Serial.println("Dữ liệu đã được lưu vào LittleFS");
}

void checkButton(int buttonIndex) {
  // Đọc trạng thái hiện tại của nút
  int reading = digitalRead(buttonPins[buttonIndex]);
  
  // Nếu trạng thái thay đổi (do noise hoặc nhấn nút)
  if (reading != lastButtonState[buttonIndex]) {
    // Reset timer debounce
    lastDebounceTime[buttonIndex] = millis();
  }
  
  // Nếu đã qua thời gian debounce
  if ((millis() - lastDebounceTime[buttonIndex]) > delayFilter) {
    // Nếu trạng thái thực sự thay đổi
    if (reading != buttonState[buttonIndex]) {
      buttonState[buttonIndex] = reading;
      
      // Chỉ đếm khi nút chuyển từ HIGH sang LOW (nhấn xuống)
      if (buttonState[buttonIndex] == LOW) {
        productCount[buttonIndex]++;
        Serial.printf("Nút %d (GPIO %d) được nhấn - Tổng sản phẩm: %d\n", buttonIndex + 1, buttonPins[buttonIndex], productCount[buttonIndex]);
        
        // Lưu dữ liệu vào file
        saveData();
      }
    }
  }
  
  // Lưu trạng thái cho lần đọc tiếp theo
  lastButtonState[buttonIndex] = reading;
}



void loadData() {
  // Kiểm tra xem file có tồn tại không
  if (!LittleFS.exists(dataFile)) {
    Serial.println("File dữ liệu không tồn tại, tạo mới...");
    saveData();
    return;
  }
  
  // Mở file để đọc
  File file = LittleFS.open(dataFile, "r");
  if (!file) {
    Serial.println("Lỗi mở file để đọc");
    return;
  }
  
  // Đọc nội dung file
  String content = file.readString();
  file.close();
  
  // Parse JSON
  JSONVar doc = JSON.parse(content);
  
  if (JSON.typeof(doc) == "undefined") {
    Serial.println("Lỗi đọc JSON từ file");
    return;
  }
  
  // Lấy dữ liệu từ JSON
  for (int i = 0; i < numButtons; i++) {
    String buttonKey = "button_" + String(buttonPins[i]);
    if (doc.hasOwnProperty(buttonKey)) {
      productCount[i] = (int)doc[buttonKey];
    }
  }
  
  Serial.println("Dữ liệu đã được tải từ LittleFS");
}

// Hàm reset counter (có thể gọi qua Serial)
void resetCounters() {
  for (int i = 0; i < numButtons; i++) {
    productCount[i] = 0;
  }
  saveData();
  Serial.println("Đã reset tất cả counter về 0");
}

// Hàm hiển thị thống kê
void printStats() {
  Serial.println("\n=== THỐNG KÊ SẢN PHẨM ===");
  int total = 0;
  for (int i = 0; i < numButtons; i++) {
    Serial.printf("Nút %d (GPIO %d): %d sản phẩm\n", 
                 i + 1, buttonPins[i], productCount[i]);
    total += productCount[i];
  }
  Serial.printf("Tổng cộng: %d sản phẩm\n", total);
  Serial.println("=======================\n");
}

// Xử lý lệnh Serial
void serialEvent() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command == "reset") {
      resetCounters();
    } else if (command == "stats") {
      printStats();
    } else if (command == "help") {
      Serial.println("Lệnh có sẵn:");
      Serial.println("- reset: Reset tất cả counter về 0");
      Serial.println("- stats: Hiển thị thống kê");
      Serial.println("- help: Hiển thị trợ giúp");
    }
  }
}
void setup() {
  Serial.begin(115200);
  
  // Khởi tạo LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Lỗi khởi tạo LittleFS");
    return;
  }
  
  // Cấu hình các chân GPIO
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonState[i] = digitalRead(buttonPins[i]);
  }
  
  // Đọc dữ liệu từ file
  loadData();
  
  // In thông tin khởi tạo
  Serial.println("ESP32-S3 Product Counter khởi tạo thành công!");
  Serial.println("Cấu hình chân GPIO:");
  for (int i = 0; i < numButtons; i++) {
    Serial.printf("Nút %d - GPIO %d: %d sản phẩm\n", i+1, buttonPins[i], productCount[i]);
  }
  Serial.println("Nhấn nút để đếm sản phẩm...");
}

void loop() {
  // Kiểm tra từng nút
  for (int i = 0; i < numButtons; i++) {
    checkButton(i);
  }
  
  delay(10); // Delay nhỏ để tránh quá tải CPU
}
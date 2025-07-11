1. Fields from Backend (ESP32) to Web
These fields are defined in the sendSystemStatus() and sendCounterStatus() functions in main.cpp, reflecting the system status, LoRa configuration, and product counting data.

a. Fields in system_status
action: "system_status"
Comment: Indicates the action type, used to identify the system status update message.
reset_count: Integer
Comment: Represents the number of times the ESP32 has reset.
free_heap: Integer (bytes)
Comment: Indicates the available heap memory in bytes.
free_psram: Integer (bytes)
Comment: Indicates the available PSRAM memory in bytes.
temperature: Float (°C)
Comment: Represents the current temperature of the ESP32, adjusted by TEMP_OFFSET.
ip_address: String
Comment: Contains the IP address of the ESP32 on the network.
uptime: Integer (milliseconds)
Comment: Represents the uptime of the system since the last boot.
inputs: JSON Array
Comment: Array of input pin statuses.
pin: Integer
Comment: The pin number of the input.
state: String ("HIGH" or "LOW")
Comment: The current state of the input pin.
outputs: JSON Array
Comment: Array of output pin statuses.
pin: Integer
Comment: The pin number of the output.
state: String ("HIGH" or "LOW")
Comment: The current state of the output pin.
loraE32: JSON Object
Comment: Contains detailed configuration and status of the LoRa E32 module.
initialized: Boolean (true or false)
Comment: Indicates whether the LoRa module is successfully initialized.
moduleInfo: String
Comment: Provides module information (e.g., "HEAD: C0, Freq: 64, Ver: 10, Features: 0").
addh: Integer (0-255)
Comment: High address byte of the LoRa module.
addl: Integer (0-255)
Comment: Low address byte of the LoRa module.
chan: Integer (0-31)
Comment: Channel number for LoRa communication.
frequency: String
Comment: Frequency description (e.g., "434.6MHz").
airDataRate: String
Comment: Air data rate (e.g., "2.4kbps").
uartBaudRate: String
Comment: UART baud rate (e.g., "9600").
transmissionPower: String
Comment: Transmission power (e.g., "20dBm").
parityBit: String
Comment: Parity bit setting (e.g., "8N1").
wirelessWakeupTime: String
Comment: Wireless wakeup time (e.g., "250ms").
fec: String
Comment: Forward Error Correction status (e.g., "On").
fixedTransmission: String
Comment: Transmission mode (e.g., "Transparent").
ioDriveMode: String
Comment: I/O drive mode (e.g., "Push-pull").
operatingMode: Integer (0-3)
Comment: Current operating mode (0: Normal, 1: Wake-Up, 2: Power-Saving, 3: Sleep).
b. Fields in counter_status
action: "counter_status"
Comment: Indicates the action type, used to identify the counter status update message.
planDisplay: Integer (0-3)
Comment: Index of the selected plan for display.
counters: JSON Array
Comment: Array of counter configurations.
pin: Integer
Comment: The pin number used for the counter.
delayFilter: Integer (milliseconds)
Comment: Debounce delay filter duration in milliseconds.
count: Integer
Comment: The current count value for the counter.
c. Fields in debug
action: "debug"
Comment: Indicates the action type, used to send debug messages.
message: String
Comment: Contains the debug message to be displayed on the web terminal.
d. Fields in login_result
action: "login_result"
Comment: Indicates the action type, used to send login result.
success: Boolean (true or false)
Comment: Indicates whether the login attempt was successful.
2. Fields from Web to Backend (ESP32)
These fields are sent from JavaScript functions in dashboard.js and script.js via WebSocket, processed in handleWebSocketMessage() in main.cpp.

a. Fields in control_output
action: "control_output"
Comment: Indicates the action type, used to control an output pin.
pin: Integer
Comment: The pin number of the output to control.
state: Boolean (true or false)
Comment: The desired state (true for HIGH, false for LOW).
b. Fields in get_lora_e32_config or get_counter_config
action: "get_lora_e32_config" or "get_counter_config"
Comment: Indicates the action type, requests the current LoRa or counter configuration.
c. Fields in refresh_lora_e32
action: "refresh_lora_e32"
Comment: Indicates the action type, requests a refresh of the LoRa E32 configuration.
d. Fields in set_lora_e32_config
action: "set_lora_e32_config"
Comment: Indicates the action type, used to set a new LoRa E32 configuration.
addh: Integer (0-255)
Comment: High address byte to set.
addl: Integer (0-255)
Comment: Low address byte to set.
chan: Integer (0-31)
Comment: Channel number to set.
uartParity: Integer (0-2)
Comment: Parity bit setting to set (0: 8N1, 1: 8O1, 2: 8E1).
uartBaudRate: Integer (0-7)
Comment: UART baud rate to set (e.g., 0: 1200, 3: 9600).
airDataRate: Integer (0-5)
Comment: Air data rate to set (e.g., 2: 2.4kbps).
fixedTransmission: Integer (0-1)
Comment: Transmission mode to set (0: Transparent, 1: Fixed).
ioDriveMode: Integer (0-1)
Comment: I/O drive mode to set (0: Open-collector, 1: Push-pull).
wirelessWakeupTime: Integer (0-7)
Comment: Wireless wakeup time to set (e.g., 0: 250ms).
fec: Integer (0-1)
Comment: FEC setting to set (0: Off, 1: On).
transmissionPower: Integer (0-3)
Comment: Transmission power to set (e.g., 3: 20dBm).
e. Fields in set_lora_operating_mode
action: "set_lora_operating_mode"
Comment: Indicates the action type, used to set the LoRa operating mode.
mode: Integer (0-3)
Comment: The operating mode to set (0: Normal, 1: Wake-Up, 2: Power-Saving, 3: Sleep).
f. Fields in set_counter_config
action: "set_counter_config"
Comment: Indicates the action type, used to set a new counter configuration.
planDisplay: Integer (0-3)
Comment: Index of the plan to display.
counters: JSON Array
Comment: Array of counter configurations to set.
pin: Integer
Comment: The pin number for the counter.
delayFilter: Integer (milliseconds)
Comment: Debounce delay filter duration in milliseconds.
g. Fields in admin_login
action: "admin_login"
Comment: Indicates the action type, used to authenticate admin login.
username: String
Comment: The username for login.
password: String
Comment: The password for login.
h. Fields in change_admin_credentials
action: "change_admin_credentials"
Comment: Indicates the action type, used to update admin credentials.
username: String
Comment: The new username to set.
password: String
Comment: The new password to set.
3. Synchronization Between BE and Web
System Status Fields: Fields like reset_count, free_heap, temperature, inputs, outputs, and loraE32 are synchronized from BE to the web for display on index.html (Console, LoRa E32 Info).
Counter Configuration Fields: Fields planDisplay and counters are synchronized between dashboard.html and BE via saveConfig() and updateStatus().
LoRa Fields: Parameters like addh, addl, chan, etc., are sent from the web to BE for updates and returned from BE to reflect the current state.
Login Fields: username and password are sent from the web to BE for authentication, with the success result returned.

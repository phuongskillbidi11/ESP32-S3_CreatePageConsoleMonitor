// Global variables
let ws = null;
const terminal = document.getElementById('terminal');

// Page navigation
function showPage(pageId) {
  document.getElementById('dashboard').classList.add('hidden');
  document.getElementById('console').classList.add('hidden');
  document.getElementById('lora').classList.add('hidden');
  document.getElementById(pageId).classList.remove('hidden');
  if (pageId === 'lora') {
    updateConfigVisibility();
  }
}

// Terminal functions
function appendTerminal(message) {
  const formattedMessage = message.replace(/\n/g, '<br>');
  terminal.innerHTML += formattedMessage + '<br>';
  terminal.scrollTop = terminal.scrollHeight;
  if (terminal.childElementCount > 100) {
    terminal.removeChild(terminal.firstChild);
  }
}

// Configuration visibility
function updateConfigVisibility() {
  const operatingMode = document.getElementById('lora-operating-mode').value;
  const configSections = ['address-config', 'rf-config', 'uart-config', 'options-config', 'set-config-btn'];
  if (operatingMode === '3') { // Sleep Mode
    configSections.forEach(section => {
      document.getElementById(section).classList.add('active');
    });
  } else {
    configSections.forEach(section => {
      document.getElementById(section).classList.remove('active');
    });
  }
}

// Output control
function toggleOutput(pin) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const button = document.getElementById(`output-btn-${pin}`);
    const state = button.classList.contains('on');
    button.classList.toggle('on', !state);
    button.classList.toggle('off', state);
    button.textContent = state ? 'OFF' : 'ON';
    ws.send(JSON.stringify({
      action: 'control_output',
      pin: pin,
      state: !state
    }));
  }
}

// LoRa E32 functions
function refreshLoRaE32() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({
      action: 'refresh_lora_e32'
    }));
    appendTerminal('Refreshing LoRa E32 configuration...');
  }
}

function getLoRaE32Config() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({
      action: 'get_lora_e32_config'
    }));
    appendTerminal('Getting LoRa E32 configuration...');
  }
}

function setLoRaE32Config() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const config = {
      action: 'set_lora_e32_config',
      addh: parseInt(document.getElementById('lora-addh').value),
      addl: parseInt(document.getElementById('lora-addl').value),
      chan: parseInt(document.getElementById('lora-chan').value),
      uartParity: parseInt(document.getElementById('lora-parity-bit').value),
      uartBaudRate: parseInt(document.getElementById('lora-uart-baud-rate').value),
      airDataRate: parseInt(document.getElementById('lora-air-data-rate').value),
      fixedTransmission: parseInt(document.getElementById('lora-fixed-transmission').value),
      ioDriveMode: parseInt(document.getElementById('lora-io-drive-mode').value),
      wirelessWakeupTime: parseInt(document.getElementById('lora-wireless-wakeup-time').value),
      fec: parseInt(document.getElementById('lora-fec').value),
      transmissionPower: parseInt(document.getElementById('lora-transmission-power').value)
    };
    ws.send(JSON.stringify(config));
    appendTerminal('Setting LoRa E32 configuration (switching to Sleep Mode temporarily)...');
  }
}

function setLoRaOperatingMode() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const mode = parseInt(document.getElementById('lora-operating-mode').value);
    ws.send(JSON.stringify({
      action: 'set_lora_operating_mode',
      mode: mode
    }));
    appendTerminal(`Setting LoRa E32 operating mode to ${['Normal', 'Wake-Up', 'Power-Saving', 'Sleep'][mode]}...`);
    updateConfigVisibility();
  }
}

// Status update function
function updateStatus(data) {
  console.log('WebSocket data received:', data);

  if (data.action === 'system_status') {
    // Update system status
    document.getElementById('reset-count').textContent = data.reset_count;
    document.getElementById('free-heap').textContent = data.free_heap;
    document.getElementById('free-psram').textContent = data.free_psram;
    document.getElementById('temperature').textContent = data.temperature.toFixed(2);
    document.getElementById('ip-address').textContent = data.ip_address;
    document.getElementById('uptime').textContent = data.uptime;

    // Update Input Status
    const inputStatus = document.getElementById('input-status');
    inputStatus.innerHTML = '';
    data.inputs.forEach(input => {
      inputStatus.innerHTML += `
        <div class="input-status">
          Pin ${input.pin}: ${input.state}
        </div>`;
      if (document.getElementById(`input-${input.pin}`)?.dataset.state !== input.state) {
        appendTerminal(`Pin ${input.pin} changed to ${input.state}`);
      }
      if (document.getElementById(`input-${input.pin}`)) {
        document.getElementById(`input-${input.pin}`).dataset.state = input.state;
      }
    });

    // Update Output Controls
    const outputControls = document.getElementById('output-controls');
    outputControls.innerHTML = '';
    data.outputs.forEach(output => {
      outputControls.innerHTML += `
        <button id="output-btn-${output.pin}" 
                class="output-btn px-4 py-2 rounded text-white ${output.state === 'HIGH' ? 'on' : 'off'}"
                onclick="toggleOutput(${output.pin})">
          ${output.state === 'HIGH' ? 'ON' : 'OFF'}
        </button>`;
    });

    // Update LoRa E32 Information
    if (data.loraE32) {
      document.getElementById('lora-initialized').textContent = data.loraE32.initialized ? 'YES' : 'NO';
      document.getElementById('lora-module-info').textContent = data.loraE32.moduleInfo || 'Not available';
      document.getElementById('lora-addh').value = data.loraE32.addh || 0;
      document.getElementById('lora-addl').value = data.loraE32.addl || 0;
      document.getElementById('lora-chan').value = data.loraE32.chan || 0;
      document.getElementById('lora-frequency').textContent = data.loraE32.frequency || 'Not available';
      document.getElementById('lora-air-data-rate').value = data.loraE32.airDataRate ? parseInt(data.loraE32.airDataRate.replace('kbps', '')) : 2;
      document.getElementById('lora-uart-baud-rate').value = data.loraE32.uartBaudRate ? parseInt(data.loraE32.uartBaudRate) : 3;
      document.getElementById('lora-transmission-power').value = data.loraE32.transmissionPower ? parseInt(data.loraE32.transmissionPower.replace('dBm', '')) : 3;
      document.getElementById('lora-parity-bit').value = data.loraE32.parityBit ? (data.loraE32.parityBit === '8N1' ? 0 : data.loraE32.parityBit === '8O1' ? 1 : 2) : 0;
      document.getElementById('lora-wireless-wakeup-time').value = data.loraE32.wirelessWakeupTime ? parseInt(data.loraE32.wirelessWakeupTime.replace('ms', '')) / 250 : 0;
      document.getElementById('lora-fec').value = data.loraE32.fec === 'On' ? 1 : 0;
      document.getElementById('lora-fixed-transmission').value = data.loraE32.fixedTransmission === 'Transparent' ? 0 : 1;
      document.getElementById('lora-io-drive-mode').value = data.loraE32.ioDriveMode === 'Push-pull' ? 1 : 0;
      document.getElementById('lora-operating-mode').value = data.loraE32.operatingMode || 0;
      updateConfigVisibility();
    }
  } else if (data.action === 'debug') {
    appendTerminal(data.message);
  }
}

// WebSocket initialization
function initWebSocket() {
  ws = new WebSocket(`ws://192.168.0.104/ws`);

  ws.onopen = () => {
    document.getElementById('connection-status').textContent = 'WebSocket: Connected';
    appendTerminal('WebSocket connected');
    ws.send(JSON.stringify({ action: 'get_lora_e32_config' }));
  };

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      if (!data.action) {
        appendTerminal('Lỗi: Nhận được tin nhắn WebSocket không hợp lệ');
        return;
      }
      updateStatus(data);
    } catch (e) {
      appendTerminal('Lỗi: Không thể phân tích tin nhắn WebSocket');
    }
  };

  ws.onclose = () => {
    document.getElementById('connection-status').textContent = 'WebSocket: Disconnected';
    appendTerminal('WebSocket disconnected');
    setTimeout(initWebSocket, 5000);
  };
}

// Initialize when page loads
document.addEventListener('DOMContentLoaded', function() {
  initWebSocket();
  showPage('dashboard');
});
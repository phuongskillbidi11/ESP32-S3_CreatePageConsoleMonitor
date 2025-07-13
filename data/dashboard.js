// Global variables
let ws = null;
const terminal = document.getElementById('terminal');

// WebSocket initialization
function initWebSocket() {
  // ws = new WebSocket(`ws://192.168.0.104/ws`);
    ws = new WebSocket(`ws://${window.location.hostname}/ws`);

  ws.onopen = () => {
    document.getElementById('connection-status').textContent = 'WebSocket: Connected';
    ws.send(JSON.stringify({ action: 'get_counter_config' }));
  };

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      if (!data.action) {
        console.error('Invalid WebSocket message');
        return;
      }
      updateStatus(data);
    } catch (e) {
      console.error('Error parsing WebSocket message');0
    }
  };

  ws.onclose = () => {
    document.getElementById('connection-status').textContent = 'WebSocket: Disconnected';
    setTimeout(initWebSocket, 5000);
  };
}

// Update status
function updateStatus(data) {
  if (data.action === 'counter_status') {
    document.getElementById('plan-display').value = data.planDisplay;
    for (let i = 1; i <= 4; i++) {
      document.getElementById(`counter-pin-${i}`).value = data.counters[i-1].pin;
      document.getElementById(`delay-filter-${i}`).value = data.counters[i-1].delayFilter;
      document.getElementById(`counter-value-${i}`).textContent = data.counters[i-1].count;
    }
  } else if (data.action === 'login_result') {
    if (data.success) {
      window.location.href = '/index.html';
    } else {
      document.getElementById('login-error').classList.remove('hidden');
    }
  }
}

// Save configuration
function saveConfig() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const config = {
      action: 'set_counter_config',
      planDisplay: parseInt(document.getElementById('plan-display').value),
      counters: [
        { pin: parseInt(document.getElementById('counter-pin-1').value), delayFilter: parseInt(document.getElementById('delay-filter-1').value) },
        { pin: parseInt(document.getElementById('counter-pin-2').value), delayFilter: parseInt(document.getElementById('delay-filter-2').value) },
        { pin: parseInt(document.getElementById('counter-pin-3').value), delayFilter: parseInt(document.getElementById('delay-filter-3').value) },
        { pin: parseInt(document.getElementById('counter-pin-4').value), delayFilter: parseInt(document.getElementById('counter-pin-4').value) },
      ]
    };
    ws.send(JSON.stringify(config));
  }
}
// Reset single counter (index 0-3)
function resetCounter(counterIndex) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({
      action: 'reset_counter',
      index: counterIndex
    }));
  }
}

// Reset all counters
function resetAllCounters() {
  if (confirm('Are you sure you want to reset ALL counters?')) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        action: 'reset_all_counters'
      }));
    }
  }
}
// Login modal functions
function showPage(pageId) {
  if (pageId === 'admin') {
    document.getElementById('login-modal').classList.remove('hidden');
  }
}

function closeLoginModal() {
  document.getElementById('login-modal').classList.add('hidden');
  document.getElementById('login-error').classList.add('hidden');
}

function login() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    const username = document.getElementById('login-username').value;
    const password = document.getElementById('login-password').value;
    ws.send(JSON.stringify({
      action: 'admin_login',
      username: username,
      password: password
    }));
  }
}

// Initialize when page loads
document.addEventListener('DOMContentLoaded', function() {
  initWebSocket();
});
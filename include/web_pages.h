#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Water Level Sensor Dashboard</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --accent: #38bdf8;
      --accent-hover: #0284c7;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --success: #22c55e;
      --warning: #f59e0b;
      --danger: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 1.5rem; max-width: 900px; margin: 0 auto; }
    h1, h2, h3 { margin-bottom: 0.75rem; color: var(--text); }
    .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 2rem; border-bottom: 1px solid #334155; padding-bottom: 1rem; }
    .badge { background: #064e3b; color: var(--success); padding: 0.25rem 0.75rem; border-radius: 9999px; font-size: 0.875rem; font-weight: 600; display: inline-flex; align-items: center; gap: 0.5rem; }
    .badge-dot { width: 8px; height: 8px; background: var(--success); border-radius: 50%; display: inline-block; animation: pulse 2s infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
    
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 1.25rem; margin-bottom: 2rem; }
    .card { background: var(--card-bg); border-radius: 0.75rem; padding: 1.25rem; border: 1px solid #334155; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); }
    .card-title { font-size: 0.875rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 0.5rem; }
    .card-value { font-size: 1.75rem; font-weight: 700; color: var(--accent); }
    .card-sub { font-size: 0.875rem; color: var(--text-muted); margin-top: 0.25rem; }
    
    .tank-container { background: var(--card-bg); border-radius: 0.75rem; padding: 1.5rem; border: 1px solid #334155; margin-bottom: 2rem; }
    .tank-bar-bg { background: #0f172a; height: 24px; border-radius: 12px; overflow: hidden; position: relative; border: 1px solid #334155; }
    .tank-bar-fill { background: linear-gradient(90deg, #0284c7, #38bdf8); height: 100%; width: 0%; transition: width 0.5s ease-in-out; }
    .tank-bar-text { position: absolute; top: 0; left: 0; right: 0; bottom: 0; display: flex; align-items: center; justify-content: center; font-size: 0.875rem; font-weight: 700; text-shadow: 0 1px 2px rgba(0,0,0,0.8); }
    
    .section { background: var(--card-bg); border-radius: 0.75rem; padding: 1.5rem; border: 1px solid #334155; margin-bottom: 1.5rem; }
    .form-group { margin-bottom: 1rem; }
    label { display: block; font-size: 0.875rem; color: var(--text-muted); margin-bottom: 0.35rem; }
    input[type="number"], input[type="file"] { width: 100%; padding: 0.65rem; background: #0f172a; border: 1px solid #334155; border-radius: 0.5rem; color: var(--text); font-size: 0.95rem; }
    input:focus { outline: none; border-color: var(--accent); }
    
    button { background: var(--accent); color: #0f172a; border: none; padding: 0.75rem 1.5rem; border-radius: 0.5rem; font-weight: 600; cursor: pointer; transition: background 0.2s; font-size: 0.95rem; }
    button:hover { background: var(--accent-hover); color: white; }
    button.btn-secondary { background: #334155; color: var(--text); }
    button.btn-secondary:hover { background: #475569; }
    button.btn-danger { background: #991b1b; color: #fecaca; }
    button.btn-danger:hover { background: var(--danger); color: white; }
    
    .progress-bg { background: #0f172a; height: 16px; border-radius: 8px; overflow: hidden; margin: 1rem 0; border: 1px solid #334155; display: none; }
    .progress-fill { background: var(--success); height: 100%; width: 0%; transition: width 0.2s; }
    .status-msg { margin-top: 0.5rem; font-size: 0.9rem; font-weight: 500; }
  </style>
</head>
<body>
  <div class="header">
    <div>
      <h1>Water Level Sensor</h1>
      <p style="color: var(--text-muted); font-size: 0.9rem;">ESP8266 Live Telemetry & Control</p>
    </div>
    <div class="badge"><span class="badge-dot"></span> Online</div>
  </div>

  <div class="tank-container">
    <div style="display:flex; justify-content:space-between; margin-bottom:0.5rem;">
      <h3>Water Tank Fill Level</h3>
      <span id="pctText" style="font-weight:700; color:var(--accent);">0%</span>
    </div>
    <div class="tank-bar-bg">
      <div class="tank-bar-fill" id="tankFill"></div>
      <div class="tank-bar-text" id="tankBarLabel">0 cm / 0 cm</div>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="card-title">Distance to Water</div>
      <div class="card-value" id="distVal">--</div>
      <div class="card-sub">cm from sensor</div>
    </div>
    <div class="card">
      <div class="card-title">Calculated Water Level</div>
      <div class="card-value" id="levelVal">--</div>
      <div class="card-sub">cm depth</div>
    </div>
    <div class="card">
      <div class="card-title">Ambient Temperature</div>
      <div class="card-value" id="tempVal">--</div>
      <div class="card-sub">BME280 / DHT11</div>
    </div>
    <div class="card">
      <div class="card-title">Relative Humidity</div>
      <div class="card-value" id="humVal">--</div>
      <div class="card-sub">humidity %</div>
    </div>
    <div class="card">
      <div class="card-title">Power Mode</div>
      <div class="card-value" id="powerModeVal" style="font-size:1.4rem;">--</div>
      <div class="card-sub" id="powerModeSub">MQTT: roof/tank_water/control</div>
    </div>
    <div class="card">
      <div class="card-title">MQTT & Signal</div>
      <div class="card-value" id="mqttVal">--</div>
      <div class="card-sub" id="rssiVal">WiFi: -- dBm</div>
    </div>
  </div>

  <div class="section">
    <h3>MQTT Diagnostic Logging (`roof/sensor/log`)</h3>
    <p style="color: var(--text-muted); font-size:0.875rem; margin-bottom:1rem;">
      Publish step-by-step diagnostic telemetry (raw samples, ping status, calculations) to MQTT topic <code>roof/sensor/log</code>.
    </p>
    <div style="display:flex; align-items:center; gap:1rem;">
      <button type="button" id="debugToggleBtn" onclick="toggleDebugLogging()">Toggle Debug Logging</button>
      <span id="debugStatus" style="font-weight:700; color:var(--success);">Status: --</span>
    </div>
  </div>

  <div class="section">
    <h3>MQTT Remote OTA Control Guide</h3>
    <p style="color: var(--text-muted); font-size:0.875rem; margin-bottom:0.5rem;">
      To keep the device awake remotely for firmware updates or Web UI maintenance while in Deep Sleep mode, publish to MQTT topic:
    </p>
    <code style="display:block; background:#0f172a; padding:0.75rem; border-radius:0.5rem; font-size:0.9rem; color:var(--accent); margin-bottom:0.5rem;">
      roof/tank_water/control &nbsp;&rarr;&nbsp; "ota"
    </code>
    <p style="color: var(--text-muted); font-size:0.85rem;">
      Payloads: <code>"ota"</code> or <code>"stay_awake"</code> keeps device awake for 10 mins. <code>"sleep"</code> resumes Deep Sleep immediately. <code>"debug_on"</code> / <code>"debug_off"</code> controls debug logs.
    </p>
  </div>

  <div class="section">
    <h3>Sensor Configuration</h3>
    <form id="configForm">
      <div class="grid" style="margin-bottom:0;">
        <div class="form-group">
          <label>Speed of Sound (m/s)</label>
          <input type="number" id="cfgSpeed" required>
        </div>
        <div class="form-group">
          <label>Tank Bottom Distance (cm)</label>
          <input type="number" id="cfgBottom" required>
        </div>
        <div class="form-group">
          <label>Max Water Level (cm)</label>
          <input type="number" id="cfgMax" required>
        </div>
      </div>
      <button type="submit" style="margin-top:0.5rem;">Save Configuration</button>
      <span class="status-msg" id="cfgStatus" style="margin-left:1rem;"></span>
    </form>
  </div>

  <div class="section">
    <h3>Over-The-Air Firmware Update</h3>
    <p style="color: var(--text-muted); font-size:0.875rem; margin-bottom:1rem;">Select a compiled PlatformIO <code>firmware.bin</code> file to upload to the ESP8266.</p>
    <input type="file" id="firmwareFile" accept=".bin">
    <div class="progress-bg" id="progressBg">
      <div class="progress-fill" id="progressFill"></div>
    </div>
    <button type="button" id="uploadBtn" style="margin-top:1rem;" onclick="uploadFirmware()">Upload Firmware</button>
    <div class="status-msg" id="otaStatus"></div>
  </div>

  <div class="section">
    <h3>Network Management</h3>
    <p style="color: var(--text-muted); font-size:0.875rem; margin-bottom:1rem;">Reset Wi-Fi credentials to switch routers or start local setup Access Point (WATER_LEVEL_SENSOR).</p>
    <button type="button" class="btn-danger" onclick="resetWifi()">Re-configure Wi-Fi Network</button>
    <div class="status-msg" id="wifiStatus"></div>
  </div>

  <script>
    function updateTelemetry() {
      fetch('/api/readings')
        .then(r => r.json())
        .then(data => {
          document.getElementById('distVal').innerText = data.distance_to_water + ' cm';
          document.getElementById('levelVal').innerText = data.water_level + ' cm';
          
          if(data.temperature !== null && !isNaN(data.temperature)) {
            document.getElementById('tempVal').innerText = data.temperature.toFixed(1) + ' °C';
          } else {
            document.getElementById('tempVal').innerText = '--';
          }

          if(data.humidity !== null && !isNaN(data.humidity)) {
            document.getElementById('humVal').innerText = data.humidity.toFixed(1) + ' %';
          } else {
            document.getElementById('humVal').innerText = '--';
          }

          let dbgEl = document.getElementById('debugStatus');
          if(data.mqtt_debug_enabled) {
            dbgEl.innerText = 'Status: ACTIVE (Publishing to roof/sensor/log)';
            dbgEl.style.color = 'var(--success)';
          } else {
            dbgEl.innerText = 'Status: DISABLED';
            dbgEl.style.color = 'var(--text-muted)';
          }

          if(data.stay_awake_remaining_sec > 0) {
            let m = Math.floor(data.stay_awake_remaining_sec / 60);
            let s = data.stay_awake_remaining_sec % 60;
            document.getElementById('powerModeVal').innerText = 'Stay-Awake';
            document.getElementById('powerModeVal').style.color = 'var(--success)';
            document.getElementById('powerModeSub').innerText = 'Remaining: ' + m + 'm ' + s + 's';
          } else {
            document.getElementById('powerModeVal').innerText = 'Deep Sleep';
            document.getElementById('powerModeVal').style.color = 'var(--warning)';
            document.getElementById('powerModeSub').innerText = 'Cycle: 1 min';
          }

          document.getElementById('mqttVal').innerText = data.mqtt_connected ? 'Connected' : 'Offline';
          document.getElementById('mqttVal').style.color = data.mqtt_connected ? 'var(--success)' : 'var(--danger)';
          document.getElementById('rssiVal').innerText = 'WiFi: ' + data.wifi_rssi + ' dBm';

          let pct = Math.max(0, Math.min(100, Math.round(data.percentage)));
          document.getElementById('pctText').innerText = pct + '%';
          document.getElementById('tankFill').style.width = pct + '%';
          document.getElementById('tankBarLabel').innerText = data.water_level + ' cm / ' + data.max_water_level + ' cm';

          if(!document.activeElement || document.activeElement.tagName !== 'INPUT') {
            document.getElementById('cfgSpeed').value = data.speed_of_sound;
            document.getElementById('cfgBottom').value = data.tank_bottom_distance;
            document.getElementById('cfgMax').value = data.max_water_level;
          }
        })
        .catch(err => console.error('Failed fetching readings:', err));
    }

    setInterval(updateTelemetry, 2000);
    updateTelemetry();

    function toggleDebugLogging() {
      fetch('/api/debug_toggle', { method: 'POST' })
        .then(r => r.text())
        .then(res => {
          updateTelemetry();
        });
    }

    document.getElementById('configForm').addEventListener('submit', function(e) {
      e.preventDefault();
      let statusEl = document.getElementById('cfgStatus');
      statusEl.innerText = 'Saving...';
      statusEl.style.color = 'var(--accent)';

      let formData = new FormData();
      formData.append('s', document.getElementById('cfgSpeed').value);
      formData.append('t', document.getElementById('cfgBottom').value);
      formData.append('m', document.getElementById('cfgMax').value);

      fetch('/api/config', { method: 'POST', body: formData })
        .then(r => r.text())
        .then(res => {
          statusEl.innerText = 'Configuration Saved!';
          statusEl.style.color = 'var(--success)';
          setTimeout(() => statusEl.innerText = '', 3000);
        })
        .catch(err => {
          statusEl.innerText = 'Error saving config.';
          statusEl.style.color = 'var(--danger)';
        });
    });

    function uploadFirmware() {
      let fileInput = document.getElementById('firmwareFile');
      let file = fileInput.files[0];
      let statusEl = document.getElementById('otaStatus');
      let btn = document.getElementById('uploadBtn');
      let pBg = document.getElementById('progressBg');
      let pFill = document.getElementById('progressFill');

      if (!file) {
        statusEl.innerText = 'Please select a firmware .bin file first.';
        statusEl.style.color = 'var(--danger)';
        return;
      }

      btn.disabled = true;
      pBg.style.display = 'block';
      pFill.style.width = '0%';
      statusEl.innerText = 'Uploading firmware...';
      statusEl.style.color = 'var(--accent)';

      let xhr = new XMLHttpRequest();
      xhr.open('POST', '/update', true);

      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          let pct = Math.round((e.loaded / e.total) * 100);
          pFill.style.width = pct + '%';
          statusEl.innerText = 'Uploading: ' + pct + '% complete...';
        }
      };

      xhr.onload = function() {
        if (xhr.status === 200) {
          pFill.style.width = '100%';
          statusEl.innerText = 'Upload Complete! Flashing & Rebooting device...';
          statusEl.style.color = 'var(--success)';
          setTimeout(() => { location.reload(); }, 10000);
        } else {
          statusEl.innerText = 'Upload Failed: ' + xhr.responseText;
          statusEl.style.color = 'var(--danger)';
          btn.disabled = false;
        }
      };

      xhr.onerror = function() {
        statusEl.innerText = 'Connection error during upload.';
        statusEl.style.color = 'var(--danger)';
        btn.disabled = false;
      };

      let formData = new FormData();
      formData.append('update', file);
      xhr.send(formData);
    }

    function resetWifi() {
      if(confirm('Are you sure you want to reset Wi-Fi credentials? The device will start AP hotspot (WATER_LEVEL_SENSOR).')) {
        let statusEl = document.getElementById('wifiStatus');
        statusEl.innerText = 'Resetting Wi-Fi and launching AP portal...';
        statusEl.style.color = 'var(--warning)';

        fetch('/api/wifi_reset', { method: 'POST' })
          .then(r => r.text())
          .then(res => {
            alert('Wi-Fi reset complete. Connect to WATER_LEVEL_SENSOR hotspot at 192.168.4.1');
          });
      }
    }
  </script>
</body>
</html>
)rawliteral";

#endif // WEB_PAGES_H

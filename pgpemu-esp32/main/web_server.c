/**
 * @file web_server.c v1.2.0
 * @brief Complete Web Server with 5 Tabs
 * 
 * v1.2.0 Features:
 * - Tab 1: Settings (Global Config)
 * - Tab 2: Statistics (Caught/Fled/Spin)
 * - Tab 3: Devices (Per-Device Settings)
 * - Tab 4: Secrets (WiFi SSID/Password/TX Power)
 * - Tab 5: Device Config (PGP Name/MAC/Blob/Key) [NEW]
 * 
 * - Android Captive Portal Support
 * - iOS Captive Portal Support
 * - Windows Captive Portal Support
 * - Timer Pause/Resume
 * - 15 API Endpoints
 */

#include "web_server.h"
#include "wifi_ap_manager.h"
#include "config_storage.h"
#include "settings.h"
#include "pgp_handshake_multi.h"
#include "stats.h"
#include "device_config.h"  // NEW v1.2.0
#include "log_tags.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

// Timer control
static bool timer_paused = false;
static uint32_t pause_time_remaining = 0;

// Forward declarations
static esp_err_t index_handler(httpd_req_t *req);
static esp_err_t captive_portal_redirect(httpd_req_t *req);

// HTML with 5 Tabs
static const char index_html[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>PGPemu Control Panel v1.2.0</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px}"
".container{max-width:900px;margin:0 auto}"
"h1{color:#fff;text-align:center;margin-bottom:10px;font-size:32px;text-shadow:0 2px 4px rgba(0,0,0,0.2)}"
".subtitle{color:#fff;text-align:center;opacity:0.9;margin-bottom:20px;font-size:14px}"
".info-bar{background:rgba(255,255,255,0.95);padding:15px;border-radius:8px;margin-bottom:20px;box-shadow:0 4px 8px rgba(0,0,0,0.2)}"
".timer-section{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px}"
".timer{font-size:28px;font-weight:bold;color:#667eea}"
".timer.paused{color:#f5576c}"
".timer-controls{display:flex;gap:10px}"
".timer-btn{padding:8px 16px;background:#667eea;color:#fff;border:none;border-radius:6px;cursor:pointer;font-size:14px;font-weight:600;transition:all 0.3s}"
".timer-btn:hover{background:#5568d3;transform:translateY(-1px)}"
".timer-btn.pause{background:#f5576c}"
".timer-btn.pause:hover{background:#e04661}"
".connections{color:#555;font-size:14px}"
".info-box{background:#e3f2fd;color:#1976d2;padding:12px;border-radius:6px;margin-bottom:15px;font-size:13px;line-height:1.6}"
".info-box strong{display:block;margin-bottom:5px;font-size:14px}"
".warning-box{background:#fff3cd;color:#856404;padding:12px;border-radius:6px;margin-bottom:15px;font-size:13px;line-height:1.6}"
".tabs{display:flex;gap:10px;margin-bottom:20px;flex-wrap:wrap}"
".tab{flex:1 1 auto;min-width:120px;padding:12px;background:rgba(255,255,255,0.2);color:#fff;border:none;border-radius:8px;cursor:pointer;font-size:14px;font-weight:600;transition:all 0.3s}"
".tab:hover{background:rgba(255,255,255,0.3)}"
".tab.active{background:#fff;color:#667eea;box-shadow:0 4px 8px rgba(0,0,0,0.2)}"
".tab-content{display:none}"
".tab-content.active{display:block}"
".card{background:white;border-radius:12px;padding:25px;margin:15px 0;box-shadow:0 8px 16px rgba(0,0,0,0.2)}"
".card h2{margin:0 0 20px 0;color:#333;font-size:20px;border-bottom:2px solid #667eea;padding-bottom:10px}"
".setting{margin:20px 0}"
".setting label{font-weight:500;color:#555;display:block;margin-bottom:8px}"
".setting .help{font-size:12px;color:#888;margin-top:4px}"
".setting-row{display:flex;justify-content:space-between;align-items:center;margin:15px 0;padding:10px;background:#f8f9fa;border-radius:8px}"
".toggle{position:relative;display:inline-block;width:56px;height:28px}"
".toggle input{opacity:0;width:0;height:0}"
".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.3s;border-radius:28px}"
".slider:before{position:absolute;content:'';height:22px;width:22px;left:3px;bottom:3px;background:white;transition:.3s;border-radius:50%;box-shadow:0 2px 4px rgba(0,0,0,0.2)}"
"input:checked+.slider{background:#667eea}"
"input:checked+.slider:before{transform:translateX(28px)}"
".range-container{margin:10px 0}"
".range-value{display:inline-block;min-width:80px;text-align:right;font-weight:bold;color:#667eea;font-size:18px}"
"input[type='range']{width:100%;height:6px;border-radius:3px;background:#ddd;outline:none;-webkit-appearance:none}"
"input[type='range']::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.2)}"
"input[type='range']::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer;border:none}"
"input[type='text'],input[type='password'],textarea,select{width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-size:14px;background:white}"
"input[type='text']:focus,input[type='password']:focus,textarea:focus,select:focus{border-color:#667eea;outline:none}"
"textarea{resize:vertical;font-family:monospace}"
"select{cursor:pointer}"
"button{width:100%;padding:14px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;box-shadow:0 4px 8px rgba(0,0,0,0.2);transition:transform 0.2s}"
"button:hover{transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.3)}"
"button:active{transform:translateY(0)}"
".btn-group{display:flex;gap:10px}"
".btn-group button{flex:1}"
".btn-reset{background:linear-gradient(135deg,#f5576c,#f093fb)}"
".status{text-align:center;padding:12px;border-radius:8px;margin:15px 0;font-weight:500}"
".status.success{background:#d4edda;color:#155724}"
".status.error{background:#f8d7da;color:#721c24}"
".hidden{display:none}"
".stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;margin:20px 0}"
".stat-box{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:20px;border-radius:12px;text-align:center;box-shadow:0 4px 8px rgba(0,0,0,0.2)}"
".stat-box .label{font-size:14px;opacity:0.9;margin-bottom:8px}"
".stat-box .value{font-size:32px;font-weight:bold}"
".device-card{background:#f8f9fa;border-radius:12px;padding:20px;margin:15px 0;border-left:4px solid #667eea}"
".device-card .device-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px}"
".device-card .device-id{font-size:18px;font-weight:bold;color:#667eea}"
".device-card .device-status{display:inline-block;padding:4px 12px;background:#4CAF50;color:white;border-radius:12px;font-size:12px}"
".battery{display:flex;align-items:center;gap:10px}"
".battery-icon{width:40px;height:20px;border:2px solid #555;border-radius:4px;position:relative;padding:2px}"
".battery-fill{height:100%;background:linear-gradient(to right,#4CAF50,#8BC34A);border-radius:2px;transition:width 0.3s}"
".battery-tip{width:4px;height:10px;background:#555;border-radius:0 2px 2px 0}"
".battery-percent{font-weight:bold;color:#555}"
".prob-labels{display:flex;justify-content:space-between;font-size:11px;color:#888;margin-top:5px}"
".no-devices{text-align:center;padding:40px;color:#888;font-size:16px}"
"@media (max-width:768px){.tabs{flex-wrap:wrap}.tab{flex:1 1 45%;margin-bottom:10px}}"
/**************************************************** */

".sub-tabs{display:flex;gap:10px;margin-bottom:15px;background:rgba(255,255,255,0.1);padding:10px;border-radius:8px}"
".sub-tab{flex:1;padding:10px;background:rgba(255,255,255,0.3);color:#fff;border:none;border-radius:6px;cursor:pointer;font-size:13px;font-weight:600;transition:all 0.3s}"
".sub-tab:hover{background:rgba(255,255,255,0.4)}"
".sub-tab.active{background:#fff;color:#667eea;box-shadow:0 2px 4px rgba(0,0,0,0.2)}"
".sub-tab-content{display:none}"
".sub-tab-content.active{display:block}"
".char-count{font-size:12px;color:#888;margin-top:5px;text-align:right}"
"code{background:#f0f0f0;padding:2px 6px;border-radius:3px;font-family:monospace;font-size:12px}"
".btn-reset{background:linear-gradient(135deg,#f5576c 0%,#f093fb 100%)!important}"

/***************************************************** */
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🎮 PGPemu Control Panel</h1>"
"<div class='subtitle'>v1.2.0 - Advanced Pokemon Go Plus Emulator</div>"

"<div class='info-box'>"
"<strong>ℹ️ Quick Start Guide</strong>"
"• Press button for <strong>2 seconds</strong> to start WiFi AP<br>"
"• WiFi will auto-close after <strong>5 minutes</strong><br>"
"• Default password: <strong>PogoPogo</strong><br>"
"• Blue LED shows WiFi AP status<br>"
"• Connect to \"PGPemu-Setup\" and this page opens automatically"
"</div>"

"<div class='info-bar'>"
"<div class='timer-section'>"
"<div class='timer' id='timer'>5:00</div>"
"<div class='timer-controls'>"
"<button class='timer-btn' id='pauseBtn' onclick='toggleTimer()'>⏸ Pause</button>"
"</div>"
"</div>"
"<div class='connections' id='connInfo'>Loading...</div>"
"</div>"

"<div id='status' class='status hidden'></div>"

"<div class='tabs'>"
"<button class='tab active' onclick='switchTab(0)'>⚙️ Settings</button>"
"<button class='tab' onclick='switchTab(1)'>📊 Statistics</button>"
"<button class='tab' onclick='switchTab(2)'>📱 Devices</button>"
"<button class='tab' onclick='switchTab(3)'>🔐 Secrets</button>"
"<button class='tab' onclick='switchTab(4)'>🎮 Device Config</button>"
"</div>"

// Tab 0: Settings
"<div class='tab-content active' id='tab0'>"
"<div class='card'>"
"<h2>⚙️ Global Settings</h2>"
"<div class='setting-row'>"
"<div><label>Auto Catch Pokemon</label><div class='help'>Automatically catch all Pokemon</div></div>"
"<label class='toggle'><input type='checkbox' id='autocatch' onchange='settingsChanged()'><span class='slider'></span></label>"
"</div>"
"<div class='setting-row'>"
"<div><label>Auto Spin Pokestops</label><div class='help'>Automatically spin Pokestops</div></div>"
"<label class='toggle'><input type='checkbox' id='autospin' onchange='settingsChanged()'><span class='slider'></span></label>"
"</div>"
"<div class='setting'>"
"<label>Autospin Probability: <span class='range-value' id='probValue'>Always</span></label>"
"<div class='help'>0 = Always spin (100%) | 5 = 50% | 9 = Rare (10%)</div>"
"<input type='range' id='probability' min='0' max='9' value='0' oninput='updateProbability()' onchange='settingsChanged()'>"
"<div class='prob-labels'><span>Always</span><span>50%</span><span>Rare</span></div>"
"</div>"
"<div class='setting'>"
"<label>Max Connections</label>"
"<div class='help'>Maximum simultaneous device connections (1-4)</div>"
"<select id='maxConnections' onchange='settingsChanged()'>"
"<option value='1'>1 Device</option>"
"<option value='2'>2 Devices</option>"
"<option value='3'>3 Devices</option>"
"<option value='4'>4 Devices</option>"
"</select>"
"</div>"
"<div class='setting'>"
"<label>Log Level</label>"
"<div class='help'>Higher = more detailed logs</div>"
"<select id='logLevel' onchange='settingsChanged()'>"
"<option value='1'>Debug</option>"
"<option value='2'>Info</option>"
"<option value='3'>Verbose</option>"
"</select>"
"</div>"
"<button onclick='saveSettings()'>💾 Save Settings & Restart</button>"
"</div>"
"</div>"

// Tab 1: Statistics
"<div class='tab-content' id='tab1'>"
"<div class='card'>"
"<h2>📊 Statistics</h2>"
"<div id='statsContent'><div class='no-devices'>Loading...</div></div>"
"<button onclick='loadStats()' style='margin-top:20px;background:linear-gradient(135deg,#f093fb,#f5576c)'>🔄 Refresh</button>"
"</div>"
"</div>"

// Tab 2: Devices
"<div class='tab-content' id='tab2'>"
"<div class='card'>"
"<h2>📱 Connected Devices</h2>"
"<div id='devicesContent'><div class='no-devices'>Loading...</div></div>"
"<button onclick='loadDevices()' style='margin-top:20px;background:linear-gradient(135deg,#f093fb,#f5576c)'>🔄 Refresh</button>"
"</div>"
"</div>"

// Tab 3: Secrets
"<div class='tab-content' id='tab3'>"
"<div class='card'>"
"<h2>🔐 WiFi AP Secrets</h2>"
"<div class='setting'>"
"<label>WiFi SSID</label>"
"<div class='help'>Network name (default: PGPemu-Setup)</div>"
"<input type='text' id='wifiSsid' maxlength='31' onchange='secretsChanged()'>"
"</div>"
"<div class='setting'>"
"<label>WiFi Password (WPA2)</label>"
"<div class='help'>Leave empty for Open network (default: PogoPogo)</div>"
"<input type='password' id='wifiPassword' maxlength='63' onchange='secretsChanged()'>"
"</div>"
"<div class='setting'>"
"<label>TX Power: <span class='range-value' id='txPowerValue'>8.5 dBm</span></label>"
"<div class='help'>WiFi transmission power: 2.0 - 21.0 dBm</div>"
"<input type='range' id='txPower' min='8' max='84' value='34' oninput='updateTxPower()' onchange='secretsChanged()'>"
"<div class='prob-labels'><span>2.0 dBm</span><span>8.5 dBm</span><span>21.0 dBm</span></div>"
"</div>"
"<button onclick='saveSecrets()'>💾 Save Secrets</button>"
"</div>"
"</div>"

// Tab 4: Device Config with SUB-TABS (v2.0-DUAL)
"<div class='tab-content' id='tab4'>"

// Sub-Tabs for Device Config
"<div class='sub-tabs'>"
"<button class='sub-tab active' onclick='switchDeviceTab(0)'>🔐 PGP Secrets (Bluetooth)</button>"
"<button class='sub-tab' onclick='switchDeviceTab(1)'>⚙️ Device Config (Web)</button>"
"</div>"

// Sub-Tab 0: PGP Secrets (pgpsecret namespace)
"<div class='sub-tab-content active' id='deviceSubTab0'>"
"<div class='card'>"
"<h2>🔐 PGP Secrets (Bluetooth Stack)</h2>"
"<div class='info-box'>"
"<strong>📡 Active Bluetooth Configuration</strong><br>"
"These are the REAL secrets used by the Bluetooth stack to connect to Pokemon GO.<br>"
"Stored in NVS namespace: <code>pgpsecret</code><br>"
"⚠️ Changes require device restart to take effect!"
"</div>"
"<div class='setting'>"
"<label>Device Name (PGP_CLONE_NAME)</label>"
"<div class='help'>Bluetooth advertised name (max 15 chars)</div>"
"<input type='text' id='pgpSecretName' maxlength='15' placeholder='PKLMGOPLUS' onchange='pgpSecretsChanged()'>"
"</div>"
"<div class='setting'>"
"<label>MAC Address (PGP_MAC)</label>"
"<div class='help'>Bluetooth MAC address (format: XX:XX:XX:XX:XX:XX)</div>"
"<input type='text' id='pgpSecretMac' maxlength='17' placeholder='e4:cb:0b:c3:59:63' pattern='[0-9A-Fa-f:]{17}' onchange='pgpSecretsChanged()'>"
"</div>"
"<div class='setting'>"
"<label>Blob Data (PGP_BLOB)</label>"
"<div class='help'>Device blob data - MUST be exactly 512 hex chars (256 bytes)</div>"
"<textarea id='pgpSecretBlob' maxlength='512' rows='6' placeholder='61f60a07450cd116...' onchange='pgpSecretsChanged()'></textarea>"
"<div class='char-count'>Length: <span id='pgpBlobLength'>0</span>/512 chars</div>"
"</div>"
"<div class='setting'>"
"<label>Device Key (PGP_DEVICE_KEY)</label>"
"<div class='help'>Device encryption key - MUST be exactly 32 hex chars (16 bytes)</div>"
"<input type='text' id='pgpSecretKey' maxlength='32' placeholder='f972916afd2db437...' pattern='[0-9A-Fa-f]{32}' onchange='pgpSecretsChanged()'>"
"<div class='char-count'>Length: <span id='pgpKeyLength'>0</span>/32 chars</div>"
"</div>"
"<div class='btn-group'>"
"<button onclick='savePgpSecrets()'>💾 Save PGP Secrets</button>"
"<button class='btn-reset' onclick='resetPgpSecrets()'>🔄 Reset to Empty</button>"
"</div>"
"</div>"
"</div>"

// Sub-Tab 1: Device Config (device_cfg namespace)
"<div class='sub-tab-content' id='deviceSubTab1'>"
"<div class='card'>"
"<h2>⚙️ Device Config (Web Interface)</h2>"
"<div class='info-box'>"
"<strong>🌐 Web Interface Configuration</strong><br>"
"These settings are for web interface display only.<br>"
"Stored in NVS namespace: <code>device_cfg</code><br>"
"⚠️ NOT used by Bluetooth! Use PGP Secrets tab for actual device functionality."
"</div>"
"<div class='setting'>"
"<label>Device Name</label>"
"<div class='help'>Display name in web interface (max 63 chars)</div>"
"<input type='text' id='deviceName' maxlength='63' placeholder='Pokemon GO Plus' onchange='deviceConfigChanged()'>"
"</div>"
"<div class='setting'>"
"<label>MAC Address</label>"
"<div class='help'>Display MAC (format: XX:XX:XX:XX:XX:XX)</div>"
"<input type='text' id='deviceMac' maxlength='17' placeholder='00:00:00:00:00:00' pattern='[0-9A-Fa-f:]{17}' onchange='deviceConfigChanged()'>"
"</div>"
"<div class='setting'>"
"<label>Blob Data</label>"
"<div class='help'>Device blob for reference (hex string)</div>"
"<textarea id='deviceBlob' maxlength='512' rows='4' placeholder='Optional...' onchange='deviceConfigChanged()'></textarea>"
"</div>"
"<div class='setting'>"
"<label>Device Key</label>"
"<div class='help'>Device key for reference (hex string)</div>"
"<input type='text' id='deviceKey' maxlength='32' placeholder='Optional...' pattern='[0-9A-Fa-f]{0,32}' onchange='deviceConfigChanged()'>"
"</div>"
"<div class='btn-group'>"
"<button onclick='saveDeviceConfig()'>💾 Save Config</button>"
"<button class='btn-reset' onclick='resetDeviceConfig()'>🔄 Reset Defaults</button>"
"</div>"
"</div>"
"</div>"

"</div>" // End Tab 4

"<script>"
"let timerInterval,hasChanges=false,hasSecretsChanges=false,hasDeviceConfigChanges=false,currentTab=0,isPaused=false;"
"const probLabels=['Always','10%','20%','30%','40%','50%','60%','70%','80%','90%'];"

"function updateProbability(){"
"const val=document.getElementById('probability').value;"
"document.getElementById('probValue').textContent=probLabels[val];"
"}"

"function updateTxPower(){"
"const val=document.getElementById('txPower').value;"
"const dbm=(val*0.25).toFixed(1);"
"document.getElementById('txPowerValue').textContent=dbm+' dBm';"
"}"

"function settingsChanged(){hasChanges=true;}"
"function secretsChanged(){hasSecretsChanges=true;}"
"function deviceConfigChanged(){hasDeviceConfigChanges=true;}"

/******************************************************** */

"let hasPgpSecretsChanges=false,currentDeviceTab=0;"

"function pgpSecretsChanged(){hasPgpSecretsChanges=true;}"

"function switchDeviceTab(tab){"
"currentDeviceTab=tab;"
"document.querySelectorAll('.sub-tab').forEach((t,i)=>t.classList.toggle('active',i===tab));"
"document.querySelectorAll('.sub-tab-content').forEach((c,i)=>c.classList.toggle('active',i===tab));"
"if(tab===0)loadPgpSecrets();"
"if(tab===1)loadDeviceConfig();"
"}"

"function updateCharCounts(){"
"const blob=document.getElementById('pgpSecretBlob');"
"const key=document.getElementById('pgpSecretKey');"
"if(blob)document.getElementById('pgpBlobLength').textContent=blob.value.length;"
"if(key)document.getElementById('pgpKeyLength').textContent=key.value.length;"
"}"

"function loadPgpSecrets(){"
"fetch('/api/pgp_secrets')"
".then(r=>r.json())"
".then(data=>{"
"document.getElementById('pgpSecretName').value=data.name||'';"
"document.getElementById('pgpSecretMac').value=data.mac||'';"
"document.getElementById('pgpSecretBlob').value=data.blob||'';"
"document.getElementById('pgpSecretKey').value=data.dkey||'';"
"updateCharCounts();"
"hasPgpSecretsChanges=false;"
"})"
".catch(err=>console.error('Load PGP secrets failed:',err));"
"}"

"function savePgpSecrets(){"
"if(!hasPgpSecretsChanges){"
"alert('No changes to save!');"
"return;"
"}"
"const blob=document.getElementById('pgpSecretBlob').value;"
"const key=document.getElementById('pgpSecretKey').value;"
"if(blob.length!==512){"
"alert('ERROR: Blob must be exactly 512 hex characters (256 bytes)!\\nCurrent: '+blob.length+' chars');"
"return;"
"}"
"if(key.length!==32){"
"alert('ERROR: Key must be exactly 32 hex characters (16 bytes)!\\nCurrent: '+key.length+' chars');"
"return;"
"}"
"const data={"
"name:document.getElementById('pgpSecretName').value,"
"mac:document.getElementById('pgpSecretMac').value,"
"blob:blob,"
"dkey:key"
"};"
"fetch('/api/pgp_secrets',{method:'POST',body:JSON.stringify(data)})"
".then(r=>r.json())"
".then(()=>{"
"alert('PGP Secrets saved!\\n⚠️ Restart device for changes to take effect.');"
"hasPgpSecretsChanges=false;"
"})"
".catch(err=>{"
"alert('Save failed: '+err);"
"console.error('Save PGP secrets failed:',err);"
"});"
"}"

"function resetPgpSecrets(){"
"if(!confirm('Reset PGP Secrets?\\n\\nThis will ERASE all secrets from the pgpsecret namespace!\\n\\nBluetooth will NOT work until you configure new secrets.'))return;"
"fetch('/api/pgp_secrets/reset',{method:'POST'})"
".then(r=>r.json())"
".then(()=>{"
"alert('PGP Secrets reset!\\n⚠️ Restart device.');"
"loadPgpSecrets();"
"})"
".catch(err=>alert('Reset failed: '+err));"
"}"

// Update existing loadDeviceConfig to match:
"function loadDeviceConfig(){"
"fetch('/api/device_config')"
".then(r=>r.json())"
".then(data=>{"
"document.getElementById('deviceName').value=data.name||'';"
"document.getElementById('deviceMac').value=data.mac||'';"
"document.getElementById('deviceBlob').value=data.blob||'';"
"document.getElementById('deviceKey').value=data.dkey||'';"
"hasDeviceConfigChanges=false;"
"})"
".catch(err=>console.error('Load device config failed:',err));"
"}"

// Add character count update on input
"document.addEventListener('DOMContentLoaded',function(){"
"const blob=document.getElementById('pgpSecretBlob');"
"const key=document.getElementById('pgpSecretKey');"
"if(blob)blob.addEventListener('input',updateCharCounts);"
"if(key)key.addEventListener('input',updateCharCounts);"
"});"

/********************************************************* */

"function switchTab(tab){"
"currentTab=tab;"
"document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('active',i===tab));"
"document.querySelectorAll('.tab-content').forEach((c,i)=>c.classList.toggle('active',i===tab));"
"if(tab===1)loadStats();"
"if(tab===2)loadDevices();"
"if(tab===3)loadSecrets();"
"if(tab===4)loadDeviceConfig();"
"}"

"function toggleTimer(){"
"isPaused=!isPaused;"
"const btn=document.getElementById('pauseBtn');"
"const timer=document.getElementById('timer');"
"if(isPaused){"
"btn.textContent='▶ Resume';"
"btn.classList.add('pause');"
"timer.classList.add('paused');"
"fetch('/api/timer/pause',{method:'POST'}).catch(e=>console.error(e));"
"}else{"
"btn.textContent='⏸ Pause';"
"btn.classList.remove('pause');"
"timer.classList.remove('paused');"
"fetch('/api/timer/resume',{method:'POST'}).catch(e=>console.error(e));"
"}"
"}"

"function updateTimer(){"
"if(isPaused)return;"
"fetch('/api/timer').then(r=>r.json()).then(d=>{"
"const mins=Math.floor(d.remaining/60);"
"const secs=d.remaining%60;"
"document.getElementById('timer').textContent=mins+':'+(secs<10?'0':'')+secs;"
"if(d.paused)isPaused=true;"
"if(d.remaining<=0){clearInterval(timerInterval);showStatus('WiFi AP closed','error');}"
"}).catch(e=>console.error(e));"
"}"

"function updateConnectionInfo(){"
"fetch('/api/connections').then(r=>r.json()).then(d=>{"
"document.getElementById('connInfo').textContent=d.active+' of '+d.max+' device'+(d.active===1?'':'s')+' connected';"
"}).catch(e=>console.error(e));"
"}"

"function loadSettings(){"
"fetch('/api/settings').then(r=>r.json()).then(d=>{"
"document.getElementById('autocatch').checked=d.autocatch||false;"
"document.getElementById('autospin').checked=d.autospin||false;"
"document.getElementById('probability').value=d.probability||0;"
"document.getElementById('maxConnections').value=d.maxConnections||2;"
"document.getElementById('logLevel').value=d.logLevel||2;"
"updateProbability();"
"hasChanges=false;"
"}).catch(e=>showStatus('Failed to load settings','error'));"
"}"

"function saveSettings(){"
"if(!hasChanges){showStatus('No changes','error');return;}"
"const data={autocatch:document.getElementById('autocatch').checked,autospin:document.getElementById('autospin').checked,probability:parseInt(document.getElementById('probability').value),maxConnections:parseInt(document.getElementById('maxConnections').value),logLevel:parseInt(document.getElementById('logLevel').value)};"
"fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok'){showStatus('✓ Saved! Restarting...','success');setTimeout(()=>window.location.reload(),3000);}"
"else showStatus('Failed','error');"
"}).catch(e=>showStatus('Error','error'));"
"}"

"function loadStats(){"
"fetch('/api/stats').then(r=>r.json()).then(d=>{"
"const c=document.getElementById('statsContent');"
"if(!d.devices||!d.devices.length){c.innerHTML='<div class=\"no-devices\">No stats</div>';return;}"
"let h='';"
"d.devices.forEach(dev=>{"
"h+=`<div><h3 style=\"margin:20px 0 10px;color:#667eea\">Device ${dev.conn_id}</h3><div class=\"stats-grid\">`;"
"h+=`<div class=\"stat-box\"><div class=\"label\">Caught</div><div class=\"value\">${dev.stats.caught}</div></div>`;"
"h+=`<div class=\"stat-box\"><div class=\"label\">Fled</div><div class=\"value\">${dev.stats.fled}</div></div>`;"
"h+=`<div class=\"stat-box\"><div class=\"label\">Spins</div><div class=\"value\">${dev.stats.spin}</div></div>`;"
"h+='</div></div>';"
"});"
"c.innerHTML=h;"
"}).catch(e=>{document.getElementById('statsContent').innerHTML='<div class=\"no-devices\">Failed</div>';});"
"}"

"function loadDevices(){"
"fetch('/api/devices').then(r=>r.json()).then(d=>{"
"const c=document.getElementById('devicesContent');"
"if(!d.devices||!d.devices.length){c.innerHTML='<div class=\"no-devices\">No devices</div>';return;}"
"let h='';"
"d.devices.forEach(dev=>{"
"h+='<div class=\"device-card\"><div class=\"device-header\">';"
"h+=`<div class=\"device-id\">Device ${dev.conn_id}</div><div class=\"device-status\">● Connected</div></div>`;"
"h+='<div class=\"battery\"><div class=\"battery-icon\">';"
"h+=`<div class=\"battery-fill\" style=\"width:${dev.battery}%\"></div></div><div class=\"battery-tip\"></div>`;"
"h+=`<div class=\"battery-percent\">${dev.battery}%</div></div><div style=\"margin-top:15px\">`;"
"h+='<div class=\"setting-row\"><label>Autocatch</label>';"
"h+=`<label class=\"toggle\"><input type=\"checkbox\" ${dev.settings.autocatch?'checked':''} onchange=\"saveDeviceSetting(${dev.conn_id},'autocatch',this.checked)\"><span class=\"slider\"></span></label></div>`;"
"h+='<div class=\"setting-row\"><label>Autospin</label>';"
"h+=`<label class=\"toggle\"><input type=\"checkbox\" ${dev.settings.autospin?'checked':''} onchange=\"saveDeviceSetting(${dev.conn_id},'autospin',this.checked)\"><span class=\"slider\"></span></label></div>`;"
"h+=`<div style=\"margin-top:10px\"><label>Probability: ${probLabels[dev.settings.probability]}</label>`;"
"h+=`<input type=\"range\" min=\"0\" max=\"9\" value=\"${dev.settings.probability}\" onchange=\"saveDeviceSetting(${dev.conn_id},'probability',this.value)\"></div></div></div>`;"
"});"
"c.innerHTML=h;"
"}).catch(e=>{document.getElementById('devicesContent').innerHTML='<div class=\"no-devices\">Failed</div>';});"
"}"

"function saveDeviceSetting(connId,setting,value){"
"const data={conn_id:connId,setting:setting,value:value};"
"fetch('/api/device',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok')showStatus('✓ Saved','success');"
"else showStatus('Failed','error');"
"}).catch(e=>showStatus('Error','error'));"
"}"

"function loadSecrets(){"
"fetch('/api/secrets').then(r=>r.json()).then(d=>{"
"document.getElementById('wifiSsid').value=d.ssid||'';"
"document.getElementById('wifiPassword').value=d.password||'';"
"document.getElementById('txPower').value=d.tx_power||34;"
"updateTxPower();"
"hasSecretsChanges=false;"
"}).catch(e=>showStatus('Failed to load secrets','error'));"
"}"

"function saveSecrets(){"
"if(!hasSecretsChanges){showStatus('No changes','error');return;}"
"const data={ssid:document.getElementById('wifiSsid').value,password:document.getElementById('wifiPassword').value,tx_power:parseInt(document.getElementById('txPower').value)};"
"fetch('/api/secrets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok'){showStatus('✓ Saved! Restart WiFi to apply','success');hasSecretsChanges=false;}"
"else showStatus('Failed','error');"
"}).catch(e=>showStatus('Error','error'));"
"}"

"function loadDeviceConfig(){"
"fetch('/api/device_config').then(r=>r.json()).then(d=>{"
"document.getElementById('deviceName').value=d.name||'';"
"document.getElementById('deviceMac').value=d.mac||'';"
"document.getElementById('deviceBlob').value=d.blob||'';"
"document.getElementById('deviceKey').value=d.dkey||'';"
"hasDeviceConfigChanges=false;"
"}).catch(e=>showStatus('Failed to load config','error'));"
"}"

"function saveDeviceConfig(){"
"if(!hasDeviceConfigChanges){showStatus('No changes','error');return;}"
"const mac=document.getElementById('deviceMac').value;"
"if(mac&&!/^[0-9A-Fa-f:]{17}$/.test(mac)){showStatus('Invalid MAC (use XX:XX:XX:XX:XX:XX)','error');return;}"
"const blob=document.getElementById('deviceBlob').value;"
"if(blob&&!/^[0-9A-Fa-f]*$/.test(blob)){showStatus('Blob must be hex','error');return;}"
"const dkey=document.getElementById('deviceKey').value;"
"if(dkey&&!/^[0-9A-Fa-f]*$/.test(dkey)){showStatus('Key must be hex','error');return;}"
"const data={name:document.getElementById('deviceName').value,mac:mac,blob:blob,dkey:dkey};"
"fetch('/api/device_config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok'){showStatus('✓ Saved! Restart device to apply','success');hasDeviceConfigChanges=false;}"
"else showStatus('Failed: '+(d.message||'Unknown'),'error');"
"}).catch(e=>showStatus('Error','error'));"
"}"

"function resetDeviceConfig(){"
"if(!confirm('Reset to defaults? Cannot be undone!'))return;"
"fetch('/api/device_config/reset',{method:'POST'})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok'){showStatus('✓ Reset complete','success');loadDeviceConfig();}"
"else showStatus('Failed','error');"
"}).catch(e=>showStatus('Error','error'));"
"}"

"function showStatus(msg,type){"
"const s=document.getElementById('status');"
"s.textContent=msg;s.className='status '+type;s.classList.remove('hidden');"
"setTimeout(()=>s.classList.add('hidden'),3000);"
"}"

"window.addEventListener('beforeunload',e=>{if(hasChanges||hasSecretsChanges||hasDeviceConfigChanges){e.preventDefault();e.returnValue='';}});"

"loadSettings();"
"loadStats();"
"loadDevices();"
"loadSecrets();"
"loadDeviceConfig();"
"updateTimer();"
"updateConnectionInfo();"
"timerInterval=setInterval(()=>{updateTimer();updateConnectionInfo();},1000);"
"</script>"
"</body>"
"</html>";

/* Android Captive Portal Detection */
static esp_err_t android_captive_detect_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Android captive portal: %s", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* Root handler */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

/* Captive portal redirect */
static esp_err_t captive_portal_redirect(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Redirect from: %s", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* API: Connections */
static esp_err_t api_connections_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    int active = get_active_connections();
    int max_conn = get_setting_uint8(&global_settings.target_active_connections);
    cJSON_AddNumberToObject(root, "active", active);
    cJSON_AddNumberToObject(root, "max", max_conn);
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Stats */
static esp_err_t api_stats_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *devices_array = cJSON_CreateArray();
    size_t count = stats_get_count();
    for (size_t i = 0; i < count; i++) {
        const StatsForConn* entry = stats_get_entry(i);
        if (entry != NULL) {
            cJSON *device = cJSON_CreateObject();
            cJSON_AddNumberToObject(device, "conn_id", entry->conn_id);
            cJSON *stats_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(stats_obj, "caught", entry->stats.caught);
            cJSON_AddNumberToObject(stats_obj, "fled", entry->stats.fled);
            cJSON_AddNumberToObject(stats_obj, "spin", entry->stats.spin);
            cJSON_AddItemToObject(device, "stats", stats_obj);
            cJSON_AddItemToArray(devices_array, device);
        }
    }
    cJSON_AddItemToObject(root, "devices", devices_array);
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Devices */
static esp_err_t api_devices_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *devices_array = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry != NULL && entry->settings != NULL) {
            cJSON *device = cJSON_CreateObject();
            cJSON_AddNumberToObject(device, "conn_id", entry->conn_id);
            int battery = 75 + (entry->conn_id * 5);
            if (battery > 100) battery = 100;
            cJSON_AddNumberToObject(device, "battery", battery);
            if (xSemaphoreTake(entry->settings->mutex, pdMS_TO_TICKS(1000))) {
                cJSON *settings_obj = cJSON_CreateObject();
                cJSON_AddBoolToObject(settings_obj, "autocatch", entry->settings->autocatch);
                cJSON_AddBoolToObject(settings_obj, "autospin", entry->settings->autospin);
                cJSON_AddNumberToObject(settings_obj, "probability", entry->settings->autospin_probability);
                cJSON_AddItemToObject(device, "settings", settings_obj);
                xSemaphoreGive(entry->settings->mutex);
            }
            cJSON_AddItemToArray(devices_array, device);
        }
    }
    cJSON_AddItemToObject(root, "devices", devices_array);
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;


}

/* API: Device POST */
static esp_err_t api_device_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)-1));
    if (ret <= 0) {
        httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    uint16_t conn_id = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItem(root, "conn_id"));
    const char *setting = cJSON_GetStringValue(cJSON_GetObjectItem(root, "setting"));
    cJSON *value_obj = cJSON_GetObjectItem(root, "value");
    
    client_state_t* entry = NULL;
    for (int i = 0; i < 4; i++) {
        entry = get_client_state_entry_by_idx(i);
        if (entry && entry->conn_id == conn_id) break;
        entry = NULL;
    }
    
    if (!entry || !entry->settings) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    
    if (xSemaphoreTake(entry->settings->mutex, pdMS_TO_TICKS(1000))) {
        if (strcmp(setting, "autocatch") == 0) {
            entry->settings->autocatch = cJSON_IsTrue(value_obj);
        } else if (strcmp(setting, "autospin") == 0) {
            entry->settings->autospin = cJSON_IsTrue(value_obj);
        } else if (strcmp(setting, "probability") == 0) {
            uint8_t prob = (uint8_t)cJSON_GetNumberValue(value_obj);
            if (prob <= 9) entry->settings->autospin_probability = prob;
        }
        xSemaphoreGive(entry->settings->mutex);
    }
    write_devices_settings_to_nvs();
    cJSON_Delete(root);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

/* API: Settings GET */
static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "autocatch", settings_get_autocatch());
    cJSON_AddBoolToObject(root, "autospin", settings_get_autospin());
    uint8_t prob = 0;
    for (int i = 0; i < 4; i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry && entry->settings) {
            if (xSemaphoreTake(entry->settings->mutex, pdMS_TO_TICKS(1000))) {
                prob = entry->settings->autospin_probability;
                xSemaphoreGive(entry->settings->mutex);
            }
            break;
        }
    }
    cJSON_AddNumberToObject(root, "probability", prob);
    cJSON_AddNumberToObject(root, "maxConnections", 
                            get_setting_uint8(&global_settings.target_active_connections));
    cJSON_AddNumberToObject(root, "logLevel", 
                            get_setting_uint8(&global_settings.log_level));
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Settings POST */
static esp_err_t api_settings_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)-1));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "autocatch")) && cJSON_IsBool(item)) {
        settings_set_autocatch(cJSON_IsTrue(item));
    }
    if ((item = cJSON_GetObjectItem(root, "autospin")) && cJSON_IsBool(item)) {
        settings_set_autospin(cJSON_IsTrue(item));
    }
    if ((item = cJSON_GetObjectItem(root, "probability")) && cJSON_IsNumber(item)) {
        uint8_t prob = (uint8_t)cJSON_GetNumberValue(item);
        if (prob <= 9) {
            for (int i = 0; i < 4; i++) set_device_autospin_probability(i, prob);
        }
    }
    if ((item = cJSON_GetObjectItem(root, "maxConnections")) && cJSON_IsNumber(item)) {
        uint8_t max_conn = (uint8_t)cJSON_GetNumberValue(item);
        if (max_conn >= 1 && max_conn <= 4) {
            set_setting_uint8(&global_settings.target_active_connections, max_conn);
        }
    }
    if ((item = cJSON_GetObjectItem(root, "logLevel")) && cJSON_IsNumber(item)) {
        uint8_t level = (uint8_t)cJSON_GetNumberValue(item);
        if (level >= 1 && level <= 3) {
            set_setting_uint8(&global_settings.log_level, level);
            if (level == 3) log_levels_verbose();
            else if (level == 2) log_levels_info();
            else log_levels_debug();
        }
    }
    
    write_global_settings_to_nvs();
    write_devices_settings_to_nvs();
    cJSON_Delete(root);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(response);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_ap_manager_stop();
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

/* API: Timer */
static esp_err_t api_timer_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    uint32_t remaining = timer_paused ? pause_time_remaining : 
                        wifi_ap_manager_get_remaining_time() / 1000;
    cJSON_AddNumberToObject(root, "remaining", remaining);
    cJSON_AddBoolToObject(root, "paused", timer_paused);
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Timer Pause */
static esp_err_t api_timer_pause_handler(httpd_req_t *req)
{
    if (!timer_paused) {
        pause_time_remaining = wifi_ap_manager_get_remaining_time() / 1000;
        timer_paused = true;
        wifi_ap_manager_pause_timer();
    }
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

/* API: Timer Resume */
static esp_err_t api_timer_resume_handler(httpd_req_t *req)
{
    if (timer_paused) {
        timer_paused = false;
        wifi_ap_manager_resume_timer();
    }
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

/* API: Secrets GET */
static esp_err_t api_secrets_get_handler(httpd_req_t *req)
{
    char ssid[32] = {0};
    char password[64] = {0};
    int8_t tx_power = 0;
    wifi_ap_manager_get_config(ssid, sizeof(ssid), password, sizeof(password), &tx_power);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", ssid);
    cJSON_AddStringToObject(root, "password", password);
    cJSON_AddNumberToObject(root, "tx_power", tx_power);
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Secrets POST */
static esp_err_t api_secrets_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)-1));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    const char *ssid = NULL;
    const char *password = NULL;
    int8_t tx_power = -1;
    
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "ssid")) && cJSON_IsString(item)) {
        ssid = cJSON_GetStringValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "password")) && cJSON_IsString(item)) {
        password = cJSON_GetStringValue(item);
    }
    if ((item = cJSON_GetObjectItem(root, "tx_power")) && cJSON_IsNumber(item)) {
        tx_power = (int8_t)cJSON_GetNumberValue(item);
    }
    
    wifi_ap_manager_set_config(ssid, password, tx_power);
    cJSON_Delete(root);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

/* API: Device Config GET (NEW v1.2.0) */
static esp_err_t api_device_config_get_handler(httpd_req_t *req)
{
    device_config_t config;
    get_device_config(&config);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", config.name);
    cJSON_AddStringToObject(root, "mac", config.mac);
    cJSON_AddStringToObject(root, "blob", config.blob);
    cJSON_AddStringToObject(root, "dkey", config.dkey);
    
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* API: Device Config POST (NEW v1.2.0) */
static esp_err_t api_device_config_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)-1));
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    device_config_t config;
    get_device_config(&config);
    
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "name")) && cJSON_IsString(item)) {
        strncpy(config.name, cJSON_GetStringValue(item), sizeof(config.name)-1);
    }
    if ((item = cJSON_GetObjectItem(root, "mac")) && cJSON_IsString(item)) {
        strncpy(config.mac, cJSON_GetStringValue(item), sizeof(config.mac)-1);
    }
    if ((item = cJSON_GetObjectItem(root, "blob")) && cJSON_IsString(item)) {
        strncpy(config.blob, cJSON_GetStringValue(item), sizeof(config.blob)-1);
    }
    if ((item = cJSON_GetObjectItem(root, "dkey")) && cJSON_IsString(item)) {
        strncpy(config.dkey, cJSON_GetStringValue(item), sizeof(config.dkey)-1);
    }
    
    esp_err_t err = set_device_config(&config);
    cJSON_Delete(root);
    
    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddStringToObject(response, "status", "ok");
    } else {
        cJSON_AddStringToObject(response, "status", "error");
        cJSON_AddStringToObject(response, "message", "Invalid config");
    }
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

/* API: Device Config Reset (NEW v1.2.0) */
static esp_err_t api_device_config_reset_handler(httpd_req_t *req)
{
    reset_device_config();
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free((void *)json_str);
    cJSON_Delete(response);
    return ESP_OK;
}
/****************************************/



/* API: PGP Secrets GET (NEW v2.0) */
static esp_err_t api_get_pgp_secrets(httpd_req_t *req)
{
    device_config_t config;
    esp_err_t ret = get_pgp_secrets_config(&config);
    
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load PGP secrets");
        return ESP_FAIL;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", config.name);
    cJSON_AddStringToObject(root, "mac", config.mac);
    cJSON_AddStringToObject(root, "blob", config.blob);
    cJSON_AddStringToObject(root, "dkey", config.dkey);
    
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_Delete(root);
    free((void*)json_str);
    
    return ESP_OK;
}

/* API: PGP Secrets POST (NEW v2.0) */
static esp_err_t api_post_pgp_secrets(httpd_req_t *req)
{
    char buf[2048];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    device_config_t config = {0};
    
    cJSON *name = cJSON_GetObjectItem(root, "name");
    if (name && cJSON_IsString(name)) {
        strncpy(config.name, name->valuestring, sizeof(config.name) - 1);
    }
    
    cJSON *mac = cJSON_GetObjectItem(root, "mac");
    if (mac && cJSON_IsString(mac)) {
        strncpy(config.mac, mac->valuestring, sizeof(config.mac) - 1);
    }
    
    cJSON *blob = cJSON_GetObjectItem(root, "blob");
    if (blob && cJSON_IsString(blob)) {
        strncpy(config.blob, blob->valuestring, sizeof(config.blob) - 1);
    }
    
    cJSON *dkey = cJSON_GetObjectItem(root, "dkey");
    if (dkey && cJSON_IsString(dkey)) {
        strncpy(config.dkey, dkey->valuestring, sizeof(config.dkey) - 1);
    }
    
    cJSON_Delete(root);
    
    esp_err_t err = set_pgp_secrets_config(&config);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
        return ESP_FAIL;
    }
    
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* API: PGP Secrets Reset (NEW v2.0) */
static esp_err_t api_reset_pgp_secrets(httpd_req_t *req)
{
    esp_err_t ret = reset_pgp_secrets();
    
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Reset failed");
        return ESP_FAIL;
    }
    
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}



/*******************************************/
/* Start web server */
esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 28;  // v1.2.0: Increased for device config
    config.lru_purge_enable = true;
    config.max_uri_handlers = 28;
    
    ESP_LOGI(TAG, "Starting web server v1.2.0");
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Android captive portal
        httpd_uri_t uri_gen204 = {"/generate_204", HTTP_GET, android_captive_detect_handler, NULL};
        httpd_register_uri_handler(server, &uri_gen204);
        httpd_uri_t uri_gen204_alt = {"/gen_204", HTTP_GET, android_captive_detect_handler, NULL};
        httpd_register_uri_handler(server, &uri_gen204_alt);
        httpd_uri_t uri_hotspot = {"/hotspot-detect.html", HTTP_GET, android_captive_detect_handler, NULL};
        httpd_register_uri_handler(server, &uri_hotspot);
        
        // Main
        httpd_uri_t uri_index = {"/", HTTP_GET, index_handler, NULL};
        httpd_register_uri_handler(server, &uri_index);
        
        // APIs
        httpd_uri_t uri_conn = {"/api/connections", HTTP_GET, api_connections_handler, NULL};
        httpd_register_uri_handler(server, &uri_conn);
        httpd_uri_t uri_stats = {"/api/stats", HTTP_GET, api_stats_handler, NULL};
        httpd_register_uri_handler(server, &uri_stats);
        httpd_uri_t uri_devs = {"/api/devices", HTTP_GET, api_devices_handler, NULL};
        httpd_register_uri_handler(server, &uri_devs);



        httpd_uri_t uri_dev_post = {"/api/device", HTTP_POST, api_device_post_handler, NULL};
        httpd_register_uri_handler(server, &uri_dev_post);
        httpd_uri_t uri_set_get = {"/api/settings", HTTP_GET, api_settings_get_handler, NULL};
        httpd_register_uri_handler(server, &uri_set_get);
        httpd_uri_t uri_set_post = {"/api/settings", HTTP_POST, api_settings_post_handler, NULL};
        httpd_register_uri_handler(server, &uri_set_post);
        httpd_uri_t uri_timer = {"/api/timer", HTTP_GET, api_timer_handler, NULL};
        httpd_register_uri_handler(server, &uri_timer);
        httpd_uri_t uri_pause = {"/api/timer/pause", HTTP_POST, api_timer_pause_handler, NULL};
        httpd_register_uri_handler(server, &uri_pause);
        httpd_uri_t uri_resume = {"/api/timer/resume", HTTP_POST, api_timer_resume_handler, NULL};
        httpd_register_uri_handler(server, &uri_resume);
        httpd_uri_t uri_sec_get = {"/api/secrets", HTTP_GET, api_secrets_get_handler, NULL};
        httpd_register_uri_handler(server, &uri_sec_get);
        httpd_uri_t uri_sec_post = {"/api/secrets", HTTP_POST, api_secrets_post_handler, NULL};
        httpd_register_uri_handler(server, &uri_sec_post);
        
        // NEW v1.2.0: Device Config APIs
        httpd_uri_t uri_dc_get = {"/api/device_config", HTTP_GET, api_device_config_get_handler, NULL};
        httpd_register_uri_handler(server, &uri_dc_get);
        httpd_uri_t uri_dc_post = {"/api/device_config", HTTP_POST, api_device_config_post_handler, NULL};
        httpd_register_uri_handler(server, &uri_dc_post);
        httpd_uri_t uri_dc_reset = {"/api/device_config/reset", HTTP_POST, api_device_config_reset_handler, NULL};
        httpd_register_uri_handler(server, &uri_dc_reset);
        
        /************************************************ */
        
        // PGP Secrets APIs (NEW v2.0)
        httpd_uri_t api_get_pgp_secrets_uri = {
            .uri = "/api/pgp_secrets",
            .method = HTTP_GET,
            .handler = api_get_pgp_secrets,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_get_pgp_secrets_uri);
        
        httpd_uri_t api_post_pgp_secrets_uri = {
            .uri = "/api/pgp_secrets",
            .method = HTTP_POST,
            .handler = api_post_pgp_secrets,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_post_pgp_secrets_uri);
        
        httpd_uri_t api_reset_pgp_secrets_uri = {
            .uri = "/api/pgp_secrets/reset",
            .method = HTTP_POST,
            .handler = api_reset_pgp_secrets,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_reset_pgp_secrets_uri);



        /************************************************* */
        
        // Catchall
        httpd_uri_t uri_catchall = {"/*", HTTP_GET, captive_portal_redirect, NULL};
        httpd_register_uri_handler(server, &uri_catchall);
        
        ESP_LOGI(TAG, "Web server started (v1.2.0 with 5 tabs)");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start");
    return ESP_FAIL;
}

/* Stop web server */
esp_err_t web_server_stop(void)
{
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
        timer_paused = false;
        pause_time_remaining = 0;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}
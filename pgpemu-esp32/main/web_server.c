/**
 * @file web_server.c v1.3.1
 * @brief Complete Web Server with 5 Tabs + BDA-Tracking info
 *
 * Endpoints
 * ---------
 *  GET  /                          → Control Panel HTML
 *  GET  /generate_204              → Android captive portal
 *  GET  /gen_204                   → Android captive portal (alt)
 *  GET  /hotspot-detect.html       → iOS captive portal
 *  GET  /api/connections
 *  GET  /api/stats
 *  GET  /api/devices
 *  POST /api/device                ← per-device settings save
 *  GET  /api/settings
 *  POST /api/settings
 *  GET  /api/timer
 *  POST /api/timer/pause
 *  POST /api/timer/resume
 *  GET  /api/secrets
 *  POST /api/secrets
 *  GET  /api/device_config
 *  POST /api/device_config
 *  POST /api/device_config/reset
 *  GET  / *                         → captive portal redirect
 */

#include "web_server.h"
#include "wifi_ap_manager.h"
#include "config_storage.h"
#include "settings.h"
#include "pgp_handshake_multi.h"
#include "stats.h"
#include "device_config.h"
#include "log_tags.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

// Timer pause/resume bookkeeping
static bool timer_paused = false;
static uint32_t pause_time_remaining = 0;

// ============================================================
// Helper
// ============================================================

/** Read the full request body into a heap buffer.  Caller must free(). */
static char* read_body(httpd_req_t *req) {
    if (req->content_len == 0) return NULL;
    char *buf = malloc(req->content_len + 1);
    if (!buf) return NULL;
    int received = 0;
    while (received < (int)req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) { free(buf); return NULL; }
        received += ret;
    }
    buf[received] = '\0';
    return buf;
}

static void json_ok(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

// ============================================================
// HTML (5-Tab Control Panel)
// ============================================================

static const char index_html[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>PGPemu Control Panel</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px}"
".container{max-width:900px;margin:0 auto}"
"h1{color:#fff;text-align:center;margin-bottom:10px;font-size:28px;text-shadow:0 2px 4px rgba(0,0,0,.2)}"
".subtitle{color:#fff;text-align:center;opacity:.9;margin-bottom:20px;font-size:14px}"
".info-bar{background:rgba(255,255,255,.95);padding:15px;border-radius:8px;margin-bottom:20px;box-shadow:0 4px 8px rgba(0,0,0,.2)}"
".timer-section{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px}"
".timer{font-size:28px;font-weight:bold;color:#667eea}"
".timer.paused{color:#f5576c}"
".timer-btn{padding:8px 16px;background:#667eea;color:#fff;border:none;border-radius:6px;cursor:pointer;font-size:14px;font-weight:600}"
".timer-btn.pause{background:#f5576c}"
".connections{color:#555;font-size:14px}"
".info-box{background:#e3f2fd;color:#1976d2;padding:12px;border-radius:6px;margin-bottom:15px;font-size:13px;line-height:1.6}"
".info-box strong{display:block;margin-bottom:5px;font-size:14px}"
".warning-box{background:#fff3cd;color:#856404;padding:12px;border-radius:6px;margin-bottom:15px;font-size:13px;line-height:1.6}"
".tabs{display:flex;gap:8px;margin-bottom:20px;flex-wrap:wrap}"
".tab{flex:1 1 auto;min-width:110px;padding:10px;background:rgba(255,255,255,.2);color:#fff;border:none;border-radius:8px;cursor:pointer;font-size:13px;font-weight:600}"
".tab.active{background:#fff;color:#667eea;box-shadow:0 4px 8px rgba(0,0,0,.2)}"
".tab-content{display:none}"
".tab-content.active{display:block}"
".card{background:#fff;border-radius:12px;padding:22px;margin:12px 0;box-shadow:0 8px 16px rgba(0,0,0,.2)}"
".card h2{margin:0 0 18px 0;color:#333;font-size:18px;border-bottom:2px solid #667eea;padding-bottom:8px}"
".setting{margin:18px 0}"
".setting label{font-weight:500;color:#555;display:block;margin-bottom:6px}"
".setting .help{font-size:12px;color:#888;margin-top:4px}"
".setting-row{display:flex;justify-content:space-between;align-items:center;margin:12px 0;padding:10px;background:#f8f9fa;border-radius:8px}"
".toggle{position:relative;display:inline-block;width:56px;height:28px}"
".toggle input{opacity:0;width:0;height:0}"
".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.3s;border-radius:28px}"
".slider:before{position:absolute;content:'';height:22px;width:22px;left:3px;bottom:3px;background:#fff;transition:.3s;border-radius:50%;box-shadow:0 2px 4px rgba(0,0,0,.2)}"
"input:checked+.slider{background:#667eea}"
"input:checked+.slider:before{transform:translateX(28px)}"
".range-value{display:inline-block;min-width:80px;text-align:right;font-weight:bold;color:#667eea;font-size:16px}"
"input[type='range']{width:100%;height:6px;border-radius:3px;background:#ddd;outline:none;-webkit-appearance:none}"
"input[type='range']::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer}"
"input[type='text'],input[type='password'],textarea,select{width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-size:14px}"
"input:focus,textarea:focus,select:focus{border-color:#667eea;outline:none}"
"textarea{resize:vertical;font-family:monospace}"
"button{width:100%;padding:13px;background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;box-shadow:0 4px 8px rgba(0,0,0,.2)}"
"button:hover{opacity:.9}"
".btn-group{display:flex;gap:10px}"
".btn-group button{flex:1}"
".btn-reset{background:linear-gradient(135deg,#f5576c,#f093fb)}"
".status{text-align:center;padding:12px;border-radius:8px;margin:12px 0;font-weight:500}"
".status.success{background:#d4edda;color:#155724}"
".status.error{background:#f8d7da;color:#721c24}"
".hidden{display:none}"
".stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin:16px 0}"
".stat-box{background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;padding:18px;border-radius:12px;text-align:center}"
".stat-box .label{font-size:13px;opacity:.9;margin-bottom:6px}"
".stat-box .value{font-size:28px;font-weight:bold}"
".device-card{background:#f8f9fa;border-radius:12px;padding:18px;margin:12px 0;border-left:4px solid #667eea}"
".device-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}"
".device-id{font-size:16px;font-weight:bold;color:#667eea}"
".device-status{padding:3px 10px;background:#4CAF50;color:#fff;border-radius:12px;font-size:11px}"
".no-devices{text-align:center;padding:36px;color:#888;font-size:15px}"
".prob-labels{display:flex;justify-content:space-between;font-size:11px;color:#888;margin-top:4px}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🎮 PGPemu Control Panel</h1>"
"<div class='subtitle'>v1.3.1 – Two-ESP32 Shared Secrets Support</div>"
"<div class='info-box'>"
"<strong>ℹ️ Quick Start</strong>"
"• Hold button <strong>2 s</strong> → WiFi AP starts (auto-closes in 2 min)<br>"
"• SSID: <strong>PGPemu-Setup</strong> · Password: <strong>PogoPogo</strong><br>"
"• Switching between two ESP32 units with same secrets is now supported"
"</div>"
"<div class='info-bar'>"
"<div class='timer-section'>"
"<div class='timer' id='timer'>2:00</div>"
"<button class='timer-btn' id='pauseBtn' onclick='toggleTimer()'>⏸ Pause</button>"
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

/* ── Tab 0: Settings ── */
"<div class='tab-content active' id='tab0'>"
"<div class='card'>"
"<h2>⚙️ Global Settings</h2>"
"<div class='setting-row'><div><label>Auto Catch</label><div class='help'>Automatically catch Pokémon</div></div>"
"<label class='toggle'><input type='checkbox' id='autocatch' onchange='settingsChanged()'><span class='slider'></span></label></div>"
"<div class='setting-row'><div><label>Auto Spin</label><div class='help'>Automatically spin Pokéstops</div></div>"
"<label class='toggle'><input type='checkbox' id='autospin' onchange='settingsChanged()'><span class='slider'></span></label></div>"
"<div class='setting'><label>Spin Probability: <span class='range-value' id='probValue'>Always</span></label>"
"<div class='help'>0 = Always · 5 = 50% · 9 = Rare (10%)</div>"
"<input type='range' id='probability' min='0' max='9' value='0' oninput='updateProbability()' onchange='settingsChanged()'>"
"<div class='prob-labels'><span>Always</span><span>50%</span><span>Rare</span></div></div>"
"<div class='setting'><label>Max Connections</label><select id='maxConnections' onchange='settingsChanged()'>"
"<option value='1'>1</option><option value='2'>2</option><option value='3'>3</option><option value='4'>4</option>"
"</select></div>"
"<div class='setting'><label>Log Level</label><select id='logLevel' onchange='settingsChanged()'>"
"<option value='1'>Debug</option><option value='2'>Info</option><option value='3'>Verbose</option>"
"</select></div>"
"<button onclick='saveSettings()'>💾 Save &amp; Restart</button>"
"</div></div>"

/* ── Tab 1: Statistics ── */
"<div class='tab-content' id='tab1'>"
"<div class='card'><h2>📊 Statistics</h2>"
"<div id='statsContent'><div class='no-devices'>Loading...</div></div>"
"<button onclick='loadStats()' style='margin-top:16px;background:linear-gradient(135deg,#f093fb,#f5576c)'>🔄 Refresh</button>"
"</div></div>"

/* ── Tab 2: Devices ── */
"<div class='tab-content' id='tab2'>"
"<div class='card'><h2>📱 Connected Devices</h2>"
"<div id='devicesContent'><div class='no-devices'>Loading...</div></div>"
"<button onclick='loadDevices()' style='margin-top:16px;background:linear-gradient(135deg,#f093fb,#f5576c)'>🔄 Refresh</button>"
"</div></div>"

/* ── Tab 3: Secrets ── */
"<div class='tab-content' id='tab3'>"
"<div class='card'><h2>🔐 WiFi AP Secrets</h2>"
"<div class='setting'><label>SSID</label><input type='text' id='wifiSsid' maxlength='31' onchange='secretsChanged()'></div>"
"<div class='setting'><label>Password (WPA2)</label><div class='help'>Leave empty for open network</div>"
"<input type='password' id='wifiPassword' maxlength='63' onchange='secretsChanged()'></div>"
"<div class='setting'><label>TX Power: <span class='range-value' id='txPowerValue'>8.5 dBm</span></label>"
"<input type='range' id='txPower' min='8' max='84' value='34' oninput='updateTxPower()' onchange='secretsChanged()'>"
"<div class='prob-labels'><span>2 dBm</span><span>8.5 dBm</span><span>21 dBm</span></div></div>"
"<button onclick='saveSecrets()'>💾 Save Secrets</button>"
"</div></div>"

/* ── Tab 4: Device Config ── */
"<div class='tab-content' id='tab4'>"
"<div class='card'><h2>🎮 PGP Device Configuration</h2>"
"<div class='warning-box'><strong>⚠️ Advanced</strong> Changes require device restart.</div>"
"<div class='setting'><label>Device Name</label><input type='text' id='deviceName' maxlength='63' onchange='deviceConfigChanged()'></div>"
"<div class='setting'><label>MAC Address (XX:XX:XX:XX:XX:XX)</label><input type='text' id='deviceMac' maxlength='17' onchange='deviceConfigChanged()'></div>"
"<div class='setting'><label>Blob (hex)</label><textarea id='deviceBlob' rows='4' onchange='deviceConfigChanged()'></textarea></div>"
"<div class='setting'><label>Device Key (hex)</label><input type='text' id='deviceKey' maxlength='32' onchange='deviceConfigChanged()'></div>"
"<div class='btn-group'>"
"<button onclick='saveDeviceConfig()'>💾 Save</button>"
"<button class='btn-reset' onclick='resetDeviceConfig()'>🔄 Reset</button>"
"</div></div></div>"

"</div>"   /* /container */

"<script>"
"let timerInterval,hasChanges=false,hasSecretsChanges=false,hasDeviceConfigChanges=false,isPaused=false;"
"const probLabels=['Always','10%','20%','30%','40%','50%','60%','70%','80%','90%'];"
"function updateProbability(){document.getElementById('probValue').textContent=probLabels[document.getElementById('probability').value];}"
"function updateTxPower(){document.getElementById('txPowerValue').textContent=(document.getElementById('txPower').value*0.25).toFixed(1)+' dBm';}"
"function settingsChanged(){hasChanges=true;}"
"function secretsChanged(){hasSecretsChanges=true;}"
"function deviceConfigChanged(){hasDeviceConfigChanges=true;}"
"function switchTab(t){"
"document.querySelectorAll('.tab').forEach((x,i)=>x.classList.toggle('active',i===t));"
"document.querySelectorAll('.tab-content').forEach((x,i)=>x.classList.toggle('active',i===t));"
"if(t===1)loadStats();if(t===2)loadDevices();if(t===3)loadSecrets();if(t===4)loadDeviceConfig();"
"}"
"function toggleTimer(){"
"isPaused=!isPaused;"
"const b=document.getElementById('pauseBtn'),ti=document.getElementById('timer');"
"if(isPaused){b.textContent='▶ Resume';b.classList.add('pause');ti.classList.add('paused');fetch('/api/timer/pause',{method:'POST'});}"
"else{b.textContent='⏸ Pause';b.classList.remove('pause');ti.classList.remove('paused');fetch('/api/timer/resume',{method:'POST'});}"
"}"
"function updateTimer(){"
"if(isPaused)return;"
"fetch('/api/timer').then(r=>r.json()).then(d=>{"
"const m=Math.floor(d.remaining/60),s=d.remaining%60;"
"document.getElementById('timer').textContent=m+':'+(s<10?'0':'')+s;"
"if(d.remaining<=0){clearInterval(timerInterval);showStatus('WiFi AP closed','error');}"
"}).catch(()=>{});"
"}"
"function updateConnInfo(){"
"fetch('/api/connections').then(r=>r.json()).then(d=>{"
"document.getElementById('connInfo').textContent=d.active+' of '+d.max+' device'+(d.active===1?'':'s')+' connected';"
"}).catch(()=>{});"
"}"
"function loadSettings(){"
"fetch('/api/settings').then(r=>r.json()).then(d=>{"
"document.getElementById('autocatch').checked=!!d.autocatch;"
"document.getElementById('autospin').checked=!!d.autospin;"
"document.getElementById('probability').value=d.probability||0;"
"document.getElementById('maxConnections').value=d.maxConnections||2;"
"document.getElementById('logLevel').value=d.logLevel||2;"
"updateProbability();hasChanges=false;"
"}).catch(()=>showStatus('Failed to load settings','error'));"
"}"
"function saveSettings(){"
"if(!hasChanges){showStatus('No changes','error');return;}"
"const data={autocatch:document.getElementById('autocatch').checked,autospin:document.getElementById('autospin').checked,"
"probability:parseInt(document.getElementById('probability').value),maxConnections:parseInt(document.getElementById('maxConnections').value),"
"logLevel:parseInt(document.getElementById('logLevel').value)};"
"fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{if(d.status==='ok'){showStatus('✓ Saved – restarting…','success');setTimeout(()=>window.location.reload(),3000);}"
"else showStatus('Failed','error');}).catch(()=>showStatus('Error','error'));"
"}"
"function loadStats(){"
"fetch('/api/stats').then(r=>r.json()).then(d=>{"
"const c=document.getElementById('statsContent');"
"if(!d.devices||!d.devices.length){c.innerHTML='<div class=\"no-devices\">No stats yet</div>';return;}"
"let h='';d.devices.forEach(dev=>{"
"h+='<div><h3 style=\"margin:16px 0 8px;color:#667eea\">Device '+dev.conn_id+'</h3><div class=\"stats-grid\">';"
"h+='<div class=\"stat-box\"><div class=\"label\">Caught</div><div class=\"value\">'+dev.stats.caught+'</div></div>';"
"h+='<div class=\"stat-box\"><div class=\"label\">Fled</div><div class=\"value\">'+dev.stats.fled+'</div></div>';"
"h+='<div class=\"stat-box\"><div class=\"label\">Spins</div><div class=\"value\">'+dev.stats.spin+'</div></div>';"
"h+='</div></div>';});c.innerHTML=h;"
"}).catch(()=>{document.getElementById('statsContent').innerHTML='<div class=\"no-devices\">Failed</div>';});"
"}"
"function loadDevices(){"
"fetch('/api/devices').then(r=>r.json()).then(d=>{"
"const c=document.getElementById('devicesContent');"
"if(!d.devices||!d.devices.length){c.innerHTML='<div class=\"no-devices\">No devices connected</div>';return;}"
"let h='';d.devices.forEach(dev=>{"
"h+='<div class=\"device-card\"><div class=\"device-header\">';"
"h+='<div class=\"device-id\">Device '+dev.conn_id+'</div><div class=\"device-status\">● Connected</div></div>';"
"h+='<div class=\"setting-row\"><label>Autocatch</label>';"
"h+='<label class=\"toggle\"><input type=\"checkbox\" '+(dev.settings.autocatch?'checked':'')+' onchange=\"saveDeviceSetting('+dev.conn_id+',\\'autocatch\\',this.checked)\"><span class=\"slider\"></span></label></div>';"
"h+='<div class=\"setting-row\"><label>Autospin</label>';"
"h+='<label class=\"toggle\"><input type=\"checkbox\" '+(dev.settings.autospin?'checked':'')+' onchange=\"saveDeviceSetting('+dev.conn_id+',\\'autospin\\',this.checked)\"><span class=\"slider\"></span></label></div>';"
"h+='<div style=\"margin-top:10px\"><label>Probability: '+probLabels[dev.settings.probability]+'</label>';"
"h+='<input type=\"range\" min=\"0\" max=\"9\" value=\"'+dev.settings.probability+'\" onchange=\"saveDeviceSetting('+dev.conn_id+',\\'probability\\',this.value)\"></div></div>';"
"});c.innerHTML=h;"
"}).catch(()=>{document.getElementById('devicesContent').innerHTML='<div class=\"no-devices\">Failed</div>';});"
"}"
"function saveDeviceSetting(connId,setting,value){"
"fetch('/api/device',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({conn_id:connId,setting:setting,value:value})})"
".then(r=>r.json()).then(d=>{if(d.status==='ok')showStatus('✓ Saved','success');else showStatus('Failed','error');})"
".catch(()=>showStatus('Error','error'));"
"}"
"function loadSecrets(){"
"fetch('/api/secrets').then(r=>r.json()).then(d=>{"
"document.getElementById('wifiSsid').value=d.ssid||'';"
"document.getElementById('wifiPassword').value=d.password||'';"
"document.getElementById('txPower').value=d.tx_power||34;"
"updateTxPower();hasSecretsChanges=false;"
"}).catch(()=>showStatus('Failed to load secrets','error'));"
"}"
"function saveSecrets(){"
"if(!hasSecretsChanges){showStatus('No changes','error');return;}"
"const data={ssid:document.getElementById('wifiSsid').value,password:document.getElementById('wifiPassword').value,tx_power:parseInt(document.getElementById('txPower').value)};"
"fetch('/api/secrets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{if(d.status==='ok'){showStatus('✓ Saved','success');hasSecretsChanges=false;}else showStatus('Failed','error');})"
".catch(()=>showStatus('Error','error'));"
"}"
"function loadDeviceConfig(){"
"fetch('/api/device_config').then(r=>r.json()).then(d=>{"
"document.getElementById('deviceName').value=d.name||'';"
"document.getElementById('deviceMac').value=d.mac||'';"
"document.getElementById('deviceBlob').value=d.blob||'';"
"document.getElementById('deviceKey').value=d.dkey||'';"
"hasDeviceConfigChanges=false;"
"}).catch(()=>showStatus('Failed to load config','error'));"
"}"
"function saveDeviceConfig(){"
"if(!hasDeviceConfigChanges){showStatus('No changes','error');return;}"
"const mac=document.getElementById('deviceMac').value;"
"if(mac&&!/^[0-9A-Fa-f:]{17}$/.test(mac)){showStatus('Invalid MAC','error');return;}"
"const data={name:document.getElementById('deviceName').value,mac:mac,"
"blob:document.getElementById('deviceBlob').value,dkey:document.getElementById('deviceKey').value};"
"fetch('/api/device_config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{if(d.status==='ok'){showStatus('✓ Saved – restart device to apply','success');hasDeviceConfigChanges=false;}"
"else showStatus('Failed: '+(d.message||'?'),'error');}).catch(()=>showStatus('Error','error'));"
"}"
"function resetDeviceConfig(){"
"if(!confirm('Reset to defaults?'))return;"
"fetch('/api/device_config/reset',{method:'POST'}).then(r=>r.json()).then(d=>{"
"if(d.status==='ok'){showStatus('✓ Reset','success');loadDeviceConfig();}else showStatus('Failed','error');})"
".catch(()=>showStatus('Error','error'));"
"}"
"function showStatus(msg,type){"
"const s=document.getElementById('status');"
"s.textContent=msg;s.className='status '+type;s.classList.remove('hidden');"
"setTimeout(()=>s.classList.add('hidden'),3000);"
"}"
"loadSettings();loadStats();loadDevices();loadSecrets();loadDeviceConfig();"
"updateTimer();updateConnInfo();"
"timerInterval=setInterval(()=>{updateTimer();updateConnInfo();},1000);"
"</script>"
"</body></html>";

// ============================================================
// Captive portal / main page
// ============================================================

static esp_err_t captive_redirect_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

// ============================================================
// /api/connections
// ============================================================

static esp_err_t api_connections_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "active", get_active_connections());
    cJSON_AddNumberToObject(root, "max", get_setting_uint8(&global_settings.target_active_connections));
    char *s = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s); cJSON_Delete(root);
    return ESP_OK;
}

// ============================================================
// /api/stats
// ============================================================

static esp_err_t api_stats_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    size_t count = stats_get_count();
    for (size_t i = 0; i < count; i++) {
        const StatsForConn *e = stats_get_entry(i);
        if (!e) continue;
        cJSON *dev = cJSON_CreateObject();
        cJSON_AddNumberToObject(dev, "conn_id", e->conn_id);
        cJSON *st = cJSON_CreateObject();
        cJSON_AddNumberToObject(st, "caught", e->stats.caught);
        cJSON_AddNumberToObject(st, "fled",   e->stats.fled);
        cJSON_AddNumberToObject(st, "spin",   e->stats.spin);
        cJSON_AddItemToObject(dev, "stats", st);
        cJSON_AddItemToArray(arr, dev);
    }
    cJSON_AddItemToObject(root, "devices", arr);
    char *s = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s); cJSON_Delete(root);
    return ESP_OK;
}

// ============================================================
// /api/devices  (GET)
// ============================================================

static esp_err_t api_devices_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        client_state_t *entry = get_client_state_entry_by_idx(i);
        if (!entry || !entry->settings) continue;
        cJSON *dev = cJSON_CreateObject();
        cJSON_AddNumberToObject(dev, "conn_id", entry->conn_id);
        if (xSemaphoreTake(entry->settings->mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            cJSON *st = cJSON_CreateObject();
            cJSON_AddBoolToObject(st, "autocatch", entry->settings->autocatch);
            cJSON_AddBoolToObject(st, "autospin",  entry->settings->autospin);
            cJSON_AddNumberToObject(st, "probability", entry->settings->autospin_probability);
            cJSON_AddItemToObject(dev, "settings", st);
            xSemaphoreGive(entry->settings->mutex);
        }
        cJSON_AddItemToArray(arr, dev);
    }
    cJSON_AddItemToObject(root, "devices", arr);
    char *s = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s); cJSON_Delete(root);
    return ESP_OK;
}

// ============================================================
// /api/device  (POST) – per-device setting save
// ============================================================

static esp_err_t api_device_post_handler(httpd_req_t *req) {
    char *buf = read_body(req);
    if (!buf) { httpd_resp_send_408(req); return ESP_FAIL; }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *conn_id_json = cJSON_GetObjectItem(root, "conn_id");
    cJSON *setting_json = cJSON_GetObjectItem(root, "setting");
    cJSON *value_json   = cJSON_GetObjectItem(root, "value");

    if (!cJSON_IsNumber(conn_id_json) || !cJSON_IsString(setting_json) || !value_json) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields");
        return ESP_FAIL;
    }

    uint16_t conn_id = (uint16_t)cJSON_GetNumberValue(conn_id_json);
    const char *setting = cJSON_GetStringValue(setting_json);

    // Find device by conn_id
    client_state_t *entry = NULL;
    for (int i = 0; i < 4; i++) {
        client_state_t *e = get_client_state_entry_by_idx(i);
        if (e && e->conn_id == conn_id) { entry = e; break; }
    }

    if (!entry || !entry->settings) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Device not found");
        return ESP_FAIL;
    }

    bool saved = false;
    if (xSemaphoreTake(entry->settings->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (strcmp(setting, "autocatch") == 0) {
            entry->settings->autocatch = cJSON_IsTrue(value_json);
            saved = true;
        } else if (strcmp(setting, "autospin") == 0) {
            entry->settings->autospin = cJSON_IsTrue(value_json);
            saved = true;
        } else if (strcmp(setting, "probability") == 0 && cJSON_IsNumber(value_json)) {
            uint8_t p = (uint8_t)cJSON_GetNumberValue(value_json);
            if (p <= 9) { entry->settings->autospin_probability = p; saved = true; }
        }
        xSemaphoreGive(entry->settings->mutex);
    }
    cJSON_Delete(root);

    if (saved) {
        write_devices_settings_to_nvs();
        ESP_LOGI(TAG, "[%d] device setting '%s' updated via web", conn_id, setting);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, saved ? "{\"status\":\"ok\"}" : "{\"status\":\"error\",\"message\":\"Invalid setting\"}");
    return ESP_OK;
}

// ============================================================
// /api/settings  (GET + POST)
// ============================================================

static esp_err_t api_settings_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "autocatch", settings_get_autocatch());
    cJSON_AddBoolToObject(root, "autospin",  settings_get_autospin());

    // pull probability from first connected device
    uint8_t prob = 0;
    for (int i = 0; i < 4; i++) {
        client_state_t *e = get_client_state_entry_by_idx(i);
        if (e && e->settings && xSemaphoreTake(e->settings->mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            prob = e->settings->autospin_probability;
            xSemaphoreGive(e->settings->mutex);
            break;
        }
    }
    cJSON_AddNumberToObject(root, "probability", prob);
    cJSON_AddNumberToObject(root, "maxConnections",
                            get_setting_uint8(&global_settings.target_active_connections));
    cJSON_AddNumberToObject(root, "logLevel",
                            get_setting_uint8(&global_settings.log_level));
    char *s = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s); cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_settings_post_handler(httpd_req_t *req) {
    char *buf = read_body(req);
    if (!buf) { httpd_resp_send_408(req); return ESP_FAIL; }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "autocatch")) && cJSON_IsBool(item))
        settings_set_autocatch(cJSON_IsTrue(item));
    if ((item = cJSON_GetObjectItem(root, "autospin")) && cJSON_IsBool(item))
        settings_set_autospin(cJSON_IsTrue(item));
    if ((item = cJSON_GetObjectItem(root, "probability")) && cJSON_IsNumber(item)) {
        uint8_t p = (uint8_t)cJSON_GetNumberValue(item);
        if (p <= 9) for (int i = 0; i < 4; i++) set_device_autospin_probability(i, p);
    }
    if ((item = cJSON_GetObjectItem(root, "maxConnections")) && cJSON_IsNumber(item)) {
        uint8_t v = (uint8_t)cJSON_GetNumberValue(item);
        if (v >= 1 && v <= 4) set_setting_uint8(&global_settings.target_active_connections, v);
    }
    if ((item = cJSON_GetObjectItem(root, "logLevel")) && cJSON_IsNumber(item)) {
        uint8_t v = (uint8_t)cJSON_GetNumberValue(item);
        if (v >= 1 && v <= 3) {
            set_setting_uint8(&global_settings.log_level, v);
            if (v == 3) log_levels_verbose();
            else if (v == 2) log_levels_info();
            else log_levels_debug();
        }
    }
    cJSON_Delete(root);

    write_global_settings_to_nvs();
    write_devices_settings_to_nvs();

    json_ok(req);

    // stop WiFi and restart after a short delay
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_ap_manager_stop();
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

// ============================================================
// /api/timer
// ============================================================

static esp_err_t api_timer_get_handler(httpd_req_t *req) {
    uint32_t remaining = timer_paused ? pause_time_remaining
                                      : wifi_ap_manager_get_remaining_time() / 1000;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "remaining", remaining);
    cJSON_AddBoolToObject(root, "paused", timer_paused);
    char *s = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s); cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_timer_pause_handler(httpd_req_t *req) {
    if (!timer_paused) {
        pause_time_remaining = wifi_ap_manager_get_remaining_time() / 1000;
        timer_paused = true;
        wifi_ap_manager_pause_timer();
    }
    json_ok(req); return ESP_OK;
}

static esp_err_t api_timer_resume_handler(httpd_req_t *req) {
    if (timer_paused) {
        timer_paused = false;
        wifi_ap_manager_resume_timer();
    }
    json_ok(req); return ESP_OK;
}

// ============================================================
// /api/secrets
// ============================================================

static esp_err_t api_secrets_get_handler(httpd_req_t *req) {
    char ssid[32] = {0}, password[64] = {0};
    int8_t tx_power = 0;
    wifi_ap_manager_get_config(ssid, sizeof(ssid), password, sizeof(password), &tx_power);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ssid", ssid);
    cJSON_AddStringToObject(root, "password", password);
    cJSON_AddNumberToObject(root, "tx_power", tx_power);
    char *s = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s); cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_secrets_post_handler(httpd_req_t *req) {
    char *buf = read_body(req);
    if (!buf) { httpd_resp_send_408(req); return ESP_FAIL; }
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON"); return ESP_FAIL; }

    const char *ssid = NULL, *password = NULL;
    int8_t tx_power = -1;
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "ssid")) && cJSON_IsString(item))
        ssid = cJSON_GetStringValue(item);
    if ((item = cJSON_GetObjectItem(root, "password")) && cJSON_IsString(item))
        password = cJSON_GetStringValue(item);
    if ((item = cJSON_GetObjectItem(root, "tx_power")) && cJSON_IsNumber(item))
        tx_power = (int8_t)cJSON_GetNumberValue(item);

    wifi_ap_manager_set_config(ssid, password, tx_power);
    cJSON_Delete(root);
    json_ok(req); return ESP_OK;
}

// ============================================================
// /api/device_config
// ============================================================

static esp_err_t api_device_config_get_handler(httpd_req_t *req) {
    device_config_t cfg;
    get_device_config(&cfg);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", cfg.name);
    cJSON_AddStringToObject(root, "mac",  cfg.mac);
    cJSON_AddStringToObject(root, "blob", cfg.blob);
    cJSON_AddStringToObject(root, "dkey", cfg.dkey);
    char *s = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s); cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_device_config_post_handler(httpd_req_t *req) {
    char *buf = read_body(req);
    if (!buf) { httpd_resp_send_408(req); return ESP_FAIL; }
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON"); return ESP_FAIL; }

    device_config_t cfg;
    get_device_config(&cfg);

    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "name")) && cJSON_IsString(item))
        strncpy(cfg.name, cJSON_GetStringValue(item), sizeof(cfg.name)-1);
    if ((item = cJSON_GetObjectItem(root, "mac")) && cJSON_IsString(item))
        strncpy(cfg.mac,  cJSON_GetStringValue(item), sizeof(cfg.mac)-1);
    if ((item = cJSON_GetObjectItem(root, "blob")) && cJSON_IsString(item))
        strncpy(cfg.blob, cJSON_GetStringValue(item), sizeof(cfg.blob)-1);
    if ((item = cJSON_GetObjectItem(root, "dkey")) && cJSON_IsString(item))
        strncpy(cfg.dkey, cJSON_GetStringValue(item), sizeof(cfg.dkey)-1);
    cJSON_Delete(root);

    esp_err_t err = set_device_config(&cfg);
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"Invalid config\"}");
    }
    return ESP_OK;
}

static esp_err_t api_device_config_reset_handler(httpd_req_t *req) {
    reset_device_config();
    json_ok(req); return ESP_OK;
}

// ============================================================
// Start / Stop
// ============================================================

esp_err_t web_server_start(void) {
    if (server != NULL) {
        ESP_LOGW(TAG, "Already running");
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 20;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 24;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

#define REG(path, method, handler) do { \
    httpd_uri_t _u = {path, method, handler, NULL}; \
    httpd_register_uri_handler(server, &_u); \
} while (0)

    // Captive portal detection (Android / iOS / Windows)
    REG("/generate_204",        HTTP_GET,  captive_redirect_handler);
    REG("/gen_204",             HTTP_GET,  captive_redirect_handler);
    REG("/hotspot-detect.html", HTTP_GET,  captive_redirect_handler);
    REG("/ncsi.txt",            HTTP_GET,  captive_redirect_handler);
    REG("/connecttest.txt",     HTTP_GET,  captive_redirect_handler);

    // Main page
    REG("/",                    HTTP_GET,  index_handler);

    // API
    REG("/api/connections",         HTTP_GET,  api_connections_handler);
    REG("/api/stats",               HTTP_GET,  api_stats_handler);
    REG("/api/devices",             HTTP_GET,  api_devices_get_handler);
    REG("/api/device",              HTTP_POST, api_device_post_handler);
    REG("/api/settings",            HTTP_GET,  api_settings_get_handler);
    REG("/api/settings",            HTTP_POST, api_settings_post_handler);
    REG("/api/timer",               HTTP_GET,  api_timer_get_handler);
    REG("/api/timer/pause",         HTTP_POST, api_timer_pause_handler);
    REG("/api/timer/resume",        HTTP_POST, api_timer_resume_handler);
    REG("/api/secrets",             HTTP_GET,  api_secrets_get_handler);
    REG("/api/secrets",             HTTP_POST, api_secrets_post_handler);
    REG("/api/device_config",       HTTP_GET,  api_device_config_get_handler);
    REG("/api/device_config",       HTTP_POST, api_device_config_post_handler);
    REG("/api/device_config/reset", HTTP_POST, api_device_config_reset_handler);

    // Catch-all → redirect (must be last)
    REG("/*",                   HTTP_GET,  captive_redirect_handler);

#undef REG

    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}

esp_err_t web_server_stop(void) {
    if (server == NULL) return ESP_ERR_INVALID_STATE;
    httpd_stop(server);
    server = NULL;
    timer_paused = false;
    pause_time_remaining = 0;
    ESP_LOGI(TAG, "Web server stopped");
    return ESP_OK;
}

bool web_server_is_running(void) {
    return server != NULL;
}

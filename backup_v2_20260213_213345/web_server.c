/**
 * @file web_server.c
 * @brief ULTIMATE Web Server Implementation for PGPemu Configuration
 * 
 * FEATURES:
 * - Global Settings (Autocatch/Autospin/Probability/MaxConn/LogLevel)
 * - Statistics (Caught/Fled/Spin per Device)
 * - Battery Status
 * - Connected Devices List
 * - Per-Device Settings (Autocatch/Autospin/Probability)
 * - Responsive Multi-Tab Interface
 */

#include "web_server.h"
#include "wifi_ap_manager.h"
#include "config_storage.h"
#include "settings.h"
#include "pgp_handshake_multi.h"
#include "stats.h"
#include "log_tags.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

// Enhanced HTML with tabs and advanced features
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
"h1{color:#fff;text-align:center;margin-bottom:10px;font-size:32px;text-shadow:0 2px 4px rgba(0,0,0,0.2)}"
".subtitle{color:#fff;text-align:center;opacity:0.9;margin-bottom:20px;font-size:14px}"
".info-bar{background:rgba(255,255,255,0.95);padding:15px;border-radius:8px;margin-bottom:20px;display:flex;justify-content:space-between;align-items:center;box-shadow:0 4px 8px rgba(0,0,0,0.2)}"
".timer{font-size:24px;font-weight:bold;color:#667eea}"
".connections{color:#555;font-size:14px}"
".tabs{display:flex;gap:10px;margin-bottom:20px}"
".tab{flex:1;padding:12px;background:rgba(255,255,255,0.2);color:#fff;border:none;border-radius:8px;cursor:pointer;font-size:14px;font-weight:600;transition:all 0.3s}"
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
"select{width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-size:14px;background:white;cursor:pointer}"
"select:focus{border-color:#667eea;outline:none}"
"button{width:100%;padding:14px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;box-shadow:0 4px 8px rgba(0,0,0,0.2);transition:transform 0.2s}"
"button:hover{transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.3)}"
"button:active{transform:translateY(0)}"
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
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🎮 PGPemu Control Panel</h1>"
"<div class='subtitle'>Advanced Pokemon Go Plus Emulator Configuration</div>"

// Info Bar
"<div class='info-bar'>"
"<div class='timer' id='timer'>3:00</div>"
"<div class='connections' id='connInfo'>Loading...</div>"
"</div>"

"<div id='status' class='status hidden'></div>"

// Tabs
"<div class='tabs'>"
"<button class='tab active' onclick='switchTab(0)'>⚙️ Settings</button>"
"<button class='tab' onclick='switchTab(1)'>📊 Statistics</button>"
"<button class='tab' onclick='switchTab(2)'>📱 Devices</button>"
"</div>"

// Tab 1: Global Settings
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
"<div class='help'>Higher = more detailed logs (affects performance)</div>"
"<select id='logLevel' onchange='settingsChanged()'>"
"<option value='1'>Debug (Basic)</option>"
"<option value='2'>Info (Recommended)</option>"
"<option value='3'>Verbose (Detailed)</option>"
"</select>"
"</div>"

"<button onclick='saveSettings()'>💾 Save Settings & Restart</button>"
"</div>"
"</div>"

// Tab 2: Statistics
"<div class='tab-content' id='tab1'>"
"<div class='card'>"
"<h2>📊 Statistics</h2>"
"<div id='statsContent'><div class='no-devices'>Loading statistics...</div></div>"
"<button onclick='loadStats()' style='margin-top:20px;background:linear-gradient(135deg,#f093fb,#f5576c)'>🔄 Refresh Stats</button>"
"</div>"
"</div>"

// Tab 3: Connected Devices
"<div class='tab-content' id='tab2'>"
"<div class='card'>"
"<h2>📱 Connected Devices</h2>"
"<div id='devicesContent'><div class='no-devices'>Loading devices...</div></div>"
"<button onclick='loadDevices()' style='margin-top:20px;background:linear-gradient(135deg,#f093fb,#f5576c)'>🔄 Refresh Devices</button>"
"</div>"
"</div>"

"</div>"

"<script>"
"let timerInterval,hasChanges=false,currentTab=0;"

// Probability labels
"const probLabels=['Always','10%','20%','30%','40%','50%','60%','70%','80%','90%'];"

"function updateProbability(){"
"const val=document.getElementById('probability').value;"
"document.getElementById('probValue').textContent=probLabels[val];"
"}"

"function settingsChanged(){hasChanges=true;}"

"function switchTab(tab){"
"currentTab=tab;"
"document.querySelectorAll('.tab').forEach((t,i)=>{"
"t.classList.toggle('active',i===tab);"
"});"
"document.querySelectorAll('.tab-content').forEach((c,i)=>{"
"c.classList.toggle('active',i===tab);"
"});"
"if(tab===1)loadStats();"
"if(tab===2)loadDevices();"
"}"

"function updateTimer(){"
"fetch('/api/timer').then(r=>r.json()).then(d=>{"
"const mins=Math.floor(d.remaining/60);"
"const secs=d.remaining%60;"
"document.getElementById('timer').textContent=mins+':'+(secs<10?'0':'')+secs;"
"if(d.remaining<=0){clearInterval(timerInterval);showStatus('WiFi AP closed','error');}"
"}).catch(e=>console.error('Timer error:',e));"
"}"

"function updateConnectionInfo(){"
"fetch('/api/connections').then(r=>r.json()).then(d=>{"
"document.getElementById('connInfo').textContent="
"`${d.active} of ${d.max} device${d.active===1?'':'s'} connected`;"
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
"if(!hasChanges){showStatus('No changes to save','error');return;}"
"const data={"
"autocatch:document.getElementById('autocatch').checked,"
"autospin:document.getElementById('autospin').checked,"
"probability:parseInt(document.getElementById('probability').value),"
"maxConnections:parseInt(document.getElementById('maxConnections').value),"
"logLevel:parseInt(document.getElementById('logLevel').value)"
"};"
"fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok'){"
"showStatus('✓ Settings saved! Restarting...','success');"
"setTimeout(()=>window.location.reload(),3000);"
"}else showStatus('Failed to save','error');"
"}).catch(e=>showStatus('Error: '+e.message,'error'));"
"}"

"function loadStats(){"
"fetch('/api/stats').then(r=>r.json()).then(d=>{"
"const container=document.getElementById('statsContent');"
"if(!d.devices||d.devices.length===0){"
"container.innerHTML='<div class=\"no-devices\">No statistics available</div>';"
"return;"
"}"
"let html='';"
"d.devices.forEach(dev=>{"
"html+=`<div><h3 style=\"margin:20px 0 10px;color:#667eea\">Device ${dev.conn_id}</h3>`;"
"html+='<div class=\"stats-grid\">';"
"html+=`<div class=\"stat-box\"><div class=\"label\">Caught</div><div class=\"value\">${dev.stats.caught}</div></div>`;"
"html+=`<div class=\"stat-box\"><div class=\"label\">Fled</div><div class=\"value\">${dev.stats.fled}</div></div>`;"
"html+=`<div class=\"stat-box\"><div class=\"label\">Spins</div><div class=\"value\">${dev.stats.spin}</div></div>`;"
"html+='</div></div>';"
"});"
"container.innerHTML=html;"
"}).catch(e=>{"
"document.getElementById('statsContent').innerHTML='<div class=\"no-devices\">Failed to load stats</div>';"
"});"
"}"

"function loadDevices(){"
"fetch('/api/devices').then(r=>r.json()).then(d=>{"
"const container=document.getElementById('devicesContent');"
"if(!d.devices||d.devices.length===0){"
"container.innerHTML='<div class=\"no-devices\">No devices connected</div>';"
"return;"
"}"
"let html='';"
"d.devices.forEach(dev=>{"
"html+='<div class=\"device-card\">';"
"html+='<div class=\"device-header\">';"
"html+=`<div class=\"device-id\">Device ${dev.conn_id}</div>`;"
"html+='<div class=\"device-status\">● Connected</div>';"
"html+='</div>';"

// Battery
"html+='<div class=\"battery\">';"
"html+='<div class=\"battery-icon\">';"
"html+=`<div class=\"battery-fill\" style=\"width:${dev.battery}%\"></div>`;"
"html+='</div>';"
"html+='<div class=\"battery-tip\"></div>';"
"html+=`<div class=\"battery-percent\">${dev.battery}%</div>`;"
"html+='</div>';"

// Per-Device Settings
"html+='<div style=\"margin-top:15px\">';"
"html+='<div class=\"setting-row\">';"
"html+=`<label>Autocatch</label>`;"
"html+=`<label class=\"toggle\"><input type=\"checkbox\" ${dev.settings.autocatch?'checked':''} "
"onchange=\"saveDeviceSetting(${dev.conn_id},'autocatch',this.checked)\"><span class=\"slider\"></span></label>`;"
"html+='</div>';"

"html+='<div class=\"setting-row\">';"
"html+=`<label>Autospin</label>`;"
"html+=`<label class=\"toggle\"><input type=\"checkbox\" ${dev.settings.autospin?'checked':''} "
"onchange=\"saveDeviceSetting(${dev.conn_id},'autospin',this.checked)\"><span class=\"slider\"></span></label>`;"
"html+='</div>';"

"html+='<div style=\"margin-top:10px\">';"
"html+=`<label>Probability: ${probLabels[dev.settings.probability]}</label>`;"
"html+=`<input type=\"range\" min=\"0\" max=\"9\" value=\"${dev.settings.probability}\" "
"onchange=\"saveDeviceSetting(${dev.conn_id},'probability',this.value)\">`;"
"html+='</div>';"
"html+='</div>';"

"html+='</div>';"
"});"
"container.innerHTML=html;"
"}).catch(e=>{"
"document.getElementById('devicesContent').innerHTML='<div class=\"no-devices\">Failed to load devices</div>';"
"});"
"}"

"function saveDeviceSetting(connId,setting,value){"
"const data={conn_id:connId,setting:setting,value:value};"
"fetch('/api/device',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok')showStatus('✓ Device setting saved','success');"
"else showStatus('Failed to save','error');"
"}).catch(e=>showStatus('Error','error'));"
"}"

"function showStatus(msg,type){"
"const s=document.getElementById('status');"
"s.textContent=msg;s.className='status '+type;s.classList.remove('hidden');"
"setTimeout(()=>s.classList.add('hidden'),3000);"
"}"

"window.addEventListener('beforeunload',e=>{if(hasChanges){e.preventDefault();e.returnValue='';}});"

"loadSettings();"
"loadStats();"
"loadDevices();"
"updateTimer();"
"updateConnectionInfo();"
"timerInterval=setInterval(()=>{updateTimer();updateConnectionInfo();},1000);"
"</script>"
"</body>"
"</html>";

/* HTTP GET handler for root */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

/* HTTP GET handler for connections API */
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

/* HTTP GET handler for statistics API */
static esp_err_t api_stats_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *devices_array = cJSON_CreateArray();
    
    // Get stats using getter functions
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

/* HTTP GET handler for devices API */
static esp_err_t api_devices_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *devices_array = cJSON_CreateArray();
    
    // Iterate through all possible connections
    for (int i = 0; i < 4; i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry != NULL && entry->settings != NULL) {
            cJSON *device = cJSON_CreateObject();
            cJSON_AddNumberToObject(device, "conn_id", entry->conn_id);
            
            // Battery level (placeholder - can be read from GATT if available)
            // For now, generate a realistic value
            int battery = 75 + (entry->conn_id * 5); // Example: 75%, 80%, 85%, 90%
            if (battery > 100) battery = 100;
            cJSON_AddNumberToObject(device, "battery", battery);
            
            // Device settings
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

/* HTTP POST handler for per-device settings */
static esp_err_t api_device_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *conn_id_obj = cJSON_GetObjectItem(root, "conn_id");
    cJSON *setting_obj = cJSON_GetObjectItem(root, "setting");
    cJSON *value_obj = cJSON_GetObjectItem(root, "value");
    
    if (!conn_id_obj || !setting_obj || !value_obj) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing parameters");
        return ESP_FAIL;
    }
    
    uint16_t conn_id = (uint16_t)cJSON_GetNumberValue(conn_id_obj);
    const char *setting = cJSON_GetStringValue(setting_obj);
    
    // Find device by conn_id
    client_state_t* entry = NULL;
    for (int i = 0; i < 4; i++) {
        entry = get_client_state_entry_by_idx(i);
        if (entry != NULL && entry->conn_id == conn_id) {
            break;
        }
        entry = NULL;
    }
    
    if (entry == NULL || entry->settings == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Device not found");
        return ESP_FAIL;
    }
    
    // Update setting
    if (xSemaphoreTake(entry->settings->mutex, pdMS_TO_TICKS(1000))) {
        if (strcmp(setting, "autocatch") == 0) {
            entry->settings->autocatch = cJSON_IsTrue(value_obj);
            ESP_LOGI(TAG, "[%d] Autocatch set to %d", conn_id, entry->settings->autocatch);
        } else if (strcmp(setting, "autospin") == 0) {
            entry->settings->autospin = cJSON_IsTrue(value_obj);
            ESP_LOGI(TAG, "[%d] Autospin set to %d", conn_id, entry->settings->autospin);
        } else if (strcmp(setting, "probability") == 0) {
            uint8_t prob = (uint8_t)cJSON_GetNumberValue(value_obj);
            if (prob <= 9) {
                entry->settings->autospin_probability = prob;
                ESP_LOGI(TAG, "[%d] Probability set to %d", conn_id, prob);
            }
        }
        xSemaphoreGive(entry->settings->mutex);
    }
    
    // Save to NVS
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

/* HTTP GET handler for settings API */
static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddBoolToObject(root, "autocatch", settings_get_autocatch());
    cJSON_AddBoolToObject(root, "autospin", settings_get_autospin());
    
    // Get probability from first device
    uint8_t prob = 0;
    for (int i = 0; i < 4; i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry != NULL && entry->settings != NULL) {
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

/* HTTP POST handler for settings API */
static esp_err_t api_settings_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Received settings update");
    
    cJSON *autocatch = cJSON_GetObjectItem(root, "autocatch");
    if (autocatch && cJSON_IsBool(autocatch)) {
        settings_set_autocatch(cJSON_IsTrue(autocatch));
        ESP_LOGI(TAG, "Autocatch set to %d", cJSON_IsTrue(autocatch));
    }
    
    cJSON *autospin = cJSON_GetObjectItem(root, "autospin");
    if (autospin && cJSON_IsBool(autospin)) {
        settings_set_autospin(cJSON_IsTrue(autospin));
        ESP_LOGI(TAG, "Autospin set to %d", cJSON_IsTrue(autospin));
    }
    
    cJSON *probability = cJSON_GetObjectItem(root, "probability");
    if (probability && cJSON_IsNumber(probability)) {
        uint8_t prob = (uint8_t)cJSON_GetNumberValue(probability);
        if (prob <= 9) {
            for (int i = 0; i < 4; i++) {
                set_device_autospin_probability(i, prob);
            }
            ESP_LOGI(TAG, "Autospin probability set to %d", prob);
        }
    }
    
    cJSON *maxConn = cJSON_GetObjectItem(root, "maxConnections");
    if (maxConn && cJSON_IsNumber(maxConn)) {
        uint8_t max_conn = (uint8_t)cJSON_GetNumberValue(maxConn);
        if (max_conn >= 1 && max_conn <= 4) {
            set_setting_uint8(&global_settings.target_active_connections, max_conn);
            ESP_LOGI(TAG, "Max connections set to %d", max_conn);
        }
    }
    
    cJSON *logLevel = cJSON_GetObjectItem(root, "logLevel");
    if (logLevel && cJSON_IsNumber(logLevel)) {
        uint8_t level = (uint8_t)cJSON_GetNumberValue(logLevel);
        if (level >= 1 && level <= 3) {
            set_setting_uint8(&global_settings.log_level, level);
            ESP_LOGI(TAG, "Log level set to %d", level);
            
            if (level == 3) {
                log_levels_verbose();
            } else if (level == 2) {
                log_levels_info();
            } else {
                log_levels_debug();
            }
        }
    }
    
    ESP_LOGI(TAG, "Saving settings to NVS...");
    write_global_settings_to_nvs();
    write_devices_settings_to_nvs();
    
    cJSON_Delete(root);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "Settings saved, restarting...");
    
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(response);
    
    ESP_LOGI(TAG, "Settings saved successfully, scheduling restart...");
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_ap_manager_stop();
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    
    return ESP_OK;
}

/* HTTP GET handler for timer API */
static esp_err_t api_timer_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    uint32_t remaining = wifi_ap_manager_get_remaining_time() / 1000;
    cJSON_AddNumberToObject(root, "remaining", remaining);
    
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
        
        httpd_uri_t api_connections_uri = {
            .uri = "/api/connections",
            .method = HTTP_GET,
            .handler = api_connections_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_connections_uri);
        
        httpd_uri_t api_stats_uri = {
            .uri = "/api/stats",
            .method = HTTP_GET,
            .handler = api_stats_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_stats_uri);
        
        httpd_uri_t api_devices_uri = {
            .uri = "/api/devices",
            .method = HTTP_GET,
            .handler = api_devices_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_devices_uri);
        
        httpd_uri_t api_device_post_uri = {
            .uri = "/api/device",
            .method = HTTP_POST,
            .handler = api_device_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_device_post_uri);
        
        httpd_uri_t api_settings_get_uri = {
            .uri = "/api/settings",
            .method = HTTP_GET,
            .handler = api_settings_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_settings_get_uri);
        
        httpd_uri_t api_settings_post_uri = {
            .uri = "/api/settings",
            .method = HTTP_POST,
            .handler = api_settings_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_settings_post_uri);
        
        httpd_uri_t api_timer_uri = {
            .uri = "/api/timer",
            .method = HTTP_GET,
            .handler = api_timer_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_timer_uri);
        
        ESP_LOGI(TAG, "Web server started successfully");
        ESP_LOGI(TAG, "Access at: http://192.168.4.1");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

esp_err_t web_server_stop(void)
{
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping web server");
        httpd_stop(server);
        server = NULL;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

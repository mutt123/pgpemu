/**
 * @file web_server.c
 * @brief Enhanced Web Server Implementation for PGPemu Configuration
 * 
 * NEW FEATURES:
 * - Autospin Probability Slider (0-9)
 * - Max Connections Setting (1-4)
 * - Log Level (Debug/Info/Verbose)
 * - Better UI with sections
 */

#include "web_server.h"
#include "wifi_ap_manager.h"
#include "config_storage.h"
#include "settings.h"
#include "pgp_handshake_multi.h"  // For client_state_t and get_client_state_entry_by_idx
#include "log_tags.h"              // For log_levels_verbose/info/debug
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

// Enhanced HTML page with all terminal options
static const char index_html[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>PGPemu Configuration</title>"
"<style>"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;max-width:700px;margin:0 auto;padding:20px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh}"
"h1{color:#fff;text-align:center;margin-bottom:10px;font-size:28px}"
".subtitle{color:#fff;text-align:center;opacity:0.9;margin-bottom:30px;font-size:14px}"
".card{background:white;border-radius:12px;padding:25px;margin:20px 0;box-shadow:0 8px 16px rgba(0,0,0,0.2)}"
".card h2{margin-top:0;color:#333;font-size:20px;border-bottom:2px solid #667eea;padding-bottom:10px}"
".setting{margin:20px 0;}"
".setting label{font-weight:500;color:#555;display:block;margin-bottom:8px}"
".setting .help{font-size:12px;color:#888;margin-top:4px}"
".toggle{position:relative;display:inline-block;width:56px;height:28px}"
".toggle input{opacity:0;width:0;height:0}"
".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.3s;border-radius:28px}"
".slider:before{position:absolute;content:'';height:22px;width:22px;left:3px;bottom:3px;background:white;transition:.3s;border-radius:50%;box-shadow:0 2px 4px rgba(0,0,0,0.2)}"
"input:checked+.slider{background:#667eea}"
"input:checked+.slider:before{transform:translateX(28px)}"
".range-container{margin:10px 0}"
".range-value{display:inline-block;min-width:80px;text-align:right;font-weight:bold;color:#667eea;font-size:18px}"
"input[type='range']{width:100%;height:6px;border-radius:3px;background:#ddd;outline:none;-webkit-appearance:none}"
"input[type='range']::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.2)}"
"input[type='range']::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer;border:none;box-shadow:0 2px 4px rgba(0,0,0,0.2)}"
"select{width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-size:14px;background:white;cursor:pointer}"
"select:focus{border-color:#667eea;outline:none}"
"button{width:100%;padding:14px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;margin-top:20px;box-shadow:0 4px 8px rgba(0,0,0,0.2);transition:transform 0.2s}"
"button:hover{transform:translateY(-2px);box-shadow:0 6px 12px rgba(0,0,0,0.3)}"
"button:active{transform:translateY(0)}"
"button.secondary{background:linear-gradient(135deg,#f093fb 0%,#f5576c 100%)}"
".info{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);padding:15px;border-radius:8px;margin:0 0 20px 0;font-size:14px;color:white;text-align:center;font-weight:500}"
".timer{font-size:20px;font-weight:bold}"
".status{text-align:center;padding:12px;border-radius:8px;margin:15px 0;font-weight:500}"
".status.success{background:#d4edda;color:#155724}"
".status.error{background:#f8d7da;color:#721c24}"
".hidden{display:none}"
".setting-row{display:flex;justify-content:space-between;align-items:center;margin:15px 0}"
".prob-labels{display:flex;justify-content:space-between;font-size:11px;color:#888;margin-top:5px}"
"</style>"
"</head>"
"<body>"
"<h1>🎮 PGPemu Setup</h1>"
"<div class='subtitle'>Configure your Pokemon Go Plus Emulator</div>"
"<div class='info'>WiFi AP will auto-close in <span class='timer' id='timer'>3:00</span></div>"
"<div id='status' class='status hidden'></div>"

// Main Settings Card
"<div class='card'>"
"<h2>⚙️ Main Settings</h2>"

"<div class='setting-row'>"
"<div><label>Auto Catch Pokemon</label><div class='help'>Automatically catch Pokemon</div></div>"
"<label class='toggle'><input type='checkbox' id='autocatch' onchange='settingsChanged()'><span class='slider'></span></label>"
"</div>"

"<div class='setting-row'>"
"<div><label>Auto Spin Pokestops</label><div class='help'>Automatically spin Pokestops</div></div>"
"<label class='toggle'><input type='checkbox' id='autospin' onchange='settingsChanged()'><span class='slider'></span></label>"
"</div>"

"<div class='setting'>"
"<label>Autospin Probability: <span class='range-value' id='probValue'>100%</span></label>"
"<div class='help'>0 = Always spin | 1-9 = 10%-90% chance | Higher = less frequent</div>"
"<div class='range-container'>"
"<input type='range' id='probability' min='0' max='9' value='0' oninput='updateProbability()' onchange='settingsChanged()'>"
"<div class='prob-labels'>"
"<span>Always (0)</span><span>50% (5)</span><span>Rare (9)</span>"
"</div>"
"</div>"
"</div>"
"</div>"

// System Settings Card
"<div class='card'>"
"<h2>🔧 System Settings</h2>"

"<div class='setting'>"
"<label>Max Connections</label>"
"<div class='help'>How many devices can connect simultaneously (1-4)</div>"
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
"</div>"

// Save Button
"<button onclick='saveSettings()'>💾 Save Settings & Restart</button>"

"<script>"
"let timerInterval;"
"let hasChanges=false;"

"function updateProbability(){"
"const val=document.getElementById('probability').value;"
"const labels=['Always','10%','20%','30%','40%','50%','60%','70%','80%','90%'];"
"document.getElementById('probValue').textContent=labels[val];"
"}"

"function settingsChanged(){"
"hasChanges=true;"
"}"

"function updateTimer(){"
"fetch('/api/timer').then(r=>r.json()).then(d=>{"
"const mins=Math.floor(d.remaining/60);"
"const secs=d.remaining%60;"
"document.getElementById('timer').textContent=mins+':'+(secs<10?'0':'')+secs;"
"if(d.remaining<=0){"
"clearInterval(timerInterval);"
"showStatus('WiFi AP closed','error');"
"}"
"}).catch(e=>console.error('Timer error:',e));"
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
"}).catch(e=>{"
"showStatus('Failed to load settings','error');"
"console.error(e);"
"});"
"}"

"function saveSettings(){"
"if(!hasChanges){"
"showStatus('No changes to save','error');"
"return;"
"}"
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
"showStatus('✓ Settings saved! Device restarting...','success');"
"setTimeout(()=>{window.location.reload();},3000);"
"}else{"
"showStatus('Failed to save settings','error');"
"}"
"}).catch(e=>{"
"showStatus('Error: '+e.message,'error');"
"console.error(e);"
"});"
"}"

"function showStatus(msg,type){"
"const s=document.getElementById('status');"
"s.textContent=msg;"
"s.className='status '+type;"
"s.classList.remove('hidden');"
"setTimeout(()=>s.classList.add('hidden'),5000);"
"}"

// Warn before leaving if unsaved changes
"window.addEventListener('beforeunload',e=>{"
"if(hasChanges){"
"e.preventDefault();"
"e.returnValue='';"
"}"
"});"

"loadSettings();"
"updateTimer();"
"timerInterval=setInterval(updateTimer,1000);"
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

/* HTTP GET handler for settings API */
static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Get current settings
    cJSON_AddBoolToObject(root, "autocatch", settings_get_autocatch());
    cJSON_AddBoolToObject(root, "autospin", settings_get_autospin());
    
    // NEW: Get autospin probability from first device
    uint8_t prob = 0;
    client_state_t* entry = NULL;
    for (int i = 0; i < 4; i++) {
        entry = get_client_state_entry_by_idx(i);
        if (entry != NULL && entry->settings != NULL) {
            if (xSemaphoreTake(entry->settings->mutex, pdMS_TO_TICKS(1000))) {
                prob = entry->settings->autospin_probability;
                xSemaphoreGive(entry->settings->mutex);
            }
            break;
        }
    }
    cJSON_AddNumberToObject(root, "probability", prob);
    
    // NEW: Get max connections
    cJSON_AddNumberToObject(root, "maxConnections", 
                            get_setting_uint8(&global_settings.target_active_connections));
    
    // NEW: Get log level
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
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Received settings update");
    
    // Update autocatch/autospin
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
    
    // NEW: Update probability for all connected devices
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
    
    // NEW: Update max connections
    cJSON *maxConn = cJSON_GetObjectItem(root, "maxConnections");
    if (maxConn && cJSON_IsNumber(maxConn)) {
        uint8_t max_conn = (uint8_t)cJSON_GetNumberValue(maxConn);
        if (max_conn >= 1 && max_conn <= 4) {
            set_setting_uint8(&global_settings.target_active_connections, max_conn);
            ESP_LOGI(TAG, "Max connections set to %d", max_conn);
        }
    }
    
    // NEW: Update log level
    cJSON *logLevel = cJSON_GetObjectItem(root, "logLevel");
    if (logLevel && cJSON_IsNumber(logLevel)) {
        uint8_t level = (uint8_t)cJSON_GetNumberValue(logLevel);
        if (level >= 1 && level <= 3) {
            set_setting_uint8(&global_settings.log_level, level);
            ESP_LOGI(TAG, "Log level set to %d", level);
            
            // Apply log level immediately
            if (level == 3) {
                log_levels_verbose();
            } else if (level == 2) {
                log_levels_info();
            } else {
                log_levels_debug();
            }
        }
    }
    
    // Save to NVS
    ESP_LOGI(TAG, "Saving settings to NVS...");
    write_global_settings_to_nvs();
    write_devices_settings_to_nvs();
    
    cJSON_Delete(root);
    
    // Prepare response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "Settings saved, restarting...");
    
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(response);
    
    // Schedule restart after response sent
    ESP_LOGI(TAG, "Settings saved successfully, scheduling restart...");
    
    // Stop WiFi AP and restart in 2 seconds
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
    
    uint32_t remaining = wifi_ap_manager_get_remaining_time() / 1000; // Convert to seconds
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
        // Register URI handlers
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
        
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

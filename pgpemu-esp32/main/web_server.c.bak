/**
 * @file web_server.c
 * @brief Web Server Implementation for PGPemu Configuration
 */

#include "web_server.h"
#include "wifi_ap_manager.h"
#include "config_storage.h"
#include "settings.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

// HTML page with configuration interface
static const char index_html[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>PGPemu Configuration</title>"
"<style>"
"body{font-family:Arial,sans-serif;max-width:600px;margin:40px auto;padding:20px;background:#f5f5f5}"
"h1{color:#333;text-align:center}"
".card{background:white;border-radius:8px;padding:20px;margin:20px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
".setting{margin:15px 0;display:flex;justify-content:space-between;align-items:center}"
".setting label{font-weight:500;color:#555}"
".toggle{position:relative;display:inline-block;width:50px;height:24px}"
".toggle input{opacity:0;width:0;height:0}"
".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;transition:.4s;border-radius:24px}"
".slider:before{position:absolute;content:'';height:18px;width:18px;left:3px;bottom:3px;background:white;transition:.4s;border-radius:50%}"
"input:checked+.slider{background:#4CAF50}"
"input:checked+.slider:before{transform:translateX(26px)}"
"button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:4px;font-size:16px;cursor:pointer;margin-top:10px}"
"button:hover{background:#45a049}"
"button.secondary{background:#2196F3}"
"button.secondary:hover{background:#0b7dda}"
".info{background:#e3f2fd;padding:10px;border-radius:4px;margin:10px 0;font-size:14px;color:#1976d2}"
".status{text-align:center;padding:10px;border-radius:4px;margin:10px 0}"
".status.success{background:#d4edda;color:#155724}"
".status.error{background:#f8d7da;color:#721c24}"
".hidden{display:none}"
"</style>"
"</head>"
"<body>"
"<h1>🎮 PGPemu Setup</h1>"
"<div class='info'>WiFi AP will auto-close in <span id='timer'>3:00</span></div>"
"<div id='status' class='status hidden'></div>"
"<div class='card'>"
"<h2>Settings</h2>"
"<div class='setting'>"
"<label>Auto Catch Pokemon</label>"
"<label class='toggle'><input type='checkbox' id='autocatch'><span class='slider'></span></label>"
"</div>"
"<div class='setting'>"
"<label>Auto Spin Pokestops</label>"
"<label class='toggle'><input type='checkbox' id='autospin'><span class='slider'></span></label>"
"</div>"
"<div class='setting'>"
"<label>Powerbank Ping</label>"
"<label class='toggle'><input type='checkbox' id='powerbank'><span class='slider'></span></label>"
"</div>"
"<div class='setting'>"
"<label>Show Actions on LED</label>"
"<label class='toggle'><input type='checkbox' id='led_actions'><span class='slider'></span></label>"
"</div>"
"<div class='setting'>"
"<label>Verbose Logging</label>"
"<label class='toggle'><input type='checkbox' id='verbose'><span class='slider'></span></label>"
"</div>"
"<button onclick='saveSettings()'>💾 Save Settings & Close WiFi</button>"
"<button class='secondary' onclick='loadSettings()'>🔄 Refresh</button>"
"</div>"
"<script>"
"let timerInterval;"
"function updateTimer(){"
"fetch('/api/status').then(r=>r.json()).then(d=>{"
"let sec=Math.floor(d.remaining_ms/1000);"
"if(sec<=0){clearInterval(timerInterval);document.getElementById('timer').textContent='Closing...';return;}"
"let min=Math.floor(sec/60);sec=sec%60;"
"document.getElementById('timer').textContent=min+':'+(sec<10?'0':'')+sec;"
"}).catch(e=>console.error(e));"
"}"
"function loadSettings(){"
"fetch('/api/settings').then(r=>r.json()).then(d=>{"
"document.getElementById('autocatch').checked=d.autocatch;"
"document.getElementById('autospin').checked=d.autospin;"
"document.getElementById('powerbank').checked=d.powerbank_ping;"
"document.getElementById('led_actions').checked=d.led_actions;"
"document.getElementById('verbose').checked=d.verbose;"
"}).catch(e=>showStatus('Failed to load settings','error'));"
"}"
"function saveSettings(){"
"const settings={"
"autocatch:document.getElementById('autocatch').checked,"
"autospin:document.getElementById('autospin').checked,"
"powerbank_ping:document.getElementById('powerbank').checked,"
"led_actions:document.getElementById('led_actions').checked,"
"verbose:document.getElementById('verbose').checked"
"};"
"fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(settings)})"
".then(r=>r.json()).then(d=>{"
"if(d.status==='ok'){showStatus('Settings saved! WiFi closing...','success');setTimeout(()=>{window.location.reload();},2000);}"
"else{showStatus('Failed to save settings','error');}"
"}).catch(e=>showStatus('Error saving settings','error'));"
"}"
"function showStatus(msg,type){"
"const s=document.getElementById('status');"
"s.textContent=msg;s.className='status '+type;s.classList.remove('hidden');"
"setTimeout(()=>s.classList.add('hidden'),5000);"
"}"
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
    
    // Get current settings from config_storage
    cJSON_AddBoolToObject(root, "autocatch", settings_get_autocatch());
    cJSON_AddBoolToObject(root, "autospin", settings_get_autospin());
    cJSON_AddBoolToObject(root, "powerbank_ping", settings_get_powerbank_ping());
    cJSON_AddBoolToObject(root, "led_actions", settings_get_led_actions());
    cJSON_AddBoolToObject(root, "verbose", settings_get_verbose());
    
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
    
    // Update settings
    cJSON *autocatch = cJSON_GetObjectItem(root, "autocatch");
    if (autocatch && cJSON_IsBool(autocatch)) {
        settings_set_autocatch(cJSON_IsTrue(autocatch));
    }
    
    cJSON *autospin = cJSON_GetObjectItem(root, "autospin");
    if (autospin && cJSON_IsBool(autospin)) {
        settings_set_autospin(cJSON_IsTrue(autospin));
    }
    
    cJSON *powerbank = cJSON_GetObjectItem(root, "powerbank_ping");
    if (powerbank && cJSON_IsBool(powerbank)) {
        settings_set_powerbank_ping(cJSON_IsTrue(powerbank));
    }
    
    cJSON *led_actions = cJSON_GetObjectItem(root, "led_actions");
    if (led_actions && cJSON_IsBool(led_actions)) {
        settings_set_led_actions(cJSON_IsTrue(led_actions));
    }
    
    cJSON *verbose = cJSON_GetObjectItem(root, "verbose");
    if (verbose && cJSON_IsBool(verbose)) {
        settings_set_verbose(cJSON_IsTrue(verbose));
    }
    
    // Save to NVS
    esp_err_t err = settings_save();
    
    cJSON_Delete(root);
    
    // Prepare response
    cJSON *response = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddStringToObject(response, "status", "ok");
        ESP_LOGI(TAG, "Settings saved successfully");
        
        // Stop WiFi AP after successful save
        wifi_ap_manager_stop();
    } else {
        cJSON_AddStringToObject(response, "status", "error");
        ESP_LOGE(TAG, "Failed to save settings");
    }
    
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

/* HTTP GET handler for status API */
static esp_err_t api_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(root, "remaining_ms", wifi_ap_manager_get_remaining_time());
    cJSON_AddBoolToObject(root, "running", wifi_ap_manager_is_running());
    
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* URI handler structure for GET / */
static const httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
};

/* URI handler for GET /api/settings */
static const httpd_uri_t api_settings_get_uri = {
    .uri       = "/api/settings",
    .method    = HTTP_GET,
    .handler   = api_settings_get_handler,
    .user_ctx  = NULL
};

/* URI handler for POST /api/settings */
static const httpd_uri_t api_settings_post_uri = {
    .uri       = "/api/settings",
    .method    = HTTP_POST,
    .handler   = api_settings_post_handler,
    .user_ctx  = NULL
};

/* URI handler for GET /api/status */
static const httpd_uri_t api_status_uri = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = api_status_handler,
    .user_ctx  = NULL
};

/**
 * @brief Start web server
 */
esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register URI handlers
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &api_settings_get_uri);
    httpd_register_uri_handler(server, &api_settings_post_uri);
    httpd_register_uri_handler(server, &api_status_uri);
    
    ESP_LOGI(TAG, "Web server started successfully");
    return ESP_OK;
}

/**
 * @brief Stop web server
 */
esp_err_t web_server_stop(void)
{
    if (server == NULL) {
        ESP_LOGW(TAG, "Web server not running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping web server");
    esp_err_t ret = httpd_stop(server);
    server = NULL;
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Web server stopped");
    } else {
        ESP_LOGE(TAG, "Failed to stop server: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Check if web server is running
 */
bool web_server_is_running(void)
{
    return server != NULL;
}

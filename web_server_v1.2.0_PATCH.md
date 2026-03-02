# 🔧 **web_server.c v1.2.0 PATCH - Device Config Tab**

## 📝 **NEUE FEATURES v1.2.0:**

**5. Tab "Device Config" mit:**
- PGP_CLONE_NAME (Device Name)
- PGP_MAC (MAC Address)
- PGP_BLOB (Blob Data)
- PGP_DEVICE_KEY (Device Key)

---

## ✏️ **ÄNDERUNG 1: HTML - 5. Tab hinzufügen**

**Location:** In der `index_html` String, bei den Tabs

**VORHER (v1.1.0):**
```html
<div class='tabs'>
<button class='tab active' onclick='switchTab(0)'>⚙️ Settings</button>
<button class='tab' onclick='switchTab(1)'>📊 Statistics</button>
<button class='tab' onclick='switchTab(2)'>📱 Devices</button>
<button class='tab' onclick='switchTab(3)'>🔐 Secrets</button>
</div>
```

**NACHHER (v1.2.0):**
```html
<div class='tabs'>
<button class='tab active' onclick='switchTab(0)'>⚙️ Settings</button>
<button class='tab' onclick='switchTab(1)'>📊 Statistics</button>
<button class='tab' onclick='switchTab(2)'>📱 Devices</button>
<button class='tab' onclick='switchTab(3)'>🔐 Secrets</button>
<button class='tab' onclick='switchTab(4)'>🎮 Device Config</button>
</div>
```

---

## ✏️ **ÄNDERUNG 2: CSS - Responsive Tabs**

**Location:** Im `<style>` Block

**HINZUFÜGEN:**
```css
/* Better tab wrapping for 5 tabs */
@media (max-width: 768px) {
  .tabs{flex-wrap:wrap}
  .tab{flex:1 1 45%;margin-bottom:10px}
}
```

---

## ✏️ **ÄNDERUNG 3: HTML - Device Config Tab Content**

**Location:** Nach Tab 3 (Secrets), vor `</div>` vom container

**HINZUFÜGEN:**
```html
<div class='tab-content' id='tab4'>
<div class='card'>
<h2>🎮 PGP Device Configuration</h2>
<div class='help' style='background:#fff3cd;color:#856404;padding:12px;border-radius:6px;margin-bottom:20px'>
⚠️ <strong>Advanced Settings</strong> - Only change if you know what you're doing!<br>
Changes take effect after device restart.
</div>

<div class='setting'>
<label>Device Name (PGP_CLONE_NAME)</label>
<div class='help'>Pokemon Go Plus device name (default: Pokemon GO Plus)</div>
<input type='text' id='deviceName' maxlength='63' 
       style='width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-family:monospace' 
       onchange='deviceConfigChanged()'>
</div>

<div class='setting'>
<label>MAC Address (PGP_MAC)</label>
<div class='help'>Bluetooth MAC address (format: XX:XX:XX:XX:XX:XX)</div>
<input type='text' id='deviceMac' maxlength='17' placeholder='AA:BB:CC:DD:EE:FF'
       style='width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-family:monospace'
       pattern='[0-9A-Fa-f:]{17}' onchange='deviceConfigChanged()'>
</div>

<div class='setting'>
<label>Blob Data (PGP_BLOB)</label>
<div class='help'>Device blob data (hex string, 256 chars max)</div>
<textarea id='deviceBlob' maxlength='256' rows='4'
          style='width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-family:monospace;resize:vertical'
          onchange='deviceConfigChanged()'></textarea>
</div>

<div class='setting'>
<label>Device Key (PGP_DEVICE_KEY)</label>
<div class='help'>Device encryption key (hex string, 32 chars)</div>
<input type='text' id='deviceKey' maxlength='32' placeholder='0123456789ABCDEF...'
       style='width:100%;padding:10px;border:2px solid #ddd;border-radius:6px;font-family:monospace'
       pattern='[0-9A-Fa-f]{0,32}' onchange='deviceConfigChanged()'>
</div>

<div style='display:flex;gap:10px'>
<button onclick='saveDeviceConfig()' style='flex:1'>💾 Save Device Config</button>
<button onclick='resetDeviceConfig()' style='flex:1;background:linear-gradient(135deg,#f5576c,#f093fb)'>
🔄 Reset to Defaults
</button>
</div>
</div>
</div>
```

---

## ✏️ **ÄNDERUNG 4: JavaScript - Device Config Functions**

**Location:** Im `<script>` Block, nach den Secrets Funktionen

**HINZUFÜGEN:**
```javascript
let hasDeviceConfigChanges=false;

function deviceConfigChanged(){hasDeviceConfigChanges=true;}

function loadDeviceConfig(){
fetch('/api/device_config').then(r=>r.json()).then(d=>{
document.getElementById('deviceName').value=d.name||'';
document.getElementById('deviceMac').value=d.mac||'';
document.getElementById('deviceBlob').value=d.blob||'';
document.getElementById('deviceKey').value=d.dkey||'';
hasDeviceConfigChanges=false;
}).catch(e=>showStatus('Failed to load device config','error'));
}

function saveDeviceConfig(){
if(!hasDeviceConfigChanges){showStatus('No changes to save','error');return;}

// Validate MAC address format
const mac=document.getElementById('deviceMac').value;
if(mac&&!/^[0-9A-Fa-f:]{17}$/.test(mac)){
showStatus('Invalid MAC address format (use XX:XX:XX:XX:XX:XX)','error');
return;
}

// Validate hex strings
const blob=document.getElementById('deviceBlob').value;
if(blob&&!/^[0-9A-Fa-f]*$/.test(blob)){
showStatus('Blob must be hex string (0-9, A-F)','error');
return;
}

const dkey=document.getElementById('deviceKey').value;
if(dkey&&!/^[0-9A-Fa-f]*$/.test(dkey)){
showStatus('Device key must be hex string (0-9, A-F)','error');
return;
}

const data={
name:document.getElementById('deviceName').value,
mac:document.getElementById('deviceMac').value,
blob:document.getElementById('deviceBlob').value,
dkey:document.getElementById('deviceKey').value
};

fetch('/api/device_config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
.then(r=>r.json()).then(d=>{
if(d.status==='ok'){
showStatus('✓ Device config saved! Restart device to apply','success');
hasDeviceConfigChanges=false;
}else showStatus('Failed to save: '+(d.message||'Unknown error'),'error');
}).catch(e=>showStatus('Error: '+e.message,'error'));
}

function resetDeviceConfig(){
if(!confirm('Reset device config to defaults? This cannot be undone!')){
return;
}

fetch('/api/device_config/reset',{method:'POST'})
.then(r=>r.json()).then(d=>{
if(d.status==='ok'){
showStatus('✓ Device config reset to defaults','success');
loadDeviceConfig();
}else showStatus('Failed to reset','error');
}).catch(e=>showStatus('Error: '+e.message,'error'));
}
```

**ÄNDERN - switchTab Funktion:**
```javascript
// VORHER:
function switchTab(tab){
currentTab=tab;
// ... rest of code
if(tab===1)loadStats();
if(tab===2)loadDevices();
if(tab===3)loadSecrets();
}

// NACHHER:
function switchTab(tab){
currentTab=tab;
// ... rest of code
if(tab===1)loadStats();
if(tab===2)loadDevices();
if(tab===3)loadSecrets();
if(tab===4)loadDeviceConfig();  // NEU!
}
```

**ÄNDERN - Initialization (am Ende des Scripts):**
```javascript
// HINZUFÜGEN:
loadDeviceConfig();
```

---

## ✏️ **ÄNDERUNG 5: C Code - Device Config API Handlers**

**Location:** Nach den Secrets API Handlers

**HINZUFÜGEN:**
```c
/* HTTP GET handler for device config API */
static esp_err_t api_device_config_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // TODO: Get from your settings system
    // Example placeholders - replace with actual getters
    const char *name = "Pokemon GO Plus";  // PGP_CLONE_NAME
    const char *mac = "00:00:00:00:00:00"; // PGP_MAC
    const char *blob = "";                  // PGP_BLOB
    const char *dkey = "";                  // PGP_DEVICE_KEY
    
    // Add to JSON
    cJSON_AddStringToObject(root, "name", name);
    cJSON_AddStringToObject(root, "mac", mac);
    cJSON_AddStringToObject(root, "blob", blob);
    cJSON_AddStringToObject(root, "dkey", dkey);
    
    const char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Device config requested");
    return ESP_OK;
}

/* HTTP POST handler for device config API */
static esp_err_t api_device_config_post_handler(httpd_req_t *req)
{
    char buf[1024];
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
    
    ESP_LOGI(TAG, "Received device config update");
    
    // Extract values
    cJSON *name_obj = cJSON_GetObjectItem(root, "name");
    if (name_obj && cJSON_IsString(name_obj)) {
        const char *name = cJSON_GetStringValue(name_obj);
        ESP_LOGI(TAG, "Device name: %s", name);
        // TODO: Save to your settings system
        // Example: set_pgp_clone_name(name);
    }
    
    cJSON *mac_obj = cJSON_GetObjectItem(root, "mac");
    if (mac_obj && cJSON_IsString(mac_obj)) {
        const char *mac = cJSON_GetStringValue(mac_obj);
        ESP_LOGI(TAG, "Device MAC: %s", mac);
        // TODO: Save to your settings system
        // Example: set_pgp_mac(mac);
    }
    
    cJSON *blob_obj = cJSON_GetObjectItem(root, "blob");
    if (blob_obj && cJSON_IsString(blob_obj)) {
        const char *blob = cJSON_GetStringValue(blob_obj);
        ESP_LOGI(TAG, "Device blob (length: %d)", strlen(blob));
        // TODO: Save to your settings system
        // Example: set_pgp_blob(blob);
    }
    
    cJSON *dkey_obj = cJSON_GetObjectItem(root, "dkey");
    if (dkey_obj && cJSON_IsString(dkey_obj)) {
        const char *dkey = cJSON_GetStringValue(dkey_obj);
        ESP_LOGI(TAG, "Device key (length: %d)", strlen(dkey));
        // TODO: Save to your settings system
        // Example: set_pgp_device_key(dkey);
    }
    
    cJSON_Delete(root);
    
    // TODO: Save to NVS
    // Example: write_device_config_to_nvs();
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "Device config saved. Restart to apply.");
    
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(response);
    
    ESP_LOGI(TAG, "Device config saved successfully");
    return ESP_OK;
}

/* HTTP POST handler for device config reset */
static esp_err_t api_device_config_reset_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Resetting device config to defaults");
    
    // TODO: Reset to default values
    // Example:
    // set_pgp_clone_name("Pokemon GO Plus");
    // set_pgp_mac("00:00:00:00:00:00");
    // set_pgp_blob("");
    // set_pgp_device_key("");
    // write_device_config_to_nvs();
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "Device config reset to defaults");
    
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(response);
    
    return ESP_OK;
}
```

---

## ✏️ **ÄNDERUNG 6: C Code - URI Registration**

**Location:** In `web_server_start()`, vor dem letzten catch-all Handler

**HINZUFÜGEN:**
```c
        httpd_uri_t api_device_config_get_uri = {
            .uri = "/api/device_config",
            .method = HTTP_GET,
            .handler = api_device_config_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_device_config_get_uri);
        
        httpd_uri_t api_device_config_post_uri = {
            .uri = "/api/device_config",
            .method = HTTP_POST,
            .handler = api_device_config_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_device_config_post_uri);
        
        httpd_uri_t api_device_config_reset_uri = {
            .uri = "/api/device_config/reset",
            .method = HTTP_POST,
            .handler = api_device_config_reset_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_device_config_reset_uri);
```

**ÄNDERN - max_open_sockets:**
```c
// VORHER (v1.1.0):
config.max_open_sockets = 17;

// NACHHER (v1.2.0):
config.max_open_sockets = 20;  // Increased for 3 more handlers
```

---

## ✏️ **ÄNDERUNG 7: C Code - Backend Integration**

**Location:** Erstelle neue Datei `device_config.h` oder füge zu `settings.h` hinzu

**NEU ERSTELLEN:**
```c
// device_config.h
#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "esp_err.h"
#include <stdint.h>

// Device Configuration
typedef struct {
    char name[64];      // PGP_CLONE_NAME
    char mac[18];       // PGP_MAC (XX:XX:XX:XX:XX:XX + null)
    char blob[257];     // PGP_BLOB (hex string + null)
    char dkey[33];      // PGP_DEVICE_KEY (hex string + null)
} device_config_t;

// Get current device config
esp_err_t get_device_config(device_config_t *config);

// Set device config (saves to NVS)
esp_err_t set_device_config(const device_config_t *config);

// Set individual fields
esp_err_t set_pgp_clone_name(const char *name);
esp_err_t set_pgp_mac(const char *mac);
esp_err_t set_pgp_blob(const char *blob);
esp_err_t set_pgp_device_key(const char *dkey);

// Reset to defaults
esp_err_t reset_device_config(void);

#endif // DEVICE_CONFIG_H
```

---

## 📋 **INTEGRATION CHECKLIST:**

### **Web Server:**
- [x] 5. Tab HTML hinzugefügt
- [x] Device Config Content hinzugefügt
- [x] JavaScript Functions hinzugefügt
- [x] API Handlers hinzugefügt
- [x] URI Registration hinzugefügt

### **Backend (TODO):**
- [ ] `device_config.h` erstellen
- [ ] `device_config.c` implementieren
- [ ] NVS Storage implementieren
- [ ] Getter/Setter Functions implementieren
- [ ] In `settings.c` integrieren
- [ ] Default Values definieren

---

## 🔧 **BACKEND IMPLEMENTATION EXAMPLE:**

```c
// device_config.c
#include "device_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "device_config";

// Defaults
#define DEFAULT_PGP_CLONE_NAME "Pokemon GO Plus"
#define DEFAULT_PGP_MAC        "00:00:00:00:00:00"
#define DEFAULT_PGP_BLOB       ""
#define DEFAULT_PGP_DEVICE_KEY ""

static device_config_t current_config = {
    .name = DEFAULT_PGP_CLONE_NAME,
    .mac = DEFAULT_PGP_MAC,
    .blob = DEFAULT_PGP_BLOB,
    .dkey = DEFAULT_PGP_DEVICE_KEY
};

esp_err_t get_device_config(device_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    memcpy(config, &current_config, sizeof(device_config_t));
    return ESP_OK;
}

esp_err_t set_device_config(const device_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    
    memcpy(&current_config, config, sizeof(device_config_t));
    
    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("device_cfg", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;
    
    nvs_set_str(nvs_handle, "name", current_config.name);
    nvs_set_str(nvs_handle, "mac", current_config.mac);
    nvs_set_str(nvs_handle, "blob", current_config.blob);
    nvs_set_str(nvs_handle, "dkey", current_config.dkey);
    
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Device config saved");
    return ret;
}

// ... weitere Funktionen
```

---

## 📊 **SUMMARY:**

**v1.2.0 Änderungen:**
- ✅ 5. Tab "Device Config"
- ✅ 4 neue Felder (Name/MAC/Blob/Key)
- ✅ Input Validation (MAC, Hex)
- ✅ Reset to Defaults Button
- ✅ 3 neue API Endpoints
- ✅ TODO Comments für Backend Integration

**Zu implementieren:**
- [ ] Backend Getter/Setter
- [ ] NVS Storage
- [ ] Integration in bestehendes Settings System

---

**Version:** v1.2.0  
**New Tab:** Device Config (5th tab)  
**API Endpoints:** +3 (total: 15)  
**Status:** Frontend Complete, Backend TODO

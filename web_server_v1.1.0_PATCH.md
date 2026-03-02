# 🔧 **web_server.c v1.1.0 PATCH**

## 📝 **ÄNDERUNGEN ZU v1.0.0:**

Diese Datei zeigt **ALLE Änderungen** die in web_server.c gemacht werden müssen um v1.1.0 Features zu aktivieren.

---

## ✏️ **ÄNDERUNG 1: HTML - 4. Tab hinzufügen**

**Location:** In der `index_html` String, bei den Tabs

**VORHER (v1.0.0):**
```html
<div class='tabs'>
<button class='tab active' onclick='switchTab(0)'>⚙️ Settings</button>
<button class='tab' onclick='switchTab(1)'>📊 Statistics</button>
<button class='tab' onclick='switchTab(2)'>📱 Devices</button>
</div>
```

**NACHHER (v1.1.0):**
```html
<div class='tabs'>
<button class='tab active' onclick='switchTab(0)'>⚙️ Settings</button>
<button class='tab' onclick='switchTab(1)'>📊 Statistics</button>
<button class='tab' onclick='switchTab(2)'>📱 Devices</button>
<button class='tab' onclick='switchTab(3)'>🔐 Secrets</button>
</div>
```

---

## ✏️ **ÄNDERUNG 2: HTML - Secrets Tab Content**

**Location:** Nach Tab 2 (Devices), vor `</div>` vom container

**HINZUFÜGEN:**
```html
<div class='tab-content' id='tab3'>
<div class='card'>
<h2>🔐 WiFi AP Secrets</h2>

<div class='setting'>
<label>WiFi SSID</label>
<div class='help'>Network name (default: PGPemu-Setup)</div>
<input type='text' id='wifiSsid' maxlength='31' 
       style='width:100%;padding:10px;border:2px solid #ddd;border-radius:6px' 
       onchange='secretsChanged()'>
</div>

<div class='setting'>
<label>WiFi Password (WPA2)</label>
<div class='help'>Leave empty for Open network (default: PogoPogo)</div>
<input type='password' id='wifiPassword' maxlength='63' 
       style='width:100%;padding:10px;border:2px solid #ddd;border-radius:6px'
       onchange='secretsChanged()'>
</div>

<div class='setting'>
<label>TX Power: <span class='range-value' id='txPowerValue'>8.5 dBm</span></label>
<div class='help'>WiFi transmission power: 2.0 - 21.0 dBm (default: 8.5 dBm)</div>
<input type='range' id='txPower' min='8' max='84' value='34' 
       oninput='updateTxPower()' onchange='secretsChanged()'>
<div class='prob-labels'><span>2.0 dBm</span><span>8.5 dBm</span><span>21.0 dBm</span></div>
</div>

<button onclick='saveSecrets()'>💾 Save Secrets & Restart WiFi</button>
</div>
</div>
```

---

## ✏️ **ÄNDERUNG 3: JavaScript - Secrets Functions**

**Location:** Im `<script>` Block, nach den bestehenden Funktionen

**HINZUFÜGEN:**
```javascript
let hasSecretsChanges=false;

function secretsChanged(){hasSecretsChanges=true;}

function updateTxPower(){
const val=document.getElementById('txPower').value;
const dbm=(val*0.25).toFixed(1);
document.getElementById('txPowerValue').textContent=dbm+' dBm';
}

function loadSecrets(){
fetch('/api/secrets').then(r=>r.json()).then(d=>{
document.getElementById('wifiSsid').value=d.ssid||'';
document.getElementById('wifiPassword').value=d.password||'';
document.getElementById('txPower').value=d.tx_power||34;
updateTxPower();
hasSecretsChanges=false;
}).catch(e=>showStatus('Failed to load secrets','error'));
}

function saveSecrets(){
if(!hasSecretsChanges){showStatus('No changes to save','error');return;}
const data={
ssid:document.getElementById('wifiSsid').value,
password:document.getElementById('wifiPassword').value,
tx_power:parseInt(document.getElementById('txPower').value)
};
fetch('/api/secrets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
.then(r=>r.json()).then(d=>{
if(d.status==='ok'){
showStatus('✓ Secrets saved! Restart WiFi to apply','success');
hasSecretsChanges=false;
}else showStatus('Failed to save','error');
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
}

// NACHHER:
function switchTab(tab){
currentTab=tab;
// ... rest of code
if(tab===1)loadStats();
if(tab===2)loadDevices();
if(tab===3)loadSecrets();  // NEU!
}
```

**ÄNDERN - Initialization:**
```javascript
// Am Ende des Scripts, HINZUFÜGEN:
loadSecrets();
```

---

## ✏️ **ÄNDERUNG 4: C Code - Secrets API Handlers**

**Location:** Nach den bestehenden API Handlers (api_timer_resume_handler)

**HINZUFÜGEN:**
```c
/* HTTP GET handler for secrets API */
static esp_err_t api_secrets_get_handler(httpd_req_t *req)
{
    char ssid[32] = {0};
    char password[64] = {0};
    int8_t tx_power = 0;
    
    wifi_ap_manager_get_config(ssid, sizeof(ssid), 
                               password, sizeof(password), 
                               &tx_power);
    
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

/* HTTP POST handler for secrets API */
static esp_err_t api_secrets_post_handler(httpd_req_t *req)
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
    
    ESP_LOGI(TAG, "Received secrets update");
    
    const char *ssid = NULL;
    const char *password = NULL;
    int8_t tx_power = -1;
    
    cJSON *ssid_obj = cJSON_GetObjectItem(root, "ssid");
    if (ssid_obj && cJSON_IsString(ssid_obj)) {
        ssid = cJSON_GetStringValue(ssid_obj);
        ESP_LOGI(TAG, "New SSID: %s", ssid);
    }
    
    cJSON *pass_obj = cJSON_GetObjectItem(root, "password");
    if (pass_obj && cJSON_IsString(pass_obj)) {
        password = cJSON_GetStringValue(pass_obj);
        if (strlen(password) > 0) {
            ESP_LOGI(TAG, "New password set (length: %d)", strlen(password));
        } else {
            ESP_LOGI(TAG, "Password cleared (Open network)");
        }
    }
    
    cJSON *tx_obj = cJSON_GetObjectItem(root, "tx_power");
    if (tx_obj && cJSON_IsNumber(tx_obj)) {
        tx_power = (int8_t)cJSON_GetNumberValue(tx_obj);
        ESP_LOGI(TAG, "New TX power: %d (%.1f dBm)", tx_power, tx_power * 0.25f);
    }
    
    // Save to NVS
    wifi_ap_manager_set_config(ssid, password, tx_power);
    
    cJSON_Delete(root);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "message", "Secrets saved. Restart WiFi AP to apply.");
    
    const char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free((void *)json_str);
    cJSON_Delete(response);
    
    ESP_LOGI(TAG, "Secrets saved successfully");
    
    return ESP_OK;
}
```

---

## ✏️ **ÄNDERUNG 5: C Code - URI Registration**

**Location:** In `web_server_start()`, vor dem letzten catch-all Handler

**HINZUFÜGEN:**
```c
        httpd_uri_t api_secrets_get_uri = {
            .uri = "/api/secrets",
            .method = HTTP_GET,
            .handler = api_secrets_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_secrets_get_uri);
        
        httpd_uri_t api_secrets_post_uri = {
            .uri = "/api/secrets",
            .method = HTTP_POST,
            .handler = api_secrets_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_secrets_post_uri);
```

**ÄNDERN - max_open_sockets:**
```c
// VORHER:
config.max_open_sockets = 15;

// NACHHER:
config.max_open_sockets = 17;  // Increased for 2 more handlers
```

---

## ✏️ **ÄNDERUNG 6: HTML - Timer Default anzeigen**

**Location:** Im HTML, timer div

**VORHER:**
```html
<div class='timer' id='timer'>3:00</div>
```

**NACHHER:**
```html
<div class='timer' id='timer'>5:00</div>
```

---

## ✏️ **ÄNDERUNG 7: HTML - Info Box updaten**

**VORHER:**
```html
• WiFi will auto-close after <strong>3 minutes</strong><br>
```

**NACHHER:**
```html
• WiFi will auto-close after <strong>5 minutes</strong><br>
```

**HINZUFÜGEN:**
```html
• Default password: <strong>PogoPogo</strong><br>
• Blue LED shows WiFi AP status<br>
```

---

## 📊 **ZUSAMMENFASSUNG DER ÄNDERUNGEN:**

| Was | Wo | Aktion |
|-----|-----|--------|
| 4. Tab Button | HTML tabs div | Hinzufügen |
| Tab 4 Content | HTML nach Tab 2 | Hinzufügen |
| Secrets JS Functions | JavaScript | Hinzufügen |
| switchTab() | JavaScript | Ändern |
| Initialization | JavaScript | Ändern |
| api_secrets_get | C Code | Hinzufügen |
| api_secrets_post | C Code | Hinzufügen |
| URI Registration | web_server_start() | Hinzufügen |
| max_open_sockets | web_server_start() | 15 → 17 |
| Timer default | HTML | 3:00 → 5:00 |
| Info box | HTML | Update |

---

## 🚀 **INSTALLATION:**

### **Option 1: Manuell patchen**
1. Öffne `web_server.c` (v1.0.0)
2. Mache ALLE 7 Änderungen oben
3. Speichern als `web_server.c`

### **Option 2: Komplett ersetzen**
1. Warte auf vollständige `web_server_v1.1.0.c` Datei
2. Kopiere über bestehende Datei

---

## ✅ **VERIFICATION:**

Nach den Änderungen sollte das Interface haben:
- [x] 4 Tabs (Settings/Statistics/Devices/Secrets)
- [x] Secrets Tab mit SSID/Password/TX Power
- [x] Timer zeigt 5:00 am Anfang
- [x] Info box erwähnt Password und LED

---

**Patch Version:** v1.1.0  
**Base Version:** v1.0.0  
**Datei:** web_server.c

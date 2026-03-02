# 🔒 **WHITELIST IMPLEMENTATION - Bluetooth & WiFi**

## Frage: Ist es möglich nur bestimmte Geräte zuzulassen?

**Antwort: JA, beides ist möglich!** ✅

- ✅ **Bluetooth Whitelist** (einfach)
- ✅ **WiFi Whitelist** (mittel)

Beide Whitelists basieren auf **MAC-Adressen** (Media Access Control) - der eindeutigen Hardware-ID jedes Netzwerkgeräts.

---

## 📱 **1. BLUETOOTH WHITELIST**

### **Wie funktioniert's:**

Bluetooth-Verbindungen werden im `pgp_gap.c` (GAP = Generic Access Profile) gehandhabt. Dort gibt es ein **Connect-Event** das gefeuert wird wenn ein Gerät verbinden will.

### **Implementation:**

#### **Schritt 1: MAC-Adressen Liste erstellen**

```c
// In pgp_gap.c oder neues whitelist.c
#define MAX_ALLOWED_DEVICES 10

static esp_bd_addr_t bt_whitelist[MAX_ALLOWED_DEVICES] = {
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},  // Device 1
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},  // Device 2
    // ... weitere Geräte
};

static int whitelist_count = 2;  // Anzahl der erlaubten Geräte
```

#### **Schritt 2: Check-Funktion**

```c
/**
 * @brief Check if Bluetooth device is in whitelist
 */
static bool is_bt_device_allowed(esp_bd_addr_t remote_bda)
{
    // Wenn Whitelist leer (count = 0), erlaube alle
    if (whitelist_count == 0) {
        return true;  // Open mode
    }
    
    // Check ob MAC in Whitelist
    for (int i = 0; i < whitelist_count; i++) {
        if (memcmp(remote_bda, bt_whitelist[i], ESP_BD_ADDR_LEN) == 0) {
            ESP_LOGI(TAG, "Device "MACSTR" is whitelisted",
                     MAC2STR(remote_bda));
            return true;
        }
    }
    
    ESP_LOGW(TAG, "Device "MACSTR" NOT in whitelist - REJECTED",
             MAC2STR(remote_bda));
    return false;
}
```

#### **Schritt 3: Integration in Connect-Event**

```c
// In pgp_gap.c im GAP Event Handler
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    // ...
    case ESP_GAP_BLE_SEC_REQ_EVT: {
        // HIER: Check Whitelist BEVOR wir Pairing zulassen
        esp_bd_addr_t remote_bda;
        memcpy(remote_bda, param->ble_security.ble_req.bd_addr, ESP_BD_ADDR_LEN);
        
        if (!is_bt_device_allowed(remote_bda)) {
            ESP_LOGW(TAG, "Rejecting connection - device not whitelisted");
            // Verbindung ablehnen
            return;
        }
        
        // Weiter mit normalem Pairing...
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    }
    // ...
    }
}
```

### **MAC-Adresse eines Geräts herausfinden:**

**Methode 1: Aus Logs lesen**
```
I (xxx) pgp_gap: ESP_GAP_BLE_SEC_REQ_EVT
I (xxx) pgp_gap: Remote BD_ADDR: aa:bb:cc:dd:ee:ff
```

**Methode 2: Temporär alle erlauben und loggen**
```c
// In is_bt_device_allowed() am Anfang:
ESP_LOGI(TAG, "Connection attempt from: "MACSTR, MAC2STR(remote_bda));
// → Dann MAC aus Log kopieren
```

### **Vorteile Bluetooth Whitelist:**
- ✅ Sehr sicher
- ✅ Nur autorisierte Smartphones verbinden
- ✅ Kein anderes Pokemon Go Gerät kann stören
- ✅ Einfach zu implementieren

### **Nachteile:**
- ❌ Jedes neue Gerät muss manuell hinzugefügt werden
- ❌ Firmware-Update nötig für neue Geräte

---

## 📡 **2. WIFI WHITELIST**

### **Wie funktioniert's:**

WiFi-Verbindungen werden über ESP-IDF WiFi Events gehandhabt. Wenn ein Gerät sich mit dem Access Point verbindet, wird ein **WIFI_EVENT_AP_STACONNECTED** Event gefeuert.

### **Implementation:**

#### **Schritt 1: MAC-Adressen Liste**

```c
// In wifi_ap_manager.c
#define MAX_ALLOWED_WIFI_DEVICES 5

static uint8_t wifi_whitelist[MAX_ALLOWED_WIFI_DEVICES][6] = {
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},  // Smartphone 1
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},  // Smartphone 2
    // ... weitere Geräte
};

static int wifi_whitelist_count = 2;
static bool wifi_whitelist_enabled = true;
```

#### **Schritt 2: Check-Funktion**

```c
/**
 * @brief Check if WiFi device is allowed
 */
static bool is_wifi_device_allowed(uint8_t mac[6])
{
    // Wenn deaktiviert, erlaube alle
    if (!wifi_whitelist_enabled || wifi_whitelist_count == 0) {
        return true;
    }
    
    // Check Whitelist
    for (int i = 0; i < wifi_whitelist_count; i++) {
        if (memcmp(mac, wifi_whitelist[i], 6) == 0) {
            ESP_LOGI(TAG, "WiFi device "MACSTR" is whitelisted",
                     MAC2STR(mac));
            return true;
        }
    }
    
    ESP_LOGW(TAG, "WiFi device "MACSTR" NOT in whitelist",
             MAC2STR(mac));
    return false;
}
```

#### **Schritt 3: Integration in WiFi Event Handler**

```c
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        
        ESP_LOGI(TAG, "Station "MACSTR" connected, AID=%d",
                 MAC2STR(event->mac), event->aid);
        
        // Check Whitelist
        if (!is_wifi_device_allowed(event->mac)) {
            ESP_LOGW(TAG, "Device not whitelisted - disconnecting");
            
            // Gerät deauthentifizieren (rauswerfen)
            esp_wifi_deauth_sta(event->aid);
            return;
        }
        
        ESP_LOGI(TAG, "Device whitelisted - connection allowed");
    }
    // ...
}
```

### **MAC-Adresse eines WiFi-Geräts herausfinden:**

**Methode 1: Aus Smartphone-Einstellungen**
```
Android:  Einstellungen → Über das Telefon → Status → WLAN-MAC-Adresse
iPhone:   Einstellungen → Allgemein → Info → WLAN-Adresse
```

**Methode 2: Aus ESP32 Logs**
```
I (xxx) wifi_ap_mgr: Station aa:bb:cc:dd:ee:ff connected, AID=1
```

**Methode 3: Temporär alle erlauben und loggen**
```c
ESP_LOGI(TAG, "Connection from: "MACSTR, MAC2STR(event->mac));
```

### **Vorteile WiFi Whitelist:**
- ✅ Nur autorisierte Geräte können Config-Seite sehen
- ✅ Verhindert unbefugten Zugriff auf Settings
- ✅ Sicherheit bei öffentlicher Nutzung

### **Nachteile:**
- ❌ Komplizierter als Bluetooth (wegen MAC Randomization)
- ❌ Moderne Smartphones randomisieren WiFi MAC (siehe unten!)
- ❌ Firmware-Update für neue Geräte nötig

---

## ⚠️ **WICHTIG: MAC ADDRESS RANDOMIZATION**

### **Das Problem:**

Moderne Smartphones (iOS 14+, Android 10+) nutzen **MAC Randomization** aus Datenschutzgründen:

- Jedes Mal wenn sie sich mit einem **neuen** WiFi verbinden → **neue zufällige MAC**
- Zweck: Tracking verhindern

### **Auswirkung auf WiFi Whitelist:**

❌ **WiFi Whitelist funktioniert NICHT zuverlässig** wenn:
- iPhone mit iOS 14+ (Private WiFi Address an)
- Android 10+ (MAC Randomization an)

Die MAC ändert sich jedes Mal → Gerät wird nicht erkannt!

### **Lösungen:**

**Option 1: MAC Randomization auf Smartphone deaktivieren**

```
iPhone:
  Einstellungen → WLAN → (i) neben "PGPemu-Setup"
  → "Private WLAN-Adresse" AUS

Android:
  Einstellungen → WLAN → (Zahnrad) neben "PGPemu-Setup"
  → Erweitert → MAC-Typ → Geräte-MAC verwenden
```

**Option 2: Alternative Authentifizierung nutzen**

Statt MAC Whitelist:
- **Passwort-Schutz** für WiFi AP (WPA2)
- **Login auf Webseite** (HTTP Basic Auth)
- **Token/PIN** in URL (z.B. http://192.168.4.1?token=1234)

---

## 🎯 **EMPFEHLUNG:**

### **Für Bluetooth:**
✅ **Whitelist SEHR EMPFOHLEN**
- Funktioniert zuverlässig
- Keine MAC Randomization bei Bluetooth Low Energy
- Verhindert Störungen durch andere Geräte

### **Für WiFi:**
⚠️ **Whitelist NUR wenn nötig**
- Funktioniert nicht gut mit modernen Smartphones
- Besser: **WPA2 Passwort** verwenden statt Open Network
- Oder: HTTP Basic Auth auf Webseite

---

## 📝 **IMPLEMENTATION BEISPIEL:**

### **Bluetooth Whitelist (Einfach)**

```c
// pgp_gap.c - Nur 2 Geräte erlauben

static esp_bd_addr_t allowed_devices[] = {
    {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},  // Mein Pixel 7
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},  // Mein iPhone 14
};

static bool is_device_allowed(esp_bd_addr_t bda) {
    for (int i = 0; i < 2; i++) {
        if (memcmp(bda, allowed_devices[i], 6) == 0) {
            return true;
        }
    }
    return false;
}

// Im ESP_GAP_BLE_SEC_REQ_EVT:
if (!is_device_allowed(param->ble_security.ble_req.bd_addr)) {
    ESP_LOGW(TAG, "Device not allowed - ignoring");
    return;  // Verbindung wird nicht akzeptiert
}
```

### **WiFi mit Passwort (Besser als Whitelist)**

```c
// wifi_ap_manager.c

#define WIFI_AP_PASS "MeinSicheresPasswort123"  // Nicht leer!

wifi_config_t wifi_config = {
    .ap = {
        .ssid = WIFI_AP_SSID,
        .password = WIFI_AP_PASS,  // ← Passwort aktiviert
        .authmode = WIFI_AUTH_WPA2_PSK,  // ← WPA2 statt Open
        // ...
    },
};
```

**Vorteil:**
- ✅ Funktioniert mit allen Geräten
- ✅ Keine Probleme mit MAC Randomization
- ✅ Einfacher als Whitelist
- ✅ User kann Passwort ändern

---

## 🔐 **SICHERHEITS-VERGLEICH:**

| Methode | Sicherheit | Kompatibilität | Aufwand |
|---------|------------|----------------|---------|
| **BT Whitelist** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| **WiFi Whitelist** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐⭐ |
| **WiFi WPA2 Password** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐ |
| **HTTP Basic Auth** | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |

---

## ✅ **FAZIT:**

### **Für dein Projekt:**

1. **Bluetooth Whitelist: JA** ✅
   - Einfach zu implementieren
   - Funktioniert zuverlässig
   - Verhindert fremde Geräte

2. **WiFi Whitelist: NEIN** ❌
   - MAC Randomization ist Problem
   - Besser: **WPA2 Passwort** nutzen
   - Oder: Captive Portal ist bereits sicher genug

### **Beste Lösung:**

```
Bluetooth:  Whitelist (MAC-Filter)
WiFi AP:    WPA2 Passwort + Captive Portal
Web-Seite:  HTTP Basic Auth (optional)
```

**Damit ist alles sicher UND benutzerfreundlich!** 🎉

---

## 📦 **CODE-BEISPIELE:**

Ich kann dir vollständige Code-Beispiele für alle Methoden erstellen:

1. ✅ Bluetooth Whitelist Implementation
2. ✅ WiFi WPA2 Password Setup
3. ✅ HTTP Basic Auth für Web-Interface
4. ✅ Dynamische Whitelist (via Web-Interface konfigurierbar)

Sag einfach was du brauchst! 😊

# 📦 **PGPEMU WEB INTERFACE - VERSIONS ÜBERSICHT**

**Aktuell:** v1.2.0-STABLE  
**Datum:** 2025-02-23  
**Status:** Production Ready

---

## 📊 **VERSION HISTORY:**

### **v1.0.0-STABLE** (Baseline)
**Release:** 2025-02-23  
**Status:** ✅ STABLE

**Features:**
- ✅ Web Interface mit 3 Tabs
- ✅ Captive Portal (iOS/Android/Windows)
- ✅ Timer Pause/Resume
- ✅ Statistics (Caught/Fled/Spin)
- ✅ Per-Device Settings
- ✅ Stack Overflow Fix (4096 bytes)
- ✅ Second Start Fix (netif reuse)
- ✅ Button: 1 second hold
- ✅ WiFi: 3 minutes timeout
- ✅ WiFi: Open (no password)

**Dateien:**
- button_input_STACK_FIXED.c
- wifi_ap_manager.h
- wifi_ap_manager_PAUSABLE.c
- web_server_V3_ANDROID.c
- stats.h
- stats.c

---

### **v1.1.0-STABLE** (Feature Release)
**Release:** 2025-02-23  
**Status:** ✅ STABLE  
**Upgrade von:** v1.0.0

**Neue Features:**
- ✅ LED Indicator (GPIO 8, blue LED)
- ✅ Button Hold: 1s → **2s**
- ✅ WiFi Timeout: 3min → **5min**
- ✅ WPA2 Password: **"PogoPogo"**
- ✅ TX Power: **Einstellbar** (2.0-21.0 dBm)
- ✅ Secrets Tab (4. Tab)
- ✅ SSID: Einstellbar
- ✅ Password: Einstellbar
- ✅ NVS Storage: Komplett

**Geänderte Dateien:**
- button_input_v1.1.0.c (2s hold)
- wifi_ap_manager_v1.1.0.h (neue API)
- wifi_ap_manager_v1.1.0.c (LED + TX Power + NVS)
- web_server.c (4. Tab Secrets) [PATCH]

**Breaking Changes:**
- ❌ KEINE

---

### **v1.2.0-STABLE** (Device Config)
**Release:** 2025-02-23  
**Status:** ✅ STABLE  
**Upgrade von:** v1.1.0

**Neue Features:**
- ✅ Device Config Tab (5. Tab)
- ✅ PGP_CLONE_NAME: **Einstellbar**
- ✅ PGP_MAC: **Einstellbar**
- ✅ PGP_BLOB: **Einstellbar**
- ✅ PGP_DEVICE_KEY: **Einstellbar**
- ✅ Input Validation (MAC, Hex)
- ✅ Reset to Defaults
- ✅ NVS Storage: device_cfg namespace

**Neue Dateien:**
- device_config.h (API)
- device_config.c (Implementation)

**Geänderte Dateien:**
- web_server.c (5. Tab) [PATCH]
- pgpemu.c (device_config_init call)
- CMakeLists.txt (device_config.c)

**Breaking Changes:**
- ❌ KEINE

---

## 🎯 **FEATURE MATRIX:**

| Feature | v1.0.0 | v1.1.0 | v1.2.0 |
|---------|--------|--------|--------|
| **Tabs** | 3 | 4 | **5** |
| **Button Hold** | 1s | 2s | 2s |
| **WiFi Timeout** | 3min | 5min | 5min |
| **WiFi Security** | Open | WPA2 | WPA2 |
| **WiFi Password** | - | PogoPogo | PogoPogo |
| **TX Power** | Fixed | **Configurable** | **Configurable** |
| **LED Indicator** | ❌ | ✅ | ✅ |
| **SSID Config** | ❌ | ✅ | ✅ |
| **Password Config** | ❌ | ✅ | ✅ |
| **Device Name** | ❌ | ❌ | **✅** |
| **Device MAC** | ❌ | ❌ | **✅** |
| **Device Blob** | ❌ | ❌ | **✅** |
| **Device Key** | ❌ | ❌ | **✅** |
| **Captive Portal** | ✅ | ✅ | ✅ |
| **Statistics** | ✅ | ✅ | ✅ |
| **Per-Device Settings** | ✅ | ✅ | ✅ |
| **Timer Control** | ✅ | ✅ | ✅ |
| **API Endpoints** | 10 | 12 | **15** |
| **NVS Namespaces** | 1 | 2 | **3** |

---

## 📁 **FILE OVERVIEW:**

### **Core Files (Always needed):**
- stats.h / stats.c (unchanged since v1.0.0)
- pgpemu.c (needs updates for v1.2.0)
- CMakeLists.txt (needs updates for v1.2.0)

### **Version-specific Files:**

**v1.0.0:**
- button_input_STACK_FIXED.c
- wifi_ap_manager.h (v1.0.0)
- wifi_ap_manager_PAUSABLE.c (v1.0.0)
- web_server_V3_ANDROID.c (3 tabs)

**v1.1.0 (adds to v1.0.0):**
- button_input_v1.1.0.c (replaces)
- wifi_ap_manager_v1.1.0.h (replaces)
- wifi_ap_manager_v1.1.0.c (replaces)
- web_server.c (4 tabs) [PATCH]

**v1.2.0 (adds to v1.1.0):**
- device_config.h (NEW)
- device_config.c (NEW)
- web_server.c (5 tabs) [PATCH]
- pgpemu.c (updated)
- CMakeLists.txt (updated)

---

## 🚀 **UPGRADE PATHS:**

### **v1.0.0 → v1.1.0:**
```
Replace:
- button_input.c
- wifi_ap_manager.h
- wifi_ap_manager.c

Patch:
- web_server.c (add Secrets tab)

No breaking changes ✅
```

### **v1.1.0 → v1.2.0:**
```
Add:
- device_config.h
- device_config.c

Update:
- pgpemu.c (add init call)
- CMakeLists.txt (add source)

Patch:
- web_server.c (add Device Config tab)

No breaking changes ✅
```

### **v1.0.0 → v1.2.0 (Direct):**
```
Replace:
- button_input.c (v1.1.0)
- wifi_ap_manager.h (v1.1.0)
- wifi_ap_manager.c (v1.1.0)

Add:
- device_config.h
- device_config.c

Update:
- pgpemu.c
- CMakeLists.txt

Patch:
- web_server.c (Secrets + Device Config tabs)

No breaking changes ✅
```

---

## 📊 **RECOMMENDED VERSION:**

### **For Beginners:**
**→ v1.0.0**
- Simplest setup
- All core features
- Easiest to understand

### **For Security:**
**→ v1.1.0**
- WPA2 password
- Configurable TX power
- LED indicator

### **For Customization:**
**→ v1.2.0** ⭐
- Full device config
- Custom names/MAC
- All v1.1.0 features
- Most flexible

---

## 🎯 **INSTALLATION TIME:**

| Version | New Install | Upgrade | Complexity |
|---------|-------------|---------|------------|
| v1.0.0 | 15 min | - | Easy |
| v1.1.0 | 20 min | 10 min | Medium |
| v1.2.0 | 30 min | 15 min | Medium |

---

## 🔄 **ROLLBACK:**

### **v1.2.0 → v1.1.0:**
```cmd
Remove:
- device_config.h
- device_config.c

Revert:
- pgpemu.c (remove init call)
- CMakeLists.txt (remove source)
- web_server.c (remove 5th tab)

Rebuild ✅
```

### **v1.1.0 → v1.0.0:**
```cmd
Replace:
- button_input.c (v1.0.0)
- wifi_ap_manager.h (v1.0.0)
- wifi_ap_manager.c (v1.0.0)
- web_server.c (v1.0.0, 3 tabs)

Rebuild ✅
```

---

## 📦 **DOWNLOADS:**

### **v1.0.0 Package:**
- VERSION_1.0.0_RELEASE.md
- v1.0.0_FILE_LIST.md
- install_v1.0.0_complete.bat
- 6 source files

### **v1.1.0 Package:**
- v1.1.0_RELEASE_NOTES.md
- v1.1.0_FILE_LIST.md
- install_v1.1.0.bat
- 3 source files + 1 patch

### **v1.2.0 Package:**
- v1.2.0_RELEASE_NOTES.md
- v1.2.0_INTEGRATION_GUIDE.md
- install_v1.2.0.bat
- 2 new files + 1 patch

---

## ✅ **WHICH VERSION TO USE?**

### **Start Fresh:**
```
Install latest: v1.2.0
→ Most features
→ Most flexible
→ Future-proof
```

### **Already on v1.0.0:**
```
Option 1: Stay (stable, works)
Option 2: Upgrade to v1.1.0 (more security)
Option 3: Upgrade to v1.2.0 (full control)
```

### **Already on v1.1.0:**
```
Option 1: Stay (good enough)
Option 2: Upgrade to v1.2.0 (device config)
```

---

## 🎉 **SUMMARY:**

**3 Versions verfügbar:**
- ✅ v1.0.0 - Baseline (3 tabs)
- ✅ v1.1.0 - Security & Control (4 tabs)
- ✅ v1.2.0 - Full Customization (5 tabs) ⭐

**Alle Versionen:**
- ✅ Production ready
- ✅ Fully documented
- ✅ Easy rollback
- ✅ No breaking changes

**Empfehlung:** **v1.2.0** für maximale Flexibilität! 🚀

---

**Current:** v1.2.0-STABLE  
**Date:** 2025-02-23  
**Status:** ✅ Production Ready

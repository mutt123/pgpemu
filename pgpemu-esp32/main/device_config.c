/**
 * @file device_config.c v2.0-DUAL
 * @brief PGP Device Configuration - DUAL NAMESPACE IMPLEMENTATION
 * 
 * =============================================================================
 * IMPLEMENTIERUNG DES DUAL-NAMESPACE SYSTEMS
 * =============================================================================
 * 
 * Diese Datei implementiert die Verwaltung von ZWEI getrennten NVS Namespaces
 * für Pokemon GO Plus Device-Konfiguration:
 * 
 * 1. "pgpsecret" - Binary data für Bluetooth
 * 2. "device_cfg" - String data für Web Interface
 * 
 * =============================================================================
 * WICHTIGE KONZEPTE:
 * =============================================================================
 * 
 * BINARY vs STRING FORMAT:
 * ------------------------
 * "pgpsecret" speichert alles als binary (wie Bluetooth es braucht):
 *   - MAC: 6 bytes (z.B. {0xe4, 0xcb, 0x0b, 0xc3, 0x59, 0x63})
 *   - Blob: 256 bytes raw data
 *   - Key: 16 bytes raw data
 * 
 * "device_cfg" speichert alles als strings (wie Web Interface es braucht):
 *   - MAC: "e4:cb:0b:c3:59:63" (17 chars + null)
 *   - Blob: "61f60a07450cd116..." (512 hex chars + null)
 *   - Key: "f972916afd2db437..." (32 hex chars + null)
 * 
 * KONVERTIERUNG:
 * --------------
 * Diese Datei macht automatisch die Konvertierung zwischen beiden Formaten:
 *   - bytes_to_hex(): Binary → Hex String
 *   - hex_to_bytes(): Hex String → Binary
 *   - Für MAC: Spezielle Formatierung mit Doppelpunkten
 * 
 * IN-MEMORY CACHE:
 * ----------------
 * Beide Configs werden im RAM gecacht für schnellen Zugriff:
 *   - current_config: "device_cfg" namespace (strings)
 *   - pgp_secrets: "pgpsecret" namespace (als strings konvertiert)
 * 
 * =============================================================================
 * GLOBALE VARIABLEN (definiert in secrets.h):
 * =============================================================================
 * 
 * extern char PGP_CLONE_NAME[16];      // Wird vom Bluetooth Stack genutzt
 * extern uint8_t PGP_MAC[6];           // Bluetooth MAC
 * extern uint8_t PGP_DEVICE_KEY[16];   // Encryption key
 * extern uint8_t PGP_BLOB[256];        // Device blob
 * 
 * Diese werden automatisch updated wenn set_pgp_secrets_config() aufgerufen wird!
 * 
 * =============================================================================
 */

#include "device_config.h"
#include "config_secrets.h"  // Für read_secrets(), reset_secrets()
#include "secrets.h"         // Für PGP_* globale Variablen
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// =============================================================================
// LOGGING TAG
// =============================================================================

static const char *TAG = "device_config";

// =============================================================================
// IN-MEMORY CONFIGURATION CACHE
// =============================================================================

/**
 * @brief Current "device_cfg" namespace configuration (RAM cache)
 * 
 * Wird initialisiert mit DEFAULT_* Werten und dann aus NVS geladen.
 * Alle get/set Operationen arbeiten mit diesem Cache.
 */
static device_config_t current_config = {
    .name = DEFAULT_PGP_CLONE_NAME,
    .mac = DEFAULT_PGP_MAC,
    .blob = DEFAULT_PGP_BLOB,
    .dkey = DEFAULT_PGP_DEVICE_KEY
};

/**
 * @brief Current "pgpsecret" namespace configuration (RAM cache)
 * 
 * Wird aus NVS geladen und von binary→string konvertiert.
 * Leer wenn keine Secrets konfiguriert sind.
 */
static device_config_t pgp_secrets = {
    .name = "",
    .mac = "",
    .blob = "",
    .dkey = ""
};

/**
 * @brief Initialization flag
 * 
 * Verhindert doppelte Initialisierung.
 */
static bool is_initialized = false;

// =============================================================================
// BINARY ↔ HEX STRING CONVERSION UTILITIES
// =============================================================================

/**
 * @brief Convert binary bytes to hex string
 * 
 * Konvertiert ein Array von bytes zu einem hex string.
 * 
 * BEISPIEL:
 * ---------
 * Input:  bytes = {0xAB, 0xCD, 0xEF}, len = 3
 * Output: hex_out = "abcdef" (6 chars + null terminator)
 * 
 * VERWENDUNG:
 * -----------
 * - MAC bytes → hex string (6 bytes → 12 hex chars)
 * - Blob bytes → hex string (256 bytes → 512 hex chars)
 * - Key bytes → hex string (16 bytes → 32 hex chars)
 * 
 * WICHTIG:
 * --------
 * - hex_out muss genug Platz haben (len * 2 + 1 bytes)!
 * - Output ist lowercase hex (a-f, nicht A-F)
 * - Null-terminiert
 * 
 * @param bytes Input binary data
 * @param len Number of bytes to convert
 * @param hex_out Output buffer (muss mindestens len*2+1 bytes groß sein!)
 */
static void bytes_to_hex(const uint8_t *bytes, size_t len, char *hex_out)
{
    // Sicherheitscheck
    if (!bytes || !hex_out) {
        ESP_LOGE(TAG, "bytes_to_hex: NULL pointer!");
        return;
    }
    
    // Konvertiere jedes byte zu 2 hex chars
    for (size_t i = 0; i < len; i++) {
        sprintf(hex_out + (i * 2), "%02x", bytes[i]);
    }
    
    // Null-terminieren
    hex_out[len * 2] = '\0';
    
    ESP_LOGD(TAG, "bytes_to_hex: %d bytes → %d hex chars", len, len * 2);
}

/**
 * @brief Convert hex string to binary bytes
 * 
 * Konvertiert einen hex string zu einem Array von bytes.
 * 
 * BEISPIEL:
 * ---------
 * Input:  hex = "abcdef" (6 chars)
 * Output: bytes_out = {0xAB, 0xCD, 0xEF} (3 bytes)
 * 
 * VERWENDUNG:
 * -----------
 * - Hex string → MAC bytes (12 hex chars → 6 bytes)
 * - Hex string → Blob bytes (512 hex chars → 256 bytes)
 * - Hex string → Key bytes (32 hex chars → 16 bytes)
 * 
 * VALIDIERUNG:
 * ------------
 * - Hex string MUSS genau expected_len * 2 chars lang sein!
 * - Nur 0-9, A-F, a-f erlaubt
 * - Ungültige chars → Fehler
 * 
 * FEHLER:
 * -------
 * - Falsche Länge → false
 * - Ungültige hex chars → false
 * - NULL pointer → false
 * 
 * @param hex Input hex string (z.B. "abcdef")
 * @param bytes_out Output buffer (muss mindestens expected_len bytes groß sein!)
 * @param expected_len Erwartete Anzahl bytes (hex muss expected_len*2 chars haben!)
 * 
 * @return true wenn erfolgreich konvertiert
 *         false bei Fehlern
 */
static bool hex_to_bytes(const char *hex, uint8_t *bytes_out, size_t expected_len)
{
    // Sicherheitschecks
    if (!hex || !bytes_out) {
        ESP_LOGE(TAG, "hex_to_bytes: NULL pointer!");
        return false;
    }
    
    size_t hex_len = strlen(hex);
    
    // Länge muss exakt passen: expected_len bytes = expected_len*2 hex chars
    if (hex_len != expected_len * 2) {
        ESP_LOGE(TAG, "hex_to_bytes: Wrong length! Expected %d hex chars, got %d", 
                 expected_len * 2, hex_len);
        return false;
    }
    
    // Konvertiere jeweils 2 hex chars zu 1 byte
    for (size_t i = 0; i < expected_len; i++) {
        // sscanf liest 2 chars und konvertiert zu 1 byte
        if (sscanf(hex + (i * 2), "%2hhx", &bytes_out[i]) != 1) {
            ESP_LOGE(TAG, "hex_to_bytes: Invalid hex at position %d (chars '%c%c')", 
                     i * 2, hex[i*2], hex[i*2+1]);
            return false;
        }
    }
    
    ESP_LOGD(TAG, "hex_to_bytes: %d hex chars → %d bytes OK", hex_len, expected_len);
    return true;
}

// =============================================================================
// VALIDATION FUNCTIONS
// =============================================================================

/**
 * @brief Validate MAC address format
 * 
 * Prüft ob ein String ein gültiges MAC address format hat.
 * 
 * GÜLTIGES FORMAT:
 * ----------------
 * - Exakt 17 Zeichen (ohne null terminator)
 * - Format: XX:XX:XX:XX:XX:XX
 * - X = Hex digit (0-9, A-F, a-f)
 * - Doppelpunkte an Positionen 2, 5, 8, 11, 14
 * 
 * POSITION MAPPING:
 * -----------------
 *   e  4  :  c  b  :  0  b  :  c  3  :  5  9  :  6  3
 *   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
 *   ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^  ^
 *   H  H  :  H  H  :  H  H  :  H  H  :  H  H  :  H  H
 * 
 * FEHLERBEISPIELE:
 * ----------------
 * ❌ "e4cb0bc35963" → zu kurz (12 statt 17)
 * ❌ "e4-cb-0b-c3-59-63" → Bindestriche statt Doppelpunkte
 * ❌ "e4:cb:0b:c3:59" → zu kurz
 * ❌ "e4:cb:0b:c3:59:63:00" → zu lang
 * ❌ "XX:XX:XX:XX:XX:XX" → X ist kein hex digit
 * 
 * @param mac MAC address string zu validieren
 * 
 * @return true wenn valid
 *         false wenn invalid oder NULL
 */
bool validate_mac_address(const char *mac)
{
    // NULL check
    if (!mac) {
        ESP_LOGW(TAG, "validate_mac: NULL pointer");
        return false;
    }
    
    // Länge muss exakt 17 sein
    size_t len = strlen(mac);
    if (len != 17) {
        ESP_LOGW(TAG, "validate_mac: Wrong length %d (expected 17)", len);
        return false;
    }
    
    // Prüfe jedes Zeichen
    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            // Position 2, 5, 8, 11, 14 → muss Doppelpunkt sein
            if (mac[i] != ':') {
                ESP_LOGW(TAG, "validate_mac: Expected ':' at position %d, got '%c'", 
                         i, mac[i]);
                return false;
            }
        } else {
            // Alle anderen Positionen → muss hex digit sein
            if (!isxdigit((unsigned char)mac[i])) {
                ESP_LOGW(TAG, "validate_mac: Expected hex digit at position %d, got '%c'", 
                         i, mac[i]);
                return false;
            }
        }
    }
    
    ESP_LOGD(TAG, "validate_mac: '%s' is VALID", mac);
    return true;
}

/**
 * @brief Validate hex string
 * 
 * Prüft ob ein String nur gültige Hex-Zeichen enthält.
 * 
 * GÜLTIGE ZEICHEN:
 * ----------------
 * - 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
 * - A, B, C, D, E, F (uppercase)
 * - a, b, c, d, e, f (lowercase)
 * 
 * UNGÜLTIGE ZEICHEN:
 * ------------------
 * - Leerzeichen, Tabs, Newlines
 * - Doppelpunkte (:)
 * - 0x oder 0X Prefix
 * - Bindestriche (-)
 * - Alles andere
 * 
 * SPEZIALFALL:
 * ------------
 * Leerer String ("") ist VALID! (wird als 0 bytes interpretiert)
 * 
 * VERWENDUNG:
 * -----------
 * - Vor hex_to_bytes() aufrufen
 * - Blob validation
 * - Key validation
 * 
 * @param hex Hex string zu validieren
 * 
 * @return true wenn nur hex chars (oder leer)
 *         false wenn ungültige Zeichen oder NULL
 */
bool validate_hex_string(const char *hex)
{
    // NULL check
    if (!hex) {
        ESP_LOGW(TAG, "validate_hex: NULL pointer");
        return false;
    }
    
    // Leerer string ist valid
    if (strlen(hex) == 0) {
        ESP_LOGD(TAG, "validate_hex: Empty string is valid");
        return true;
    }
    
    // Prüfe jedes Zeichen
    for (size_t i = 0; i < strlen(hex); i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            ESP_LOGW(TAG, "validate_hex: Invalid char '%c' (0x%02X) at position %d", 
                     hex[i], hex[i], i);
            return false;
        }
    }
    
    ESP_LOGD(TAG, "validate_hex: %d hex chars are VALID", strlen(hex));
    return true;
}

// =============================================================================
// NVS ACCESS FUNCTIONS - "device_cfg" NAMESPACE
// =============================================================================

/**
 * @brief Load "device_cfg" namespace from NVS
 * 
 * Liest die String-basierte Konfiguration aus NVS.
 * 
 * WAS ES MACHT:
 * -------------
 * 1. Öffnet NVS "device_cfg" namespace (read-only)
 * 2. Liest alle 4 Felder (name, mac, blob, dkey)
 * 3. Füllt current_config struct
 * 4. Wenn Feld nicht existiert → nutzt DEFAULT_* Wert
 * 5. Schließt NVS
 * 
 * FEHLERBEHANDLUNG:
 * -----------------
 * - Namespace existiert nicht → ESP_OK (nutzt defaults)
 * - Einzelnes Feld fehlt → Nutzt default für dieses Feld
 * - NVS read error → Nutzt default für dieses Feld
 * 
 * LOGS:
 * -----
 * - INFO: Geladene Konfiguration (Name, MAC)
 * - INFO: "No saved device_cfg" wenn namespace leer
 * 
 * @return ESP_OK immer (Fehler sind nicht kritisch)
 */
static esp_err_t load_device_cfg_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // Öffne "device_cfg" namespace (read-only)
    ret = nvs_open("device_cfg", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No saved device_cfg, using defaults");
        return ESP_OK;  // Nicht kritisch, defaults werden genutzt
    }
    
    // Lese Name
    size_t name_len = sizeof(current_config.name);
    ret = nvs_get_str(nvs_handle, "name", current_config.name, &name_len);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "device_cfg/name not found, using default");
        strcpy(current_config.name, DEFAULT_PGP_CLONE_NAME);
    }
    
    // Lese MAC
    size_t mac_len = sizeof(current_config.mac);
    ret = nvs_get_str(nvs_handle, "mac", current_config.mac, &mac_len);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "device_cfg/mac not found, using default");
        strcpy(current_config.mac, DEFAULT_PGP_MAC);
    }
    
    // Lese Blob
    size_t blob_len = sizeof(current_config.blob);
    ret = nvs_get_str(nvs_handle, "blob", current_config.blob, &blob_len);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "device_cfg/blob not found, using default");
        strcpy(current_config.blob, DEFAULT_PGP_BLOB);
    }
    
    // Lese Device Key
    size_t dkey_len = sizeof(current_config.dkey);
    ret = nvs_get_str(nvs_handle, "dkey", current_config.dkey, &dkey_len);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "device_cfg/dkey not found, using default");
        strcpy(current_config.dkey, DEFAULT_PGP_DEVICE_KEY);
    }
    
    // Schließe NVS
    nvs_close(nvs_handle);
    
    // Log geladene Config
    ESP_LOGI(TAG, "device_cfg loaded: Name='%s', MAC='%s'", 
             current_config.name, current_config.mac);
    
    return ESP_OK;
}

/**
 * @brief Load "pgpsecret" namespace from NVS and convert to strings
 * 
 * Liest die Binary-basierten Secrets und konvertiert zu Strings.
 * 
 * WAS ES MACHT:
 * -------------
 * 1. Ruft read_secrets() auf (aus config_secrets.c)
 *    └─ Liest binary data aus "pgpsecret" namespace
 * 2. Konvertiert binary → strings:
 *    ├─ MAC bytes → "XX:XX:XX:XX:XX:XX"
 *    ├─ Blob bytes → hex string (512 chars)
 *    └─ Key bytes → hex string (32 chars)
 * 3. Füllt pgp_secrets struct
 * 
 * BINARY → STRING MAPPING:
 * -------------------------
 * Name: char[16] → strncpy (direkt)
 * MAC: uint8_t[6] → sprintf mit %02x format
 * Blob: uint8_t[256] → bytes_to_hex() (256 → 512 hex chars)
 * Key: uint8_t[16] → bytes_to_hex() (16 → 32 hex chars)
 * 
 * FEHLERBEHANDLUNG:
 * -----------------
 * - read_secrets() returns false → Secrets leer
 * - pgp_secrets wird mit leeren strings initialisiert
 * - ESP_FAIL returned (aber nicht kritisch)
 * 
 * LOGS:
 * -----
 * - WARN: "No pgpsecret data found" wenn leer
 * - INFO: Geladene Secrets (Name, MAC) wenn vorhanden
 * 
 * @return ESP_OK wenn Secrets geladen
 *         ESP_FAIL wenn keine Secrets konfiguriert
 */
static esp_err_t load_pgp_secrets_from_nvs(void)
{
    char name[16];
    uint8_t mac[6], key[16], blob[256];
    
    // Lese binary data aus "pgpsecret" namespace
    // (Nutzt config_secrets.c Funktion)
    if (!read_secrets(name, mac, key, blob)) {
        ESP_LOGW(TAG, "No pgpsecret data found");
        
        // Initialisiere pgp_secrets mit leeren strings
        strcpy(pgp_secrets.name, "");
        strcpy(pgp_secrets.mac, "");
        strcpy(pgp_secrets.blob, "");
        strcpy(pgp_secrets.dkey, "");
        
        return ESP_FAIL;  // Nicht kritisch, aber Secrets fehlen
    }
    
    // === KONVERTIERUNG BINARY → STRING ===
    
    // Name: Direkt kopieren (ist schon string)
    strncpy(pgp_secrets.name, name, sizeof(pgp_secrets.name) - 1);
    pgp_secrets.name[sizeof(pgp_secrets.name) - 1] = '\0';
    
    // MAC: 6 bytes → "XX:XX:XX:XX:XX:XX" (17 chars)
    sprintf(pgp_secrets.mac, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Blob: 256 bytes → 512 hex chars
    bytes_to_hex(blob, 256, pgp_secrets.blob);
    
    // Key: 16 bytes → 32 hex chars
    bytes_to_hex(key, 16, pgp_secrets.dkey);
    
    // Log geladene Secrets
    ESP_LOGI(TAG, "pgpsecret loaded: Name='%s', MAC='%s'", 
             pgp_secrets.name, pgp_secrets.mac);
    ESP_LOGD(TAG, "  Blob length: %d chars", strlen(pgp_secrets.blob));
    ESP_LOGD(TAG, "  Key length: %d chars", strlen(pgp_secrets.dkey));
    
    return ESP_OK;
}

/**
 * @brief Save "device_cfg" namespace to NVS
 * 
 * Schreibt current_config struct zu NVS.
 * 
 * WAS ES MACHT:
 * -------------
 * 1. Öffnet NVS "device_cfg" namespace (read-write)
 * 2. Schreibt alle 4 Felder als strings
 * 3. Committed Änderungen (macht persistent)
 * 4. Schließt NVS
 * 
 * FEHLERBEHANDLUNG:
 * -----------------
 * - NVS open failed → Error log + ESP_FAIL
 * - Set failed → Springt zu cleanup
 * - Commit failed → Error log
 * - Handle wird immer geschlossen (cleanup:)
 * 
 * LOGS:
 * -----
 * - ERROR: Bei NVS Fehlern
 * - INFO: "device_cfg saved to NVS" bei Erfolg
 * 
 * @return ESP_OK bei Erfolg
 *         ESP_FAIL bei NVS Fehlern
 */
static esp_err_t save_device_cfg_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // Öffne "device_cfg" namespace (read-write)
    ret = nvs_open("device_cfg", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open device_cfg: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Schreibe Name
    ret = nvs_set_str(nvs_handle, "name", current_config.name);
    if (ret != ESP_OK) goto cleanup;
    
    // Schreibe MAC
    ret = nvs_set_str(nvs_handle, "mac", current_config.mac);
    if (ret != ESP_OK) goto cleanup;
    
    // Schreibe Blob
    ret = nvs_set_str(nvs_handle, "blob", current_config.blob);
    if (ret != ESP_OK) goto cleanup;
    
    // Schreibe Device Key
    ret = nvs_set_str(nvs_handle, "dkey", current_config.dkey);
    if (ret != ESP_OK) goto cleanup;
    
    // Commit (macht Änderungen persistent)
    ret = nvs_commit(nvs_handle);
    
cleanup:
    // Schließe Handle (immer, auch bei Fehler)
    nvs_close(nvs_handle);
    
    // Log Resultat
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "device_cfg saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save device_cfg: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Save "pgpsecret" namespace to NVS (convert strings to binary)
 * 
 * KRITISCHE FUNKTION: Schreibt ECHTE Bluetooth Secrets!
 * 
 * WAS ES MACHT:
 * -------------
 * 1. Konvertiert strings → binary:
 *    ├─ MAC "XX:XX:XX:XX:XX:XX" → 6 bytes
 *    ├─ Blob hex string → 256 bytes
 *    └─ Key hex string → 16 bytes
 * 2. Öffnet NVS "pgpsecret" namespace (read-write)
 * 3. Schreibt binary data zu NVS
 * 4. Committed Änderungen
 * 5. WICHTIG: Updated globale PGP_* Variablen!
 * 
 * GLOBALE VARIABLEN UPDATE:
 * -------------------------
 * Nach erfolgreichem Speichern werden diese Variablen updated
 * (definiert in secrets.h, genutzt von Bluetooth Stack):
 * - PGP_CLONE_NAME[16]
 * - PGP_MAC[6]
 * - PGP_DEVICE_KEY[16]
 * - PGP_BLOB[256]
 * 
 * STRING → BINARY KONVERTIERUNG:
 * -------------------------------
 * MAC: sscanf mit %2hhx (parsed 2 hex chars zu 1 byte)
 * Blob: hex_to_bytes() (512 hex chars → 256 bytes)
 * Key: hex_to_bytes() (32 hex chars → 16 bytes)
 * 
 * VALIDIERUNG:
 * ------------
 * - MAC format wird geprüft (sscanf return value)
 * - Blob hex wird geprüft (hex_to_bytes return value)
 * - Key hex wird geprüft (hex_to_bytes return value)
 * - Bei Fehler: ESP_ERR_INVALID_ARG + Handle wird geschlossen
 * 
 * FEHLERBEHANDLUNG:
 * -----------------
 * - Ungültiges MAC format → ESP_ERR_INVALID_ARG
 * - Ungültiges Blob format → ESP_ERR_INVALID_ARG
 * - Ungültiges Key format → ESP_ERR_INVALID_ARG
 * - NVS write failed → ESP_FAIL
 * 
 * LOGS:
 * -----
 * - ERROR: Bei Konvertierungs- oder NVS-Fehlern
 * - INFO: "pgpsecret saved to NVS" bei Erfolg
 * 
 * @return ESP_OK bei Erfolg (globale Variablen sind updated!)
 *         ESP_ERR_INVALID_ARG bei Konvertierungsfehlern
 *         ESP_FAIL bei NVS Fehlern
 */
static esp_err_t save_pgp_secrets_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    uint8_t mac[6], key[16], blob[256];
    
    // === KONVERTIERUNG STRING → BINARY ===
    
    // MAC: "XX:XX:XX:XX:XX:XX" → 6 bytes
    // sscanf parsed jeweils 2 hex chars und schreibt zu mac[]
    if (sscanf(pgp_secrets.mac, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        ESP_LOGE(TAG, "Invalid MAC format: %s", pgp_secrets.mac);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Blob: 512 hex chars → 256 bytes
    if (!hex_to_bytes(pgp_secrets.blob, blob, 256)) {
        ESP_LOGE(TAG, "Invalid blob format");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Key: 32 hex chars → 16 bytes
    if (!hex_to_bytes(pgp_secrets.dkey, key, 16)) {
        ESP_LOGE(TAG, "Invalid key format");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Öffne "pgpsecret" namespace (read-write)
    ret = nvs_open("pgpsecret", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open pgpsecret: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Schreibe Name (string)
    ret = nvs_set_str(nvs_handle, "name", pgp_secrets.name);
    if (ret != ESP_OK) goto cleanup;
    
    // Schreibe MAC (binary, 6 bytes)
    ret = nvs_set_blob(nvs_handle, "mac", mac, sizeof(mac));
    if (ret != ESP_OK) goto cleanup;
    
    // Schreibe Device Key (binary, 16 bytes)
    ret = nvs_set_blob(nvs_handle, "dkey", key, sizeof(key));
    if (ret != ESP_OK) goto cleanup;
    
    // Schreibe Blob (binary, 256 bytes)
    ret = nvs_set_blob(nvs_handle, "blob", blob, sizeof(blob));
    if (ret != ESP_OK) goto cleanup;
    
    // Commit
    ret = nvs_commit(nvs_handle);
    
cleanup:
    // Schließe Handle
    nvs_close(nvs_handle);
    
    // Update globale Variablen bei Erfolg
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "pgpsecret saved to NVS");
        
        // === UPDATE GLOBALE PGP_* VARIABLEN ===
        // Diese werden vom Bluetooth Stack genutzt!
        strcpy(PGP_CLONE_NAME, pgp_secrets.name);
        memcpy(PGP_MAC, mac, 6);
        memcpy(PGP_DEVICE_KEY, key, 16);
        memcpy(PGP_BLOB, blob, 256);
        
        ESP_LOGI(TAG, "Global PGP_* variables updated");
        ESP_LOGW(TAG, "⚠️ RESTART DEVICE for Bluetooth to use new secrets!");
    } else {
        ESP_LOGE(TAG, "Failed to save pgpsecret: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

// =============================================================================
// PUBLIC FUNCTIONS - INITIALIZATION
// =============================================================================

/**
 * @brief Initialize device configuration system
 * 
 * WICHTIGSTE FUNKTION: Muss ZUERST aufgerufen werden!
 * 
 * WAS ES MACHT:
 * -------------
 * 1. Prüft ob schon initialisiert (verhindert doppelte Init)
 * 2. Lädt "device_cfg" namespace aus NVS
 * 3. Lädt "pgpsecret" namespace aus NVS (und konvertiert zu strings)
 * 4. Loggt beide Configs für Debug
 * 5. Setzt is_initialized Flag
 * 
 * CALL SEQUENCE:
 * --------------
 * ```c
 * void app_main(void) {
 *     nvs_flash_init();      // 1. NVS initialisieren
 *     device_config_init();  // 2. Diese Funktion!
 *     // ... rest of init
 * }
 * ```
 * 
 * LOG OUTPUT:
 * -----------
 * Bei erfolgreichem Laden siehst du im Serial Monitor:
 * 
 * ```
 * I device_config: Initializing device configuration (DUAL namespace)
 * I device_config: device_cfg loaded: Name='Pokemon GO Plus', MAC='00:00:00:00:00:00'
 * I device_config: pgpsecret loaded: Name='PKLMGOPLUS', MAC='e4:cb:0b:c3:59:63'
 * I device_config: === DEVICE_CFG namespace ===
 * I device_config:   Name: Pokemon GO Plus
 * I device_config:   MAC:  00:00:00:00:00:00
 * I device_config:   Blob: [EMPTY]
 * I device_config:   Key:  [EMPTY]
 * I device_config: === PGPSECRET namespace ===
 * I device_config:   Name: PKLMGOPLUS
 * I device_config:   MAC:  e4:cb:0b:c3:59:63
 * I device_config:   Blob: [SET]
 * I device_config:   Key:  [SET]
 * ```
 * 
 * FEHLERBEHANDLUNG:
 * -----------------
 * - Wenn "device_cfg" leer → Nutzt defaults (kein Fehler)
 * - Wenn "pgpsecret" leer → Warnung loggen (kein Fehler)
 * - Doppelte Initialisierung → Warning loggen
 * 
 * @return ESP_OK immer
 */
esp_err_t device_config_init(void)
{
    // Verhindere doppelte Initialisierung
    if (is_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing device configuration (DUAL namespace)");
    
    // Lade beide Namespaces aus NVS
    load_device_cfg_from_nvs();   // "device_cfg" → current_config
    load_pgp_secrets_from_nvs();  // "pgpsecret" → pgp_secrets
    
    // Markiere als initialisiert
    is_initialized = true;
    
    // === DEBUG LOGGING ===
    // Zeigt beide Configs im Serial Monitor
    
    ESP_LOGI(TAG, "=== DEVICE_CFG namespace ===");
    ESP_LOGI(TAG, "  Name: %s", current_config.name);
    ESP_LOGI(TAG, "  MAC:  %s", current_config.mac);
    ESP_LOGI(TAG, "  Blob: %s", strlen(current_config.blob) > 0 ? "[SET]" : "[EMPTY]");
    ESP_LOGI(TAG, "  Key:  %s", strlen(current_config.dkey) > 0 ? "[SET]" : "[EMPTY]");
    
    ESP_LOGI(TAG, "=== PGPSECRET namespace ===");
    ESP_LOGI(TAG, "  Name: %s", strlen(pgp_secrets.name) > 0 ? pgp_secrets.name : "[EMPTY]");
    ESP_LOGI(TAG, "  MAC:  %s", strlen(pgp_secrets.mac) > 0 ? pgp_secrets.mac : "[EMPTY]");
    ESP_LOGI(TAG, "  Blob: %s", strlen(pgp_secrets.blob) > 0 ? "[SET]" : "[EMPTY]");
    ESP_LOGI(TAG, "  Key:  %s", strlen(pgp_secrets.dkey) > 0 ? "[SET]" : "[EMPTY]");
    
    return ESP_OK;
}

// =============================================================================
// PUBLIC FUNCTIONS - "device_cfg" NAMESPACE
// =============================================================================

/**
 * @brief Get current "device_cfg" namespace configuration
 * 
 * Returned die Web Interface Konfiguration.
 * 
 * VERWENDUNG:
 * -----------
 * - Web Interface API: GET /api/device_config
 * - Backup/Export Funktion
 * - Debug/Logging
 * 
 * BEISPIEL:
 * ---------
 * ```c
 * device_config_t cfg;
 * 
 * if (get_device_config(&cfg) == ESP_OK) {
 *     printf("Name: %s\n", cfg.name);
 *     printf("MAC: %s\n", cfg.mac);
 * }
 * ```
 * 
 * @param config Output buffer (wird gefüllt)
 * 
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG wenn config == NULL
 */
esp_err_t get_device_config(device_config_t *config)
{
    // Parameter validation
    if (!config) {
        ESP_LOGE(TAG, "get_device_config: config is NULL!");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Auto-init falls nicht initialisiert
    if (!is_initialized) {
        ESP_LOGW(TAG, "Not initialized, calling device_config_init()");
        device_config_init();
    }
    
    // Kopiere current_config → output
    memcpy(config, &current_config, sizeof(device_config_t));
    
    ESP_LOGD(TAG, "get_device_config: Returned config (Name='%s')", config->name);
    return ESP_OK;
}

/**
 * @brief Set "device_cfg" namespace configuration
 * 
 * Schreibt neue Web Interface Konfiguration.
 * 
 * VALIDIERUNG:
 * ------------
 * - MAC: Muss XX:XX:XX:XX:XX:XX format haben
 * - Blob: Nur hex chars erlaubt (wenn nicht leer)
 * - Key: Nur hex chars erlaubt (wenn nicht leer)
 * 
 * WICHTIG:
 * --------
 * ⚠️ Ändert NICHT "pgpsecret"!
 * ⚠️ Bluetooth sieht diese Änderungen NICHT!
 * ⚠️ Nur für Web Interface Display!
 * 
 * VERWENDUNG:
 * -----------
 * - Web Interface API: POST /api/device_config
 * - Import/Restore Funktion
 * 
 * BEISPIEL:
 * ---------
 * ```c
 * device_config_t cfg = {
 *     .name = "My Device",
 *     .mac = "aa:bb:cc:dd:ee:ff",
 *     .blob = "",  // optional
 *     .dkey = ""   // optional
 * };
 * 
 * if (set_device_config(&cfg) == ESP_OK) {
 *     printf("Config saved!\n");
 * }
 * ```
 * 
 * @param config Neue Konfiguration
 * 
 * @return ESP_OK bei Erfolg
 *         ESP_ERR_INVALID_ARG bei Validierungsfehlern
 */
esp_err_t set_device_config(const device_config_t *config)
{
    // Parameter validation
    if (!config) {
        ESP_LOGE(TAG, "set_device_config: config is NULL!");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Auto-init falls nicht initialisiert
    if (!is_initialized) {
        ESP_LOGW(TAG, "Not initialized, calling device_config_init()");
        device_config_init();
    }
    
    // === VALIDIERUNG ===
    
    // Validate MAC (wenn nicht leer)
    if (strlen(config->mac) > 0 && !validate_mac_address(config->mac)) {
        ESP_LOGE(TAG, "Invalid MAC address format: %s", config->mac);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate blob (wenn nicht leer)
    if (strlen(config->blob) > 0 && !validate_hex_string(config->blob)) {
        ESP_LOGE(TAG, "Invalid blob format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate device key (wenn nicht leer)
    if (strlen(config->dkey) > 0 && !validate_hex_string(config->dkey)) {
        ESP_LOGE(TAG, "Invalid device key format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Kopiere neue config → current_config
    memcpy(&current_config, config, sizeof(device_config_t));
    
    ESP_LOGI(TAG, "set_device_config: New config set (Name='%s')", current_config.name);
    
    // Speichere zu NVS
    return save_device_cfg_to_nvs();
}

/**
 * @brief Reset "device_cfg" namespace to defaults
 * 
 * Setzt Web Interface Config auf DEFAULT_* Werte.
 * 
 * WICHTIG:
 * --------
 * ⚠️ Löscht NICHT "pgpsecret"!
 * ⚠️ Bluetooth funktioniert weiterhin!
 * 
 * VERWENDUNG:
 * -----------
 * - Web Interface: Reset Button
 * - Factory Reset (nur Web Teil)
 * 
 * @return ESP_OK bei Erfolg
 */
esp_err_t reset_device_config(void)
{
    ESP_LOGI(TAG, "Resetting device_cfg to defaults");
    
    // Setze auf defaults
    strcpy(current_config.name, DEFAULT_PGP_CLONE_NAME);
    strcpy(current_config.mac, DEFAULT_PGP_MAC);
    strcpy(current_config.blob, DEFAULT_PGP_BLOB);
    strcpy(current_config.dkey, DEFAULT_PGP_DEVICE_KEY);
    
    is_initialized = true;
    
    // Speichere zu NVS
    return save_device_cfg_to_nvs();
}

// =============================================================================
// PUBLIC FUNCTIONS - "pgpsecret" NAMESPACE (NEW v2.0)
// =============================================================================

/**
 * @brief Get "pgpsecret" namespace configuration
 * 
 * Returned die ECHTEN Bluetooth Secrets (als strings).
 * 
 * VERWENDUNG:
 * -----------
 * - Web Interface API: GET /api/pgp_secrets
 * - Anzeige der echten Bluetooth Konfiguration
 * - Backup/Export der Secrets
 * 
 * BEISPIEL:
 * ---------
 * ```c
 * device_config_t secrets;
 * 
 * if (get_pgp_secrets_config(&secrets) == ESP_OK) {
 *     printf("REAL Name: %s\n", secrets.name);
 *     printf("REAL MAC: %s\n", secrets.mac);
 *     printf("Blob length: %d chars\n", strlen(secrets.blob));
 * }
 * ```
 * 
 * @param config Output buffer (wird gefüllt)
 * 
 * @return ESP_OK bei Erfolg
 *         ESP_ERR_INVALID_ARG wenn config == NULL
 */
esp_err_t get_pgp_secrets_config(device_config_t *config)
{
    // Parameter validation
    if (!config) {
        ESP_LOGE(TAG, "get_pgp_secrets_config: config is NULL!");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Auto-init falls nicht initialisiert
    if (!is_initialized) {
        ESP_LOGW(TAG, "Not initialized, calling device_config_init()");
        device_config_init();
    }
    
    // Kopiere pgp_secrets → output
    memcpy(config, &pgp_secrets, sizeof(device_config_t));
    
    ESP_LOGD(TAG, "get_pgp_secrets_config: Returned secrets (Name='%s')", config->name);
    return ESP_OK;
}

/**
 * @brief Set "pgpsecret" namespace configuration
 * 
 * ⚠️ KRITISCHE FUNKTION! ⚠️
 * Ändert die ECHTEN Bluetooth Secrets die Pokemon GO sieht!
 * 
 * STRENGE VALIDIERUNG:
 * --------------------
 * - MAC: MUSS XX:XX:XX:XX:XX:XX format haben
 * - Blob: MUSS EXAKT 512 hex chars sein!
 * - Key: MUSS EXAKT 32 hex chars sein!
 * - Nur hex chars erlaubt (0-9, A-F, a-f)
 * 
 * WAS ES MACHT:
 * -------------
 * 1. Validiert alle Eingaben (sehr streng!)
 * 2. Konvertiert strings → binary
 * 3. Schreibt zu "pgpsecret" NVS
 * 4. Updated globale PGP_* Variablen
 * 5. ⚠️ Änderungen aktiv nach Device Restart!
 * 
 * VERWENDUNG:
 * -----------
 * - Web Interface API: POST /api/pgp_secrets
 * - Import neuer Secrets
 * - Änderung der Bluetooth Konfiguration
 * 
 * BEISPIEL:
 * ---------
 * ```c
 * device_config_t secrets = {
 *     .name = "PKLMGOPLUS",
 *     .mac = "e4:cb:0b:c3:59:63",
 *     .blob = "61f60a07..."  // MUSS 512 chars sein!
 *     .dkey = "f972916a..."  // MUSS 32 chars sein!
 * };
 * 
 * if (set_pgp_secrets_config(&secrets) == ESP_OK) {
 *     printf("Saved! RESTART DEVICE NOW!\n");
 * }
 * ```
 * 
 * @param config Neue Secrets
 * 
 * @return ESP_OK bei Erfolg
 *         ESP_ERR_INVALID_ARG bei Validierungsfehlern
 */
esp_err_t set_pgp_secrets_config(const device_config_t *config)
{
    // Parameter validation
    if (!config) {
        ESP_LOGE(TAG, "set_pgp_secrets_config: config is NULL!");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Auto-init falls nicht initialisiert
    if (!is_initialized) {
        ESP_LOGW(TAG, "Not initialized, calling device_config_init()");
        device_config_init();
    }
    
    // === STRENGE VALIDIERUNG ===
    
    // Validate MAC
    if (strlen(config->mac) > 0 && !validate_mac_address(config->mac)) {
        ESP_LOGE(TAG, "Invalid MAC address format: %s", config->mac);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate blob hex string
    if (strlen(config->blob) > 0 && !validate_hex_string(config->blob)) {
        ESP_LOGE(TAG, "Invalid blob format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate device key hex string
    if (strlen(config->dkey) > 0 && !validate_hex_string(config->dkey)) {
        ESP_LOGE(TAG, "Invalid device key format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // === LÄNGEN-VALIDIERUNG (KRITISCH!) ===
    
    // Blob MUSS exakt 512 hex chars sein (256 bytes)
    if (strlen(config->blob) != 512) {
        ESP_LOGE(TAG, "Blob must be exactly 512 hex chars (256 bytes)! Got: %d", 
                 strlen(config->blob));
        return ESP_ERR_INVALID_ARG;
    }
    
    // Key MUSS exakt 32 hex chars sein (16 bytes)
    if (strlen(config->dkey) != 32) {
        ESP_LOGE(TAG, "Key must be exactly 32 hex chars (16 bytes)! Got: %d", 
                 strlen(config->dkey));
        return ESP_ERR_INVALID_ARG;
    }
    
    // Kopiere neue config → pgp_secrets
    memcpy(&pgp_secrets, config, sizeof(device_config_t));
    
    ESP_LOGI(TAG, "set_pgp_secrets_config: New secrets set (Name='%s')", 
             pgp_secrets.name);
    
    // Speichere zu NVS (konvertiert strings → binary)
    return save_pgp_secrets_to_nvs();
}

/**
 * @brief Reset "pgpsecret" namespace
 * 
 * ⚠️ GEFÄHRLICH! ⚠️
 * Löscht ALLE echten Bluetooth Secrets!
 * 
 * NACH DEM RESET:
 * ---------------
 * - Bluetooth funktioniert NICHT mehr!
 * - Pokemon GO findet Device NICHT!
 * - Neue Secrets MÜSSEN konfiguriert werden!
 * 
 * VERWENDUNG:
 * -----------
 * - Factory Reset
 * - Vor Konfiguration neuer Secrets
 * - Troubleshooting
 * 
 * @return ESP_OK bei Erfolg
 *         ESP_FAIL bei Fehler
 */
esp_err_t reset_pgp_secrets(void)
{
    ESP_LOGI(TAG, "Resetting pgpsecret");
    ESP_LOGW(TAG, "⚠️ This will DELETE all Bluetooth secrets!");
    
    // Rufe reset_secrets() aus config_secrets.c auf
    // Diese Funktion löscht "pgpsecret" NVS namespace
    bool success = reset_secrets();
    
    if (success) {
        // Leere pgp_secrets cache
        strcpy(pgp_secrets.name, "");
        strcpy(pgp_secrets.mac, "");
        strcpy(pgp_secrets.blob, "");
        strcpy(pgp_secrets.dkey, "");
        
        ESP_LOGI(TAG, "pgpsecret reset successful");
        ESP_LOGW(TAG, "⚠️ Device will NOT work until new secrets are configured!");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "pgpsecret reset FAILED!");
        return ESP_FAIL;
    }
}

// =============================================================================
// LEGACY COMPATIBILITY FUNCTIONS
// =============================================================================

/**
 * @brief Legacy: Set PGP clone name
 * 
 * DEPRECATED: Nutze stattdessen set_device_config()
 * Schreibt NUR zu "device_cfg", NICHT zu "pgpsecret"!
 */
esp_err_t set_pgp_clone_name(const char *name)
{
    if (!name) return ESP_ERR_INVALID_ARG;
    if (!is_initialized) device_config_init();
    
    strncpy(current_config.name, name, sizeof(current_config.name) - 1);
    current_config.name[sizeof(current_config.name) - 1] = '\0';
    
    ESP_LOGI(TAG, "Legacy: set_pgp_clone_name('%s')", current_config.name);
    
    return save_device_cfg_to_nvs();
}

/**
 * @brief Legacy: Get PGP clone name
 * 
 * DEPRECATED: Nutze stattdessen get_device_config()
 * Liest von "device_cfg", NICHT von "pgpsecret"!
 */
const char* get_pgp_clone_name(void)
{
    if (!is_initialized) device_config_init();
    return current_config.name;
}

/**
 * @brief Legacy: Set PGP MAC
 * 
 * DEPRECATED: Nutze stattdessen set_device_config()
 */
esp_err_t set_pgp_mac(const char *mac)
{
    if (!mac) return ESP_ERR_INVALID_ARG;
    if (strlen(mac) > 0 && !validate_mac_address(mac)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!is_initialized) device_config_init();
    
    strncpy(current_config.mac, mac, sizeof(current_config.mac) - 1);
    current_config.mac[sizeof(current_config.mac) - 1] = '\0';
    
    ESP_LOGI(TAG, "Legacy: set_pgp_mac('%s')", current_config.mac);
    
    return save_device_cfg_to_nvs();
}

/**
 * @brief Legacy: Get PGP MAC
 * 
 * DEPRECATED: Nutze stattdessen get_device_config()
 */
const char* get_pgp_mac(void)
{
    if (!is_initialized) device_config_init();
    return current_config.mac;
}

/**
 * @brief Legacy: Set PGP blob
 * 
 * DEPRECATED: Nutze stattdessen set_device_config()
 */
esp_err_t set_pgp_blob(const char *blob)
{
    if (!blob) return ESP_ERR_INVALID_ARG;
    if (strlen(blob) > 0 && !validate_hex_string(blob)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!is_initialized) device_config_init();
    
    strncpy(current_config.blob, blob, sizeof(current_config.blob) - 1);
    current_config.blob[sizeof(current_config.blob) - 1] = '\0';
    
    ESP_LOGI(TAG, "Legacy: set_pgp_blob (length=%d)", strlen(current_config.blob));
    
    return save_device_cfg_to_nvs();
}

/**
 * @brief Legacy: Get PGP blob
 * 
 * DEPRECATED: Nutze stattdessen get_device_config()
 */
const char* get_pgp_blob(void)
{
    if (!is_initialized) device_config_init();
    return current_config.blob;
}

/**
 * @brief Legacy: Set PGP device key
 * 
 * DEPRECATED: Nutze stattdessen set_device_config()
 */
esp_err_t set_pgp_device_key(const char *dkey)
{
    if (!dkey) return ESP_ERR_INVALID_ARG;
    if (strlen(dkey) > 0 && !validate_hex_string(dkey)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!is_initialized) device_config_init();
    
    strncpy(current_config.dkey, dkey, sizeof(current_config.dkey) - 1);
    current_config.dkey[sizeof(current_config.dkey) - 1] = '\0';
    
    ESP_LOGI(TAG, "Legacy: set_pgp_device_key (length=%d)", strlen(current_config.dkey));
    
    return save_device_cfg_to_nvs();
}

/**
 * @brief Legacy: Get PGP device key
 * 
 * DEPRECATED: Nutze stattdessen get_device_config()
 */
const char* get_pgp_device_key(void)
{
    if (!is_initialized) device_config_init();
    return current_config.dkey;
}

// =============================================================================
// END OF FILE
// =============================================================================

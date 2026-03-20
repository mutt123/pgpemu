/**
 * @file device_config.h v2.0-DUAL
 * @brief PGP Device Configuration Management - DUAL NAMESPACE SUPPORT
 * 
 * =============================================================================
 * ZWECK DIESER DATEI:
 * =============================================================================
 * 
 * Diese Datei verwaltet Pokemon GO Plus Device-Konfigurationen und löst das
 * Problem von ZWEI getrennten NVS Namespaces die nicht synchronisiert waren.
 * 
 * PROBLEM (v1.x):
 * ---------------
 * - Namespace "pgpsecret" (binary data) → Wird von Bluetooth Stack genutzt
 * - Namespace "device_cfg" (string data) → Wird von Web Interface genutzt
 * - KEIN SYNC zwischen beiden → Web Interface zeigt falsche Daten!
 * 
 * LÖSUNG (v2.0):
 * --------------
 * - Beide Namespaces werden separat verwaltet
 * - Web Interface kann BEIDE sehen und editieren
 * - Jeder Namespace hat eigene Get/Set Funktionen
 * - Proper binary ↔ hex string Konvertierung
 * 
 * =============================================================================
 * NAMESPACE ÜBERSICHT:
 * =============================================================================
 * 
 * 1. "pgpsecret" - Bluetooth Stack Configuration
 *    ├─ Gespeichert: Binary Format
 *    ├─ Verwendet von: Bluetooth GAP, Pokemon GO Connection
 *    ├─ Quelle: secrets.csv beim Flash
 *    └─ WICHTIG: Änderungen erfordern Device Restart!
 * 
 * 2. "device_cfg" - Web Interface Configuration
 *    ├─ Gespeichert: String Format (hex strings)
 *    ├─ Verwendet von: Web Interface Anzeige
 *    ├─ Quelle: Web Interface Eingaben
 *    └─ Optional: Kann leer sein (defaults werden genutzt)
 * 
 * =============================================================================
 * AUTHOR: Generated for pgpemu-esp32 project
 * DATE: 2026-03-10
 * =============================================================================
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// DEFAULT VALUES (für "device_cfg" namespace)
// =============================================================================

#define DEFAULT_PGP_CLONE_NAME "Pokemon GO Plus"
#define DEFAULT_PGP_MAC        "00:00:00:00:00:00"
#define DEFAULT_PGP_BLOB       ""
#define DEFAULT_PGP_DEVICE_KEY ""

// =============================================================================
// DEVICE CONFIGURATION STRUCTURE
// =============================================================================

/**
 * @brief Device Configuration Structure (String Format für Web Interface)
 */
typedef struct {
    char name[64];      /**< Device name (max 63 chars) */
    char mac[18];       /**< MAC address "XX:XX:XX:XX:XX:XX" */
    char blob[513];     /**< Blob data (hex string, 512 chars für pgpsecret) */
    char dkey[33];      /**< Device key (hex string, 32 chars für pgpsecret) */
} device_config_t;

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * @brief Initialize device configuration system
 * 
 * Lädt BEIDE Namespaces ("device_cfg" und "pgpsecret") aus NVS.
 * MUSS vor allen anderen Funktionen aufgerufen werden!
 * 
 * @return ESP_OK on success
 */
esp_err_t device_config_init(void);

// =============================================================================
// "device_cfg" NAMESPACE FUNCTIONS
// =============================================================================

/**
 * @brief Get "device_cfg" configuration
 * @param config Output buffer
 * @return ESP_OK on success
 */
esp_err_t get_device_config(device_config_t *config);

/**
 * @brief Set "device_cfg" configuration
 * @param config New configuration
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on validation error
 */
esp_err_t set_device_config(const device_config_t *config);

/**
 * @brief Reset "device_cfg" to defaults
 * @return ESP_OK on success
 */
esp_err_t reset_device_config(void);

// =============================================================================
// "pgpsecret" NAMESPACE FUNCTIONS (NEW v2.0)
// =============================================================================

/**
 * @brief Get "pgpsecret" configuration (echte Bluetooth Secrets!)
 * @param config Output buffer (binary→string converted)
 * @return ESP_OK on success
 */
esp_err_t get_pgp_secrets_config(device_config_t *config);

/**
 * @brief Set "pgpsecret" configuration (KRITISCH: ändert Bluetooth!)
 * 
 * WICHTIG: 
 * - Blob MUSS exakt 512 hex chars sein
 * - Key MUSS exakt 32 hex chars sein
 * - Device Restart erforderlich!
 * 
 * @param config New secrets
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on validation error
 */
esp_err_t set_pgp_secrets_config(const device_config_t *config);

/**
 * @brief Reset "pgpsecret" (⚠️ LÖSCHT alle Secrets!)
 * @return ESP_OK on success
 */
esp_err_t reset_pgp_secrets(void);

// =============================================================================
// LEGACY COMPATIBILITY
// =============================================================================

esp_err_t set_pgp_clone_name(const char *name);
const char* get_pgp_clone_name(void);
esp_err_t set_pgp_mac(const char *mac);
const char* get_pgp_mac(void);
esp_err_t set_pgp_blob(const char *blob);
const char* get_pgp_blob(void);
esp_err_t set_pgp_device_key(const char *dkey);
const char* get_pgp_device_key(void);

// =============================================================================
// VALIDATION
// =============================================================================

/**
 * @brief Validate MAC format (XX:XX:XX:XX:XX:XX)
 */
bool validate_mac_address(const char *mac);

/**
 * @brief Validate hex string (nur 0-9, A-F, a-f)
 */
bool validate_hex_string(const char *hex);

#endif // DEVICE_CONFIG_H

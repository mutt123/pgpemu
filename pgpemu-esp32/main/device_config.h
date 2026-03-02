/**
 * @file device_config.h
 * @brief PGP Device Configuration Management
 * 
 * Manages Pokemon Go Plus device-specific settings:
 * - Clone Name (advertised device name)
 * - MAC Address (Bluetooth MAC)
 * - Blob Data (device-specific blob)
 * - Device Key (encryption key)
 * 
 * All settings are stored in NVS and persist across reboots.
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Default values
#define DEFAULT_PGP_CLONE_NAME "Pokemon GO Plus"
#define DEFAULT_PGP_MAC        "00:00:00:00:00:00"
#define DEFAULT_PGP_BLOB       ""
#define DEFAULT_PGP_DEVICE_KEY ""

// Device Configuration Structure
typedef struct {
    char name[64];      // PGP_CLONE_NAME - Advertised device name
    char mac[18];       // PGP_MAC - MAC address (format: XX:XX:XX:XX:XX:XX)
    char blob[257];     // PGP_BLOB - Device blob data (hex string)
    char dkey[33];      // PGP_DEVICE_KEY - Device encryption key (hex string)
} device_config_t;

/**
 * @brief Initialize device configuration system
 * 
 * Loads configuration from NVS or sets defaults if not found.
 * Must be called before using any other functions.
 * 
 * @return ESP_OK on success
 */
esp_err_t device_config_init(void);

/**
 * @brief Get current device configuration
 * 
 * @param config Pointer to device_config_t structure to fill
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if config is NULL
 */
esp_err_t get_device_config(device_config_t *config);

/**
 * @brief Set complete device configuration
 * 
 * Updates all fields and saves to NVS.
 * 
 * @param config Pointer to device_config_t structure with new values
 * @return ESP_OK on success
 */
esp_err_t set_device_config(const device_config_t *config);

/**
 * @brief Set PGP clone name
 * 
 * @param name New device name (max 63 chars)
 * @return ESP_OK on success
 */
esp_err_t set_pgp_clone_name(const char *name);

/**
 * @brief Get PGP clone name
 * 
 * @return Pointer to current device name (read-only)
 */
const char* get_pgp_clone_name(void);

/**
 * @brief Set PGP MAC address
 * 
 * @param mac New MAC address (format: XX:XX:XX:XX:XX:XX)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if format invalid
 */
esp_err_t set_pgp_mac(const char *mac);

/**
 * @brief Get PGP MAC address
 * 
 * @return Pointer to current MAC address (read-only)
 */
const char* get_pgp_mac(void);

/**
 * @brief Set PGP blob data
 * 
 * @param blob New blob data (hex string, max 256 chars)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if not valid hex
 */
esp_err_t set_pgp_blob(const char *blob);

/**
 * @brief Get PGP blob data
 * 
 * @return Pointer to current blob data (read-only)
 */
const char* get_pgp_blob(void);

/**
 * @brief Set PGP device key
 * 
 * @param dkey New device key (hex string, max 32 chars)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if not valid hex
 */
esp_err_t set_pgp_device_key(const char *dkey);

/**
 * @brief Get PGP device key
 * 
 * @return Pointer to current device key (read-only)
 */
const char* get_pgp_device_key(void);

/**
 * @brief Reset device configuration to defaults
 * 
 * Resets all fields to DEFAULT_* values and saves to NVS.
 * 
 * @return ESP_OK on success
 */
esp_err_t reset_device_config(void);

/**
 * @brief Validate MAC address format
 * 
 * Checks if string is valid MAC address (XX:XX:XX:XX:XX:XX)
 * 
 * @param mac MAC address string to validate
 * @return true if valid, false otherwise
 */
bool validate_mac_address(const char *mac);

/**
 * @brief Validate hex string
 * 
 * Checks if string contains only hex characters (0-9, A-F, a-f)
 * 
 * @param hex Hex string to validate
 * @return true if valid, false otherwise
 */
bool validate_hex_string(const char *hex);

#endif // DEVICE_CONFIG_H

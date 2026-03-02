/**
 * @file device_config.c
 * @brief PGP Device Configuration Management Implementation
 */

#include "device_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "device_config";

// Current configuration (in memory)
static device_config_t current_config = {
    .name = DEFAULT_PGP_CLONE_NAME,
    .mac = DEFAULT_PGP_MAC,
    .blob = DEFAULT_PGP_BLOB,
    .dkey = DEFAULT_PGP_DEVICE_KEY
};

static bool is_initialized = false;

/**
 * @brief Validate MAC address format
 */
bool validate_mac_address(const char *mac)
{
    if (!mac || strlen(mac) != 17) {
        return false;
    }
    
    // Format: XX:XX:XX:XX:XX:XX
    for (int i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            // Should be colon
            if (mac[i] != ':') return false;
        } else {
            // Should be hex digit
            if (!isxdigit((unsigned char)mac[i])) return false;
        }
    }
    
    return true;
}

/**
 * @brief Validate hex string
 */
bool validate_hex_string(const char *hex)
{
    if (!hex) return false;
    
    for (size_t i = 0; i < strlen(hex); i++) {
        if (!isxdigit((unsigned char)hex[i])) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Load configuration from NVS
 */
static esp_err_t load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("device_cfg", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No saved config, using defaults");
        return ESP_OK;  // Not an error, just use defaults
    }
    
    size_t name_len = sizeof(current_config.name);
    ret = nvs_get_str(nvs_handle, "name", current_config.name, &name_len);
    if (ret != ESP_OK) {
        strcpy(current_config.name, DEFAULT_PGP_CLONE_NAME);
    }
    
    size_t mac_len = sizeof(current_config.mac);
    ret = nvs_get_str(nvs_handle, "mac", current_config.mac, &mac_len);
    if (ret != ESP_OK) {
        strcpy(current_config.mac, DEFAULT_PGP_MAC);
    }
    
    size_t blob_len = sizeof(current_config.blob);
    ret = nvs_get_str(nvs_handle, "blob", current_config.blob, &blob_len);
    if (ret != ESP_OK) {
        strcpy(current_config.blob, DEFAULT_PGP_BLOB);
    }
    
    size_t dkey_len = sizeof(current_config.dkey);
    ret = nvs_get_str(nvs_handle, "dkey", current_config.dkey, &dkey_len);
    if (ret != ESP_OK) {
        strcpy(current_config.dkey, DEFAULT_PGP_DEVICE_KEY);
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Config loaded: Name='%s', MAC='%s'", 
             current_config.name, current_config.mac);
    
    return ESP_OK;
}

/**
 * @brief Save configuration to NVS
 */
static esp_err_t save_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("device_cfg", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_str(nvs_handle, "name", current_config.name);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_str(nvs_handle, "mac", current_config.mac);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_str(nvs_handle, "blob", current_config.blob);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_str(nvs_handle, "dkey", current_config.dkey);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_commit(nvs_handle);
    
cleanup:
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Device config saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Initialize device configuration system
 */
esp_err_t device_config_init(void)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing device configuration");
    
    // Load from NVS
    esp_err_t ret = load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config, using defaults");
    }
    
    is_initialized = true;
    
    ESP_LOGI(TAG, "Device config initialized");
    ESP_LOGI(TAG, "  Name: %s", current_config.name);
    ESP_LOGI(TAG, "  MAC:  %s", current_config.mac);
    ESP_LOGI(TAG, "  Blob: %s", strlen(current_config.blob) > 0 ? "[SET]" : "[EMPTY]");
    ESP_LOGI(TAG, "  Key:  %s", strlen(current_config.dkey) > 0 ? "[SET]" : "[EMPTY]");
    
    return ESP_OK;
}

/**
 * @brief Get current device configuration
 */
esp_err_t get_device_config(device_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_initialized) {
        ESP_LOGW(TAG, "Not initialized, calling device_config_init()");
        device_config_init();
    }
    
    memcpy(config, &current_config, sizeof(device_config_t));
    return ESP_OK;
}

/**
 * @brief Set complete device configuration
 */
esp_err_t set_device_config(const device_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_initialized) {
        ESP_LOGW(TAG, "Not initialized, calling device_config_init()");
        device_config_init();
    }
    
    // Validate MAC if provided
    if (strlen(config->mac) > 0 && !validate_mac_address(config->mac)) {
        ESP_LOGE(TAG, "Invalid MAC address format");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate blob if provided
    if (strlen(config->blob) > 0 && !validate_hex_string(config->blob)) {
        ESP_LOGE(TAG, "Invalid blob format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate device key if provided
    if (strlen(config->dkey) > 0 && !validate_hex_string(config->dkey)) {
        ESP_LOGE(TAG, "Invalid device key format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy new config
    memcpy(&current_config, config, sizeof(device_config_t));
    
    // Save to NVS
    return save_to_nvs();
}

/**
 * @brief Set PGP clone name
 */
esp_err_t set_pgp_clone_name(const char *name)
{
    if (!name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_initialized) {
        device_config_init();
    }
    
    strncpy(current_config.name, name, sizeof(current_config.name) - 1);
    current_config.name[sizeof(current_config.name) - 1] = '\0';
    
    ESP_LOGI(TAG, "Device name set to: %s", current_config.name);
    
    return save_to_nvs();
}

/**
 * @brief Get PGP clone name
 */
const char* get_pgp_clone_name(void)
{
    if (!is_initialized) {
        device_config_init();
    }
    
    return current_config.name;
}

/**
 * @brief Set PGP MAC address
 */
esp_err_t set_pgp_mac(const char *mac)
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(mac) > 0 && !validate_mac_address(mac)) {
        ESP_LOGE(TAG, "Invalid MAC address format: %s", mac);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_initialized) {
        device_config_init();
    }
    
    strncpy(current_config.mac, mac, sizeof(current_config.mac) - 1);
    current_config.mac[sizeof(current_config.mac) - 1] = '\0';
    
    ESP_LOGI(TAG, "MAC address set to: %s", current_config.mac);
    
    return save_to_nvs();
}

/**
 * @brief Get PGP MAC address
 */
const char* get_pgp_mac(void)
{
    if (!is_initialized) {
        device_config_init();
    }
    
    return current_config.mac;
}

/**
 * @brief Set PGP blob data
 */
esp_err_t set_pgp_blob(const char *blob)
{
    if (!blob) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(blob) > 0 && !validate_hex_string(blob)) {
        ESP_LOGE(TAG, "Invalid blob format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_initialized) {
        device_config_init();
    }
    
    strncpy(current_config.blob, blob, sizeof(current_config.blob) - 1);
    current_config.blob[sizeof(current_config.blob) - 1] = '\0';
    
    ESP_LOGI(TAG, "Blob data set (length: %d)", strlen(current_config.blob));
    
    return save_to_nvs();
}

/**
 * @brief Get PGP blob data
 */
const char* get_pgp_blob(void)
{
    if (!is_initialized) {
        device_config_init();
    }
    
    return current_config.blob;
}

/**
 * @brief Set PGP device key
 */
esp_err_t set_pgp_device_key(const char *dkey)
{
    if (!dkey) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(dkey) > 0 && !validate_hex_string(dkey)) {
        ESP_LOGE(TAG, "Invalid device key format (must be hex)");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_initialized) {
        device_config_init();
    }
    
    strncpy(current_config.dkey, dkey, sizeof(current_config.dkey) - 1);
    current_config.dkey[sizeof(current_config.dkey) - 1] = '\0';
    
    ESP_LOGI(TAG, "Device key set (length: %d)", strlen(current_config.dkey));
    
    return save_to_nvs();
}

/**
 * @brief Get PGP device key
 */
const char* get_pgp_device_key(void)
{
    if (!is_initialized) {
        device_config_init();
    }
    
    return current_config.dkey;
}

/**
 * @brief Reset device configuration to defaults
 */
esp_err_t reset_device_config(void)
{
    ESP_LOGI(TAG, "Resetting device config to defaults");
    
    strcpy(current_config.name, DEFAULT_PGP_CLONE_NAME);
    strcpy(current_config.mac, DEFAULT_PGP_MAC);
    strcpy(current_config.blob, DEFAULT_PGP_BLOB);
    strcpy(current_config.dkey, DEFAULT_PGP_DEVICE_KEY);
    
    is_initialized = true;
    
    return save_to_nvs();
}

/**
 * @file wifi_ap_manager.h
 * @brief Temporary WiFi Access Point Manager for PGPemu configuration
 * 
 * Provides WiFi AP functionality that runs alongside Bluetooth for 3 minutes
 * Triggered by holding button for 1 second
 */

#ifndef WIFI_AP_MANAGER_H
#define WIFI_AP_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

// WiFi AP Configuration
#define WIFI_AP_SSID            "PGPemu-Setup"
#define WIFI_AP_PASS            ""  // Open network
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_TIMEOUT_MS      (3 * 60 * 1000)  // 3 minutes

// WiFi AP Status
typedef enum {
    WIFI_AP_STATE_STOPPED = 0,
    WIFI_AP_STATE_STARTING,
    WIFI_AP_STATE_RUNNING,
    WIFI_AP_STATE_STOPPING
} wifi_ap_state_t;

/**
 * @brief Initialize WiFi AP Manager
 * @return ESP_OK on success
 */
esp_err_t wifi_ap_manager_init(void);

/**
 * @brief Start temporary WiFi AP (runs for WIFI_AP_TIMEOUT_MS)
 * @return ESP_OK on success
 */
esp_err_t wifi_ap_manager_start(void);

/**
 * @brief Stop WiFi AP manually
 * @return ESP_OK on success
 */
esp_err_t wifi_ap_manager_stop(void);

/**
 * @brief Check if WiFi AP is currently running
 * @return true if running, false otherwise
 */
bool wifi_ap_manager_is_running(void);

/**
 * @brief Get current WiFi AP state
 * @return Current state
 */
wifi_ap_state_t wifi_ap_manager_get_state(void);

/**
 * @brief Get remaining time in milliseconds before auto-shutdown
 * @return Remaining time in ms, 0 if not running
 */
uint32_t wifi_ap_manager_get_remaining_time(void);

#endif // WIFI_AP_MANAGER_H

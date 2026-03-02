/**
 * @file wifi_ap_manager.h
 * @brief WiFi Access Point Manager v1.1.0
 * 
 * NEW in v1.1.0:
 * - Configurable TX power (8.5 dBm default)
 * - 5 minute timeout (was 3)
 * - WPA2 password support
 * - Configurable SSID/Password/TX Power
 * - LED indicator
 */

#ifndef WIFI_AP_MANAGER_H
#define WIFI_AP_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

// WiFi AP Configuration Defaults
#define WIFI_AP_SSID_DEFAULT       "PGPemu-Setup"
#define WIFI_AP_PASS_DEFAULT       "PogoPogo"      // WPA2 password
#define WIFI_AP_CHANNEL            1
#define WIFI_AP_MAX_CONNECTIONS    4
#define WIFI_AP_TIMEOUT_MS         (5 * 60 * 1000)  // 5 minutes (was 3)
#define WIFI_AP_TX_POWER_DEFAULT   34               // 8.5 dBm (34 * 0.25)

// TX Power range: 8 to 84 (2 dBm to 21 dBm in 0.25 dBm steps)
#define WIFI_AP_TX_POWER_MIN       8    // 2.0 dBm
#define WIFI_AP_TX_POWER_MAX       84   // 21.0 dBm

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
 * @brief Cleanup WiFi AP Manager (destroy netif completely)
 * @return ESP_OK on success
 */
esp_err_t wifi_ap_manager_cleanup(void);

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

/**
 * @brief Pause the WiFi AP timeout timer
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not running
 */
esp_err_t wifi_ap_manager_pause_timer(void);

/**
 * @brief Resume the WiFi AP timeout timer
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not paused
 */
esp_err_t wifi_ap_manager_resume_timer(void);

/**
 * @brief Get current WiFi AP configuration
 * 
 * @param ssid Buffer for SSID (can be NULL)
 * @param ssid_len Size of SSID buffer
 * @param password Buffer for password (can be NULL)
 * @param pass_len Size of password buffer
 * @param tx_power Pointer to store TX power (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t wifi_ap_manager_get_config(char *ssid, size_t ssid_len,
                                     char *password, size_t pass_len,
                                     int8_t *tx_power);

/**
 * @brief Set WiFi AP configuration (takes effect on next start)
 * 
 * Settings are saved to NVS and persist across reboots.
 * Changes take effect when WiFi AP is started next time.
 * 
 * @param ssid New SSID (NULL to keep current)
 * @param password New password (NULL to keep current, "" for Open network)
 * @param tx_power New TX power in 0.25 dBm units (8-84, or -1 to keep current)
 * @return ESP_OK on success
 */
esp_err_t wifi_ap_manager_set_config(const char *ssid, const char *password,
                                     int8_t tx_power);

#endif // WIFI_AP_MANAGER_H

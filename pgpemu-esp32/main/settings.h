#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_gatt_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    // any read/write must lock this
    SemaphoreHandle_t mutex;

    // set how many client connections are allowed at the same time
    uint8_t target_active_connections;

    // 1 = debug, 2 = info, 3 = verbose
    uint8_t log_level;
} GlobalSettings;

extern GlobalSettings global_settings;

typedef struct {
    // any read/write must lock this
    SemaphoreHandle_t mutex;

    esp_bd_addr_t bda;

    // NEW: Random session identifier for retoggle matching
    uint32_t session_id;  // Range: 1-999999

    // gotcha functions
    bool autocatch, autospin;

    // 0 = spin everything, 1 to 9 = N/10
    uint8_t autospin_probability;

    // Retoggle state tracking (for bag full/box full scenarios)
    bool autospin_retoggle_pending;
    bool autocatch_retoggle_pending;
    TickType_t autospin_retoggle_time;   // when to restore autospin
    TickType_t autocatch_retoggle_time;  // when to restore autocatch
} DeviceSettings;

void init_global_settings();
void global_settings_ready();
bool toggle_setting(bool* var);
bool toggle_device_autospin(uint8_t c);
bool toggle_device_autocatch(uint8_t c);
uint8_t set_device_autospin_probability(uint8_t c, uint8_t autospin_probability);
bool cycle_log_level(uint8_t* var);
bool get_setting(bool* var);
char* get_setting_log_value(bool* var);
uint8_t get_setting_uint8(uint8_t* var);
bool set_setting_uint8(uint8_t* var, const uint8_t val);

// Generate random session ID (1-999999)
uint32_t generate_session_id(void);

// Toggle functions using session_id
bool toggle_device_autospin_by_session(uint32_t session_id);
bool toggle_device_autocatch_by_session(uint32_t session_id);

// ============================================================
// NEW: Web Interface Getter/Setter Functions
// ============================================================

// Note: Since pgpemu uses per-device settings, we'll use the 
// first connected device's settings for the web interface.
// For global settings, these functions will use device index 0.

/**
 * @brief Get autocatch setting for first connected device
 * @return true if autocatch enabled, false otherwise
 */
bool settings_get_autocatch(void);

/**
 * @brief Set autocatch setting for first connected device
 * @param enabled true to enable, false to disable
 */
void settings_set_autocatch(bool enabled);

/**
 * @brief Get autospin setting for first connected device
 * @return true if autospin enabled, false otherwise
 */
bool settings_get_autospin(void);

/**
 * @brief Set autospin setting for first connected device
 * @param enabled true to enable, false to disable
 */
void settings_set_autospin(bool enabled);

/**
 * @brief Get powerbank ping setting (placeholder - not in original pgpemu)
 * @return false (not implemented in original pgpemu)
 */
bool settings_get_powerbank_ping(void);

/**
 * @brief Set powerbank ping setting (placeholder - not in original pgpemu)
 * @param enabled ignored
 */
void settings_set_powerbank_ping(bool enabled);

/**
 * @brief Get LED actions setting (placeholder - not in original pgpemu)
 * @return false (not implemented in original pgpemu)
 */
bool settings_get_led_actions(void);

/**
 * @brief Set LED actions setting (placeholder - not in original pgpemu)
 * @param enabled ignored
 */
void settings_set_led_actions(bool enabled);

/**
 * @brief Get verbose logging setting
 * @return true if log_level >= 3, false otherwise
 */
bool settings_get_verbose(void);

/**
 * @brief Set verbose logging setting
 * @param enabled true for verbose (level 3), false for info (level 2)
 */
void settings_set_verbose(bool enabled);

/**
 * @brief Save settings to NVS (placeholder - implement if NVS persistence needed)
 * @return ESP_OK on success
 * 
 * Note: shortcuts/pgpemu doesn't have NVS persistence for runtime settings.
 * This is a placeholder that you can implement if you want settings to persist.
 */
esp_err_t settings_save(void);

#endif /* SETTINGS_H */

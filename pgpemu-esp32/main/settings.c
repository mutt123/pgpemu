#include "settings.h"

#include "config_secrets.h"
#include "esp_log.h"
#include "esp_random.h"
#include "log_tags.h"
#include "mutex_helpers.h"
#include "pgp_handshake_multi.h"

#include <stdint.h>

// runtime settings
GlobalSettings global_settings = {
    .mutex = NULL,
    .target_active_connections = 1,
    .log_level = 1,
};

void init_global_settings() {
    global_settings.mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(global_settings.mutex, portMAX_DELAY);  // block until end of this function
}

void global_settings_ready() {
    xSemaphoreGive(global_settings.mutex);
}

bool cycle_log_level(uint8_t* var) {
    if (!var || !mutex_acquire_timeout(global_settings.mutex, 10000)) {
        return false;
    }

    (*var)++;

    if (*var > 3) {
        *var = 1;
    }

    mutex_release(global_settings.mutex);
    return true;
}

bool toggle_setting(bool* var) {
    if (!var || !mutex_acquire_timeout(global_settings.mutex, 10000)) {
        return false;
    }

    *var = !*var;

    mutex_release(global_settings.mutex);

    return true;
}

bool toggle_device_autospin(uint8_t c) {
    client_state_t* entry = get_client_state_entry_by_idx(c);

    if (entry == NULL || entry->settings == NULL) {
        return false;
    }

    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    entry->settings->autospin = !entry->settings->autospin;

    xSemaphoreGive(entry->settings->mutex);

    return entry->settings->autospin;
}

bool toggle_device_autocatch(uint8_t c) {
    client_state_t* entry = get_client_state_entry_by_idx(c);

    if (entry == NULL || entry->settings == NULL) {
        return false;
    }

    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return false;
    }

    entry->settings->autocatch = !entry->settings->autocatch;

    xSemaphoreGive(entry->settings->mutex);

    return entry->settings->autocatch;
}

uint8_t set_device_autospin_probability(uint8_t c, uint8_t autospin_probability) {
    client_state_t* entry = get_client_state_entry_by_idx(c);

    if (entry == NULL || entry->settings == NULL) {
        return 0;
    }

    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return 0;
    }

    if (autospin_probability > 9) {
        ESP_LOGW(SETTING_TASK_TAG,
            "[%d] invalid autospin probability: %d (0-9 allowed)",
            entry->conn_id,
            autospin_probability);
        xSemaphoreGive(entry->settings->mutex);
        return entry->settings->autospin_probability;
    }

    entry->settings->autospin_probability = autospin_probability;

    xSemaphoreGive(entry->settings->mutex);

    return entry->settings->autospin_probability;
}

bool get_setting(bool* var) {
    if (!var || !xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        return false;
    }

    bool result = *var;

    xSemaphoreGive(global_settings.mutex);
    return result;
}

char* get_setting_log_value(bool* var) {
    return get_setting(var) ? "on" : "off";
}

uint8_t get_setting_uint8(uint8_t* var) {
    if (!var || !xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        return 0;
    }

    uint8_t result = *var;

    xSemaphoreGive(global_settings.mutex);
    return result;
}

bool set_setting_uint8(uint8_t* var, const uint8_t val) {
    if (!var || !xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        return false;
    }

    *var = val;

    xSemaphoreGive(global_settings.mutex);
    return true;
}

uint32_t generate_session_id(void) {
    // Generate random number from 1 to 999999
    return (esp_random() % 999999) + 1;
}

bool toggle_device_autospin_by_session(uint32_t session_id) {
    // Search all connected devices for matching session_id
    for (int i = 0; i < get_max_connections(); i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry == NULL || entry->settings == NULL) {
            continue;
        }

        // Compare session IDs
        if (entry->settings->session_id == session_id) {
            if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
                return false;
            }

            entry->settings->autospin = !entry->settings->autospin;
            xSemaphoreGive(entry->settings->mutex);

            ESP_LOGI(SETTING_TASK_TAG,
                "[%d] autospin toggled to %d (session=%lu)",
                entry->conn_id,
                entry->settings->autospin,
                (unsigned long)session_id);

            return true;
        }
    }

    // No device found with this session_id (likely disconnected)
    ESP_LOGW(SETTING_TASK_TAG, "session_id=%lu not found, device likely disconnected", (unsigned long)session_id);
    return false;
}

bool toggle_device_autocatch_by_session(uint32_t session_id) {
    // Search all connected devices for matching session_id
    for (int i = 0; i < get_max_connections(); i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry == NULL || entry->settings == NULL) {
            continue;
        }

        // Compare session IDs
        if (entry->settings->session_id == session_id) {
            if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
                return false;
            }

            entry->settings->autocatch = !entry->settings->autocatch;
            xSemaphoreGive(entry->settings->mutex);

            ESP_LOGI(SETTING_TASK_TAG,
                "[%d] autocatch toggled to %d (session=%lu)",
                entry->conn_id,
                entry->settings->autocatch,
                (unsigned long)session_id);

            return true;
        }
    }

    // No device found with this session_id (likely disconnected)
    ESP_LOGW(SETTING_TASK_TAG, "session_id=%lu not found, device likely disconnected", (unsigned long)session_id);
    return false;
}

// ============================================================
// NEW: Web Interface Getter/Setter Functions
// ============================================================

/**
 * @brief Get first connected device's settings entry
 * Helper function for web interface
 */
static client_state_t* get_first_device_entry(void) {
    // Try to get first active connection
    for (int i = 0; i < get_max_connections(); i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        if (entry != NULL && entry->settings != NULL) {
            return entry;
        }
    }
    return NULL;
}

bool settings_get_autocatch(void) {
    client_state_t* entry = get_first_device_entry();
    
    if (entry == NULL || entry->settings == NULL) {
        // No device connected, return default
        return true;  // Default: autocatch enabled
    }
    
    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return true;  // Default on timeout
    }
    
    bool result = entry->settings->autocatch;
    xSemaphoreGive(entry->settings->mutex);
    
    return result;
}

void settings_set_autocatch(bool enabled) {
    // Set for all connected devices
    for (int i = 0; i < get_max_connections(); i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        
        if (entry == NULL || entry->settings == NULL) {
            continue;
        }
        
        if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
            continue;
        }
        
        entry->settings->autocatch = enabled;
        xSemaphoreGive(entry->settings->mutex);
        
        ESP_LOGI(SETTING_TASK_TAG, "[%d] autocatch set to %d via web interface", 
                 entry->conn_id, enabled);
    }
}

bool settings_get_autospin(void) {
    client_state_t* entry = get_first_device_entry();
    
    if (entry == NULL || entry->settings == NULL) {
        // No device connected, return default
        return true;  // Default: autospin enabled
    }
    
    if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
        return true;  // Default on timeout
    }
    
    bool result = entry->settings->autospin;
    xSemaphoreGive(entry->settings->mutex);
    
    return result;
}

void settings_set_autospin(bool enabled) {
    // Set for all connected devices
    for (int i = 0; i < get_max_connections(); i++) {
        client_state_t* entry = get_client_state_entry_by_idx(i);
        
        if (entry == NULL || entry->settings == NULL) {
            continue;
        }
        
        if (!xSemaphoreTake(entry->settings->mutex, 10000 / portTICK_PERIOD_MS)) {
            continue;
        }
        
        entry->settings->autospin = enabled;
        xSemaphoreGive(entry->settings->mutex);
        
        ESP_LOGI(SETTING_TASK_TAG, "[%d] autospin set to %d via web interface", 
                 entry->conn_id, enabled);
    }
}

bool settings_get_powerbank_ping(void) {
    // Placeholder: shortcuts/pgpemu doesn't have this feature
    // Return false by default
    // You can implement this if you add the powerbank ping feature
    return false;
}

void settings_set_powerbank_ping(bool enabled) {
    // Placeholder: shortcuts/pgpemu doesn't have this feature
    // Log that this feature is not implemented
    ESP_LOGW(SETTING_TASK_TAG, "Powerbank ping feature not implemented in shortcuts/pgpemu");
    (void)enabled;  // Suppress unused warning
}

bool settings_get_led_actions(void) {
    // Placeholder: shortcuts/pgpemu doesn't have a toggle for LED actions
    // The LED always shows actions, so return true
    // You can implement this if you want to add LED on/off control
    return true;
}

void settings_set_led_actions(bool enabled) {
    // Placeholder: shortcuts/pgpemu doesn't have LED on/off toggle
    // Log that this feature is not implemented
    ESP_LOGW(SETTING_TASK_TAG, "LED actions toggle not implemented in shortcuts/pgpemu");
    (void)enabled;  // Suppress unused warning
}

bool settings_get_verbose(void) {
    uint8_t log_level = get_setting_uint8(&global_settings.log_level);
    // Verbose is true if log_level is 3 (verbose), false otherwise
    return (log_level >= 3);
}

void settings_set_verbose(bool enabled) {
    if (!xSemaphoreTake(global_settings.mutex, portMAX_DELAY)) {
        return;
    }
    
    // Set log_level to 3 (verbose) if enabled, else 2 (info)
    global_settings.log_level = enabled ? 3 : 2;
    
    xSemaphoreGive(global_settings.mutex);
    
    ESP_LOGI(SETTING_TASK_TAG, "Verbose logging set to %d via web interface", enabled);
}

esp_err_t settings_save(void) {
    // Placeholder: shortcuts/pgpemu doesn't have NVS persistence for runtime settings
    // You can implement this if you want to add NVS storage
    
    ESP_LOGI(SETTING_TASK_TAG, "Settings saved (runtime only - no NVS persistence)");
    
    // Return success - settings are saved in RAM
    return ESP_OK;
}

/**
 * @file pgpemu.c
 * @brief Main entry point for PGPemu ESP32-C3
 * 
 * Pokemon Go Plus Emulator with WiFi AP configuration support
 */

#include "button_input.h"
#include "config_secrets.h"
#include "config_storage.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "log_tags.h"
#include "pgp_autobutton.h"
#include "pgp_autosetting.h"
#include "pgp_bluetooth.h"
#include "secrets.h"
#include "settings.h"
#include "setup_button.h"
#include "uart.h"

// WiFi AP Feature
#include "wifi_ap_manager.h"
// Note: button_wifi_trigger.h nicht mehr nötig - Logik ist in button_input.c

void app_main() {
    // ========================================
    // UART Menu
    // ========================================
    // Put it first because it purges all logs
    init_uart();

    // ========================================
    // Logging Setup
    // ========================================
    // Set log levels which let init msgs through
    log_levels_debug();

    // ========================================
    // Reset Reason Check
    // ========================================
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(PGPEMU_TAG, "reset reason: %d", reset_reason);

    if (reset_reason == ESP_RST_BROWNOUT) {
        // Keep it from bootlooping too quick when powering from low battery
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }

    // ========================================
    // NVS Initialization
    // ========================================
    init_settings_nvs_partition();

    // ========================================
    // CRITICAL: Event Loop Initialization
    // Must be initialized BEFORE WiFi AP Manager!
    // ========================================
    ESP_LOGI(PGPEMU_TAG, "Initializing event loop...");
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(PGPEMU_TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(PGPEMU_TAG, "Event loop already exists (OK)");
    } else {
        ESP_LOGI(PGPEMU_TAG, "Event loop created successfully");
    }

    // ========================================
    // Settings Initialization
    // ========================================
    init_global_settings();
    read_stored_global_settings(false);

    // ========================================
    // Restore Log Levels from Settings
    // ========================================
    if (global_settings.log_level == 3) {
        ESP_LOGI(PGPEMU_TAG, "log levels verbose");
        log_levels_verbose();
    } else if (global_settings.log_level == 2) {
        ESP_LOGI(PGPEMU_TAG, "log levels info");
        log_levels_info();
    } else {
        ESP_LOGI(PGPEMU_TAG, "log levels debug");
        log_levels_debug();
    }

    // ========================================
    // Secrets Loading
    // ========================================
    // Read secrets from NVS (settings are safe to use because mutex is still locked)
    read_secrets(PGP_CLONE_NAME, PGP_MAC, PGP_DEVICE_KEY, PGP_BLOB);

    if (!PGP_VALID()) {
        // Release mutex
        global_settings_ready();
        ESP_LOGE(PGPEMU_TAG, "NO PGP SECRETS AVAILABLE!");
        ESP_LOGE(PGPEMU_TAG, "See README.md to set them");
        return;
    }

    // ========================================
    // Setup Button Check
    // ========================================
    if (setup_button_pressed_on_boot()) {
        global_settings_ready();  // Release mutex
        ESP_LOGI(PGPEMU_TAG, "setup button pressed on boot; continuing startup");
    }

    // ========================================
    // WiFi AP Manager Initialization
    // Initialize AFTER event loop, BEFORE button_input
    // ========================================
    ESP_LOGI(PGPEMU_TAG, "Initializing WiFi AP Manager...");
    ret = wifi_ap_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(PGPEMU_TAG, "Failed to initialize WiFi AP Manager: %s", 
                 esp_err_to_name(ret));
        // Continue anyway - WiFi AP is optional feature
        ESP_LOGW(PGPEMU_TAG, "Continuing without WiFi AP support");
    } else {
        ESP_LOGI(PGPEMU_TAG, "WiFi AP Manager initialized");
    }

    // ========================================
    // Button Input Task
    // This now includes WiFi trigger logic
    // ========================================
    init_button_input();

    // ========================================
    // Autosetting Task
    // ========================================
    if (!init_autosetting()) {
        ESP_LOGE(PGPEMU_TAG, "Creating autosetting task failed");
        return;
    }

    // ========================================
    // Autobutton Task
    // ========================================
    if (!init_autobutton()) {
        ESP_LOGE(PGPEMU_TAG, "Creating autobutton task failed");
        return;
    }

    // ========================================
    // Bluetooth Initialization
    // ========================================
    // Set clone MAC and start bluetooth
    if (!init_bluetooth()) {
        ESP_LOGE(PGPEMU_TAG, "Bluetooth init failed");
        return;
    }

    // ========================================
    // Startup Complete
    // ========================================
    ESP_LOGI(PGPEMU_TAG, "Device: %s", PGP_CLONE_NAME);
    ESP_LOGI(PGPEMU_TAG,
        "MAC: %02x:%02x:%02x:%02x:%02x:%02x",
        PGP_MAC[0],
        PGP_MAC[1],
        PGP_MAC[2],
        PGP_MAC[3],
        PGP_MAC[4],
        PGP_MAC[5]);
    ESP_LOGI(PGPEMU_TAG, "Ready.");
    ESP_LOGI(PGPEMU_TAG, "");
    ESP_LOGI(PGPEMU_TAG, "=== WiFi AP Configuration ===");
    ESP_LOGI(PGPEMU_TAG, "Hold button for 1 second to start WiFi AP");
    ESP_LOGI(PGPEMU_TAG, "SSID: PGPemu-Setup");
    ESP_LOGI(PGPEMU_TAG, "Web UI: http://192.168.4.1");
    ESP_LOGI(PGPEMU_TAG, "=============================");
    ESP_LOGI(PGPEMU_TAG, "");

    // ========================================
    // Make Settings Available
    // ========================================
    global_settings_ready();
}

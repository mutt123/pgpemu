/**
 * @file button_wifi_trigger.c
 * @brief Button WiFi Trigger Implementation
 */

#include "button_wifi_trigger.h"
#include "wifi_ap_manager.h"
#include "web_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "btn_wifi";

// State tracking
static bool button_was_pressed = false;
static uint32_t button_press_start_ms = 0;
static bool wifi_triggered = false;

/**
 * @brief Initialize button WiFi trigger
 */
esp_err_t button_wifi_trigger_init(void)
{
    ESP_LOGI(TAG, "Button WiFi trigger initialized (hold for %d ms)", 
             BUTTON_WIFI_TRIGGER_HOLD_MS);
    
    button_was_pressed = false;
    button_press_start_ms = 0;
    wifi_triggered = false;
    
    return ESP_OK;
}

/**
 * @brief Process button state for WiFi trigger
 */
void button_wifi_trigger_process(bool button_pressed, uint32_t timestamp_ms)
{
    // Button just pressed
    if (button_pressed && !button_was_pressed) {
        button_press_start_ms = timestamp_ms;
        wifi_triggered = false;
        ESP_LOGD(TAG, "Button pressed at %lu ms", timestamp_ms);
    }
    
    // Button being held
    if (button_pressed && button_was_pressed && !wifi_triggered) {
        uint32_t hold_duration = timestamp_ms - button_press_start_ms;
        
        if (hold_duration >= BUTTON_WIFI_TRIGGER_HOLD_MS) {
            ESP_LOGI(TAG, "Button held for %lu ms - triggering WiFi AP", hold_duration);
            wifi_triggered = true;
            
            // Start WiFi AP if not already running
            if (!wifi_ap_manager_is_running()) {
                esp_err_t ret = wifi_ap_manager_start();
                if (ret == ESP_OK) {
                    // Start web server
                    ret = web_server_start();
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to start web server");
                        wifi_ap_manager_stop();
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to start WiFi AP");
                }
            } else {
                ESP_LOGI(TAG, "WiFi AP already running");
            }
        }
    }
    
    // Button released
    if (!button_pressed && button_was_pressed) {
        uint32_t hold_duration = timestamp_ms - button_press_start_ms;
        ESP_LOGD(TAG, "Button released after %lu ms", hold_duration);
        
        if (!wifi_triggered && hold_duration < BUTTON_WIFI_TRIGGER_HOLD_MS) {
            ESP_LOGD(TAG, "Short press detected (%lu ms) - not triggering WiFi", 
                     hold_duration);
        }
    }
    
    button_was_pressed = button_pressed;
}

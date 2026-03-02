/**
 * @file button_input.c v1.3.1-COEXIST
 * @brief Button input handler with WiFi AP trigger and Bluetooth restart
 * 
 * FEATURES:
 * - Stack increased from 2048 to 4096 bytes
 * - Reduced polling frequency to save CPU
 * - WiFi AP trigger on 2s hold
 * - Bluetooth advertising restart after WiFi (ESP32-C3 coexistence fix)
 * 
 * CRITICAL FIX v1.3.1:
 * - Restarts Bluetooth advertising after WiFi AP starts
 * - PRIMARY fix for ESP32-C3 WiFi-BLE coexistence issue
 * - Works reliably without explicit coexistence API
 */

#include "button_input.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "log_tags.h"
#include "pgp_gap.h"
#include "pgp_handshake_multi.h"
#include "settings.h"

// WiFi AP Manager
#include "wifi_ap_manager.h"
#include "web_server.h"

// Button WiFi Trigger Threshold - v1.1.0: Changed from 1s to 2s
#define BUTTON_WIFI_TRIGGER_HOLD_MS 2000  // 2 seconds
#define BUTTON_POLL_INTERVAL_MS 100  // Poll every 100ms (reduced from 50ms)

static const int CONFIG_GPIO_INPUT_BUTTON0 = GPIO_NUM_3;

static void button_input_task(void* pvParameters);
static QueueHandle_t button_input_queue;

int get_button_gpio() {
    return CONFIG_GPIO_INPUT_BUTTON0;
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(button_input_queue, &gpio_num, NULL);
}

void init_button_input() {
    // create a queue to handle gpio event from isr
    // use size 1 to drop further button events while a press is being handled in the task
    button_input_queue = xQueueCreate(1, sizeof(uint32_t));

    gpio_config_t io_conf = {};
    // interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // bit mask of the pins
    io_conf.pin_bit_mask = (1ULL << CONFIG_GPIO_INPUT_BUTTON0);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // install gpio isr service
    gpio_install_isr_service(0);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(CONFIG_GPIO_INPUT_BUTTON0, gpio_isr_handler, (void*)CONFIG_GPIO_INPUT_BUTTON0);

    // start gpio task - INCREASED STACK SIZE from 2048 to 4096
    xTaskCreate(button_input_task, "button_input", 4096, NULL, 15, NULL);
}

static void button_input_task(void* pvParameters) {
    uint32_t button_event;
    
    // WiFi Trigger State
    uint32_t button_press_start_time = 0;
    bool button_currently_pressed = false;
    bool wifi_trigger_checked = false;

    ESP_LOGI(BUTTON_INPUT_TAG, "task start (stack size: 4096)");

    while (true) {
        // ========================================
        // WiFi AP TRIGGER DETECTION
        // Poll less frequently (every 100ms) to reduce CPU load
        // ========================================
        bool button_state = (gpio_get_level(CONFIG_GPIO_INPUT_BUTTON0) == 0);
        uint32_t current_time_ms = (uint32_t)(esp_timer_get_time() / 1000);
        
        // Button pressed down
        if (button_state && !button_currently_pressed) {
            button_press_start_time = current_time_ms;
            button_currently_pressed = true;
            wifi_trigger_checked = false;
            ESP_LOGD(BUTTON_INPUT_TAG, "Button pressed at %lu ms", current_time_ms);
        }
        
        // Button being held - check for WiFi trigger
        if (button_state && button_currently_pressed && !wifi_trigger_checked) {
            uint32_t hold_duration = current_time_ms - button_press_start_time;
            
            if (hold_duration >= BUTTON_WIFI_TRIGGER_HOLD_MS) {
                ESP_LOGI(BUTTON_INPUT_TAG, "Button held for %lu ms - STARTING WiFi AP", hold_duration);
                wifi_trigger_checked = true;
                
                // ===== DIRECT WiFi AP START =====
                if (!wifi_ap_manager_is_running()) {
                    ESP_LOGI(BUTTON_INPUT_TAG, "Calling wifi_ap_manager_start()...");
                    esp_err_t ret = wifi_ap_manager_start();
                    
                    if (ret == ESP_OK) {
                        ESP_LOGI(BUTTON_INPUT_TAG, "WiFi AP started successfully!");
                        
                        // Start web server
                        ret = web_server_start();
                        if (ret == ESP_OK) {
                            ESP_LOGI(BUTTON_INPUT_TAG, "Web server started successfully!");
                            
                            // CRITICAL FIX: Restart Bluetooth Advertising
                            // ESP32-C3 WiFi-BLE coexistence: WiFi can suppress BLE advertising
                            // Must explicitly restart advertising after WiFi starts
                            ESP_LOGI(BUTTON_INPUT_TAG, "Checking Bluetooth advertising status...");
                            vTaskDelay(pdMS_TO_TICKS(500));  // Wait for WiFi to settle
                            
                            int target_conn = get_setting_uint8(&global_settings.target_active_connections);
                            int active_conn = get_active_connections();
                            
                            if (active_conn < target_conn) {
                                ESP_LOGI(BUTTON_INPUT_TAG, "Restarting Bluetooth advertising (WiFi-BLE coexistence fix)");
                                pgp_advertise();
                            } else {
                                ESP_LOGI(BUTTON_INPUT_TAG, "Bluetooth advertising not needed (%d/%d connections)", 
                                         active_conn, target_conn);
                            }
                        } else {
                            ESP_LOGE(BUTTON_INPUT_TAG, "Failed to start web server: %s", esp_err_to_name(ret));
                            wifi_ap_manager_stop();
                        }
                    } else {
                        ESP_LOGE(BUTTON_INPUT_TAG, "Failed to start WiFi AP: %s", esp_err_to_name(ret));
                    }
                } else {
                    ESP_LOGI(BUTTON_INPUT_TAG, "WiFi AP already running");
                }
                
                // Clear queue to prevent normal button event
                xQueueReset(button_input_queue);
            }
        }
        
        // Button released
        if (!button_state && button_currently_pressed) {
            uint32_t hold_duration = current_time_ms - button_press_start_time;
            ESP_LOGD(BUTTON_INPUT_TAG, "Button released after %lu ms", hold_duration);
            button_currently_pressed = false;
            
            // If WiFi was triggered, ignore normal button function
            if (wifi_trigger_checked) {
                ESP_LOGI(BUTTON_INPUT_TAG, "WiFi AP triggered - ignoring normal button function");
                xQueueReset(button_input_queue);
            }
        }

        // ========================================
        // ORIGINAL: Normal Button Function
        // Use longer timeout for reduced CPU usage
        // ========================================
        if (xQueueReceive(button_input_queue, &button_event, pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS))) {
            // Only process if WiFi was NOT triggered
            if (!wifi_trigger_checked) {
                ESP_LOGV(BUTTON_INPUT_TAG, "button0 down");

                // debounce
                vTaskDelay(200 / portTICK_PERIOD_MS);
                if (gpio_get_level(CONFIG_GPIO_INPUT_BUTTON0) != 0) {
                    // not pressed anymore
                    continue;
                }

                ESP_LOGD(BUTTON_INPUT_TAG, "button0 pressed (normal function)");

                int target_active_connections = get_setting_uint8(&global_settings.target_active_connections);
                int active_connections = get_active_connections();
                
                if (active_connections < target_active_connections) {
                    // target connections not reached, so we should be advertising currently
                    ESP_LOGI(BUTTON_INPUT_TAG, "button -> don't advertise");
                    pgp_advertise_stop();
                } else if (active_connections + 1 <= CONFIG_BT_ACL_CONNECTIONS) {
                    // target connections reached but more connections still possible
                    ESP_LOGI(BUTTON_INPUT_TAG, "button -> advertise");
                    pgp_advertise();
                } else {
                    ESP_LOGW(BUTTON_INPUT_TAG, "button -> max. BT connections reached");
                }
            }
        }
    }

    vTaskDelete(NULL);
}
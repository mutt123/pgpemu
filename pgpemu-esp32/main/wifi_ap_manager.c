/**
 * @file wifi_ap_manager.c
 * @brief WiFi AP Manager Implementation with Debug Logs
 * 
 * FINAL FIX: Removed enum definition (already in header)
 * Added missing includes for MACSTR/MAC2STR
 */

#include "wifi_ap_manager.h"
#include "web_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"         // For MACSTR and MAC2STR
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "wifi_ap_mgr";

// State tracking - USE typedef from header!
static wifi_ap_state_t wifi_ap_state = WIFI_AP_STATE_STOPPED;
static TimerHandle_t timeout_timer = NULL;
static int64_t ap_start_time = 0;

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "WiFi Event: base=%s, id=%ld", event_base, event_id);
    
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP started successfully!");
    } else if (event_id == WIFI_EVENT_AP_STOP) {
        ESP_LOGI(TAG, "WiFi AP stopped");
    }
}

// Timeout callback
static void timeout_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Timeout reached - stopping WiFi AP");
    wifi_ap_manager_stop();
}

/**
 * @brief Initialize WiFi AP Manager
 */
esp_err_t wifi_ap_manager_init(void)
{
    ESP_LOGI(TAG, "=== INITIALIZING WiFi AP Manager ===");
    
    // Create timeout timer (one-shot)
    ESP_LOGI(TAG, "Creating timeout timer (%d seconds)...", WIFI_AP_TIMEOUT_MS / 1000);
    timeout_timer = xTimerCreate("wifi_ap_timeout",
                                 pdMS_TO_TICKS(WIFI_AP_TIMEOUT_MS),
                                 pdFALSE,  // One-shot timer
                                 NULL,
                                 timeout_timer_callback);
    
    if (timeout_timer == NULL) {
        ESP_LOGE(TAG, "FAILED to create timeout timer!");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Timeout timer created successfully");
    wifi_ap_state = WIFI_AP_STATE_STOPPED;
    ESP_LOGI(TAG, "WiFi AP Manager initialization COMPLETE");
    
    return ESP_OK;
}

/**
 * @brief Start temporary WiFi AP
 */
esp_err_t wifi_ap_manager_start(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "=== STARTING WiFi AP ===");
    ESP_LOGI(TAG, "========================================");
    
    ESP_LOGI(TAG, "Current state: %d", wifi_ap_state);
    
    if (wifi_ap_state != WIFI_AP_STATE_STOPPED) {
        ESP_LOGW(TAG, "WiFi AP already running or starting (state=%d)", wifi_ap_state);
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Config: SSID='%s', Channel=%d, Timeout=%d sec",
             WIFI_AP_SSID, WIFI_AP_CHANNEL, WIFI_AP_TIMEOUT_MS / 1000);
    
    wifi_ap_state = WIFI_AP_STATE_STARTING;
    ESP_LOGI(TAG, "State changed to: STARTING");
    
    // Step 1: Initialize netif
    ESP_LOGI(TAG, "Step 1/6: Initializing netif...");
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "FAILED to init netif: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    ESP_LOGI(TAG, "Netif initialized: %s", esp_err_to_name(ret));
    
    // Step 2: Create event loop (if not exists)
    ESP_LOGI(TAG, "Step 2/6: Creating event loop...");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "FAILED to create event loop: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    ESP_LOGI(TAG, "Event loop created: %s", esp_err_to_name(ret));
    
    // Step 3: Create default AP netif
    ESP_LOGI(TAG, "Step 3/6: Creating default WiFi AP netif...");
    esp_netif_t *netif_ap = esp_netif_create_default_wifi_ap();
    if (netif_ap == NULL) {
        ESP_LOGE(TAG, "FAILED to create default WiFi AP netif!");
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "WiFi AP netif created successfully");
    
    // Step 4: Initialize WiFi
    ESP_LOGI(TAG, "Step 4/6: Initializing WiFi with default config...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "FAILED to init WiFi: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    ESP_LOGI(TAG, "WiFi initialized: %s", esp_err_to_name(ret));
    
    // Step 5: Register event handler
    ESP_LOGI(TAG, "Step 5/6: Registering event handler...");
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to register event handler: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    ESP_LOGI(TAG, "Event handler registered successfully");
    
    // Step 6: Configure and start AP
    ESP_LOGI(TAG, "Step 6/6: Configuring WiFi AP...");
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,  // Open network
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    
    ESP_LOGI(TAG, "Setting WiFi mode to AP...");
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to set WiFi mode: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    ESP_LOGI(TAG, "WiFi mode set to AP successfully");
    
    ESP_LOGI(TAG, "Setting WiFi config...");
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to set WiFi config: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    ESP_LOGI(TAG, "WiFi config set successfully");
    
    ESP_LOGI(TAG, "Starting WiFi...");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to start WiFi: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    ESP_LOGI(TAG, "WiFi started successfully!");
    
    // Start timeout timer
    ESP_LOGI(TAG, "Starting timeout timer...");
    if (xTimerStart(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "FAILED to start timeout timer");
        esp_wifi_stop();
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Timeout timer started");
    
    ap_start_time = esp_timer_get_time();
    wifi_ap_state = WIFI_AP_STATE_RUNNING;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "WiFi AP STARTED SUCCESSFULLY!");
    ESP_LOGI(TAG, "SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "IP: 192.168.4.1");
    ESP_LOGI(TAG, "Auto-close in: %d seconds", WIFI_AP_TIMEOUT_MS / 1000);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    return ESP_OK;
}

/**
 * @brief Stop WiFi AP
 */
esp_err_t wifi_ap_manager_stop(void)
{
    ESP_LOGI(TAG, "=== STOPPING WiFi AP ===");
    
    if (wifi_ap_state == WIFI_AP_STATE_STOPPED) {
        ESP_LOGW(TAG, "WiFi AP already stopped");
        return ESP_OK;
    }
    
    wifi_ap_state = WIFI_AP_STATE_STOPPING;
    
    // Stop web server first
    ESP_LOGI(TAG, "Stopping web server...");
    web_server_stop();
    
    // Stop timeout timer
    ESP_LOGI(TAG, "Stopping timeout timer...");
    if (timeout_timer != NULL) {
        xTimerStop(timeout_timer, 0);
    }
    
    // Stop WiFi
    ESP_LOGI(TAG, "Stopping WiFi...");
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "Unregistering event handler...");
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    
    ESP_LOGI(TAG, "Deinitializing WiFi...");
    esp_wifi_deinit();
    
    wifi_ap_state = WIFI_AP_STATE_STOPPED;
    ESP_LOGI(TAG, "WiFi AP stopped successfully");
    
    return ESP_OK;
}

/**
 * @brief Check if WiFi AP is running
 */
bool wifi_ap_manager_is_running(void)
{
    bool running = (wifi_ap_state == WIFI_AP_STATE_RUNNING);
    ESP_LOGD(TAG, "is_running() = %d (state=%d)", running, wifi_ap_state);
    return running;
}

/**
 * @brief Get current state
 */
wifi_ap_state_t wifi_ap_manager_get_state(void)
{
    return wifi_ap_state;
}

/**
 * @brief Get remaining time in milliseconds
 */
uint32_t wifi_ap_manager_get_remaining_time(void)
{
    if (wifi_ap_state != WIFI_AP_STATE_RUNNING) {
        return 0;
    }
    
    int64_t elapsed_us = esp_timer_get_time() - ap_start_time;
    int64_t remaining_ms = (WIFI_AP_TIMEOUT_MS * 1000LL - elapsed_us) / 1000LL;
    
    if (remaining_ms < 0) {
        return 0;
    }
    
    return (uint32_t)remaining_ms;
}

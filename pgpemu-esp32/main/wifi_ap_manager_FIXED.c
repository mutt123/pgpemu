/**
 * @file wifi_ap_manager.c
 * @brief Temporary WiFi Access Point Manager Implementation
 */

#include "wifi_ap_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"       // For esp_timer_get_time()
#include "esp_mac.h"         // For MACSTR and MAC2STR
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "wifi_ap_mgr";

// State tracking
static wifi_ap_state_t wifi_ap_state = WIFI_AP_STATE_STOPPED;
static TimerHandle_t timeout_timer = NULL;
static int64_t ap_start_time = 0;

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static void timeout_timer_callback(TimerHandle_t xTimer);

/**
 * @brief WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

/**
 * @brief Timer callback to stop WiFi AP after timeout
 */
static void timeout_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "WiFi AP timeout reached, stopping...");
    wifi_ap_manager_stop();
}

/**
 * @brief Initialize WiFi AP Manager
 */
esp_err_t wifi_ap_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi AP Manager");
    
    // Create timeout timer (one-shot)
    timeout_timer = xTimerCreate("wifi_ap_timeout",
                                 pdMS_TO_TICKS(WIFI_AP_TIMEOUT_MS),
                                 pdFALSE,  // One-shot timer
                                 NULL,
                                 timeout_timer_callback);
    
    if (timeout_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timeout timer");
        return ESP_FAIL;
    }
    
    wifi_ap_state = WIFI_AP_STATE_STOPPED;
    return ESP_OK;
}

/**
 * @brief Start temporary WiFi AP
 */
esp_err_t wifi_ap_manager_start(void)
{
    if (wifi_ap_state != WIFI_AP_STATE_STOPPED) {
        ESP_LOGW(TAG, "WiFi AP already running or starting");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting WiFi AP: SSID='%s', Timeout=%d seconds",
             WIFI_AP_SSID, WIFI_AP_TIMEOUT_MS / 1000);
    
    wifi_ap_state = WIFI_AP_STATE_STARTING;
    
    // Initialize WiFi
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init netif: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    
    // Create default AP netif
    esp_netif_t *netif_ap = esp_netif_create_default_wifi_ap();
    if (netif_ap == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi AP netif");
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ESP_FAIL;
    }
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init WiFi: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    
    // Register event handler
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    
    // Configure AP
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
    
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ret;
    }
    
    // Start timeout timer
    if (xTimerStart(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timeout timer");
        esp_wifi_stop();
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        return ESP_FAIL;
    }
    
    ap_start_time = esp_timer_get_time();
    wifi_ap_state = WIFI_AP_STATE_RUNNING;
    
    ESP_LOGI(TAG, "WiFi AP started successfully");
    ESP_LOGI(TAG, "Connect to SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Open browser to: http://192.168.4.1");
    
    return ESP_OK;
}

/**
 * @brief Stop WiFi AP
 */
esp_err_t wifi_ap_manager_stop(void)
{
    if (wifi_ap_state == WIFI_AP_STATE_STOPPED) {
        ESP_LOGW(TAG, "WiFi AP already stopped");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping WiFi AP");
    wifi_ap_state = WIFI_AP_STATE_STOPPING;
    
    // Stop timeout timer
    if (timeout_timer != NULL) {
        xTimerStop(timeout_timer, 0);
    }
    
    // Unregister event handler
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    
    // Stop WiFi
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
    }
    
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to deinit WiFi: %s", esp_err_to_name(ret));
    }
    
    wifi_ap_state = WIFI_AP_STATE_STOPPED;
    ap_start_time = 0;
    
    ESP_LOGI(TAG, "WiFi AP stopped");
    return ESP_OK;
}

/**
 * @brief Check if WiFi AP is running
 */
bool wifi_ap_manager_is_running(void)
{
    return wifi_ap_state == WIFI_AP_STATE_RUNNING;
}

/**
 * @brief Get current state
 */
wifi_ap_state_t wifi_ap_manager_get_state(void)
{
    return wifi_ap_state;
}

/**
 * @brief Get remaining time before auto-shutdown
 */
uint32_t wifi_ap_manager_get_remaining_time(void)
{
    if (!wifi_ap_manager_is_running() || ap_start_time == 0) {
        return 0;
    }
    
    int64_t elapsed_us = esp_timer_get_time() - ap_start_time;
    int64_t remaining_us = (WIFI_AP_TIMEOUT_MS * 1000LL) - elapsed_us;
    
    if (remaining_us <= 0) {
        return 0;
    }
    
    return (uint32_t)(remaining_us / 1000);  // Convert to milliseconds
}

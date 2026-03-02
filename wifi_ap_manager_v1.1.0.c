/**
 * @file wifi_ap_manager.c
 * @brief WiFi AP Manager v1.1.0
 * 
 * NEW in v1.1.0:
 * - Configurable TX power (default 8.5 dBm)
 * - Blue LED indicator (GPIO 8, active LOW)
 * - 5 minute timeout (was 3)
 * - WPA2 password support (default: PogoPogo)
 * - Configurable SSID/Password via NVS
 */

#include "wifi_ap_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <string.h>

static const char *TAG = "wifi_ap_mgr";

// LED Configuration
#define LED_GPIO GPIO_NUM_8
#define LED_ON  0   // Active LOW
#define LED_OFF 1

// State tracking
static wifi_ap_state_t wifi_ap_state = WIFI_AP_STATE_STOPPED;
static TimerHandle_t timeout_timer = NULL;
static int64_t ap_start_time = 0;

// Network interface - persistent across start/stop cycles
static esp_netif_t *netif_ap = NULL;

// Pause/Resume state
static bool timer_is_paused = false;
static uint32_t pause_time_remaining_ms = 0;
static int64_t pause_start_time = 0;

// WiFi AP Configuration (stored in NVS)
typedef struct {
    char ssid[32];
    char password[64];
    int8_t tx_power;  // in 0.25 dBm units (8.5 dBm = 34)
} wifi_ap_config_t;

static wifi_ap_config_t current_config = {
    .ssid = WIFI_AP_SSID_DEFAULT,
    .password = WIFI_AP_PASS_DEFAULT,
    .tx_power = WIFI_AP_TX_POWER_DEFAULT
};

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static void timeout_timer_callback(TimerHandle_t xTimer);
static void led_init(void);
static void led_on(void);
static void led_off(void);
static esp_err_t load_config_from_nvs(void);
static esp_err_t save_config_to_nvs(void);

/**
 * @brief Initialize LED
 */
static void led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    led_off();  // Start with LED off
}

/**
 * @brief Turn LED on (active LOW)
 */
static void led_on(void)
{
    gpio_set_level(LED_GPIO, LED_ON);
}

/**
 * @brief Turn LED off (active LOW)
 */
static void led_off(void)
{
    gpio_set_level(LED_GPIO, LED_OFF);
}

/**
 * @brief Load WiFi AP config from NVS
 */
static esp_err_t load_config_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("wifi_ap", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No saved config, using defaults");
        return ESP_OK;  // Use defaults
    }
    
    size_t ssid_len = sizeof(current_config.ssid);
    ret = nvs_get_str(nvs_handle, "ssid", current_config.ssid, &ssid_len);
    if (ret != ESP_OK) {
        strcpy(current_config.ssid, WIFI_AP_SSID_DEFAULT);
    }
    
    size_t pass_len = sizeof(current_config.password);
    ret = nvs_get_str(nvs_handle, "password", current_config.password, &pass_len);
    if (ret != ESP_OK) {
        strcpy(current_config.password, WIFI_AP_PASS_DEFAULT);
    }
    
    int8_t tx_power;
    ret = nvs_get_i8(nvs_handle, "tx_power", &tx_power);
    if (ret == ESP_OK) {
        current_config.tx_power = tx_power;
    } else {
        current_config.tx_power = WIFI_AP_TX_POWER_DEFAULT;
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Loaded config: SSID='%s', TX Power=%d (%.1f dBm)",
             current_config.ssid, current_config.tx_power, 
             current_config.tx_power * 0.25f);
    
    return ESP_OK;
}

/**
 * @brief Save WiFi AP config to NVS
 */
static esp_err_t save_config_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("wifi_ap", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_str(nvs_handle, "ssid", current_config.ssid);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_str(nvs_handle, "password", current_config.password);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_set_i8(nvs_handle, "tx_power", current_config.tx_power);
    if (ret != ESP_OK) goto cleanup;
    
    ret = nvs_commit(nvs_handle);
    
cleanup:
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Config saved: SSID='%s', TX Power=%d (%.1f dBm)",
                 current_config.ssid, current_config.tx_power,
                 current_config.tx_power * 0.25f);
    } else {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

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
    if (timer_is_paused) {
        ESP_LOGD(TAG, "Timer callback ignored - timer is paused");
        return;
    }
    
    ESP_LOGI(TAG, "WiFi AP timeout reached, stopping...");
    wifi_ap_manager_stop();
}

/**
 * @brief Initialize WiFi AP Manager
 */
esp_err_t wifi_ap_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi AP Manager v1.1.0");
    
    // Initialize LED
    led_init();
    ESP_LOGI(TAG, "LED initialized (GPIO %d, active LOW)", LED_GPIO);
    
    // Load config from NVS
    load_config_from_nvs();
    
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
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    
    ESP_LOGI(TAG, "WiFi AP Manager v1.1.0 initialization COMPLETE");
    ESP_LOGI(TAG, "Config: SSID='%s', Password='%s', TX Power=%.1f dBm, Timeout=%d sec",
             current_config.ssid, 
             strlen(current_config.password) > 0 ? "****" : "(Open)",
             current_config.tx_power * 0.25f,
             WIFI_AP_TIMEOUT_MS / 1000);
    
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
             current_config.ssid, WIFI_AP_TIMEOUT_MS / 1000);
    
    wifi_ap_state = WIFI_AP_STATE_STARTING;
    
    // Turn LED on
    led_on();
    ESP_LOGI(TAG, "LED ON (WiFi AP active)");
    
    // Initialize WiFi
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init netif: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    // Create netif only if not already created
    if (netif_ap == NULL) {
        netif_ap = esp_netif_create_default_wifi_ap();
        if (netif_ap == NULL) {
            ESP_LOGE(TAG, "Failed to create default WiFi AP netif");
            wifi_ap_state = WIFI_AP_STATE_STOPPED;
            led_off();
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "WiFi AP netif created");
    } else {
        ESP_LOGI(TAG, "Reusing existing WiFi AP netif");
    }
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to init WiFi: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    // Register event handler
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    // Configure AP
    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.ap.ssid, current_config.ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(current_config.ssid);
    wifi_config.ap.channel = WIFI_AP_CHANNEL;
    wifi_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
    wifi_config.ap.pmf_cfg.required = false;
    
    // Set password (WPA2 or Open)
    if (strlen(current_config.password) > 0) {
        memcpy(wifi_config.ap.password, current_config.password, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_LOGI(TAG, "Security: WPA2-PSK");
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG, "Security: OPEN (no password)");
    }
    
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    // Set TX power
    ret = esp_wifi_set_max_tx_power(current_config.tx_power);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TX Power set to %d (%.1f dBm)", 
                 current_config.tx_power, current_config.tx_power * 0.25f);
    } else {
        ESP_LOGW(TAG, "Failed to set TX power: %s", esp_err_to_name(ret));
    }
    
    // Start timeout timer
    if (xTimerStart(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timeout timer");
        esp_wifi_stop();
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ESP_FAIL;
    }
    
    ap_start_time = esp_timer_get_time();
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    wifi_ap_state = WIFI_AP_STATE_RUNNING;
    
    ESP_LOGI(TAG, "WiFi AP started successfully");
    ESP_LOGI(TAG, "Connect to SSID: %s", current_config.ssid);
    if (strlen(current_config.password) > 0) {
        ESP_LOGI(TAG, "Password required (WPA2)");
    }
    ESP_LOGI(TAG, "Open browser to: http://192.168.4.1");
    ESP_LOGI(TAG, "Captive Portal enabled - page will open automatically");
    
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
    
    // Turn LED off
    led_off();
    ESP_LOGI(TAG, "LED OFF (WiFi AP stopped)");
    
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
    
    // DON'T destroy netif here - keep it for next start
    
    wifi_ap_state = WIFI_AP_STATE_STOPPED;
    ap_start_time = 0;
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    
    ESP_LOGI(TAG, "WiFi AP stopped (netif preserved for reuse)");
    return ESP_OK;
}

/**
 * @brief Cleanup WiFi AP Manager (destroy netif completely)
 */
esp_err_t wifi_ap_manager_cleanup(void)
{
    if (wifi_ap_state != WIFI_AP_STATE_STOPPED) {
        wifi_ap_manager_stop();
    }
    
    if (netif_ap != NULL) {
        ESP_LOGI(TAG, "Destroying WiFi AP netif");
        esp_netif_destroy(netif_ap);
        netif_ap = NULL;
    }
    
    if (timeout_timer != NULL) {
        xTimerDelete(timeout_timer, 0);
        timeout_timer = NULL;
    }
    
    ESP_LOGI(TAG, "WiFi AP Manager cleanup complete");
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
    
    if (timer_is_paused) {
        return pause_time_remaining_ms;
    }
    
    int64_t elapsed_us = esp_timer_get_time() - ap_start_time;
    int64_t remaining_us = (WIFI_AP_TIMEOUT_MS * 1000LL) - elapsed_us;
    
    if (remaining_us <= 0) {
        return 0;
    }
    
    return (uint32_t)(remaining_us / 1000);
}

/**
 * @brief Pause the WiFi AP timeout timer
 */
esp_err_t wifi_ap_manager_pause_timer(void)
{
    if (!wifi_ap_manager_is_running()) {
        ESP_LOGW(TAG, "Cannot pause timer - WiFi AP not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (timer_is_paused) {
        ESP_LOGD(TAG, "Timer already paused");
        return ESP_OK;
    }
    
    if (xTimerStop(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to stop timeout timer");
        return ESP_FAIL;
    }
    
    pause_time_remaining_ms = wifi_ap_manager_get_remaining_time();
    pause_start_time = esp_timer_get_time();
    timer_is_paused = true;
    
    ESP_LOGI(TAG, "Timer paused with %lu ms remaining", 
             (unsigned long)pause_time_remaining_ms);
    
    return ESP_OK;
}

/**
 * @brief Resume the WiFi AP timeout timer
 */
esp_err_t wifi_ap_manager_resume_timer(void)
{
    if (!wifi_ap_manager_is_running()) {
        ESP_LOGW(TAG, "Cannot resume timer - WiFi AP not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!timer_is_paused) {
        ESP_LOGD(TAG, "Timer not paused");
        return ESP_OK;
    }
    
    int64_t current_time = esp_timer_get_time();
    int64_t total_timeout_us = WIFI_AP_TIMEOUT_MS * 1000LL;
    int64_t pause_remaining_us = pause_time_remaining_ms * 1000LL;
    
    ap_start_time = current_time - (total_timeout_us - pause_remaining_us);
    
    if (xTimerChangePeriod(timeout_timer, 
                          pdMS_TO_TICKS(pause_time_remaining_ms), 
                          0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to restart timeout timer");
        return ESP_FAIL;
    }
    
    if (xTimerStart(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timeout timer");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Timer resumed with %lu ms remaining", 
             (unsigned long)pause_time_remaining_ms);
    
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    
    return ESP_OK;
}

/**
 * @brief Get current WiFi AP configuration
 */
esp_err_t wifi_ap_manager_get_config(char *ssid, size_t ssid_len,
                                     char *password, size_t pass_len,
                                     int8_t *tx_power)
{
    if (ssid && ssid_len > 0) {
        strncpy(ssid, current_config.ssid, ssid_len - 1);
        ssid[ssid_len - 1] = '\0';
    }
    
    if (password && pass_len > 0) {
        strncpy(password, current_config.password, pass_len - 1);
        password[pass_len - 1] = '\0';
    }
    
    if (tx_power) {
        *tx_power = current_config.tx_power;
    }
    
    return ESP_OK;
}

/**
 * @brief Set WiFi AP configuration (takes effect on next start)
 */
esp_err_t wifi_ap_manager_set_config(const char *ssid, const char *password,
                                     int8_t tx_power)
{
    if (ssid && strlen(ssid) > 0 && strlen(ssid) < sizeof(current_config.ssid)) {
        strncpy(current_config.ssid, ssid, sizeof(current_config.ssid) - 1);
        current_config.ssid[sizeof(current_config.ssid) - 1] = '\0';
        ESP_LOGI(TAG, "SSID updated: %s", current_config.ssid);
    }
    
    if (password && strlen(password) < sizeof(current_config.password)) {
        strncpy(current_config.password, password, sizeof(current_config.password) - 1);
        current_config.password[sizeof(current_config.password) - 1] = '\0';
        if (strlen(password) > 0) {
            ESP_LOGI(TAG, "Password updated (length: %d)", strlen(current_config.password));
        } else {
            ESP_LOGI(TAG, "Password cleared (Open network)");
        }
    }
    
    if (tx_power >= 8 && tx_power <= 84) {  // 2 dBm to 21 dBm
        current_config.tx_power = tx_power;
        ESP_LOGI(TAG, "TX Power updated: %d (%.1f dBm)", 
                 current_config.tx_power, current_config.tx_power * 0.25f);
    }
    
    return save_config_to_nvs();
}

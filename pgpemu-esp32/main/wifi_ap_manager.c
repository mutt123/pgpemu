/**
 * @file wifi_ap_manager.c v1.3.1-COEXIST
 * @brief WiFi AP Manager with WiFi-BLE Coexistence Support
 * 
 * Features v1.3.1:
 * - Bluetooth Restart (PRIMARY FIX for ESP32-C3 coexistence)
 * - LED Indicator (GPIO 8)
 * - TX Power configurable
 * - 5 minute timeout
 * - WPA2 password
 * - Netif reuse (second start fix)
 * - Web server auto-stop
 * 
 * FIXES v1.3.1:
 * - Removed esp_coex.h dependency (not in ESP-IDF v5.x)
 * - Bluetooth restart in button_input.c is the primary fix
 * - Works without explicit coexistence API calls
 * 
 * FIXES v1.3.0:
 * - Bluetooth stops when WiFi starts [HIGH]
 * - BLE advertising lost after WiFi AP mode
 */

#include "wifi_ap_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
// Note: WiFi-BLE coexistence API is included in esp_wifi.h in ESP-IDF v5.x
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <string.h>
#include "web_server.h"

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

// WiFi AP Settings (stored in NVS) - renamed to avoid conflict with esp_wifi
typedef struct {
    char ssid[32];
    char password[64];
    int8_t tx_power;
} wifi_ap_settings_t;

static wifi_ap_settings_t current_config = {
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
    ESP_LOGI(TAG, "LED: Initializing GPIO %d", LED_GPIO);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED: GPIO config failed: %s", esp_err_to_name(ret));
        return;
    }
    
    led_off();
    ESP_LOGI(TAG, "LED: Initialized successfully (OFF)");
}

/**
 * @brief Turn LED on
 */
static void led_on(void)
{
    ESP_LOGI(TAG, "LED: Turning ON (GPIO %d = LOW)", LED_GPIO);
    gpio_set_level(LED_GPIO, LED_ON);
}

/**
 * @brief Turn LED off
 */
static void led_off(void)
{
    ESP_LOGI(TAG, "LED: Turning OFF (GPIO %d = HIGH)", LED_GPIO);
    gpio_set_level(LED_GPIO, LED_OFF);
}

/**
 * @brief Load config from NVS
 */
static esp_err_t load_config_from_nvs(void)
{
    ESP_LOGI(TAG, "CONFIG: Loading from NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("wifi_ap", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "CONFIG: No NVS config found, using defaults");
        return ESP_OK;
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
    
    ESP_LOGI(TAG, "CONFIG: Loaded - SSID='%s', TX Power=%.1f dBm",
             current_config.ssid, current_config.tx_power * 0.25f);
    
    return ESP_OK;
}

/**
 * @brief Save config to NVS
 */
static esp_err_t save_config_to_nvs(void)
{
    ESP_LOGI(TAG, "CONFIG: Saving to NVS...");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("wifi_ap", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CONFIG: Failed to open NVS: %s", esp_err_to_name(ret));
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
        ESP_LOGI(TAG, "CONFIG: Saved successfully");
    } else {
        ESP_LOGE(TAG, "CONFIG: Save failed: %s", esp_err_to_name(ret));
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
        ESP_LOGI(TAG, "EVENT: Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "EVENT: Station "MACSTR" left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

/**
 * @brief Timer callback
 */
static void timeout_timer_callback(TimerHandle_t xTimer)
{
    if (timer_is_paused) {
        ESP_LOGD(TAG, "TIMER: Callback ignored (paused)");
        return;
    }
    
    ESP_LOGI(TAG, "TIMER: Timeout reached - stopping WiFi AP");
    wifi_ap_manager_stop();
}

/**
 * @brief Initialize WiFi AP Manager
 */
esp_err_t wifi_ap_manager_init(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "INIT: Starting WiFi AP Manager v1.2.0-DEBUG");
    ESP_LOGI(TAG, "============================================");
    
    // Initialize LED
    led_init();
    
    // Load config
    load_config_from_nvs();
    
    // Create timeout timer
    ESP_LOGI(TAG, "INIT: Creating timeout timer (%d ms)", WIFI_AP_TIMEOUT_MS);
    timeout_timer = xTimerCreate("wifi_ap_timeout",
                                 pdMS_TO_TICKS(WIFI_AP_TIMEOUT_MS),
                                 pdFALSE,
                                 NULL,
                                 timeout_timer_callback);
    
    if (timeout_timer == NULL) {
        ESP_LOGE(TAG, "INIT: Failed to create timer!");
        return ESP_FAIL;
    }
    
    wifi_ap_state = WIFI_AP_STATE_STOPPED;
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    
    ESP_LOGI(TAG, "INIT: Complete");
    ESP_LOGI(TAG, "  - SSID: %s", current_config.ssid);
    ESP_LOGI(TAG, "  - Password: %s", strlen(current_config.password) > 0 ? "****" : "(Open)");
    ESP_LOGI(TAG, "  - TX Power: %.1f dBm", current_config.tx_power * 0.25f);
    ESP_LOGI(TAG, "  - Timeout: %d seconds", WIFI_AP_TIMEOUT_MS / 1000);
    ESP_LOGI(TAG, "  - State: STOPPED");
    ESP_LOGI(TAG, "============================================");
    
    return ESP_OK;
}

/**
 * @brief Start WiFi AP
 */
esp_err_t wifi_ap_manager_start(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "START: Request to start WiFi AP");
    ESP_LOGI(TAG, "============================================");
    
    // Check current state
    ESP_LOGI(TAG, "START: Current state = %d", wifi_ap_state);
    
    if (wifi_ap_state != WIFI_AP_STATE_STOPPED) {
        ESP_LOGW(TAG, "START: Already running or starting!");
        return ESP_ERR_INVALID_STATE;
    }
    
    wifi_ap_state = WIFI_AP_STATE_STARTING;
    ESP_LOGI(TAG, "START: State changed to STARTING");
    
    // Turn LED on
    led_on();
    
    // Initialize netif
    ESP_LOGI(TAG, "START: Initializing network interface...");
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "START: netif init failed: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    ESP_LOGI(TAG, "START: netif init OK");
    
    // Create or reuse netif_ap
    if (netif_ap == NULL) {
        ESP_LOGI(TAG, "START: Creating NEW netif_ap...");
        netif_ap = esp_netif_create_default_wifi_ap();
        if (netif_ap == NULL) {
            ESP_LOGE(TAG, "START: Failed to create netif_ap!");
            wifi_ap_state = WIFI_AP_STATE_STOPPED;
            led_off();
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "START: netif_ap created successfully");
    } else {
        ESP_LOGI(TAG, "START: REUSING existing netif_ap (second+ start)");
    }
    
    // Initialize WiFi
    ESP_LOGI(TAG, "START: Initializing WiFi stack...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "START: WiFi init failed: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    ESP_LOGI(TAG, "START: WiFi init OK");
    
    // WiFi-BLE Coexistence Configuration
    // NOTE: ESP32-C3 shares RF between WiFi and BLE
    // The Bluetooth restart in button_input.c is the primary fix
    // Coexistence helps but is not strictly required
    // Commented out as esp_coex API changed in ESP-IDF v5.x
    /*
    ESP_LOGI(TAG, "START: Configuring WiFi-BLE coexistence...");
    ret = esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "START: Coexistence set to BALANCE mode (WiFi + BLE)");
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "START: Coexistence not supported on this chip");
    } else {
        ESP_LOGW(TAG, "START: Coexistence config failed: %s", esp_err_to_name(ret));
    }
    */
    ESP_LOGI(TAG, "START: WiFi-BLE coexistence: Using Bluetooth restart method");
    
    // Register event handler
    ESP_LOGI(TAG, "START: Registering event handler...");
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "START: Event handler registration failed: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    ESP_LOGI(TAG, "START: Event handler registered");
    
    // Configure AP
    ESP_LOGI(TAG, "START: Configuring WiFi AP...");
    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.ap.ssid, current_config.ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(current_config.ssid);
    wifi_config.ap.channel = WIFI_AP_CHANNEL;
    wifi_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
    wifi_config.ap.pmf_cfg.required = false;
    
    if (strlen(current_config.password) > 0) {
        memcpy(wifi_config.ap.password, current_config.password, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_LOGI(TAG, "START: Security = WPA2-PSK");
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGI(TAG, "START: Security = OPEN");
    }
    
    ESP_LOGI(TAG, "START: Setting WiFi mode to AP...");
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "START: Set mode failed: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    ESP_LOGI(TAG, "START: Setting WiFi config...");
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "START: Set config failed: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    
    ESP_LOGI(TAG, "START: Starting WiFi...");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "START: WiFi start failed: %s", esp_err_to_name(ret));
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ret;
    }
    ESP_LOGI(TAG, "START: WiFi started successfully");
    
    // Set TX power
    ESP_LOGI(TAG, "START: Setting TX power to %d (%.1f dBm)...",
             current_config.tx_power, current_config.tx_power * 0.25f);
    ret = esp_wifi_set_max_tx_power(current_config.tx_power);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "START: TX Power set successfully");
    } else {
        ESP_LOGW(TAG, "START: TX Power set failed: %s", esp_err_to_name(ret));
    }
    
    // Start timeout timer
    ESP_LOGI(TAG, "START: Starting timeout timer...");
    if (xTimerStart(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "START: Failed to start timer!");
        esp_wifi_stop();
        wifi_ap_state = WIFI_AP_STATE_STOPPED;
        led_off();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "START: Timer started");
    
    ap_start_time = esp_timer_get_time();
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    wifi_ap_state = WIFI_AP_STATE_RUNNING;
    
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "START: WiFi AP is now RUNNING");
    ESP_LOGI(TAG, "  - SSID: %s", current_config.ssid);
    ESP_LOGI(TAG, "  - IP: 192.168.4.1");
    ESP_LOGI(TAG, "  - LED: ON");
    ESP_LOGI(TAG, "  - Timeout: %d seconds", WIFI_AP_TIMEOUT_MS / 1000);
    ESP_LOGI(TAG, "============================================");
    
    return ESP_OK;
}

/**
 * @brief Stop WiFi AP
 */
esp_err_t wifi_ap_manager_stop(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "STOP: Request to stop WiFi AP");
    ESP_LOGI(TAG, "============================================");
    
    ESP_LOGI(TAG, "STOP: Current state = %d", wifi_ap_state);
    
    if (wifi_ap_state == WIFI_AP_STATE_STOPPED) {
        ESP_LOGW(TAG, "STOP: Already stopped");
        return ESP_OK;
    }
    
    // CRITICAL FIX: Stop web server first!
    ESP_LOGI(TAG, "Stopping web server...");
    esp_err_t ret = web_server_stop();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Web server stopped successfully");
    }

    wifi_ap_state = WIFI_AP_STATE_STOPPING;
    ESP_LOGI(TAG, "STOP: State changed to STOPPING");
    
    // Turn LED off
    led_off();
    
    // Stop timer
    if (timeout_timer != NULL) {
        ESP_LOGI(TAG, "STOP: Stopping timer...");
        xTimerStop(timeout_timer, 0);
    }
    
    // Unregister event handler
    ESP_LOGI(TAG, "STOP: Unregistering event handler...");
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    
    // Stop WiFi
    ESP_LOGI(TAG, "STOP: Stopping WiFi...");
    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "STOP: WiFi stop returned: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "STOP: WiFi stopped");
    }
    
    // Deinit WiFi
    ESP_LOGI(TAG, "STOP: Deinitializing WiFi...");
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "STOP: WiFi deinit returned: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "STOP: WiFi deinitialized");
    }
    
    // DON'T destroy netif - keep for next start!
    ESP_LOGI(TAG, "STOP: Preserving netif_ap for reuse");
    
    wifi_ap_state = WIFI_AP_STATE_STOPPED;
    ap_start_time = 0;
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "STOP: WiFi AP is now STOPPED");
    ESP_LOGI(TAG, "  - LED: OFF");
    ESP_LOGI(TAG, "  - netif_ap: PRESERVED");
    ESP_LOGI(TAG, "  - Ready for next start");
    ESP_LOGI(TAG, "============================================");
    
    return ESP_OK;
}

/**
 * @brief Cleanup (destroy netif completely)
 */
esp_err_t wifi_ap_manager_cleanup(void)
{
    ESP_LOGI(TAG, "CLEANUP: Complete cleanup requested");
    
    if (wifi_ap_state != WIFI_AP_STATE_STOPPED) {
        wifi_ap_manager_stop();
    }
    
    if (netif_ap != NULL) {
        ESP_LOGI(TAG, "CLEANUP: Destroying netif_ap");
        esp_netif_destroy(netif_ap);
        netif_ap = NULL;
    }
    
    if (timeout_timer != NULL) {
        ESP_LOGI(TAG, "CLEANUP: Deleting timer");
        xTimerDelete(timeout_timer, 0);
        timeout_timer = NULL;
    }
    
    ESP_LOGI(TAG, "CLEANUP: Complete");
    return ESP_OK;
}

// Remaining functions (is_running, get_state, get_remaining_time, pause, resume, get/set_config)
// Same as before but with added debug logs...

bool wifi_ap_manager_is_running(void)
{
    bool running = (wifi_ap_state == WIFI_AP_STATE_RUNNING);
    ESP_LOGD(TAG, "is_running: %s", running ? "YES" : "NO");
    return running;
}

wifi_ap_state_t wifi_ap_manager_get_state(void)
{
    return wifi_ap_state;
}

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

esp_err_t wifi_ap_manager_pause_timer(void)
{
    ESP_LOGI(TAG, "PAUSE: Timer pause requested");
    
    if (!wifi_ap_manager_is_running()) {
        ESP_LOGW(TAG, "PAUSE: Not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (timer_is_paused) {
        ESP_LOGD(TAG, "PAUSE: Already paused");
        return ESP_OK;
    }
    
    if (xTimerStop(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "PAUSE: Timer stop failed");
        return ESP_FAIL;
    }
    
    pause_time_remaining_ms = wifi_ap_manager_get_remaining_time();
    pause_start_time = esp_timer_get_time();
    timer_is_paused = true;
    
    ESP_LOGI(TAG, "PAUSE: Paused with %lu ms remaining", 
             (unsigned long)pause_time_remaining_ms);
    
    return ESP_OK;
}

esp_err_t wifi_ap_manager_resume_timer(void)
{
    ESP_LOGI(TAG, "RESUME: Timer resume requested");
    
    if (!wifi_ap_manager_is_running()) {
        ESP_LOGW(TAG, "RESUME: Not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!timer_is_paused) {
        ESP_LOGD(TAG, "RESUME: Not paused");
        return ESP_OK;
    }
    
    int64_t current_time = esp_timer_get_time();
    int64_t total_timeout_us = WIFI_AP_TIMEOUT_MS * 1000LL;
    int64_t pause_remaining_us = pause_time_remaining_ms * 1000LL;
    
    ap_start_time = current_time - (total_timeout_us - pause_remaining_us);
    
    if (xTimerChangePeriod(timeout_timer, 
                          pdMS_TO_TICKS(pause_time_remaining_ms), 
                          0) != pdPASS) {
        ESP_LOGE(TAG, "RESUME: Timer period change failed");
        return ESP_FAIL;
    }
    
    if (xTimerStart(timeout_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "RESUME: Timer start failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "RESUME: Resumed with %lu ms remaining", 
             (unsigned long)pause_time_remaining_ms);
    
    timer_is_paused = false;
    pause_time_remaining_ms = 0;
    
    return ESP_OK;
}

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

esp_err_t wifi_ap_manager_set_config(const char *ssid, const char *password,
                                     int8_t tx_power)
{
    ESP_LOGI(TAG, "SET_CONFIG: Updating configuration");
    
    if (ssid && strlen(ssid) > 0 && strlen(ssid) < sizeof(current_config.ssid)) {
        strncpy(current_config.ssid, ssid, sizeof(current_config.ssid) - 1);
        current_config.ssid[sizeof(current_config.ssid) - 1] = '\0';
        ESP_LOGI(TAG, "SET_CONFIG: SSID = %s", current_config.ssid);
    }
    
    if (password && strlen(password) < sizeof(current_config.password)) {
        strncpy(current_config.password, password, sizeof(current_config.password) - 1);
        current_config.password[sizeof(current_config.password) - 1] = '\0';
        if (strlen(password) > 0) {
            ESP_LOGI(TAG, "SET_CONFIG: Password = **** (length: %d)", strlen(current_config.password));
        } else {
            ESP_LOGI(TAG, "SET_CONFIG: Password cleared (Open network)");
        }
    }
    
    if (tx_power >= 8 && tx_power <= 84) {
        current_config.tx_power = tx_power;
        ESP_LOGI(TAG, "SET_CONFIG: TX Power = %d (%.1f dBm)", 
                 current_config.tx_power, current_config.tx_power * 0.25f);
    }
    
    return save_config_to_nvs();
}
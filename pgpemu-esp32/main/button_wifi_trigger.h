/**
 * @file button_wifi_trigger.h
 * @brief Button handler extension for WiFi AP trigger
 * 
 * This module extends the existing button_input.c functionality
 * to detect a 1-second button hold and trigger WiFi AP
 */

#ifndef BUTTON_WIFI_TRIGGER_H
#define BUTTON_WIFI_TRIGGER_H

#include "esp_err.h"
#include <stdbool.h>

// Button hold threshold for WiFi AP trigger (1 second)
#define BUTTON_WIFI_TRIGGER_HOLD_MS 1000

/**
 * @brief Initialize button WiFi trigger handler
 * @return ESP_OK on success
 */
esp_err_t button_wifi_trigger_init(void);

/**
 * @brief Process button state for WiFi trigger detection
 * 
 * Call this from button ISR or polling loop
 * @param button_pressed Current button state (true = pressed)
 * @param timestamp_ms Current timestamp in milliseconds
 */
void button_wifi_trigger_process(bool button_pressed, uint32_t timestamp_ms);

#endif // BUTTON_WIFI_TRIGGER_H

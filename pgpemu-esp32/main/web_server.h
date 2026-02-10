/**
 * @file web_server.h
 * @brief Web Server for PGPemu configuration interface
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Start web server on port 80
 * @return ESP_OK on success
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop web server
 * @return ESP_OK on success
 */
esp_err_t web_server_stop(void);

/**
 * @brief Check if web server is running
 * @return true if running, false otherwise
 */
bool web_server_is_running(void);

#endif // WEB_SERVER_H

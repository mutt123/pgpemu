# Example CMakeLists.txt for pgpemu-esp32/main/
# This shows how to integrate the WiFi AP files

idf_component_register(
    SRCS 
        # Existing core files
        "pgpemu.c"
        "stats.c"
        "log_tags.c"
        
        # Bluetooth files
        "pgp_bluetooth.c"
        "pgp_gap.c"
        "pgp_cert.c"
        "pgp_gatts.c"
        "pgp_handshake.c"
        "pgp_handshake_multi.c"
        
        # Feature files
        "pgp_led_handler.c"
        "pgp_autobutton.c"
        "pgp_autosetting.c"
        "button_input.c"
        "setup_button.c"
        
        # Storage files
        "config_storage.c"
        "config_secrets.c"
        "settings.c"
        "nvs_helper.c"
        "secrets.c"
        
        # Communication
        "uart.c"
        
        # NEW: WiFi AP and Web Server files
        "wifi_ap_manager.c"
        "web_server.c"
        "button_wifi_trigger.c"
        
    INCLUDE_DIRS "."
    
    REQUIRES 
        bt
        driver
        esp_timer
        freertos
        mbedtls
        nvs_flash
        esp_wifi          # NEW: Required for WiFi AP
        esp_http_server   # NEW: Required for web server
        json              # NEW: Required for JSON parsing
)

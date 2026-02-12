// pgpemu.c - PATCH für Event Loop Initialisierung
// Füge dies NACH nvs_flash_init() hinzu, VOR allen anderen Inits

void app_main(void)
{
    // ========================================
    // NVS Initialisierung (existing code)
    // ========================================
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ========================================
    // !! NEU: Event Loop HIER initialisieren !!
    // ========================================
    ESP_LOGI(PGPEMU_TAG, "Initializing event loop...");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(PGPEMU_TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(PGPEMU_TAG, "Event loop already exists (OK)");
    } else {
        ESP_LOGI(PGPEMU_TAG, "Event loop created successfully");
    }
    // ========================================

    // ========================================
    // Logging & Stats (existing code)
    // ========================================
    init_log_tags();
    init_stats();

    // ========================================
    // Secrets laden (existing code)
    // ========================================
    if (!read_secrets()) {
        ESP_LOGE(PGPEMU_TAG, "Error loading secrets from NVS!");
        ESP_LOGE(PGPEMU_TAG, "See README.md to set them");
        return;
    }

    // ========================================
    // Settings initialisieren (existing code)
    // ========================================
    if (!init_global_settings()) {
        ESP_LOGE(PGPEMU_TAG, "Error loading settings from NVS!");
        ESP_LOGE(PGPEMU_TAG, "Will use defaults");
    }

    // ========================================
    // Setup Button (existing code)
    // ========================================
    if (setup_button_pressed_on_boot()) {
        global_settings_ready();
        ESP_LOGI(PGPEMU_TAG, "setup button pressed on boot; continuing startup");
    }

    // ========================================
    // WiFi AP Manager & Button WiFi Trigger
    // ========================================
    ESP_LOGI(PGPEMU_TAG, "Initializing WiFi AP Manager...");
    ret = wifi_ap_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(PGPEMU_TAG, "Failed to initialize WiFi AP Manager: %s", esp_err_to_name(ret));
        // Continue anyway - WiFi is optional
    } else {
        ESP_LOGI(PGPEMU_TAG, "WiFi AP Manager initialized");
    }

    // Note: Button WiFi Trigger nicht mehr nötig wenn button_input_FIXED.c verwendet wird
    // ESP_ERROR_CHECK(button_wifi_trigger_init());

    // ========================================
    // Rest des Codes (existing code)
    // ========================================
    init_button_input();

    if (!init_autosetting()) {
        ESP_LOGI(PGPEMU_TAG, "creating setting task failed");
        return;
    }

    if (!init_autobutton()) {
        ESP_LOGI(PGPEMU_TAG, "creating button task failed");
        return;
    }

    if (!init_bluetooth()) {
        ESP_LOGI(PGPEMU_TAG, "bluetooth init failed");
        return;
    }

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

    global_settings_ready();
}

// ========================================
// Zusammenfassung der Änderungen:
// ========================================
// 1. Event Loop wird VOR allen anderen Subsystemen initialisiert
// 2. WiFi AP Manager Init wird early im Boot-Prozess aufgerufen
// 3. Fehlerbehandlung für Event Loop (OK wenn bereits existiert)
// 4. WiFi AP Manager Fehler sind nicht-fatal (continue anyway)
//
// Diese Reihenfolge ist wichtig:
//   NVS → Event Loop → Secrets → Settings → WiFi AP → Rest

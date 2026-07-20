idf_component_register(
    SRCS "wifi_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_http_server esp_netif esp_timer esp_wifi json nvs_flash task_config
    EMBED_FILES "portal/index.html" "portal/index.js" "portal/index.css"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

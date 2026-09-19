idf_component_register(
    SRCS "wifi_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_http_server esp_netif esp_timer esp_wifi json lwip nvs_flash task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# Portal assets are embedded gzipped and served with Content-Encoding: gzip. The raw
# index.js is ~100 KB; sending that from the tight internal-RAM budget in AP mode stalls
# past the httpd send timeout, while the ~24 KB gzipped copy goes through.
idf_build_get_property(python PYTHON)
foreach(asset index.html index.js index.css)
    set(asset_src "${CMAKE_CURRENT_SOURCE_DIR}/portal/${asset}")
    set(asset_gz "${CMAKE_CURRENT_BINARY_DIR}/${asset}.gz")
    add_custom_command(
        OUTPUT "${asset_gz}"
        COMMAND "${python}" -c
            "import gzip,sys; open(sys.argv[2],'wb').write(gzip.compress(open(sys.argv[1],'rb').read(),9,mtime=0))"
            "${asset_src}" "${asset_gz}"
        DEPENDS "${asset_src}"
        VERBATIM)
    target_add_binary_data(${COMPONENT_LIB} "${asset_gz}" BINARY)
endforeach()

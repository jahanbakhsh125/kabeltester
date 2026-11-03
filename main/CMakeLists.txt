idf_component_register(
    SRCS "main.c"
    EMBED_TXTFILES "index.html"
    INCLUDE_DIRS "." "../config"
    PRIV_REQUIRES esp_driver_gpio esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_http_client
)

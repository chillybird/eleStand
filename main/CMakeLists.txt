idf_component_register(SRCS "http_api.c" "wifi.c" "serial_in.c" "cmd_queue.c" "app_main.c" "servo.c"
                       PRIV_REQUIRES driver esp_timer esp_driver_usb_serial_jtag esp_wifi esp_http_server nvs_flash esp_netif
                       INCLUDE_DIRS "")

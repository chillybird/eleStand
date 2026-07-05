idf_component_register(SRCS "app_main.c" "servo.c" "switch.c"
                       PRIV_REQUIRES driver esp_timer esp_driver_usb_serial_jtag
                       INCLUDE_DIRS "")

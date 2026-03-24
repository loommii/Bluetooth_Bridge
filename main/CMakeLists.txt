set(srcs "esp_hid_device_main.c"
         "esp_hid_gap.c"
         "usb_host_mouse.c"
         "ws2812_led.c"
         "pair_mode.c")
set(include_dirs ".")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid usb_host_hid driver esp_driver_rmt esp_driver_gpio
                       PRIV_REQUIRES nvs_flash)

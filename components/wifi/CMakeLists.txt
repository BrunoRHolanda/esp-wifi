set(component_srcs "src/wifi.c")

idf_component_register(SRCS "${component_srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS ""
                       PRIV_REQUIRES ""
                       REQUIRES "freertos" "esp_system" "esp_wifi" "esp_event" "lwip")
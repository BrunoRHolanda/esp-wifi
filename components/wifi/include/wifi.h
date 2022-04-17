#ifndef WIFI_H
#define WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "wifi station";

static EventGroupHandle_t wifi_event;

#define WIFI_CONNECTED BIT0
#define WIFI_FAIL      BIT1

static int reconect_wifi_retry_count = 0;

static void wifi_event_retry_reconect();
static void wifi_event_log_ip(ip_event_got_ip_t* event);
static void event_handler(void* arg, esp_event_base_t type, int32_t id, void* data);
void install_in_station_mode_wifi_service();
void connect_to_ap_wifi_service(wifi_sta_config_t sta_config);

#ifdef __cplusplus
}
#endif

#endif  // WIFI_H


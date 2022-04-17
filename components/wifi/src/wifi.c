#include "wifi.h"

static void wifi_event_retry_reconect()
{
	if (reconect_wifi_retry_count < 3) {
		esp_wifi_connect();

		reconect_wifi_retry_count++;

		ESP_LOGI(TAG, "Tentando se conectar ao Access Point");
	} else {
		xEventGroupSetBits(wifi_event, WIFI_FAIL);
	}

	ESP_LOGI(TAG, "Falha ao tentar se conectar ao Access Point!");
}

static void wifi_event_log_ip(ip_event_got_ip_t* event)
{
	ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
	reconect_wifi_retry_count = 0;
	xEventGroupSetBits(wifi_event, WIFI_CONNECTED);
}

static void event_handler(void* arg, esp_event_base_t type, int32_t id, void* data)
{
	if (type == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (type == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
		wifi_event_retry_reconect();
	} else if (type == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
		wifi_event_log_ip((ip_event_got_ip_t*) data);
	}
}

void install_in_station_mode_wifi_service()
{
	wifi_event = xEventGroupCreate();

	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&event_handler,
														NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&event_handler,
														NULL,
														&instance_got_ip));

	ESP_LOGI(TAG, "inicialização do wifi finalizada com sucesso!");
}

void connect_to_ap_wifi_service(wifi_sta_config_t sta_config)
{
	wifi_config_t wifi_config = {
			.sta = sta_config,
	};

		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
		ESP_ERROR_CHECK(esp_wifi_start());

		EventBits_t bits = xEventGroupWaitBits(
					wifi_event,
		            WIFI_CONNECTED | WIFI_FAIL,
		            pdFALSE,
		            pdFALSE,
		            portMAX_DELAY);

		if (bits & WIFI_CONNECTED) {
			ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
					sta_config.ssid, sta_config.password);
		} else if (bits & WIFI_FAIL) {
			ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
					sta_config.ssid, sta_config.password);
		} else {
			ESP_LOGE(TAG, "UNEXPECTED EVENT");
		}
}

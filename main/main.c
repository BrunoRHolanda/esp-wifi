#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include "wifi.h"

#define WIFI_SSID "LIG10-FIBRA_3047-4141_Maria"
#define WIFI_PASSWORD "2106ma91"

#define MAX_HTTP_OUTPUT_BUFFER 2048
#define MAX_HTTP_INPUT_BUFFER 2048

QueueHandle_t xQueue_button;

void install_nvs_flash_service()
{
	esp_err_t ret = nvs_flash_init();

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	  ESP_ERROR_CHECK(nvs_flash_erase());
	  ret = nvs_flash_init();
	}

	ESP_ERROR_CHECK(ret);
}

void install_led_service()
{
	gpio_config_t io_led_conf = {
			.intr_type = GPIO_INTR_DISABLE,
			.mode = GPIO_MODE_OUTPUT,
			.pull_down_en = 0,
			.pull_up_en = 0,
			.pin_bit_mask = (1ULL<<GPIO_NUM_2)
	};

	gpio_config(&io_led_conf);

	gpio_set_level(GPIO_NUM_2, 0);
}

void set_level_led_service(int level)
{
	gpio_set_level(GPIO_NUM_2, level);
}

void led_handler(cJSON *json)
{
	set_level_led_service(cJSON_GetObjectItem(json, "value")->valueint);
}

void install_button_service()
{
	gpio_config_t io_button_conf = {
			.intr_type = GPIO_INTR_DISABLE,
			.mode = GPIO_MODE_INPUT,
			.pull_down_en = 1,
			.pull_up_en = 0,
			.pin_bit_mask = (1ULL<<GPIO_NUM_5)
	};

	gpio_config(&io_button_conf);

	gpio_set_level(GPIO_NUM_5, 0);
}

esp_err_t  http_event_handler(esp_http_client_event_t *evt)
{
	static int output_len;       // Stores number of bytes read

	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
			ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
			break;
		case HTTP_EVENT_ON_DATA:
			ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

			if (!esp_http_client_is_chunked_response(evt->client)) {
				memcpy(evt->user_data + output_len, evt->data, evt->data_len);
				output_len += evt->data_len;
			} else {
				memcpy(evt->user_data, evt->data, evt->data_len);
			}
			break;
		case HTTP_EVENT_ON_FINISH:
			ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");

			output_len = 0;
			break;
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");

			int mbedtls_err = 0;

			esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);

			if (err != 0) {
				ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
				ESP_LOGI(TAG, "esp error: %s", esp_err_to_name(err));
				ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
				return err;
			}
			output_len = 0;
			break;
	}

	return ESP_OK;
}

esp_http_client_handle_t create_http_client(const char* url, char* response_buffer)
{
	esp_http_client_config_t config = {
		.url = url,
		.event_handler = http_event_handler,
		.user_data = response_buffer,
		.buffer_size = MAX_HTTP_OUTPUT_BUFFER,
		.buffer_size_tx = MAX_HTTP_INPUT_BUFFER,
		.disable_auto_redirect = true,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	esp_http_client_set_header(client, "Accept", "application/json");

	return client;
}

esp_err_t http_get(esp_http_client_handle_t client, const char* url)
{
	esp_http_client_set_url(client, url);
	esp_http_client_set_method(client, HTTP_METHOD_GET);

	return esp_http_client_perform(client);
}

esp_err_t http_post(esp_http_client_handle_t client, const char* url, cJSON *content)
{
	char* post_data = cJSON_Print(content);

	esp_http_client_set_url(client, url);
	esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_header(client, "Content-Type", "application/json");
	esp_http_client_set_post_field(client, post_data, strlen(post_data));


	return esp_http_client_perform(client);
}

void vTask_http_get(void * arg)
{
	char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};

	while(1) {
		esp_http_client_handle_t client = create_http_client("http://tkth.com.br/misc/cloud/api/led/status", local_response_buffer);

		esp_err_t err = http_get(client, "http://tkth.com.br/misc/cloud/api/led/status");

		 if (err == ESP_OK) {
			ESP_LOGI(
					TAG,
					"HTTP Status = %d, content_length = %d",
					esp_http_client_get_status_code(client),
					esp_http_client_get_content_length(client)
			);
		} else {
			ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
		}

		printf("response: %s\n", local_response_buffer);

		cJSON *json_response = cJSON_Parse(local_response_buffer);

		led_handler(json_response);

		free(json_response);

		esp_http_client_cleanup(client);

		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}
}

void vTask_http_post(void * arg)
{
	char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
	unsigned int btn_level = 0;

	while(1) {
		if (xQueueReceive(xQueue_button, &btn_level, portMAX_DELAY)) {
			esp_http_client_handle_t client = create_http_client(
					"http://tkth.com.br/misc/cloud/api/led/status",
					local_response_buffer
			);

			cJSON *json;
			json = cJSON_CreateObject();
			cJSON_AddNumberToObject(json, "value", !btn_level);//{ "value": 0 } -

			esp_err_t err = http_post(client, "http://www.tkth.com.br/misc/cloud/api/button", json);

			 if (err == ESP_OK) {
				ESP_LOGI(
						TAG,
						"HTTP Status = %d, content_length = %d",
						esp_http_client_get_status_code(client),
						esp_http_client_get_content_length(client)
				);
			} else {
				ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
			}

			printf("response: %s\n", local_response_buffer);

			cJSON_Delete(json);
			esp_http_client_cleanup(client);

			btn_level = 0;
		}
	}
}

void kernel(void)
{
	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

	wifi_sta_config_t sta_config = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASSWORD,
			.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
	};

	connect_to_ap_wifi_service(sta_config);

	ESP_LOGI(TAG, "WIFI_CONNECTED");

	xQueue_button = xQueueCreate(10,sizeof(unsigned int));

	xTaskCreate(vTask_http_get, "vTask_http_get", 8192, NULL, 5, NULL);
	xTaskCreate(vTask_http_post, "vTask_http_post", 8192, NULL, 5, NULL);

	while(1) {
		unsigned int btn_level = gpio_get_level(GPIO_NUM_5);

		xQueueSend(xQueue_button, (void *)&btn_level, portMAX_DELAY);

		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}

void boot(void)
{
	install_nvs_flash_service();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	install_in_station_mode_wifi_service();
	install_led_service();
	install_button_service();

	kernel();
}

void app_main(void)
{
	boot();
}

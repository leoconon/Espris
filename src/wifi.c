#include <stdio.h>
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <wifi.h>
#include "esp_http_client.h"
#include <utils.h>

#define WIFI_USER CONFIG_EXAMPLE_WIFI_SSID
#define WIFI_PASS CONFIG_EXAMPLE_WIFI_PASSWORD
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define USERNAME CONFIG_ESPRIS_USERNAME

static const char *TAG = "wifi";
EventGroupHandle_t wifiEventGroup;
char wifiIp[20];

void eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/**
 * Inicializa a Non-volatile storage
 * 
 * Este espaço de memória reservado armazena dados necessários para a calibração do PHY.
 * Devido ao fato de o ESP não possuir EEPROM é necessário separar um pedaço da memória 
 * de programa para armazenar dados não voláteis
 */
void initNvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/**
 * Função que faz a configuração de wifi e inicializa os eventos relacionados
 */
void initWifi() {
    wifiEventGroup = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_USER,
            .password = WIFI_PASS,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/**
 * Event handler dos eventos de wifi
 */
void eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Tentando se conectar ao wifi...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wifi desconectado!");
        xEventGroupSetBits(wifiEventGroup, WIFI_FAIL_BIT);
        xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        sprintf(wifiIp, IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Conectado! O IP atribuido é: %s", wifiIp);
        xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(wifiEventGroup, WIFI_FAIL_BIT);
    } else {
        ESP_LOGI(TAG, "Erro de wifi");
        xEventGroupSetBits(wifiEventGroup, WIFI_FAIL_BIT);
        xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT);
    }
}

void sendGamingScoreTask(void *pvParameters) {
    bufferGamingScore = xQueueCreate(1, sizeof(int));
    int actualScore = 0;
    char url[100];
    loop {
        xQueueReceive(bufferGamingScore, &actualScore, 50000);
        if (actualScore > 0) {
            sprintf(url, "http://espris.herokuapp.com/gaming/%s/%i", USERNAME, actualScore);
            esp_http_client_config_t config = {
                .url = url
            };
            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_err_t err = esp_http_client_perform(client);
            esp_http_client_cleanup(client);
            actualScore = 0;
        }
    }
}
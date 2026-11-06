// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Example 60: an ESP32-C3 joins the Wi-Fi network "SimNodus" (WPA2, password
// "simnodus-wifi") and serves a web page that shows its LED and its chip
// temperature and switches the LED on GPIO8 (ESP-IDF v5.5.2).
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define LED_GPIO 8
#define WIFI_SSID "SimNodus"
#define WIFI_PASSWORD "simnodus-wifi"

static temperature_sensor_handle_t sensor;
static int led;

static esp_err_t page(httpd_req_t* request)
{
    char query[32];
    char value[4];
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK
        && httpd_query_key_value(query, "led", value, sizeof(value)) == ESP_OK) {
        led = value[0] == '1';
        gpio_set_level(LED_GPIO, led);
        printf("web: LED %s\n", led ? "on" : "off");
    }
    float celsius = 0;
    temperature_sensor_get_celsius(sensor, &celsius);
    char html[512];
    snprintf(html, sizeof(html),
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>ESP32-C3</title></head><body>"
        "<h1>ESP32-C3 in SimNodus</h1>"
        "<p>LED: <b>%s</b></p>"
        "<p>Chip temperature: %.1f &deg;C</p>"
        "<p><a href=\"/?led=1\">LED on</a> | <a href=\"/?led=0\">LED off</a> | <a href=\"/\">Refresh</a></p>"
        "</body></html>",
        led ? "on" : "off", celsius);
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_send(request, html, HTTPD_RESP_USE_STRLEN);
}

static void on_event(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("wifi: not connected, trying again\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t* got = (const ip_event_got_ip_t*)data;
        printf("web server: http://" IPSTR "/\n", IP2STR(&got->ip_info.ip));
    }
}

void app_main(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
    const temperature_sensor_config_t sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    ESP_ERROR_CHECK(temperature_sensor_install(&sensor_config, &sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(sensor));

    esp_err_t status = nvs_flash_init();
    if (status == ESP_ERR_NVS_NO_FREE_PAGES || status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        status = nvs_flash_init();
    }
    ESP_ERROR_CHECK(status);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    wifi_config_t config = {0};
    strcpy((char*)config.sta.ssid, WIFI_SSID);
    strcpy((char*)config.sta.password, WIFI_PASSWORD);
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    printf("wifi: joining %s\n", WIFI_SSID);

    httpd_handle_t server = NULL;
    const httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &server_config));
    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = page, .user_ctx = NULL};
    httpd_register_uri_handler(server, &root);
}

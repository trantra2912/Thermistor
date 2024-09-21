#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/i2c.h"
#include <math.h>
#include "../components/lcd_i2c.h"
#include "../components/DS_1307.h"

#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include "unistd.h"
#include <time.h>

static const char *TAG_MQTT = "MQTT_EXAMPLE";
#define EXAMPLE_ESP_WIFI_SSID "Realme3proTQT"
#define EXAMPLE_ESP_WIFI_PASS "123456789"
#define MAX_RETRY 10
static int retry_cnt = 0;

#define THINGSPEAK_API_KEY "YOUR_THINGSPEAK_API_KEY"
#define THINGSPEAK_URI "mqtt://mqtt3.thingspeak.com:1883"
uint32_t MQTT_CONNECTED = 0;
static void mqtt_app_start(void);

#define Den1 27
#define Den2 26

float V0 = 4095, R1 = 5000, R0 = 10000, B = 3950, T0 = 298.15;
float V1, R2, r, T;

static esp_adc_cal_characteristics_t adc1_chars;
static const char *TAG = "i2c-simple-example";
char buffer0[15];
char buffer1[15];

QueueHandle_t queue;
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_NUM_0;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

static esp_err_t wifi_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG_MQTT, "Trying to connect with Wi-Fi\n");
        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG_MQTT, "Wi-Fi connected\n");
        break;

    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG_MQTT, "got ip: starting MQTT Client\n");
        mqtt_app_start();
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "disconnected: Retrying Wi-Fi\n");
        if (retry_cnt++ < MAX_RETRY)
        {
            esp_wifi_connect();
        }
        else
            ESP_LOGI(TAG_MQTT, "Max Retry Failed: Wi-Fi Connection\n");
        break;

    default:
        break;
    }
    return ESP_OK;
}

void wifi_init(void)
{
    esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_netif_init();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

esp_mqtt_client_handle_t client;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
        MQTT_CONNECTED = 1;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        MQTT_CONNECTED = 0;
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG_MQTT, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    ESP_LOGI(TAG_MQTT, "STARTING MQTT");
    esp_mqtt_client_config_t mqttConfig = {
        .uri = THINGSPEAK_URI,
        .username = "ICwiKBYCMgUSFjMiHRgQPSE",
        .client_id = "ICwiKBYCMgUSFjMiHRgQPSE",
        .password = "enT4M0pNIfj6Dsu23yt5Cf7+",
    };

    client = esp_mqtt_client_init(&mqttConfig);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

int getMedianNum(int bArray[], int iFilterLen, adc1_channel_t adc_chan)
{
    int adc = 0;
    for (int i = 0; i < iFilterLen; i++)
    {
        bArray[i] = adc1_get_raw(adc_chan);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    for (int i = 0; i < iFilterLen - 1; i++)
    {
        for (int j = 0; j < iFilterLen - i - 1; j++)
        {
            if (bArray[j] > bArray[j + 1])
            {
                int bTemp = bArray[j];
                bArray[j] = bArray[j + 1];
                bArray[j + 1] = bTemp;
            }
        }
    }
    for (int i = 0; i < iFilterLen; i++)
    {
        adc += bArray[i];
    }
    adc /= iFilterLen;
    return adc;
}

void Do(void *arg)
{
#define SCOUNT 50
    int analogBuffer1[SCOUNT];
    int a;
    int count = 0;
    char temperature[10];
    r = R0 * exp(-B / T0);
    while (1)
    {
        V1 = getMedianNum(analogBuffer1, SCOUNT, ADC1_CHANNEL_4);
        R2 = R1 * (V0 / V1 - 1);
        T = B / log(R2 / r) - 273.15;
        sprintf(buffer1, "T=%.2f", T);
        lcd_put_cur(1, 0);
        lcd_send_string(buffer1);
        count++;
        if (count == 4)
        {
            if (MQTT_CONNECTED)
            {
                count = 0;
                sprintf(temperature, "%.2f", T);
                esp_mqtt_client_publish(client, "channels/2154705/publish/fields/field1", temperature, 0, 0, 0);
                printf("publish thanh cong\n ");
            }
            else
            {
                ESP_LOGE(TAG, "MQTT Not connected");
            }
        }
        if (T < 20)
        {
            a = 1;
            xQueueSend(queue, &a, (TickType_t)0);
            xQueueSend(queue, &a, (TickType_t)0); // ghi dữ liệu vào hàng đợi
        }
        else if (T >= 20 && T <= 30)
        {
            a = 2;
            xQueueSend(queue, &a, (TickType_t)0);
            xQueueSend(queue, &a, (TickType_t)0);
        }
        else
        {
            a = 3;
            xQueueSend(queue, &a, (TickType_t)0);
            xQueueSend(queue, &a, (TickType_t)0);
        }
    }
}

void Nhay1(void *arg)
{
    int b; // tạo kí tự để lưu trữ dữ liệu sẽ nhận từ hàng đợi
    while (1)
    {
        xQueueReceive(queue, &b, (TickType_t)5000); // đọc dữ liệu từ hàng đợi
        if (b == 1)
        {
            printf("Received data from queue == %d, ", b);
            for (int i = 0; i < 5; i++)
            {
                gpio_set_level(Den1, 1);
                vTaskDelay(500 / portTICK_RATE_MS);
                gpio_set_level(Den1, 0);
                vTaskDelay(500 / portTICK_RATE_MS);
            }
        }
        else if (b == 2)
        {
            printf("Received data from queue == %d, ", b);
            for (int i = 0; i < 5; i++)
            {
                gpio_set_level(Den1, 1);
                vTaskDelay(500 / portTICK_RATE_MS);
                gpio_set_level(Den1, 0);
                vTaskDelay(500 / portTICK_RATE_MS);
            }
        }
        else if (b == 3)
        {
            printf("Received data from queue == %d, ", b);
            gpio_set_level(Den1, 0);
            vTaskDelay(5000 / portTICK_RATE_MS);
        }
    }
}

void Nhay2(void *arg)
{
    int c; // tạo kí tự để lưu trữ dữ liệu sẽ nhận từ hàng đợi
    while (1)
    {
        xQueueReceive(queue, &c, (TickType_t)1000); // đọc dữ liệu từ hàng đợi
        if (c == 1)
        {
            printf("T1= %f, V1=%f\n", T, V1);
            gpio_set_level(Den2, 0);
            vTaskDelay(5000 / portTICK_RATE_MS);
        }
        else if (c == 2)
        {
            printf("T2= %f, V1=%f\n", T, V1);
            for (int i = 0; i < 10; i++)
            {
                gpio_set_level(Den2, 1);
                vTaskDelay(250 / portTICK_RATE_MS);
                gpio_set_level(Den2, 0);
                vTaskDelay(250 / portTICK_RATE_MS);
            }
        }
        else if (c == 3)
        {
            printf("T3= %f, V1=%f\n", T, V1);
            for (int i = 0; i < 20; i++)
            {
                gpio_set_level(Den2, 1);
                vTaskDelay(125 / portTICK_RATE_MS);
                gpio_set_level(Den2, 0);
                vTaskDelay(125 / portTICK_RATE_MS);
            }
        }
    }
}

void app_main(void)
{
    gpio_pad_select_gpio(Den1); // đặt cấu hình Den1 làm GPIO
    gpio_pad_select_gpio(Den2);
    gpio_set_direction(Den1, GPIO_MODE_OUTPUT); // đặt Den1 làm đầu ra
    gpio_set_direction(Den2, GPIO_MODE_OUTPUT);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars); // hiệu chỉnh ADC1 ở mức suy giảm 11db

    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);                   // đặt cấu hình ADC1 ở độ rộng bit mặc định (12bit)
    adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11); // đặt tham số suy giảm của ADC1 kênh 4 là GPIO32 thành 11db

    queue = xQueueCreate(5, sizeof(int)); // tạo hàng đợi

    xTaskCreate(Do, "Do", 2048, NULL, 10, NULL);
    xTaskCreatePinnedToCore(Nhay1, "Nhay1", 2048, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(Nhay2, "Nhay2", 2048, NULL, 10, NULL, 0);
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init();

    struct tm time = {
        .tm_year = 123,
        .tm_mon = 9,
        .tm_mday = 7,
        .tm_wday = 06,
        .tm_hour = 11,
        .tm_min = 30,
        .tm_sec = 30,
    };
    ds1307_set_time(&time);
    lcd_init();
    lcd_clear();
    while (1)
    {
        ds1307_get_time(&time);
        const char *day_of_week = get_day_of_week(&time);
        const char *time_now = get_time_string(&time);
        printf("%02d:%02d:%02d %s %02d/%02d/%04d \n", time.tm_hour, time.tm_min, time.tm_sec, day_of_week,
               time.tm_mday, time.tm_mon + 1, time.tm_year + 1900);
        sprintf(buffer1, "T=%.2f", T);
        lcd_put_cur(1, 0);
        lcd_send_string(buffer1);
        sprintf(buffer0, "%s", time_now);
        lcd_put_cur(0, 0);
        lcd_send_string(buffer0);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}
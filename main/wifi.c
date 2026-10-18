#include "wifi.h"

#include <string.h>
#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi";
static EventGroupHandle_t s_events;
#define WIFI_CONNECTED_BIT BIT0

#define SETUP_AP_CHANNEL      1
#define SETUP_AP_MAX_CLIENTS  2

static bool s_sta_wanted;
static volatile int64_t s_offline_since_ms;  // 0 while connected

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (xEventGroupGetBits(s_events) & WIFI_CONNECTED_BIT) s_offline_since_ms = now_ms();
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        if (s_sta_wanted) esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *ev = data;
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&ev->ip_info.ip));
        s_offline_since_ms = 0;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void)
{
    s_events = xEventGroupCreate();
    s_offline_since_ms = now_ms();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(sta, BOARD_HOSTNAME);
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    // Always-on display: favour latency over power.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

void wifi_connect(const char *ssid, const char *password)
{
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password));
    s_sta_wanted = false;
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    s_sta_wanted = true;
    ESP_LOGI(TAG, "joining '%s'", ssid);
    esp_wifi_connect();
}

void wifi_disconnect(void)
{
    s_sta_wanted = false;
    esp_wifi_disconnect();
}

bool wifi_wait_connected(uint32_t timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(s_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                                           timeout_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    return bits & WIFI_CONNECTED_BIT;
}

bool wifi_is_connected(void)
{
    return xEventGroupGetBits(s_events) & WIFI_CONNECTED_BIT;
}

int64_t wifi_offline_ms(void)
{
    int64_t since = s_offline_since_ms;
    return since ? now_ms() - since : 0;
}

void wifi_start_ap(const char *ssid)
{
    wifi_config_t ap = {
        .ap = {
            .channel = SETUP_AP_CHANNEL,
            .max_connection = SETUP_AP_MAX_CLIENTS,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    strlcpy((char *)ap.ap.ssid, ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_LOGI(TAG, "setup network '%s' open", ssid);
}

int wifi_scan(wifi_ap_record_t *records, int max)
{
    wifi_scan_config_t scan = {.show_hidden = false};
    if (esp_wifi_scan_start(&scan, true) != ESP_OK) return 0;
    uint16_t n = max;
    if (esp_wifi_scan_get_ap_records(&n, records) != ESP_OK) return 0;
    return n;
}

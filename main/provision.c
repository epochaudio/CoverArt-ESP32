#include "provision.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "store.h"
#include "ui.h"
#include "wifi.h"

static const char *TAG = "setup";

#define SETUP_AP_PREFIX          "CoverArt-"
#define SETUP_AP_ADDR            "192.168.4.1"   // ESP-IDF default soft-AP address
#define DNS_PORT                 53
#define DNS_TTL_SEC              60
#define DNS_TASK_STACK           3072
#define JOIN_TIMEOUT_MS          20000  // per attempt to join the entered network
#define SAVED_RETRY_INTERVAL_MS  120000 // how often the previously saved network is retried
#define SAVED_RETRY_MS           15000
#define RESTART_DELAY_MS         1500
#define MAX_SCAN_RESULTS         20
#define PAGE_BYTES               8192

typedef struct {
    char ssid[33];
    char password[65];
} join_request_t;

typedef enum { RESULT_NONE, RESULT_FAILED } last_result_t;

static QueueHandle_t s_requests;
static SemaphoreHandle_t s_scan_lock;
static wifi_ap_record_t s_scan[MAX_SCAN_RESULTS];
static int s_scan_count;
static volatile last_result_t s_last_result;
static char s_last_ssid[33];
static char s_ap_ssid[32];

// ---------------------------------------------------------------- captive DNS

// Answers every A query with the setup address, so whatever a phone looks up
// to detect captive portals lands on our page.
static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(DNS_PORT), .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (sock < 0 || bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "dns socket failed");
        vTaskDelete(NULL);
    }
    const uint32_t answer_ip = inet_addr(SETUP_AP_ADDR);
    uint8_t buf[512];
    for (;;) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf) - 16, 0, (struct sockaddr *)&from, &from_len);
        if (n < 12) continue;
        // Header: id(2) flags(2) qdcount(2) ancount(2) nscount(2) arcount(2), then one question.
        int q = 12;
        while (q < n && buf[q] != 0) q += buf[q] + 1;
        if (q + 5 > n) continue;
        uint16_t qtype = (buf[q + 1] << 8) | buf[q + 2];
        int end = q + 5;
        buf[2] = 0x81;  // response, recursion desired
        buf[3] = 0x80;  // recursion available, no error
        buf[6] = 0;
        buf[7] = qtype == 1 ? 1 : 0;  // one answer for A queries only
        buf[8] = buf[9] = buf[10] = buf[11] = 0;
        if (qtype == 1) {
            const uint8_t answer[] = {
                0xC0, 0x0C,                       // name: pointer to the question
                0x00, 0x01, 0x00, 0x01,           // type A, class IN
                0, 0, 0, DNS_TTL_SEC,             // TTL
                0x00, 0x04,                       // address length
            };
            memcpy(buf + end, answer, sizeof(answer));
            memcpy(buf + end + sizeof(answer), &answer_ip, 4);
            end += sizeof(answer) + 4;
        }
        sendto(sock, buf, end, 0, (struct sockaddr *)&from, from_len);
    }
}

// ---------------------------------------------------------------- page

static void rescan(void)
{
    xSemaphoreTake(s_scan_lock, portMAX_DELAY);
    s_scan_count = wifi_scan(s_scan, MAX_SCAN_RESULTS);
    xSemaphoreGive(s_scan_lock);
    ESP_LOGI(TAG, "%d networks found", s_scan_count);
}

static size_t append(char *buf, size_t at, const char *text)
{
    size_t n = strlen(text);
    if (at + n >= PAGE_BYTES) return at;
    memcpy(buf + at, text, n + 1);
    return at + n;
}

static size_t append_escaped(char *buf, size_t at, const char *text)
{
    char one[2] = {0};
    for (; *text; text++) {
        switch (*text) {
        case '&': at = append(buf, at, "&amp;"); break;
        case '<': at = append(buf, at, "&lt;"); break;
        case '>': at = append(buf, at, "&gt;"); break;
        case '"': at = append(buf, at, "&quot;"); break;
        default: one[0] = *text; at = append(buf, at, one); break;
        }
    }
    return at;
}

static const char PAGE_HEAD[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>CoverArt Wi-Fi</title><style>"
    "body{font-family:-apple-system,system-ui,sans-serif;max-width:420px;margin:24px auto;padding:0 16px;"
    "background:#111;color:#eee}h1{font-size:22px}p{color:#aaa;line-height:1.5}"
    "label{display:block;margin:16px 0 6px}select,input{width:100%;box-sizing:border-box;padding:12px;"
    "font-size:16px;border-radius:8px;border:1px solid #444;background:#222;color:#eee}"
    "button{margin-top:20px;width:100%;padding:14px;font-size:17px;border:0;border-radius:8px;"
    "background:#4a8;color:#fff}.err{color:#f88}a{color:#8cf}</style></head><body>";

static char *render_form(void)
{
    char *page = malloc(PAGE_BYTES);
    if (!page) return NULL;
    size_t at = append(page, 0, PAGE_HEAD);
    at = append(page, at, "<h1>连接 Wi-Fi<br>Connect to Wi-Fi</h1>");
    if (s_last_result == RESULT_FAILED) {
        at = append(page, at, "<p class=err>无法连接“");
        at = append_escaped(page, at, s_last_ssid);
        at = append(page, at, "”，请检查密码后重试。<br>Could not join this network. Check the password and try again.</p>");
    }
    at = append(page, at,
                "<p>仅支持 2.4 GHz 网络。<br>Only 2.4 GHz networks are supported.</p>"
                "<form method=post action=/connect><label>网络 Network</label><select name=ssid>");
    xSemaphoreTake(s_scan_lock, portMAX_DELAY);
    for (int i = 0; i < s_scan_count; i++) {
        const char *ssid = (const char *)s_scan[i].ssid;
        bool duplicate = ssid[0] == '\0';
        for (int j = 0; j < i && !duplicate; j++) duplicate = strcmp(ssid, (const char *)s_scan[j].ssid) == 0;
        if (duplicate) continue;
        at = append(page, at, "<option value=\"");
        at = append_escaped(page, at, ssid);
        at = append(page, at, "\">");
        at = append_escaped(page, at, ssid);
        at = append(page, at, "</option>");
    }
    xSemaphoreGive(s_scan_lock);
    at = append(page, at,
                "</select><label>或手动输入网络名称 Or type the network name</label>"
                "<input name=manual autocomplete=off autocapitalize=none>"
                "<label>密码 Password</label><input name=password type=password autocomplete=off>"
                "<button>连接 Connect</button></form>"
                "<p><a href=/rescan>重新扫描 Rescan</a></p></body></html>");
    return page;
}

static esp_err_t send_page(httpd_req_t *req, char *page)
{
    if (!page) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    free(page);
    return err;
}

static void url_decode(char *s)
{
    char *out = s;
    for (; *s; s++) {
        if (*s == '+') {
            *out++ = ' ';
        } else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char hex[3] = {s[1], s[2], 0};
            *out++ = (char)strtol(hex, NULL, 16);
            s += 2;
        } else {
            *out++ = *s;
        }
    }
    *out = '\0';
}

static esp_err_t on_connect(httpd_req_t *req)
{
    char body[256];
    int n = req->content_len < sizeof(body) - 1 ? req->content_len : sizeof(body) - 1;
    int got = httpd_req_recv(req, body, n);
    if (got <= 0) return httpd_resp_send_500(req);
    body[got] = '\0';

    join_request_t jr = {0};
    char manual[sizeof(jr.ssid) * 3] = "", ssid[sizeof(jr.ssid) * 3] = "", password[sizeof(jr.password) * 3] = "";
    httpd_query_key_value(body, "manual", manual, sizeof(manual));
    httpd_query_key_value(body, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(body, "password", password, sizeof(password));
    url_decode(manual);
    url_decode(ssid);
    url_decode(password);
    strlcpy(jr.ssid, manual[0] ? manual : ssid, sizeof(jr.ssid));
    strlcpy(jr.password, password, sizeof(jr.password));
    if (jr.ssid[0] == '\0') {
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        return httpd_resp_send(req, NULL, 0);
    }
    xQueueOverwrite(s_requests, &jr);

    char *page = malloc(PAGE_BYTES);
    if (!page) return httpd_resp_send_500(req);
    size_t at = append(page, 0, PAGE_HEAD);
    at = append(page, at, "<h1>正在连接<br>Connecting</h1><p>“");
    at = append_escaped(page, at, jr.ssid);
    append(page, at,
           "”</p><p>请留意设备屏幕。连接成功后设备会自动重启，此设置网络随之关闭，手机会回到原来的网络。"
           "若屏幕重新显示二维码，说明连接失败，请重新扫码再试。</p>"
           "<p>Watch the device screen. On success the device restarts and this setup network closes. "
           "If the QR code shows again, joining failed: scan it and try again.</p></body></html>");
    return send_page(req, page);
}

static esp_err_t on_rescan(httpd_req_t *req)
{
    rescan();
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

// Serves the form at "/" and redirects every other path there; the redirect
// is what makes iOS and Android open the captive-portal sheet.
static esp_err_t on_any_get(httpd_req_t *req)
{
    if (strcmp(req->uri, "/") == 0) return send_page(req, render_form());
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" SETUP_AP_ADDR "/");
    return httpd_resp_send(req, NULL, 0);
}

static void start_http(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.lru_purge_enable = true;
    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));
    const httpd_uri_t routes[] = {
        {.uri = "/connect", .method = HTTP_POST, .handler = on_connect},
        {.uri = "/rescan", .method = HTTP_GET, .handler = on_rescan},
        {.uri = "/*", .method = HTTP_GET, .handler = on_any_get},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) httpd_register_uri_handler(server, &routes[i]);
}

// ---------------------------------------------------------------- flow

static void restart_joined(const char *ssid)
{
    const char *lines[] = {"WI-FI OK", "Restarting"};
    ui_message(lines, 2);
    ESP_LOGI(TAG, "joined '%s', restarting", ssid);
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
    esp_restart();
}

void provision_run(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), SETUP_AP_PREFIX "%02X%02X%02X", mac[3], mac[4], mac[5]);
    s_requests = xQueueCreate(1, sizeof(join_request_t));
    s_scan_lock = xSemaphoreCreateMutex();

    char saved_ssid[33], saved_password[65];
    const bool have_saved = store_get_wifi(saved_ssid, sizeof(saved_ssid), saved_password, sizeof(saved_password));
    wifi_disconnect();
    wifi_start_ap(s_ap_ssid);
    rescan();
    xTaskCreate(dns_task, "dns", DNS_TASK_STACK, NULL, 4, NULL);
    start_http();
    ui_setup(s_ap_ssid);

    int64_t next_saved_retry = esp_timer_get_time() / 1000 + SAVED_RETRY_INTERVAL_MS;
    for (;;) {
        join_request_t jr;
        if (xQueueReceive(s_requests, &jr, pdMS_TO_TICKS(1000))) {
            const char *lines[] = {"CONNECTING", jr.ssid};
            ui_message(lines, 2);
            wifi_connect(jr.ssid, jr.password);
            if (wifi_wait_connected(JOIN_TIMEOUT_MS)) {
                store_set_wifi(jr.ssid, jr.password);
                restart_joined(jr.ssid);
            }
            wifi_disconnect();
            strlcpy(s_last_ssid, jr.ssid, sizeof(s_last_ssid));
            s_last_result = RESULT_FAILED;
            ESP_LOGW(TAG, "could not join '%s'", jr.ssid);
            ui_setup(s_ap_ssid);
        } else if (have_saved && esp_timer_get_time() / 1000 >= next_saved_retry) {
            wifi_connect(saved_ssid, saved_password);
            if (wifi_wait_connected(SAVED_RETRY_MS)) restart_joined(saved_ssid);
            wifi_disconnect();
            next_saved_retry = esp_timer_get_time() / 1000 + SAVED_RETRY_INTERVAL_MS;
        }
    }
}

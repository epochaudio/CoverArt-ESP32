#include "store.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "store";
static const char *NAMESPACE = "coverart";
static bool s_ready;

void store_init(void)
{
    // Never erase on failure: the partition may also hold another firmware's
    // settings. Without NVS the device still runs, it just cannot remember.
    esp_err_t err = nvs_flash_init();
    s_ready = err == ESP_OK;
    if (!s_ready) ESP_LOGE(TAG, "nvs unavailable (%s), settings will not persist", esp_err_to_name(err));
}

static bool open_ns(nvs_open_mode_t mode, nvs_handle_t *h)
{
    return s_ready && nvs_open(NAMESPACE, mode, h) == ESP_OK;
}

static bool get_str(nvs_handle_t h, const char *key, char *out, size_t len)
{
    size_t n = len;
    return nvs_get_str(h, key, out, &n) == ESP_OK;
}

static void commit_close(nvs_handle_t h)
{
    if (nvs_commit(h) != ESP_OK) ESP_LOGE(TAG, "commit failed");
    nvs_close(h);
}

static void set_pair(const char *k1, const char *v1, const char *k2, const char *v2)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READWRITE, &h)) return;
    nvs_set_str(h, k1, v1);
    nvs_set_str(h, k2, v2);
    commit_close(h);
}

bool store_get_wifi(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READONLY, &h)) return false;
    bool ok = get_str(h, "wifi_ssid", ssid, ssid_len) && get_str(h, "wifi_pass", password, password_len);
    nvs_close(h);
    return ok && ssid[0] != '\0';
}

void store_set_wifi(const char *ssid, const char *password)
{
    set_pair("wifi_ssid", ssid, "wifi_pass", password);
}

bool store_get_token(const char *core_id, char *token, size_t len)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READONLY, &h)) return false;
    char saved_core[64];
    bool ok = get_str(h, "core_id", saved_core, sizeof(saved_core)) &&
              strcmp(saved_core, core_id) == 0 && get_str(h, "token", token, len);
    nvs_close(h);
    return ok;
}

void store_set_pairing(const char *core_id, const char *token)
{
    set_pair("core_id", core_id, "token", token);
}

bool store_get_core_address(char *host, size_t host_len, uint16_t *port)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READONLY, &h)) return false;
    bool ok = get_str(h, "core_host", host, host_len) && nvs_get_u16(h, "core_port", port) == ESP_OK;
    nvs_close(h);
    return ok;
}

void store_set_core_address(const char *host, uint16_t port)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READWRITE, &h)) return;
    nvs_set_str(h, "core_host", host);
    nvs_set_u16(h, "core_port", port);
    commit_close(h);
}

void store_clear_core_address(void)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READWRITE, &h)) return;
    nvs_erase_key(h, "core_host");
    nvs_erase_key(h, "core_port");
    commit_close(h);
}

bool store_get_output(char *output_id, size_t id_len, char *name, size_t name_len)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READONLY, &h)) return false;
    bool ok = get_str(h, "output_id", output_id, id_len);
    if (ok && !get_str(h, "output_name", name, name_len)) name[0] = '\0';
    nvs_close(h);
    return ok;
}

void store_set_output(const char *output_id, const char *name)
{
    set_pair("output_id", output_id, "output_name", name);
}

int store_bump_power_cycles(void)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READWRITE, &h)) return 0;
    uint8_t n = 0;
    nvs_get_u8(h, "power_cycles", &n);
    n++;
    nvs_set_u8(h, "power_cycles", n);
    commit_close(h);
    return n;
}

void store_clear_power_cycles(void)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READWRITE, &h)) return;
    uint8_t n = 0;
    if (nvs_get_u8(h, "power_cycles", &n) == ESP_OK && n != 0) {  // write only when needed
        nvs_set_u8(h, "power_cycles", 0);
        nvs_commit(h);
    }
    nvs_close(h);
}

void store_erase_all(void)
{
    nvs_handle_t h;
    if (!open_ns(NVS_READWRITE, &h)) return;
    nvs_erase_all(h);
    commit_close(h);
    ESP_LOGW(TAG, "all device settings erased");
}

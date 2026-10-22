#include "roon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "config.h"
#include "cover.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "json_scan.h"
#include "moo.h"
#include "sood.h"
#include "screen_guard.h"
#include "store.h"
#include "ui.h"
#include "wifi.h"

static const char *TAG = "roon";

#define SVC_REGISTRY   "com.roonlabs.registry:1"
#define SVC_TRANSPORT  "com.roonlabs.transport:2"
#define SVC_IMAGE      "com.roonlabs.image:1"
#define SVC_SETTINGS   "com.roonlabs.settings:1"
#define SVC_PING       "com.roonlabs.ping:1"
#define SVC_STATUS     "com.roonlabs.status:2"
#define SETTING_ZONE   "zone"

#define WS_OP_CONTINUATION 0x0
#define WS_OP_TEXT         0x1
#define WS_OP_BINARY       0x2

#define EVENT_QUEUE_DEPTH  16
#define EVENT_WAIT_MS      1000
#define HEAP_REPORT_MS     60000
#define ROON_TASK_STACK    8192
#define WS_TASK_STACK      6144
// Knob input older than this is dropped instead of replayed late (e.g. after
// a reconnect); also the longest wait for Core to confirm a volume change.
#define CONTROL_EXPIRY_MS  1000
#define VOLUME_REPLY_TIMEOUT_MS 3000
// A cover that has not fully arrived by then means a stalled link (seen on
// weak 2.4 GHz): the session is dropped and the reconnect fetches it again.
#define IMAGE_REPLY_TIMEOUT_MS 10000
// A paired device re-registers in well under a second; only if Core stays
// silent this long (e.g. the extension was disabled in Roon) is the user asked
// to enable it. Unpaired devices ask right away.
#define AUTH_PROMPT_DELAY_MS   5000
#define VOLUME_MAX_PENDING_STEPS 50

// Shown only until the first cover of this boot; afterwards the last cover
// stays up and state changes go to the log.
typedef enum { UI_FINDING_CORE, UI_CORE_NOT_FOUND, UI_AWAITING_AUTH, UI_SELECT_ZONE, UI_NOTHING_PLAYING } ui_status_t;

typedef enum { EV_CONNECTED, EV_DISCONNECTED, EV_MESSAGE, EV_CONTROL, EV_VOLUME } event_type_t;
typedef struct {
    event_type_t type;
    uint8_t *data;   // EV_MESSAGE
    size_t len;
    int value;       // EV_CONTROL: roon_control_t, EV_VOLUME: steps
    int64_t at_ms;   // when the input happened
} event_t;

// Per-connection protocol state; reset on every new WebSocket session.
typedef struct {
    uint32_t next_request_id;
    uint32_t info_id, register_id, zones_id, image_id;
    int64_t register_sent_ms;        // 0 once registered or before registering
    bool zone_seen;                  // the selected output was in the last snapshot
    bool registered;
    char core_id[64];
    char settings_subscription[24];  // Core's subscribe_settings Request-Id
    char status_subscription[24];    // Core's subscribe_status Request-Id
    char image_key[128];             // cover currently shown or requested
    uint32_t zones_subscription;     // key of the live subscribe_zones
    bool volume_supported;           // selected output has a volume control
    uint32_t volume_id;              // in-flight change_volume, 0 if none
    int64_t volume_sent_ms;
    int pending_volume_steps;        // turns not yet sent, coalesced
    int64_t pending_volume_ms;       // time of the latest coalesced turn
    int64_t image_requested_ms;      // 0 when no image request is outstanding
    bool abort;                      // drop this session after the current message
} session_t;

static QueueHandle_t s_events;
static esp_websocket_client_handle_t s_ws;
static session_t s;
static char s_output_id[96], s_output_name[96];
static bool s_cover_shown;
static char s_extension_id[64], s_display_name[64];
// Address of the current session and whether it came from the NVS cache
// rather than a fresh SOOD answer.
static char s_core_host[16];
static uint16_t s_core_port;
static bool s_core_from_cache;

// ---------------------------------------------------------------- helpers

static void show_status(ui_status_t st)
{
    static const char *names[] = {
        [UI_FINDING_CORE] = "finding Roon Core",
        [UI_CORE_NOT_FOUND] = "Roon Core not found",
        [UI_AWAITING_AUTH] = "awaiting authorization in Roon",
        [UI_SELECT_ZONE] = "select a zone in Roon",
        [UI_NOTHING_PLAYING] = "nothing playing",
    };
    static int shown = -1;
    if ((int)st == shown) return;
    shown = st;
    ESP_LOGI(TAG, "status: %s", names[st]);
    if (s_cover_shown) return;
    switch (st) {
    case UI_FINDING_CORE: {
        const char *l[] = {"FINDING", "ROON CORE"};
        ui_message(l, 2);
        break;
    }
    case UI_CORE_NOT_FOUND: {
        const char *l[] = {"ROON CORE", "NOT FOUND", "", "Retrying"};
        ui_message(l, 4);
        break;
    }
    case UI_AWAITING_AUTH: {
        const char *l[] = {"In Roon open", "Settings >", "Extensions,", "enable", ROON_DISPLAY_NAME_BASE};
        ui_message(l, 5);
        break;
    }
    case UI_NOTHING_PLAYING: {
        const char *l[] = {"NOTHING", "PLAYING"};
        ui_message(l, 2);
        break;
    }
    case UI_SELECT_ZONE: {
        const char *l[] = {"In Roon", "extension", "settings,", "choose a zone"};
        ui_message(l, 4);
        break;
    }
    }
}

static void send_raw(char *buf, size_t len)
{
    if (!buf) return;
    if (esp_websocket_client_send_bin(s_ws, buf, len, pdMS_TO_TICKS(WS_NETWORK_TIMEOUT_MS)) < 0)
        ESP_LOGW(TAG, "send failed");
    free(buf);
}

static uint32_t send_request(const char *path, cJSON *body)
{
    uint32_t id = s.next_request_id++;
    char rid[12];
    snprintf(rid, sizeof(rid), "%lu", (unsigned long)id);
    char *json = body ? cJSON_PrintUnformatted(body) : NULL;
    size_t len;
    send_raw(moo_format("REQUEST", path, rid, json, &len), len);
    cJSON_free(json);
    cJSON_Delete(body);
    return id;
}

static void send_reply(const char *verb, const char *status, const char *rid, cJSON *body)
{
    char *json = body ? cJSON_PrintUnformatted(body) : NULL;
    size_t len;
    send_raw(moo_format(verb, status, rid, json, &len), len);
    cJSON_free(json);
    cJSON_Delete(body);
}

static cJSON *parse_body(const moo_msg_t *m)
{
    if (m->body_len == 0 || !strstr(m->content_type, "application/json")) return NULL;
    return cJSON_ParseWithLength((const char *)m->body, m->body_len);
}

static const char *str_of(const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

// ---------------------------------------------------------------- zones

static bool zone_has_selected_output(const cJSON *zone)
{
    const cJSON *output;
    cJSON_ArrayForEach(output, cJSON_GetObjectItemCaseSensitive(zone, "outputs")) {
        const char *id = str_of(output, "output_id");
        if (id && strcmp(id, s_output_id) == 0) return true;
    }
    return false;
}

static void request_image(const char *image_key)
{
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "image_key", image_key);
    cJSON_AddStringToObject(b, "scale", "fit");
    cJSON_AddNumberToObject(b, "width", COVER_SIZE);
    cJSON_AddNumberToObject(b, "height", COVER_SIZE);
    cJSON_AddStringToObject(b, "format", "image/jpeg");
    s.image_id = send_request(SVC_IMAGE "/get_image", b);
    s.image_requested_ms = esp_timer_get_time() / 1000;
}

// Records whether the selected output can change volume (fixed-volume
// outputs carry no "volume" object), so knob turns can be ignored early.
static void update_volume_support(const cJSON *zone)
{
    const cJSON *output;
    cJSON_ArrayForEach(output, cJSON_GetObjectItemCaseSensitive(zone, "outputs")) {
        const char *id = str_of(output, "output_id");
        if (!id || strcmp(id, s_output_id) != 0) continue;
        const cJSON *volume = cJSON_GetObjectItemCaseSensitive(output, "volume");
        bool supported = cJSON_IsObject(volume);
        if (supported != s.volume_supported) {
            s.volume_supported = supported;
            if (supported) {
                const cJSON *value = cJSON_GetObjectItemCaseSensitive(volume, "value");
                const cJSON *step = cJSON_GetObjectItemCaseSensitive(volume, "step");
                ESP_LOGI(TAG, "volume: type %s, value %g, step %g", str_of(volume, "type") ?: "?",
                         cJSON_IsNumber(value) ? value->valuedouble : 0, cJSON_IsNumber(step) ? step->valuedouble : 0);
            } else {
                ESP_LOGW(TAG, "selected output has fixed volume; knob turns are ignored");
            }
        }
        return;
    }
}

// Applies one zone object if it holds the selected output. Paused or stopped
// keeps the last cover; a track without artwork blanks the screen.
static void apply_zone(const char *text, size_t len, void *ctx)
{
    if (s_output_id[0] == '\0') return;
    cJSON *zone = cJSON_ParseWithLength(text, len);
    if (!zone) {
        ESP_LOGW(TAG, "zone object not parsed (%u bytes)", (unsigned)len);
        return;
    }
    if (!zone_has_selected_output(zone)) goto done;
    s.zone_seen = true;
    update_volume_support(zone);
    const char *state = str_of(zone, "state");
    screen_guard_set_playing(state && strcmp(state, "playing") == 0);
    const cJSON *np = cJSON_GetObjectItemCaseSensitive(zone, "now_playing");
    if (!np) {  // stopped: keep the last cover, or say so if none was shown yet
        show_status(UI_NOTHING_PLAYING);
        goto done;
    }
    const char *key = str_of(np, "image_key");
    if (!key) {
        if (s.image_key[0]) {
            s.image_key[0] = '\0';
            cover_clear();
        }
        goto done;
    }
    if (strcmp(key, s.image_key) == 0) goto done;
    strlcpy(s.image_key, key, sizeof(s.image_key));
    screen_guard_wake();  // a new track is worth seeing, even while paused
    const cJSON *line = cJSON_GetObjectItemCaseSensitive(np, "one_line");
    ESP_LOGI(TAG, "now playing in '%s': %s", str_of(zone, "display_name") ?: "?",
             line ? (str_of(line, "line1") ?: "") : "");
    request_image(key);
done:
    cJSON_Delete(zone);
}

// Zone snapshots and deltas are walked one zone at a time instead of parsing
// the whole message, so their size does not multiply into a JSON tree.
// zones_removed is ignored: a vanished zone keeps its last cover on screen.
static void on_zones(const moo_msg_t *m)
{
    if (!strstr(m->content_type, "application/json")) return;
    const char *body = (const char *)m->body;
    const bool snapshot = strcmp(m->name, "Subscribed") == 0;
    if (snapshot) s.zone_seen = false;
    json_scan_array_objects(body, m->body_len, "zones", apply_zone, NULL);          // Subscribed
    json_scan_array_objects(body, m->body_len, "zones_added", apply_zone, NULL);    // Changed
    json_scan_array_objects(body, m->body_len, "zones_changed", apply_zone, NULL);  // Changed
    // The selected output is in no zone (e.g. the device is switched off).
    if (snapshot && !s.zone_seen && s_output_id[0]) {
        screen_guard_set_playing(false);
        show_status(UI_NOTHING_PLAYING);
    }
}

// Starts a fresh zone subscription; Core answers with a full snapshot, which
// is how a newly selected zone gets its current cover without a local cache.
static void subscribe_zones(void)
{
    char key[16];
    if (s.zones_subscription) {
        snprintf(key, sizeof(key), "%lu", (unsigned long)s.zones_subscription);
        cJSON *b = cJSON_CreateObject();
        cJSON_AddStringToObject(b, "subscription_key", key);
        send_request(SVC_TRANSPORT "/unsubscribe_zones", b);
    }
    snprintf(key, sizeof(key), "%lu", (unsigned long)++s.zones_subscription);
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "subscription_key", key);
    s.zones_id = send_request(SVC_TRANSPORT "/subscribe_zones", b);
}

// ---------------------------------------------------------------- status

// Line shown under the extension in Roon > Settings > Extensions.
static cJSON *status_body(void)
{
    char message[160];
    if (s_output_id[0])
        snprintf(message, sizeof(message), "已连接 Roon Core，显示 %s", s_output_name);
    else
        snprintf(message, sizeof(message), "已连接 Roon Core，请在设置中选择 Zone");
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "message", message);
    cJSON_AddBoolToObject(b, "is_error", false);
    return b;
}

static void publish_status(void)
{
    if (s.status_subscription[0]) send_reply("CONTINUE", "Changed", s.status_subscription, status_body());
}

// ---------------------------------------------------------------- settings

static cJSON *settings_layout(const char *output_id, const char *name)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *values = cJSON_AddObjectToObject(root, "values");
    if (output_id && output_id[0]) {
        cJSON *zone = cJSON_AddObjectToObject(values, SETTING_ZONE);
        cJSON_AddStringToObject(zone, "output_id", output_id);
        // Roon clients read either field to echo the current choice.
        cJSON_AddStringToObject(zone, "name", name);
        cJSON_AddStringToObject(zone, "display_name", name);
    }
    cJSON *layout = cJSON_AddArrayToObject(root, "layout");
    cJSON *picker = cJSON_CreateObject();
    cJSON_AddStringToObject(picker, "type", "zone");
    cJSON_AddStringToObject(picker, "title", "Select playback zone");
    cJSON_AddStringToObject(picker, "setting", SETTING_ZONE);
    cJSON_AddItemToArray(layout, picker);
    cJSON_AddBoolToObject(root, "has_error", false);
    return root;
}

static cJSON *wrap_settings(cJSON *layout)
{
    cJSON *b = cJSON_CreateObject();
    cJSON_AddItemToObject(b, "settings", layout);
    return b;
}

static void on_save_settings(const moo_msg_t *m)
{
    cJSON *body = parse_body(m);
    const cJSON *zone = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "settings"), "values"),
        SETTING_ZONE);
    const char *id = str_of(zone, "output_id");
    const char *name = str_of(zone, "name") ?: str_of(zone, "display_name") ?: "";
    bool dry_run = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(body, "is_dry_run"));

    if (!id) {
        // No zone picked: echo the stored choice so the dialog stays consistent.
        send_reply("COMPLETE", "Success", m->request_id, wrap_settings(settings_layout(s_output_id, s_output_name)));
    } else if (dry_run) {
        send_reply("COMPLETE", "Success", m->request_id, wrap_settings(settings_layout(id, name)));
    } else {
        strlcpy(s_output_id, id, sizeof(s_output_id));
        strlcpy(s_output_name, name, sizeof(s_output_name));
        store_set_output(s_output_id, s_output_name);
        ESP_LOGI(TAG, "zone set to '%s' (output %s)", s_output_name, s_output_id);
        send_reply("COMPLETE", "Success", m->request_id, wrap_settings(settings_layout(s_output_id, s_output_name)));
        if (s.settings_subscription[0])
            send_reply("CONTINUE", "Changed", s.settings_subscription,
                       wrap_settings(settings_layout(s_output_id, s_output_name)));
        publish_status();
        s.image_key[0] = '\0';  // force a fetch for the newly chosen zone
        subscribe_zones();
    }
    cJSON_Delete(body);
}

// ---------------------------------------------------------------- controls

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static bool controls_ready(void)
{
    return s.registered && s_output_id[0] != '\0';
}

static void send_control(roon_control_t control)
{
    static const char *names[] = {[ROON_CONTROL_PLAYPAUSE] = "playpause", [ROON_CONTROL_NEXT] = "next"};
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "zone_or_output_id", s_output_id);
    cJSON_AddStringToObject(b, "control", names[control]);
    send_request(SVC_TRANSPORT "/control", b);
    ESP_LOGI(TAG, "control %s", names[control]);
}

// Sends coalesced knob turns, one change_volume in flight at a time, so a fast
// spin becomes a few larger steps rather than a backlog of small ones.
static void flush_volume(void)
{
    const int64_t now = now_ms();
    if (s.volume_id && now - s.volume_sent_ms > VOLUME_REPLY_TIMEOUT_MS) {
        ESP_LOGW(TAG, "volume change not confirmed");
        s.volume_id = 0;
    }
    if (s.pending_volume_steps && now - s.pending_volume_ms > CONTROL_EXPIRY_MS) {
        ESP_LOGW(TAG, "dropped %d stale volume steps", s.pending_volume_steps);
        s.pending_volume_steps = 0;
    }
    if (!s.pending_volume_steps || s.volume_id || !controls_ready()) return;
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "output_id", s_output_id);
    cJSON_AddStringToObject(b, "how", "relative_step");
    cJSON_AddNumberToObject(b, "value", s.pending_volume_steps);
    s.volume_id = send_request(SVC_TRANSPORT "/change_volume", b);
    s.volume_sent_ms = now;
    ESP_LOGI(TAG, "volume %+d steps", s.pending_volume_steps);
    s.pending_volume_steps = 0;
}

static void on_control_event(const event_t *ev)
{
    if (now_ms() - ev->at_ms > CONTROL_EXPIRY_MS || !controls_ready()) return;
    if (ev->type == EV_CONTROL) {
        s.pending_volume_steps = 0;  // a click supersedes unsent turns
        send_control((roon_control_t)ev->value);
    } else if (s.volume_supported) {
        int steps = s.pending_volume_steps + ev->value;
        if (steps > VOLUME_MAX_PENDING_STEPS) steps = VOLUME_MAX_PENDING_STEPS;
        if (steps < -VOLUME_MAX_PENDING_STEPS) steps = -VOLUME_MAX_PENDING_STEPS;
        s.pending_volume_steps = steps;
        s.pending_volume_ms = ev->at_ms;
    }
}

// ---------------------------------------------------------------- dispatch

static void on_core_request(const moo_msg_t *m)
{
    const char *name = m->name;
    if (strcmp(name, SVC_PING "/ping") == 0) {
        send_reply("COMPLETE", "Success", m->request_id, NULL);
    } else if (strcmp(name, SVC_STATUS "/subscribe_status") == 0) {
        strlcpy(s.status_subscription, m->request_id, sizeof(s.status_subscription));
        send_reply("CONTINUE", "Subscribed", m->request_id, status_body());
    } else if (strcmp(name, SVC_STATUS "/unsubscribe_status") == 0) {
        s.status_subscription[0] = '\0';
        send_reply("COMPLETE", "Unsubscribed", m->request_id, NULL);
    } else if (strcmp(name, SVC_STATUS "/get_status") == 0) {
        send_reply("COMPLETE", "Success", m->request_id, status_body());
    } else if (strcmp(name, SVC_SETTINGS "/subscribe_settings") == 0) {
        strlcpy(s.settings_subscription, m->request_id, sizeof(s.settings_subscription));
        send_reply("CONTINUE", "Subscribed", m->request_id, wrap_settings(settings_layout(s_output_id, s_output_name)));
    } else if (strcmp(name, SVC_SETTINGS "/unsubscribe_settings") == 0) {
        s.settings_subscription[0] = '\0';
        send_reply("COMPLETE", "Unsubscribed", m->request_id, NULL);
    } else if (strcmp(name, SVC_SETTINGS "/get_settings") == 0) {
        send_reply("COMPLETE", "Success", m->request_id, wrap_settings(settings_layout(s_output_id, s_output_name)));
    } else if (strcmp(name, SVC_SETTINGS "/save_settings") == 0) {
        on_save_settings(m);
    } else {
        ESP_LOGW(TAG, "unsupported request %s", name);
        cJSON *b = cJSON_CreateObject();
        cJSON_AddStringToObject(b, "error", "unsupported");
        send_reply("COMPLETE", "InvalidRequest", m->request_id, b);
    }
}

static void send_register(void)
{
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "extension_id", s_extension_id);
    cJSON_AddStringToObject(b, "display_name", s_display_name);
    cJSON_AddStringToObject(b, "display_version", esp_app_get_description()->version);
    cJSON_AddStringToObject(b, "publisher", ROON_PUBLISHER);
    cJSON_AddStringToObject(b, "email", ROON_EMAIL);
    char token[128];
    const bool paired = store_get_token(s.core_id, token, sizeof(token));
    if (paired) cJSON_AddStringToObject(b, "token", token);
    const char *required[] = {SVC_TRANSPORT, SVC_IMAGE};
    const char *provided[] = {SVC_PING, SVC_SETTINGS, SVC_STATUS};
    cJSON_AddItemToObject(b, "required_services", cJSON_CreateStringArray(required, 2));
    cJSON_AddItemToObject(b, "optional_services", cJSON_CreateArray());
    cJSON_AddItemToObject(b, "provided_services", cJSON_CreateStringArray(provided, sizeof(provided) / sizeof(provided[0])));
    s.register_id = send_request(SVC_REGISTRY "/register", b);
    // Core answers only once the extension is enabled in Roon. A paired device
    // is normally answered at once, so it asks only if that does not happen.
    if (paired)
        s.register_sent_ms = now_ms();
    else
        show_status(UI_AWAITING_AUTH);
}

static void remember_core_address(void)
{
    char host[sizeof(s_core_host)];
    uint16_t port;
    if (store_get_core_address(host, sizeof(host), &port) && strcmp(host, s_core_host) == 0 && port == s_core_port)
        return;  // unchanged: spare the flash a write
    store_set_core_address(s_core_host, s_core_port);
}

static void on_core_reply(const moo_msg_t *m)
{
    uint32_t id = strtoul(m->request_id, NULL, 10);
    if (id == s.info_id) {
        cJSON *body = parse_body(m);
        strlcpy(s.core_id, str_of(body, "core_id") ?: "", sizeof(s.core_id));
        ESP_LOGI(TAG, "core '%s' id %s", str_of(body, "display_name") ?: "?", s.core_id);
        cJSON_Delete(body);
        char token[128];
        if (s_core_from_cache && !store_get_token(s.core_id, token, sizeof(token))) {
            // The cached address now answers as some other Core: forget it.
            ESP_LOGW(TAG, "cached address is not the paired Core, forgetting it");
            store_clear_core_address();
            s.abort = true;
            return;
        }
        send_register();
    } else if (id == s.register_id) {
        cJSON *body = parse_body(m);
        const char *token = str_of(body, "token");
        if (strcmp(m->name, "Registered") == 0 && token) {
            store_set_pairing(str_of(body, "core_id") ?: s.core_id, token);
            s.registered = true;
            s.register_sent_ms = 0;
            ESP_LOGI(TAG, "registered with core");
            remember_core_address();
            subscribe_zones();
            if (s_output_id[0] == '\0') show_status(UI_SELECT_ZONE);
        } else {
            ESP_LOGW(TAG, "register answered %s %s", m->verb, m->name);
        }
        cJSON_Delete(body);
    } else if (id == s.zones_id) {
        on_zones(m);
    } else if (id == s.volume_id) {
        if (strcmp(m->name, "Success") != 0) ESP_LOGW(TAG, "volume change answered %s %s", m->verb, m->name);
        s.volume_id = 0;
    } else if (id == s.image_id) {
        s.image_requested_ms = 0;
        if (strcmp(m->name, "Success") == 0 && strstr(m->content_type, "image/jpeg")) {
            if (cover_show_jpeg(m->body, m->body_len)) s_cover_shown = true;
        } else {
            ESP_LOGW(TAG, "image request answered %s %s", m->verb, m->name);
        }
    }
}

static void on_message(uint8_t *data, size_t len)
{
    moo_msg_t m;
    if (!moo_parse(data, len, &m)) {
        ESP_LOGW(TAG, "unparseable message (%u bytes)", (unsigned)len);
        return;
    }
    if (strcmp(m.verb, "REQUEST") == 0)
        on_core_request(&m);
    else
        on_core_reply(&m);
}

// ---------------------------------------------------------------- transport

// Message being assembled. Ownership passes to the roon task on completion,
// so a message occupies memory once, sized to what actually arrived.
static uint8_t *s_rx;
static size_t s_rx_len, s_rx_cap;
static bool s_rx_overflow;

static void post(event_type_t type, uint8_t *data, size_t len)
{
    event_t ev = {.type = type, .data = data, .len = len};
    xQueueSend(s_events, &ev, portMAX_DELAY);
}

static void rx_discard(void)
{
    free(s_rx);
    s_rx = NULL;
    s_rx_len = s_rx_cap = 0;
}

// Reassembles WebSocket chunks (large frames arrive in buffer-sized pieces,
// possibly across continuation frames) into whole MOO messages.
static void on_ws_data(const esp_websocket_event_data_t *d)
{
    if (d->op_code != WS_OP_TEXT && d->op_code != WS_OP_BINARY && d->op_code != WS_OP_CONTINUATION) return;
    if (d->op_code != WS_OP_CONTINUATION && d->payload_offset == 0) {
        rx_discard();
        s_rx_overflow = false;
    }
    if (d->payload_offset == 0) {
        // First chunk of a frame: its total length is known, grow once.
        size_t need = s_rx_len + d->payload_len;
        if (need > MOO_MAX_MESSAGE_BYTES) {
            s_rx_overflow = true;
        } else if (!s_rx_overflow && need > s_rx_cap) {
            uint8_t *grown = realloc(s_rx, need);
            if (grown) {
                s_rx = grown;
                s_rx_cap = need;
            } else {
                s_rx_overflow = true;
            }
        }
    }
    if (!s_rx_overflow && s_rx_len + d->data_len <= s_rx_cap) {
        memcpy(s_rx + s_rx_len, d->data_ptr, d->data_len);
        s_rx_len += d->data_len;
    }
    bool frame_done = d->payload_offset + d->data_len >= d->payload_len;
    if (!frame_done || !d->fin) return;
    if (s_rx_overflow) {
        ESP_LOGE(TAG, "message of %d+ bytes dropped (limit %d, largest free block %u)", (int)(s_rx_len + d->payload_len),
                 MOO_MAX_MESSAGE_BYTES, (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        rx_discard();
        return;
    }
    if (s_rx_len >= MOO_LARGE_MESSAGE_BYTES) ESP_LOGI(TAG, "large message %u bytes", (unsigned)s_rx_len);
    post(EV_MESSAGE, s_rx, s_rx_len);
    s_rx = NULL;
    s_rx_len = s_rx_cap = 0;
}

static void on_ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        post(EV_CONNECTED, NULL, 0);
        break;
    case WEBSOCKET_EVENT_DATA:
        on_ws_data(data);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
    case WEBSOCKET_EVENT_ERROR:
        post(EV_DISCONNECTED, NULL, 0);
        break;
    default:
        break;
    }
}

static void reset_session(void)
{
    memset(&s, 0, sizeof(s));
    s.next_request_id = 1;
}

static void report_heap(void)
{
    ESP_LOGI(TAG, "heap internal free %u (min %u, largest block %u), psram free %u (min %u)",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}

// Runs one WebSocket session until it drops. Returns true if pairing completed.
static bool run_session(const char *host, uint16_t port)
{
    char uri[96];
    snprintf(uri, sizeof(uri), "ws://%s:%u%s", host, port, ROON_WS_PATH);
    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .buffer_size = WS_RX_CHUNK_BYTES,
        .task_stack = WS_TASK_STACK,
        .disable_auto_reconnect = true,
        .network_timeout_ms = WS_NETWORK_TIMEOUT_MS,
        .ping_interval_sec = WS_PING_INTERVAL_SEC,
        .pingpong_timeout_sec = WS_PINGPONG_TIMEOUT_SEC,
    };
    reset_session();
    s_ws = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, on_ws_event, NULL);
    ESP_LOGI(TAG, "connecting %s", uri);
    esp_websocket_client_start(s_ws);

    int64_t last_report = 0;
    bool open = true;
    while (open) {
        event_t ev;
        if (xQueueReceive(s_events, &ev, pdMS_TO_TICKS(EVENT_WAIT_MS))) {
            switch (ev.type) {
            case EV_CONNECTED:
                ESP_LOGI(TAG, "connected");
                s.info_id = send_request(SVC_REGISTRY "/info", NULL);
                break;
            case EV_MESSAGE:
                on_message(ev.data, ev.len);
                free(ev.data);
                if (s.abort) open = false;
                break;
            case EV_DISCONNECTED:
                ESP_LOGW(TAG, "connection lost");
                open = false;
                break;
            case EV_CONTROL:
            case EV_VOLUME:
                on_control_event(&ev);
                break;
            }
        }
        if (open) flush_volume();
        if (s.register_sent_ms && now_ms() - s.register_sent_ms > AUTH_PROMPT_DELAY_MS) {
            s.register_sent_ms = 0;
            show_status(UI_AWAITING_AUTH);
        }
        if (open && s.image_requested_ms && now_ms() - s.image_requested_ms > IMAGE_REPLY_TIMEOUT_MS) {
            ESP_LOGW(TAG, "cover not received in %d ms, reconnecting", IMAGE_REPLY_TIMEOUT_MS);
            open = false;
        }
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_report >= HEAP_REPORT_MS) {
            report_heap();
            last_report = now;
        }
    }
    screen_guard_set_playing(false);  // unknown while Core is unreachable: let the screen rest
    esp_websocket_client_stop(s_ws);
    esp_websocket_client_destroy(s_ws);
    s_ws = NULL;
    rx_discard();
    event_t ev;
    while (xQueueReceive(s_events, &ev, 0)) free(ev.data);
    return s.registered;
}

// Finds the Core: a fresh SOOD answer first, else the last address that
// worked (checked against the paired core_id once connected).
static bool locate_core(void)
{
    s_core_from_cache = false;
    if (sood_discover(s_core_host, sizeof(s_core_host), &s_core_port)) return true;
    if (store_get_core_address(s_core_host, sizeof(s_core_host), &s_core_port)) {
        ESP_LOGW(TAG, "no SOOD answer, trying last known Core %s:%u", s_core_host, s_core_port);
        s_core_from_cache = true;
        return true;
    }
    return false;
}

static void roon_task(void *arg)
{
    uint32_t delay_ms = RECONNECT_DELAY_MIN_MS;
    for (;;) {
        wifi_wait_connected(UINT32_MAX);
        show_status(UI_FINDING_CORE);
        bool paired = false;
        if (locate_core()) {
            paired = run_session(s_core_host, s_core_port);
        } else {
            ESP_LOGW(TAG, "no Roon Core answered discovery");
            show_status(UI_CORE_NOT_FOUND);
        }
        delay_ms = paired ? RECONNECT_DELAY_MIN_MS
                          : (delay_ms * 2 > RECONNECT_DELAY_MAX_MS ? RECONNECT_DELAY_MAX_MS : delay_ms * 2);
        ESP_LOGI(TAG, "reconnecting in %lu ms", (unsigned long)delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// Called from the knob task. Never blocks: input is dropped if the queue is
// full, which only happens while the Roon task is stuck reconnecting.
static void post_input(event_type_t type, int value)
{
    event_t ev = {.type = type, .value = value, .at_ms = now_ms()};
    if (!s_events || xQueueSend(s_events, &ev, 0) != pdTRUE) ESP_LOGW(TAG, "input dropped");
}

void roon_control(roon_control_t control)
{
    post_input(EV_CONTROL, control);
}

void roon_change_volume(int steps)
{
    post_input(EV_VOLUME, steps);
}

// Per-device identity: Roon keys pairing and settings by extension id, so
// each display gets its own (base id + last three MAC bytes).
static void init_identity(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_extension_id, sizeof(s_extension_id), "%s.%02x%02x%02x", ROON_EXTENSION_ID_BASE, mac[3], mac[4], mac[5]);
    snprintf(s_display_name, sizeof(s_display_name), "%s_%s", ROON_DISPLAY_NAME_BASE, BOARD_NAME);
    ESP_LOGI(TAG, "extension %s as '%s'", s_extension_id, s_display_name);
}

void roon_start(void)
{
    init_identity();
    s_events = xQueueCreate(EVENT_QUEUE_DEPTH, sizeof(event_t));
    if (!store_get_output(s_output_id, sizeof(s_output_id), s_output_name, sizeof(s_output_name)))
        s_output_id[0] = '\0';
    else
        ESP_LOGI(TAG, "saved zone '%s'", s_output_name);
    xTaskCreate(roon_task, "roon", ROON_TASK_STACK, NULL, 5, NULL);
}

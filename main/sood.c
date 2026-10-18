#include "sood.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "config.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lwip/sockets.h"

static const char *TAG = "sood";

// Wire format: "SOOD" <version=2> <type> then properties, each
// <key len:1><key><value len:2 BE><value>. A value length of 0xFFFF means null.
#define SOOD_VERSION       2
#define SOOD_TYPE_QUERY    'Q'
#define SOOD_HEADER_LEN    6
#define SOOD_NULL_VALUE    0xFFFF

static size_t put_prop(uint8_t *buf, size_t at, size_t cap, const char *key, const char *value)
{
    size_t kl = strlen(key), vl = strlen(value);
    if (at + 1 + kl + 2 + vl > cap) return at;
    buf[at++] = (uint8_t)kl;
    memcpy(buf + at, key, kl);
    at += kl;
    buf[at++] = (uint8_t)(vl >> 8);
    buf[at++] = (uint8_t)vl;
    memcpy(buf + at, value, vl);
    return at + vl;
}

static size_t build_query(uint8_t *buf, size_t cap)
{
    char tid[33];
    snprintf(tid, sizeof(tid), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(),
             (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
    memcpy(buf, "SOOD", 4);
    buf[4] = SOOD_VERSION;
    buf[5] = SOOD_TYPE_QUERY;
    size_t at = put_prop(buf, SOOD_HEADER_LEN, cap, "query_service_id", SOOD_ROON_SERVICE_ID);
    return put_prop(buf, at, cap, "_tid", tid);
}

// Finds a property in a reply; returns false if absent or the packet is malformed.
static bool find_prop(const uint8_t *p, size_t len, const char *key, char *out, size_t out_len)
{
    if (len < SOOD_HEADER_LEN || memcmp(p, "SOOD", 4) != 0) return false;
    size_t at = SOOD_HEADER_LEN, key_len = strlen(key);
    while (at < len) {
        size_t kl = p[at++];
        if (kl == 0 || at + kl + 2 > len) return false;
        const uint8_t *k = p + at;
        at += kl;
        size_t vl = ((size_t)p[at] << 8) | p[at + 1];
        at += 2;
        if (vl == SOOD_NULL_VALUE) vl = 0;
        if (at + vl > len) return false;
        if (kl == key_len && strncasecmp((const char *)k, key, kl) == 0) {
            size_t n = vl < out_len - 1 ? vl : out_len - 1;
            memcpy(out, p + at, n);
            out[n] = '\0';
            return true;
        }
        at += vl;
    }
    return false;
}

static void send_query(int sock, const uint8_t *q, size_t len)
{
    struct sockaddr_in to = {.sin_family = AF_INET, .sin_port = htons(SOOD_PORT)};
    to.sin_addr.s_addr = inet_addr(SOOD_MULTICAST_ADDR);
    sendto(sock, q, len, 0, (struct sockaddr *)&to, sizeof(to));
    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(sock, q, len, 0, (struct sockaddr *)&to, sizeof(to));
}

bool sood_discover(char *host, size_t host_len, uint16_t *port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return false;
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    struct timeval tv = {.tv_sec = 0, .tv_usec = 200 * 1000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in local = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_ANY)};
    bind(sock, (struct sockaddr *)&local, sizeof(local));

    uint8_t query[160];
    size_t qlen = build_query(query, sizeof(query));
    uint8_t rx[1024];
    bool found = false;
    int64_t start = esp_timer_get_time() / 1000, last_send = -SOOD_RESEND_MS;

    while (!found && esp_timer_get_time() / 1000 - start < SOOD_LISTEN_MS) {
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_send >= SOOD_RESEND_MS) {
            send_query(sock, query, qlen);
            last_send = now;
        }
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &from_len);
        if (n <= 0) continue;
        char service[64], http_port[8], name[64] = "";
        if (!find_prop(rx, n, "service_id", service, sizeof(service)) ||
            strcmp(service, SOOD_ROON_SERVICE_ID) != 0 ||
            !find_prop(rx, n, "http_port", http_port, sizeof(http_port)))
            continue;
        int p = atoi(http_port);
        if (p <= 0 || p > 65535) continue;
        find_prop(rx, n, "name", name, sizeof(name));
        inet_ntoa_r(from.sin_addr, host, host_len);
        *port = (uint16_t)p;
        ESP_LOGI(TAG, "Core '%s' at %s:%u", name, host, *port);
        found = true;
    }
    close(sock);
    return found;
}

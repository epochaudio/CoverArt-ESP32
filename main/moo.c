#include "moo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool copy_field(char *dst, size_t cap, const char *src, size_t n)
{
    if (n >= cap) return false;
    memcpy(dst, src, n);
    dst[n] = '\0';
    return true;
}

static const char *find_line_end(const char *p, const char *end)
{
    const char *nl = memchr(p, '\n', end - p);
    return nl ? nl : end;
}

bool moo_parse(const uint8_t *data, size_t len, moo_msg_t *out)
{
    memset(out, 0, sizeof(*out));
    const char *p = (const char *)data, *end = p + len;

    // First line: "MOO/1 VERB name..." (name may contain spaces).
    const char *eol = find_line_end(p, end);
    if (eol - p < 6 || strncmp(p, "MOO/1 ", 6) != 0) return false;
    const char *verb = p + 6;
    const char *sp = memchr(verb, ' ', eol - verb);
    const char *verb_end = sp ? sp : eol;
    if (!copy_field(out->verb, sizeof(out->verb), verb, verb_end - verb)) return false;
    if (sp) {
        const char *name_end = eol;
        if (name_end > sp && name_end[-1] == '\r') name_end--;
        if (!copy_field(out->name, sizeof(out->name), sp + 1, name_end - sp - 1)) return false;
    }

    size_t content_length = 0;
    p = eol + 1;
    while (p < end) {
        eol = find_line_end(p, end);
        size_t line_len = eol - p;
        if (line_len > 0 && p[line_len - 1] == '\r') line_len--;
        if (line_len == 0) {  // blank line: body follows
            p = eol + 1;
            break;
        }
        const char *colon = memchr(p, ':', line_len);
        if (!colon) return false;
        size_t key_len = colon - p;
        const char *val = colon + 1;
        while (val < p + line_len && *val == ' ') val++;
        size_t val_len = p + line_len - val;
        if (key_len == 10 && strncasecmp(p, "Request-Id", 10) == 0) {
            if (!copy_field(out->request_id, sizeof(out->request_id), val, val_len)) return false;
        } else if (key_len == 12 && strncasecmp(p, "Content-Type", 12) == 0) {
            copy_field(out->content_type, sizeof(out->content_type), val, val_len);
        } else if (key_len == 14 && strncasecmp(p, "Content-Length", 14) == 0) {
            content_length = strtoul(val, NULL, 10);
        }
        p = eol + 1;
    }
    if (out->request_id[0] == '\0') return false;
    if (p > end) p = end;
    if (content_length > (size_t)(end - p)) return false;
    out->body = (const uint8_t *)p;
    out->body_len = content_length;
    return true;
}

char *moo_format(const char *verb, const char *name, const char *request_id,
                 const char *json, size_t *out_len)
{
    size_t body_len = json ? strlen(json) : 0;
    size_t cap = 160 + strlen(name) + strlen(request_id) + body_len;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int n;
    if (json) {
        n = snprintf(buf, cap,
                     "MOO/1 %s %s\nRequest-Id: %s\nContent-Type: application/json\n"
                     "Content-Length: %u\n\n%s",
                     verb, name, request_id, (unsigned)body_len, json);
    } else {
        n = snprintf(buf, cap, "MOO/1 %s %s\nRequest-Id: %s\n\n", verb, name, request_id);
    }
    if (n < 0 || (size_t)n >= cap) {
        free(buf);
        return NULL;
    }
    *out_len = (size_t)n;
    return buf;
}

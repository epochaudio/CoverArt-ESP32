#include "json_scan.h"

#include <stdbool.h>
#include <string.h>

// Index just past the string starting at json[i] == '"', or len if unterminated.
static size_t skip_string(const char *json, size_t len, size_t i)
{
    for (i++; i < len; i++) {
        if (json[i] == '\\') i++;
        else if (json[i] == '"') return i + 1;
    }
    return len;
}

// Index just past the object or array starting at json[i], or len if unbalanced.
static size_t skip_container(const char *json, size_t len, size_t i)
{
    int depth = 0;
    while (i < len) {
        char c = json[i];
        if (c == '"') {
            i = skip_string(json, len, i);
            continue;
        }
        if (c == '{' || c == '[') depth++;
        else if ((c == '}' || c == ']') && --depth == 0) return i + 1;
        i++;
    }
    return len;
}

static size_t skip_space(const char *json, size_t len, size_t i)
{
    while (i < len && (json[i] == ' ' || json[i] == '\n' || json[i] == '\r' || json[i] == '\t')) i++;
    return i;
}

int json_scan_array_objects(const char *json, size_t len, const char *key, json_object_fn each, void *ctx)
{
    const size_t key_len = strlen(key);
    size_t i = skip_space(json, len, 0);
    if (i >= len || json[i] != '{') return 0;
    i++;
    // Walk the top-level members: "name": value, ...
    while (i < len) {
        i = skip_space(json, len, i);
        if (i >= len || json[i] == '}') return 0;
        if (json[i] == ',') {
            i++;
            continue;
        }
        if (json[i] != '"') return 0;
        size_t name_start = i + 1;
        i = skip_string(json, len, i);
        bool match = i - name_start - 1 == key_len && memcmp(json + name_start, key, key_len) == 0;
        i = skip_space(json, len, i);
        if (i >= len || json[i] != ':') return 0;
        i = skip_space(json, len, i + 1);
        if (i >= len) return 0;
        if (!match || json[i] != '[') {
            // Skip a value of any type.
            if (json[i] == '"') i = skip_string(json, len, i);
            else if (json[i] == '{' || json[i] == '[') i = skip_container(json, len, i);
            else while (i < len && json[i] != ',' && json[i] != '}') i++;
            continue;
        }
        // The wanted array: hand each object element to the callback.
        int count = 0;
        i++;
        while (i < len) {
            i = skip_space(json, len, i);
            if (i >= len || json[i] == ']') return count;
            if (json[i] == ',') {
                i++;
            } else if (json[i] == '{') {
                size_t end = skip_container(json, len, i);
                each(json + i, end - i, ctx);
                count++;
                i = end;
            } else {
                return count;  // not an array of objects
            }
        }
        return count;
    }
    return 0;
}

#pragma once
#include <stddef.h>

// Calls `each` with the raw text of every object in the top-level array named
// `key` of a JSON document, without building a tree for the whole document.
// Lets a large zone snapshot be handled one zone at a time on boards without
// PSRAM. Returns the number of objects visited.
typedef void (*json_object_fn)(const char *object, size_t len, void *ctx);
int json_scan_array_objects(const char *json, size_t len, const char *key, json_object_fn each, void *ctx);

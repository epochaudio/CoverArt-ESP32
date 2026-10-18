#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// MOO/1 framing as spoken by Roon over WebSocket binary frames:
//   MOO/1 <VERB> <name>\n<Header: value>\n...\n\n<body>
// VERB is REQUEST (name = service/method) or COMPLETE / CONTINUE (name = status).
typedef struct {
    char verb[12];
    char name[128];
    char request_id[24];
    char content_type[48];
    const uint8_t *body;   // points into the parsed buffer, not terminated
    size_t body_len;
} moo_msg_t;

bool moo_parse(const uint8_t *data, size_t len, moo_msg_t *out);

// Formats a message into a heap buffer (caller frees). json may be NULL.
char *moo_format(const char *verb, const char *name, const char *request_id,
                 const char *json, size_t *out_len);

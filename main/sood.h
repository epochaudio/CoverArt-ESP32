#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Asks the LAN for a Roon Core over SOOD (multicast + broadcast query, unicast
// replies). Returns the first Core that answers with Roon's service id.
bool sood_discover(char *host, size_t host_len, uint16_t *port);

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Persistent device state in its own NVS namespace; other firmware's
// namespaces on the shared nvs partition are never touched.
void store_init(void);

// Home Wi-Fi entered during setup.
bool store_get_wifi(char *ssid, size_t ssid_len, char *password, size_t password_len);
void store_set_wifi(const char *ssid, const char *password);

// Pairing: one Core per device, token is valid only for that core_id.
bool store_get_token(const char *core_id, char *token, size_t len);
void store_set_pairing(const char *core_id, const char *token);

// Last address SOOD found for the paired Core, used when discovery gets no
// answer (multicast is the first thing a weak or filtering network drops).
bool store_get_core_address(char *host, size_t host_len, uint16_t *port);
void store_set_core_address(const char *host, uint16_t port);
void store_clear_core_address(void);

// Zone chosen in the Roon extension settings, identified by one of its outputs.
bool store_get_output(char *output_id, size_t id_len, char *name, size_t name_len);
void store_set_output(const char *output_id, const char *name);

// Screen brightness chosen in Roon's extension settings, percent; 0 if none.
int store_get_brightness(void);
void store_set_brightness(int percent);

// Quick power-on counter for the factory-reset gesture (see factory_reset.h).
int store_bump_power_cycles(void);
void store_clear_power_cycles(void);

// Factory reset: forget Wi-Fi, pairing, Core address and zone.
void store_erase_all(void);

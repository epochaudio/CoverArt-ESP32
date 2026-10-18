#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Decodes a baseline JPEG and shows it centred on a black screen.
bool cover_show_jpeg(const uint8_t *jpeg, size_t len);
// Blank screen for a track that has no artwork.
void cover_clear(void);

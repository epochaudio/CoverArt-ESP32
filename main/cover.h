#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Decodes a baseline JPEG and shows it centred (plus the current offset) with
// black margins around it.
bool cover_show_jpeg(const uint8_t *jpeg, size_t len);
// Blank screen for a track that has no artwork.
void cover_clear(void);
// Redraws the current cover after an overlay. False if no cover is up (or the
// board does not keep it), leaving the screen to the caller.
bool cover_redraw(void);
// Moves the cover by (dx, dy) pixels from centre and redraws it if it is on
// screen. Used against OLED burn-in; needs SCREEN_PIXEL_SHIFT to keep the image.
void cover_set_offset(int dx, int dy);

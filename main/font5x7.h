#pragma once

#define FONT5X7_WIDTH   5
#define FONT5X7_HEIGHT  7
// Row-major FONT5X7_WIDTH * FONT5X7_HEIGHT chars, '#' = on. Unknown chars map to '?'.
const char *font5x7_glyph(char c);

#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "display.h"
#include "esp_log.h"
#include "font5x7.h"
#include "qrcode.h"

static const char *TAG = "ui";

#define TEXT_SCALE        2
#define TEXT_ADVANCE      ((FONT5X7_WIDTH + 1) * TEXT_SCALE)
#define TEXT_HEIGHT       (FONT5X7_HEIGHT * TEXT_SCALE)
#define TEXT_LINE_PITCH   (TEXT_HEIGHT + 4 * TEXT_SCALE)

// QR code: version 3 holds the setup Wi-Fi string; 2 white modules of quiet
// zone on each side so phones find it against the black screen.
#define QR_MAX_VERSION    5
#define QR_QUIET_MODULES  2
#define QR_BOX_PX         100   // fits inside the round panel with text above and below

// Setup screen rows (panel centre is at LCD_HEIGHT / 2).
#define SETUP_TITLE_Y     36
#define SETUP_QR_TOP_Y    60
#define SETUP_HINT_Y      168
#define SETUP_NAME_Y      186

static uint16_t white(void) { return display_color(0xFF, 0xFF, 0xFF); }

static void draw_text(int y, const char *text)
{
    const int len = strlen(text);
    int x = (LCD_WIDTH - len * TEXT_ADVANCE) / 2;
    uint16_t glyph[FONT5X7_WIDTH * TEXT_SCALE * TEXT_HEIGHT];
    const uint16_t on = white();
    for (int i = 0; i < len; i++, x += TEXT_ADVANCE) {
        const char *g = font5x7_glyph(text[i]);
        for (int py = 0; py < TEXT_HEIGHT; py++) {
            for (int px = 0; px < FONT5X7_WIDTH * TEXT_SCALE; px++) {
                bool lit = g[(py / TEXT_SCALE) * FONT5X7_WIDTH + px / TEXT_SCALE] == '#';
                glyph[py * FONT5X7_WIDTH * TEXT_SCALE + px] = lit ? on : 0;
            }
        }
        if (x >= 0 && x + FONT5X7_WIDTH * TEXT_SCALE <= LCD_WIDTH)
            display_draw(x, y, FONT5X7_WIDTH * TEXT_SCALE, TEXT_HEIGHT, glyph);
    }
}

void ui_message(const char *const lines[], int count)
{
    if (count > UI_MAX_LINES) count = UI_MAX_LINES;
    display_fill(0);
    int y = (LCD_HEIGHT - (count * TEXT_LINE_PITCH - (TEXT_LINE_PITCH - TEXT_HEIGHT))) / 2;
    for (int i = 0; i < count; i++, y += TEXT_LINE_PITCH) draw_text(y, lines[i]);
}

// Draws the QR code one module row at a time.
static void draw_qr(esp_qrcode_handle_t qr, void *ctx)
{
    const int size = esp_qrcode_get_size(qr);
    const int modules = size + 2 * QR_QUIET_MODULES;
    const int scale = QR_BOX_PX / modules;
    const int px = modules * scale;
    const int x0 = (LCD_WIDTH - px) / 2;
    uint16_t *row = malloc((size_t)px * scale * sizeof(uint16_t));
    if (!row) return;
    const uint16_t on = 0, off = white();  // dark modules on a white quiet zone
    for (int my = 0; my < modules; my++) {
        for (int mx = 0; mx < modules; mx++) {
            int qx = mx - QR_QUIET_MODULES, qy = my - QR_QUIET_MODULES;
            bool dark = qx >= 0 && qy >= 0 && qx < size && qy < size && esp_qrcode_get_module(qr, qx, qy);
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++) row[sy * px + mx * scale + sx] = dark ? on : off;
        }
        display_draw(x0, SETUP_QR_TOP_Y + my * scale, px, scale, row);
    }
    free(row);
}

void ui_setup(const char *ap_ssid)
{
    char payload[96];
    // Standard Wi-Fi QR: the camera app offers to join the open network.
    snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", ap_ssid);
    display_fill(0);
    draw_text(SETUP_TITLE_Y, "WI-FI SETUP");
    esp_qrcode_config_t cfg = {
        .display_func_with_cb = draw_qr,
        .max_qrcode_version = QR_MAX_VERSION,
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
        .user_data = (void *)ap_ssid,  // non-NULL selects the callback form
    };
    if (esp_qrcode_generate(&cfg, payload) != ESP_OK) ESP_LOGE(TAG, "QR code not generated");
    draw_text(SETUP_HINT_Y, "Scan or join");
    draw_text(SETUP_NAME_Y, ap_ssid);
}

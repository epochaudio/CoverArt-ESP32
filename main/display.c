#include "display.h"

#include <string.h>
#include "board_interface.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "display";
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_bounce;
static SemaphoreHandle_t s_done;

#define BOUNCE_PIXELS (LCD_WIDTH * LCD_BOUNCE_ROWS)

static bool on_flushed(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}

void display_init(void)
{
    s_done = xSemaphoreCreateBinary();
    s_bounce = heap_caps_malloc(BOUNCE_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(s_bounce);
    s_panel = board_display_init(on_flushed, NULL);
    display_fill(0);
    board_display_set_brightness(100);
    ESP_LOGI(TAG, "%s panel %dx%d ready", BOARD_NAME, LCD_WIDTH, LCD_HEIGHT);
}

void display_set_brightness(uint8_t percent)
{
    board_display_set_brightness(percent);
}

static void push(int x, int y, int w, int h)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, s_bounce));
    xSemaphoreTake(s_done, portMAX_DELAY);
}

void display_fill(uint16_t color_be)
{
    for (int i = 0; i < BOUNCE_PIXELS; i++) s_bounce[i] = color_be;
    for (int y = 0; y < LCD_HEIGHT; y += LCD_BOUNCE_ROWS) {
        int rows = LCD_HEIGHT - y < LCD_BOUNCE_ROWS ? LCD_HEIGHT - y : LCD_BOUNCE_ROWS;
        push(0, y, LCD_WIDTH, rows);
    }
}

static int align_down(int v) { return v - v % LCD_DRAW_ALIGN; }
static int align_up(int v) { return align_down(v + LCD_DRAW_ALIGN - 1); }
static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// Panels with a drawing grid (LCD_DRAW_ALIGN > 1) get the area widened to the
// grid; the added border repeats the nearest edge pixel of the source, so the
// at most LCD_DRAW_ALIGN - 1 extra pixels per side match their neighbours.
void display_draw(int x, int y, int w, int h, const uint16_t *pixels_be)
{
    const int x0 = align_down(x), y0 = align_down(y);
    const int x1 = align_up(x + w) < LCD_WIDTH ? align_up(x + w) : LCD_WIDTH;
    const int y1 = align_up(y + h) < LCD_HEIGHT ? align_up(y + h) : LCD_HEIGHT;
    const int out_w = x1 - x0;
    const int max_rows = align_down(BOUNCE_PIXELS / out_w);
    const bool padded = x0 != x || out_w != w || y0 != y || y1 != y + h;

    for (int top = y0; top < y1; top += max_rows) {
        const int rows = y1 - top < max_rows ? y1 - top : max_rows;
        for (int r = 0; r < rows; r++) {
            const int src_row = clamp(top + r, y, y + h - 1) - y;
            const uint16_t *src = pixels_be + (size_t)src_row * w;
            uint16_t *dst = s_bounce + (size_t)r * out_w;
            if (!padded) {
                memcpy(dst, src, (size_t)w * sizeof(uint16_t));
            } else {
                for (int c = 0; c < out_w; c++) dst[c] = src[clamp(x0 + c, x, x + w - 1) - x];
            }
        }
        push(x0, top, out_w, rows);
    }
}

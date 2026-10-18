#include "cover.h"

#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/tjpgd.h"

static const char *TAG = "cover";

// The ROM TJpgDec (baseline JPEG, RGB888 output) needs ~3.1 KB of work pool.
#define TJPGD_POOL_BYTES   3200
// MCUs are at most 16x16 pixels (4:2:0 subsampling).
#define MCU_MAX_ROWS       16

typedef struct {
    const uint8_t *jpeg;
    size_t len, pos;
    uint16_t *strip;   // one MCU row of the image, panel-order RGB565
    int x0, y0;        // image origin on the panel
} decode_ctx_t;

static UINT read_input(JDEC *jd, BYTE *buf, UINT n)
{
    decode_ctx_t *ctx = jd->device;
    if (n > ctx->len - ctx->pos) n = ctx->len - ctx->pos;
    if (buf) memcpy(buf, ctx->jpeg + ctx->pos, n);
    ctx->pos += n;
    return n;
}

// Called per decoded block; blocks arrive left to right, one MCU row at a
// time. The row is drawn once its rightmost block is in.
static UINT write_block(JDEC *jd, void *bitmap, JRECT *r)
{
    decode_ctx_t *ctx = jd->device;
    const uint8_t *rgb = bitmap;
    const int w = r->right - r->left + 1;
    const int strip_top = r->top - r->top % (jd->msy * 8);
    for (int y = r->top; y <= r->bottom; y++) {
        uint16_t *dst = ctx->strip + (size_t)(y - strip_top) * jd->width + r->left;
        for (int x = 0; x < w; x++, rgb += 3) dst[x] = display_color(rgb[0], rgb[1], rgb[2]);
    }
    if (r->right == jd->width - 1) {
        display_draw(ctx->x0, ctx->y0 + strip_top, jd->width, r->bottom - strip_top + 1, ctx->strip);
    }
    return 1;
}

void cover_clear(void)
{
    display_fill(0);
}

bool cover_show_jpeg(const uint8_t *jpeg, size_t len)
{
    int64_t t0 = esp_timer_get_time();
    decode_ctx_t ctx = {.jpeg = jpeg, .len = len};
    void *pool = malloc(TJPGD_POOL_BYTES);
    JDEC jd;
    bool ok = false;
    if (!pool) return false;

    JRESULT res = jd_prepare(&jd, read_input, pool, TJPGD_POOL_BYTES, &ctx);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "not a decodable JPEG (%u bytes): %d", (unsigned)len, res);
        goto out;
    }
    if (jd.width > LCD_WIDTH || jd.height > LCD_HEIGHT) {
        ESP_LOGE(TAG, "cover %ux%u exceeds panel", jd.width, jd.height);
        goto out;
    }
    ctx.strip = heap_caps_malloc((size_t)jd.width * MCU_MAX_ROWS * sizeof(uint16_t), MALLOC_CAP_INTERNAL);
    if (!ctx.strip) goto out;
    ctx.x0 = (LCD_WIDTH - jd.width) / 2;
    ctx.y0 = (LCD_HEIGHT - jd.height) / 2;
    // A cover smaller than the panel leaves borders from the previous one.
    if (jd.width < LCD_WIDTH || jd.height < LCD_HEIGHT) display_fill(0);

    res = jd_decomp(&jd, write_block, 0);
    ok = res == JDR_OK;
    if (ok) {
        ESP_LOGI(TAG, "shown %ux%u from %u B jpeg in %lld ms", jd.width, jd.height, (unsigned)len,
                 (esp_timer_get_time() - t0) / 1000);
    } else {
        ESP_LOGE(TAG, "decode failed: %d", res);
    }
    free(ctx.strip);
out:
    free(pool);
    return ok;
}

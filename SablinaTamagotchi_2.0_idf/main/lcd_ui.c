// lcd_ui.c — raw SPI driver matching TFT_eSPI rotation=1 exactly
// NO esp_lcd framework. Same MADCTL + CASET/RASET logic as Arduino.
//
// Pins: MOSI=45 SCLK=40 CS=42 DC=41 RST=39 BL=46
// Landscape 320×172: MADCTL=0x68 (MX|MV|BGR), CASET no offset, RASET offset=34
// TFT_eSPI ST7789_Rotation.h rotation=1 _init_width=172: colstart=0, rowstart=34

#include "lcd_ui.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG_UI = "LCD_UI";

/* ── Pins ───────────────────────────────────────────────────────── */
#define PIN_MOSI  45
#define PIN_SCLK  40
#define PIN_CS    42
#define PIN_DC    41
#define PIN_RST   39
#define PIN_BL    46
#define SPI_FREQ  40000000

/* ── Screen dimensions (landscape) ─────────────────────────────── */
#define SCR_W     320
#define SCR_H     172
#define CHAR_W      8
#define CHAR_H      8
#define ROW_OFFSET 34   /* TFT_eSPI rowstart=34 for rotation=1, _init_width=172 */

/* ── SPI handle ─────────────────────────────────────────────────── */
static spi_device_handle_t s_spi = NULL;

/* ── Strip buffer (SCR_W × CHAR_H pixels, internal SRAM, DMA-safe) */
static uint16_t s_strip[SCR_W * CHAR_H];

/* ── Small buffers for SPI commands/params (must be in DRAM for DMA) */
static uint8_t s_cmd_buf[1];
static uint8_t s_dat_buf[64];

/* ── SPI pre-transfer: set DC line ──────────────────────────────── */
static void IRAM_ATTR spi_pre_cb(spi_transaction_t *t)
{
    gpio_set_level(PIN_DC, (int)t->user);
}

/* ── Low-level SPI send ─────────────────────────────────────────── */
static void lcd_cmd(uint8_t cmd)
{
    s_cmd_buf[0] = cmd;
    spi_transaction_t t = {
        .length    = 8,
        .tx_buffer = s_cmd_buf,
        .user      = (void *)0,   /* DC=0 */
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void lcd_data(const uint8_t *data, size_t len)
{
    if (!len) return;
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = data,
        .user      = (void *)1,   /* DC=1 */
    };
    spi_device_polling_transmit(s_spi, &t);
}

/* helper for short inline data */
#define DWRITE(...) do { \
    static const uint8_t _d[] = {__VA_ARGS__}; \
    memcpy(s_dat_buf, _d, sizeof(_d)); \
    lcd_data(s_dat_buf, sizeof(_d)); \
} while(0)

/* ── Reset ──────────────────────────────────────────────────────── */
static void lcd_reset(void)
{
    gpio_set_level(PIN_CS,  0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_CS,  1);
}

/* ── Init sequence: exact Waveshare Arduino demo, landscape MADCTL ─ */
static void lcd_init_seq(void)
{
    lcd_cmd(0x11);                          /* SLPOUT */
    vTaskDelay(pdMS_TO_TICKS(120));

    /* MADCTL=0x68: MX(0x40)|MV(0x20)|BGR(0x08) = landscape, matches TFT_eSPI rotation=1 */
    lcd_cmd(0x36); DWRITE(0x68);
    lcd_cmd(0x3A); DWRITE(0x05);            /* COLMOD: 16bpp RGB565 */

    lcd_cmd(0xB0); DWRITE(0x00, 0xE8);
    lcd_cmd(0xB2); DWRITE(0x0C, 0x0C, 0x00, 0x33, 0x33);
    lcd_cmd(0xB7); DWRITE(0x35);
    lcd_cmd(0xBB); DWRITE(0x35);
    lcd_cmd(0xC0); DWRITE(0x2C);
    lcd_cmd(0xC2); DWRITE(0x01);
    lcd_cmd(0xC3); DWRITE(0x13);
    lcd_cmd(0xC4); DWRITE(0x20);
    lcd_cmd(0xC6); DWRITE(0x0F);
    lcd_cmd(0xD0); DWRITE(0xA4, 0xA1);
    lcd_cmd(0xD6); DWRITE(0xA1);

    /* Gamma — exact Waveshare values */
    lcd_cmd(0xE0); DWRITE(0xF0,0x00,0x04,0x04,0x04,0x05,0x29,0x33,0x3E,0x38,0x12,0x12,0x28,0x30);
    lcd_cmd(0xE1); DWRITE(0xF0,0x07,0x0A,0x0D,0x0B,0x07,0x28,0x33,0x3E,0x36,0x14,0x14,0x29,0x32);

    lcd_cmd(0x21);                          /* INVON  */
    lcd_cmd(0x11);                          /* SLPOUT again (Waveshare does this) */
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_cmd(0x29);                          /* DISPON */
}

/* ── Set window + RAMWR ─────────────────────────────────────────── */
/* Exact match of TFT_eSPI rotation=1 for ST7789, _init_width=172:
   - MADCTL=0x68 (MX|MV|BGR), colstart=0, rowstart=34
   - CASET = x + 0   (landscape X, no offset)
   - RASET = y + 34  (landscape Y + rowstart)
   Source: TFT_eSPI/TFT_Drivers/ST7789_Rotation.h case 1 _init_width==172 */
static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_cmd(0x2A);  /* CASET — no offset (TFT_eSPI colstart=0) */
    s_dat_buf[0] = x0 >> 8; s_dat_buf[1] = x0 & 0xFF;
    s_dat_buf[2] = x1 >> 8; s_dat_buf[3] = x1 & 0xFF;
    lcd_data(s_dat_buf, 4);

    uint16_t r0 = y0 + ROW_OFFSET, r1 = y1 + ROW_OFFSET;
    lcd_cmd(0x2B);  /* RASET — add ROW_OFFSET=34 (TFT_eSPI rowstart=34) */
    s_dat_buf[0] = r0 >> 8; s_dat_buf[1] = r0 & 0xFF;
    s_dat_buf[2] = r1 >> 8; s_dat_buf[3] = r1 & 0xFF;
    lcd_data(s_dat_buf, 4);

    lcd_cmd(0x2C);  /* RAMWR */
}

/* ── Send pixel buffer to window ────────────────────────────────── */
/* Strip is uint16_t; ST7789 with RAMCTRL 0xB0[3]=1 (little-endian)
   expects low byte first — which is how ESP32 stores uint16_t in memory.
   No byte-swap needed. */
static void lcd_push_pixels(const uint16_t *buf, size_t npix)
{
    spi_transaction_t t = {
        .length    = npix * 16,
        .tx_buffer = buf,
        .user      = (void *)1,   /* DC=1 */
    };
    spi_device_polling_transmit(s_spi, &t);
}

/* ── Push strip: one 8-line band starting at screen row y_top ────── */
static void strip_push(int y_top, int nrows)
{
    lcd_set_window(0, (uint16_t)y_top, SCR_W - 1, (uint16_t)(y_top + nrows - 1));
    lcd_push_pixels(s_strip, (size_t)SCR_W * nrows);
}

/* Push only an x-subregion of the strip to avoid leaking stale pixels. */
static void strip_push_region(int x0, int x1, int y_top, int nrows)
{
    if (x0 < 0) x0 = 0;
    if (x1 >= SCR_W) x1 = SCR_W - 1;
    if (x0 > x1 || nrows <= 0) return;

    int w = x1 - x0 + 1;
    for (int sy = 0; sy < nrows; sy++) {
        int y = y_top + sy;
        lcd_set_window((uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y);
        lcd_push_pixels(&s_strip[sy * SCR_W + x0], (size_t)w);
    }
}

/* ── Backlight ──────────────────────────────────────────────────── */
static void bl_init(void)
{
    ledc_timer_config_t tmr = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz         = 5000,
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_1,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tmr);
    ledc_channel_config_t ch = {
        .channel    = LEDC_CHANNEL_1,
        .duty       = 0,
        .gpio_num   = PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_1,
    };
    ledc_channel_config(&ch);
    uint32_t max = (1u << 13) - 1;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, (max * 80) / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ── Palette ────────────────────────────────────────────────────── */
/* RAMCTRL 0xB0 byte2=0xE8 sets little-endian: display expects low byte first.
   ESP32 stores uint16_t little-endian in memory, so NO byte-swap needed.
   Just encode as normal RGB565. */
static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
}

static uint16_t g_black, g_white, g_cyan, g_yellow, g_red,
                g_green, g_dkgray, g_orange, g_dkblue;

static void init_palette(void)
{
    g_black  = rgb(  0,   0,   0);
    g_white  = rgb(255, 255, 255);
    g_cyan   = rgb(  0, 210, 210);
    g_yellow = rgb(255, 210,   0);
    g_red    = rgb(220,  40,  40);
    g_green  = rgb( 40, 200,  40);
    g_dkgray = rgb( 35,  35,  35);
    g_orange = rgb(255, 130,   0);
    g_dkblue = rgb(  0,   0,  70);
}

/* ── 8×8 IBM PC font, bit7=leftmost pixel ───────────────────────── */
static const uint8_t s_font[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
    {0x18,0x18,0x09,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x0C,0x0C,0x06,0x00},
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00},
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00,0x00},
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x06,0x00},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    {0x00,0x00,0x3F,0x00,0x3F,0x00,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00},
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00},
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* ── Arduino layout constants (match config.h) ──────────────────── */
/* Landscape: 320×172. Same as Arduino SCREEN_W=320, SCREEN_H=172. */
#define GAME_X    1       /* game canvas left edge */
#define GAME_Y    14      /* game canvas top edge (below top-bar) */
#define GAME_W    234     /* game canvas width */
#define GAME_H    157     /* game canvas height = 172-14-1 */
#define SIDEBAR_X 236     /* sidebar left edge */
#define SIDEBAR_W 84      /* sidebar width */
#define TOPBAR_H  14      /* top bar height in pixels */

/* Arduino maingif fallback path: sablinagif[][10000], 41 frames, 100×100. */
#define FACE_SRC_W   100
#define FACE_SRC_H   100
#define FACE_FRAMES  41
#define FACE_FRAME_PIX   ((size_t)FACE_SRC_W * FACE_SRC_H)
#define FACE_FRAME_BYTES (FACE_FRAME_PIX * 2)
/* Virtual position in the 128×128 game space (same as Arduino maingif fallback). */
#define FACE_VX   14
#define FACE_VY   14

/* Face animation state */
static FILE           *s_face_file  = NULL;
static uint16_t       *s_face_buf   = NULL;
static int             s_face_frame = 0;
static uint32_t        s_face_next_ms = 0;
#define FACE_DELAY_MS  150

/* ── Strip helpers ──────────────────────────────────────────────── */
static void strip_fill(uint16_t c)
{
    for (int i = 0; i < SCR_W * CHAR_H; i++) s_strip[i] = c;
}

static void strip_draw_char_row(int x, int sy, char ch, uint16_t fg, uint16_t bg)
{
    uint8_t idx = (uint8_t)ch;
    if (idx < 32 || idx > 127) idx = 32;
    uint8_t bits = s_font[idx - 32][sy];
    for (int i = 0; i < CHAR_W; i++) {
        int px = x + i;
        if (px < 0 || px >= SCR_W) continue;
        s_strip[sy * SCR_W + px] = (bits & (0x80 >> i)) ? fg : bg;
    }
}

/* Render one text strip row at y_screen covering x0..x1 (inclusive).
   Pixels outside the text are filled with bg. */
static void render_text_strip_region(int x0, int x1, int y_screen,
                                     const char *text, uint16_t fg, uint16_t bg)
{
    /* fill whole strip with bg first */
    for (int sy = 0; sy < CHAR_H; sy++)
        for (int x = x0; x <= x1 && x < SCR_W; x++)
            s_strip[sy * SCR_W + x] = bg;
    if (text) {
        int cx = x0;
        while (*text && cx + CHAR_W - 1 <= x1) {
            for (int sy = 0; sy < CHAR_H; sy++)
                strip_draw_char_row(cx, sy, *text, fg, bg);
            cx += CHAR_W;
            text++;
        }
    }
    strip_push_region(x0, x1, y_screen, CHAR_H);
}

/* Render a full-width text strip */
static void render_text_strip(int x0, int y_screen, const char *text,
                               uint16_t fg, uint16_t bg)
{
    render_text_strip_region(x0, SCR_W - 1, y_screen, text, fg, bg);
}

/* Word-wrap text into region [x0..x1, y0..y_max), 8px rows, bg-filled */
static void render_wrapped_region(int x0, int x1, int y0, int y_max,
                                   const char *text, uint16_t fg, uint16_t bg)
{
    int max_cols = (x1 - x0 + 1) / CHAR_W;
    int y = y0;
    const char *p = text ? text : "";

    while (y + CHAR_H <= y_max) {
        if (!*p) {
            /* fill remaining with bg */
            for (int sy = 0; sy < CHAR_H; sy++)
                for (int x = x0; x <= x1 && x < SCR_W; x++)
                    s_strip[sy * SCR_W + x] = bg;
            strip_push_region(x0, x1, y, CHAR_H);
            y += CHAR_H;
            continue;
        }
        /* find how many chars fit */
        int len = 0, last_sp = -1;
        const char *q = p;
        while (*q && len < max_cols) {
            if (*q == '\n') break;
            if (*q == ' ') last_sp = len;
            q++; len++;
        }
        int take = len;
        if (*q && *q != '\n' && last_sp > 0) take = last_sp;
        char line[41];
        int li = 0;
        for (int i = 0; i < take && p[i] && li < 40; i++) line[li++] = p[i];
        line[li] = '\0';

        render_text_strip_region(x0, x1, y, line, fg, bg);
        y += CHAR_H;
        p += take;
        if (*p == '\n' || *p == ' ') p++;
    }
}

/* ── Face animation (nearest-neighbour scaled from FACE_SRC_W×FACE_SRC_H) ─
   Destination area: GAME_X..GAME_X+GAME_W-1, GAME_Y..GAME_Y+GAME_H-1
    Source virtual rect: vx=FACE_VX, vy=FACE_VY, vw=FACE_SRC_W, vh=FACE_SRC_H
   in 128×128 virtual space scaled to GAME_W×GAME_H.
    This matches Arduino pushImageScaled exactly. */
static void face_load_frame(int frame)
{
    if (!s_face_file || !s_face_buf) return;
    long offset = (long)frame * (long)FACE_FRAME_BYTES;
    if (fseek(s_face_file, offset, SEEK_SET) != 0) return;
    fread(s_face_buf, 2, FACE_FRAME_PIX, s_face_file);
}

static FILE *open_face_asset(void)
{
    static const char *k_paths[] = {
        "/data/sablinagif_100x100.rgb565",
        "/data/faces/sablinagif_100x100.rgb565",
    };
    for (size_t i = 0; i < sizeof(k_paths) / sizeof(k_paths[0]); i++) {
        FILE *f = fopen(k_paths[i], "rb");
        if (f) {
            ESP_LOGI(TAG_UI, "Face file opened: %s", k_paths[i]);
            return f;
        }
    }
    return NULL;
}

/* Draw current face frame into the game area via strip-by-strip SPI. */
static void face_draw_to_display(void)
{
    if (!s_face_buf) return;

    /* Destination rect on screen (nearest-neighbour scaled from virt 0..127 space) */
    int dst_x0 = GAME_X + (int32_t)FACE_VX * GAME_W / 128;
    int dst_y0 = GAME_Y + (int32_t)FACE_VY * GAME_H / 128;
     int dst_x1 = GAME_X + (int32_t)(FACE_VX + FACE_SRC_W) * GAME_W / 128 - 1;
     int dst_y1 = GAME_Y + (int32_t)(FACE_VY + FACE_SRC_H) * GAME_H / 128 - 1;

    int dw = dst_x1 - dst_x0 + 1;
    int dh = dst_y1 - dst_y0 + 1;
    if (dw <= 0 || dh <= 0) return;

    /* Send row by row, scaling source rows */
    lcd_set_window((uint16_t)dst_x0, (uint16_t)dst_y0, (uint16_t)dst_x1, (uint16_t)dst_y1);
    for (int dy = 0; dy < dh; dy++) {
        int sy = (int32_t)dy * FACE_SRC_H / dh;
        const uint16_t *src_row = s_face_buf + (size_t)sy * FACE_SRC_W;
        /* Build one row in s_strip (reuse first dw cells) */
        for (int dx = 0; dx < dw && dx < SCR_W; dx++) {
            int sx = (int32_t)dx * FACE_SRC_W / dw;
            s_strip[dx] = src_row[sx];
        }
        lcd_push_pixels(s_strip, (size_t)dw);
    }
}

/* ── Sidebar drawing ────────────────────────────────────────────── */
/* Draws stats sidebar at SIDEBAR_X, height SCREEN_H using 8×8 font. */
static void draw_sidebar(const char *name, int hunger, int rest, int clean, int coins)
{
    /* Each sidebar text row: render a strip 8px tall into the sidebar band */
    int sx = SIDEBAR_X + 2;
    int sw = SCR_W - 1;

    /* row 0: name */
    char sbuf[16];
    snprintf(sbuf, sizeof(sbuf), "%.10s", name ? name : "SABLINA");
    render_text_strip_region(sx, sw, 0, sbuf, g_cyan, g_dkblue);

    /* rows 1-4: H/R/C/$ bars */
    struct { const char *lbl; int val; uint16_t fg; } stats[] = {
        { "H", hunger, (hunger > 50) ? g_green : (hunger > 25) ? g_yellow : g_red },
        { "R", rest,   (rest   > 50) ? g_green : (rest   > 25) ? g_yellow : g_red },
        { "C", clean,  (clean  > 50) ? g_green : (clean  > 25) ? g_yellow : g_red },
        { "$", coins,  g_orange },
    };
    for (int i = 0; i < 4; i++) {
        char row[14];
        /* label + value: "H:100" */
        snprintf(row, sizeof(row), "%s:%3d", stats[i].lbl, stats[i].val);
        render_text_strip_region(sx, sw, 8 * (1 + i), row, stats[i].fg, g_black);
    }

    /* rows 5..20: fill rest with black */
    for (int r = 5; r <= 21; r++) {
        int y = 8 * r;
        if (y + CHAR_H > SCR_H) break;
        strip_fill(g_black);
        strip_push(y, CHAR_H);
    }
    /* vertical divider line — skip for now (requires pixel-level push) */
}

/* ── Thought bubble (drawn in game area BELOW face) ─────────────── */
/* Rows from thought_y to game bottom, left of sidebar */
static void draw_thought(const char *thought, const char *tool, const char *result)
{
    /* thought area: x0=GAME_X, x1=GAME_X+GAME_W-1, y from bubble_y downward */
    int bubble_y = GAME_Y + (int32_t)96 * GAME_H / 128;  /* same clip as Arduino */
    int bottom   = GAME_Y + GAME_H;
    int x0 = GAME_X;
    int x1 = SIDEBAR_X - 1;

    /* tool line at top of bubble */
    if (tool && tool[0]) {
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "[%s]", tool);
        render_text_strip_region(x0, x1, bubble_y, tbuf, g_cyan, g_black);
        bubble_y += CHAR_H;
    }

    /* word-wrapped thought */
    render_wrapped_region(x0, x1, bubble_y, bottom - CHAR_H, thought, g_white, g_black);

    /* result at very bottom */
    {
        char rbuf[40] = "";
        if (result && result[0]) snprintf(rbuf, sizeof(rbuf), "%.39s", result);
        render_text_strip_region(x0, x1, bottom - CHAR_H, rbuf, g_orange, g_dkgray);
    }
}

/* ── Top bar ────────────────────────────────────────────────────── */
static void draw_topbar(const char *tool)
{
    char tbuf[40];
    snprintf(tbuf, sizeof(tbuf), "SABLINA  %s", tool ? tool : "");
    render_text_strip_region(0, SCR_W - 1, 0, tbuf, g_white, g_dkblue);
    /* second row of top bar */
    render_text_strip_region(0, SCR_W - 1, 8, "", 0, g_dkblue);
}

/* ── Black-fill game area between face and bubble ────────────────── */
static void clear_game_below_face(void)
{
    int face_bottom = GAME_Y + (int32_t)(FACE_VY + FACE_SRC_H) * GAME_H / 128;
    int bubble_y    = GAME_Y + (int32_t)96 * GAME_H / 128;
    if (face_bottom >= bubble_y) return;
    strip_fill(g_black);
    for (int y = face_bottom; y < bubble_y && y + CHAR_H <= SCR_H; y += CHAR_H) {
        int rows = (y + CHAR_H <= bubble_y) ? CHAR_H : (bubble_y - y);
        strip_push(y, rows);
    }
}

/* ── Game area above face (top few pixels) ───────────────────────── */
static void clear_game_above_face(void)
{
    int face_top = GAME_Y + (int32_t)FACE_VY * GAME_H / 128;
    strip_fill(g_black);
    for (int y = GAME_Y; y < face_top && y < SCR_H; y += CHAR_H) {
        int rows = ((y + CHAR_H) <= face_top) ? CHAR_H : (face_top - y);
        strip_push(y, rows);
    }
}

/* ── Public: init ───────────────────────────────────────────────── */
void lcd_ui_init(void)
{
    init_palette();

    /* Configure DC, RST as output GPIOs */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_DC) | (1ULL << PIN_RST),
        .pull_up_en = 0, .pull_down_en = 0, .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* SPI bus — max transfer large enough for one face row */
    spi_bus_config_t buscfg = {
        .mosi_io_num     = PIN_MOSI,
        .sclk_io_num     = PIN_SCLK,
        .miso_io_num     = -1,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = SCR_W * FACE_SRC_H * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_FREQ,
        .mode           = 0,
        .spics_io_num   = PIN_CS,
        .queue_size     = 1,
        .pre_cb         = spi_pre_cb,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &s_spi));

    lcd_reset();
    lcd_init_seq();

    /* Clear screen to black */
    strip_fill(g_black);
    for (int y = 0; y < SCR_H; y += CHAR_H) {
        int rows = ((y + CHAR_H) <= SCR_H) ? CHAR_H : (SCR_H - y);
        strip_push(y, rows);
    }

    bl_init();
    ESP_LOGI(TAG_UI, "Display ready %dx%d, BL=GPIO%d", SCR_W, SCR_H, PIN_BL);

    s_face_file = open_face_asset();
    if (!s_face_file) {
        ESP_LOGW(TAG_UI, "Face file not found in SPIFFS");
    } else {
        s_face_buf = heap_caps_malloc(FACE_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_face_buf) s_face_buf = heap_caps_malloc(FACE_FRAME_BYTES, MALLOC_CAP_DEFAULT);
        if (!s_face_buf) {
            ESP_LOGW(TAG_UI, "No RAM for sablinagif frame buffer");
            fclose(s_face_file);
            s_face_file = NULL;
        } else {
            ESP_LOGI(TAG_UI, "Using sablinagif SPIFFS clip: %d frames, %dx%d", FACE_FRAMES, FACE_SRC_W, FACE_SRC_H);
        }
    }
}

/* ── Public: update ─────────────────────────────────────────────── */
void lcd_ui_update(int hunger, int rest, int clean, int coins,
                   const char *tool, const char *thought, const char *result)
{
    if (!s_spi) return;

    (void)tool;
    (void)thought;
    (void)result;

    /* 1. Face animation (priority render path) */
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (now >= s_face_next_ms) {
        face_load_frame(s_face_frame);
        s_face_frame = (s_face_frame + 1) % FACE_FRAMES;
        s_face_next_ms = now + FACE_DELAY_MS;
        clear_game_above_face();
        face_draw_to_display();
        clear_game_below_face();
    }

    /* 2. Sidebar */
    draw_sidebar("SABLINA", hunger, rest, clean, coins);
}

/* ── Weak hook override ─────────────────────────────────────────── */
void mikuru_ui_show_floating_popup(const char *text, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (!text || !s_spi) return;
    int bubble_y = GAME_Y + (int32_t)96 * GAME_H / 128;
    int bottom   = GAME_Y + GAME_H;
    render_wrapped_region(GAME_X, SIDEBAR_X - 1, bubble_y, bottom - CHAR_H, text, g_white, g_black);
}

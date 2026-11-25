/*******************************************************************************
 * Size: 5 px (3x5 glyphs, 3 px advance)
 * Bpp: 1
 * Generated from the Tom Thumb 3x5 bitmap font
 * (BSD-3-Clause, Copyright 1999 Brian J. Swetland / Vassilii Khachaturov)
 ******************************************************************************/

#include "lvgl.h"

#ifndef LV_FONT_TERMINAL_3X5
#define LV_FONT_TERMINAL_3X5 1
#endif

#if LV_FONT_TERMINAL_3X5

/*-----------------
 *    BITMAPS
 *----------------*/

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    0x00, 0xE8, 0xB4, 0xBE, 0xFA, 0x79, 0xE4, 0x85, 0x42, 0xDB, 0xD6, 0xC0,
    0x6A, 0x40, 0x95, 0x80, 0xAA, 0x80, 0x5D, 0x00, 0x60, 0xE0, 0x80, 0x25,
    0x48, 0x76, 0xDC, 0x75, 0x40, 0xC5, 0x4E, 0xC5, 0x1C, 0xB7, 0x92, 0xF3,
    0x1C, 0x73, 0xDE, 0xE5, 0x48, 0xF7, 0xDE, 0xF7, 0x9C, 0xA0, 0x46, 0x2A,
    0x22, 0xE3, 0x80, 0x88, 0xA8, 0xE5, 0x04, 0x57, 0xC6, 0x57, 0xDA, 0xD7,
    0x5C, 0x72, 0x46, 0xD6, 0xDC, 0xF3, 0xCE, 0xF3, 0xC8, 0x73, 0xD6, 0xB7,
    0xDA, 0xE9, 0x2E, 0x24, 0xD4, 0xB7, 0x5A, 0x92, 0x4E, 0xBF, 0xDA, 0xBF,
    0xFA, 0x56, 0xD4, 0xD7, 0x48, 0x56, 0xF6, 0xD7, 0xEA, 0x71, 0x1C, 0xE9,
    0x24, 0xB6, 0xD6, 0xB6, 0xA4, 0xB7, 0xFA, 0xB5, 0x5A, 0xB5, 0x24, 0xE5,
    0x4E, 0xF2, 0x4E, 0x88, 0x80, 0xE4, 0x9E, 0x54, 0xE0, 0x90, 0xCE, 0xF0,
    0x9A, 0xDC, 0x72, 0x30, 0x2E, 0xD6, 0x77, 0x30, 0x2B, 0xA4, 0x77, 0x94,
    0x9A, 0xDA, 0xB8, 0x20, 0x9A, 0x80, 0x97, 0x6A, 0xC9, 0x2E, 0xFF, 0xD0,
    0xD6, 0xD0, 0x56, 0xA0, 0xD6, 0xE8, 0x76, 0xB2, 0x72, 0x40, 0x79, 0xE0,
    0x5D, 0x26, 0xB6, 0xB0, 0xB7, 0xA0, 0xBF, 0xF0, 0xA9, 0x50, 0xB5, 0x94,
    0xEF, 0x70, 0x6A, 0x26, 0xD8, 0xC8, 0xAC, 0x78,
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,
        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 48, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 4}, /* ' ' */
    {.bitmap_index = 1, .adv_w = 48, .box_w = 1, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '!' */
    {.bitmap_index = 2, .adv_w = 48, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3}, /* '"' */
    {.bitmap_index = 3, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '#' */
    {.bitmap_index = 5, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '$' */
    {.bitmap_index = 7, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '%' */
    {.bitmap_index = 9, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '&' */
    {.bitmap_index = 11, .adv_w = 48, .box_w = 1, .box_h = 2, .ofs_x = 0, .ofs_y = 3}, /* "'" */
    {.bitmap_index = 12, .adv_w = 48, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '(' */
    {.bitmap_index = 14, .adv_w = 48, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* ')' */
    {.bitmap_index = 16, .adv_w = 48, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 2}, /* '*' */
    {.bitmap_index = 18, .adv_w = 48, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 1}, /* '+' */
    {.bitmap_index = 20, .adv_w = 48, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 0}, /* ',' */
    {.bitmap_index = 21, .adv_w = 48, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 2}, /* '-' */
    {.bitmap_index = 22, .adv_w = 48, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0}, /* '.' */
    {.bitmap_index = 23, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '/' */
    {.bitmap_index = 25, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '0' */
    {.bitmap_index = 27, .adv_w = 48, .box_w = 2, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '1' */
    {.bitmap_index = 29, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '2' */
    {.bitmap_index = 31, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '3' */
    {.bitmap_index = 33, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '4' */
    {.bitmap_index = 35, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '5' */
    {.bitmap_index = 37, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '6' */
    {.bitmap_index = 39, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '7' */
    {.bitmap_index = 41, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '8' */
    {.bitmap_index = 43, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '9' */
    {.bitmap_index = 45, .adv_w = 48, .box_w = 1, .box_h = 3, .ofs_x = 0, .ofs_y = 1}, /* ':' */
    {.bitmap_index = 46, .adv_w = 48, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* ';' */
    {.bitmap_index = 47, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '<' */
    {.bitmap_index = 49, .adv_w = 48, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 1}, /* '=' */
    {.bitmap_index = 51, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '>' */
    {.bitmap_index = 53, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '?' */
    {.bitmap_index = 55, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '@' */
    {.bitmap_index = 57, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'A' */
    {.bitmap_index = 59, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'B' */
    {.bitmap_index = 61, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'C' */
    {.bitmap_index = 63, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'D' */
    {.bitmap_index = 65, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'E' */
    {.bitmap_index = 67, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'F' */
    {.bitmap_index = 69, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'G' */
    {.bitmap_index = 71, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'H' */
    {.bitmap_index = 73, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'I' */
    {.bitmap_index = 75, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'J' */
    {.bitmap_index = 77, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'K' */
    {.bitmap_index = 79, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'L' */
    {.bitmap_index = 81, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'M' */
    {.bitmap_index = 83, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'N' */
    {.bitmap_index = 85, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'O' */
    {.bitmap_index = 87, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'P' */
    {.bitmap_index = 89, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'Q' */
    {.bitmap_index = 91, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'R' */
    {.bitmap_index = 93, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'S' */
    {.bitmap_index = 95, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'T' */
    {.bitmap_index = 97, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'U' */
    {.bitmap_index = 99, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'V' */
    {.bitmap_index = 101, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'W' */
    {.bitmap_index = 103, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'X' */
    {.bitmap_index = 105, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'Y' */
    {.bitmap_index = 107, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'Z' */
    {.bitmap_index = 109, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '[' */
    {.bitmap_index = 111, .adv_w = 48, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 1}, /* '\\' */
    {.bitmap_index = 113, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* ']' */
    {.bitmap_index = 115, .adv_w = 48, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3}, /* '^' */
    {.bitmap_index = 116, .adv_w = 48, .box_w = 3, .box_h = 1, .ofs_x = 0, .ofs_y = 0}, /* '_' */
    {.bitmap_index = 117, .adv_w = 48, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 3}, /* '`' */
    {.bitmap_index = 118, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'a' */
    {.bitmap_index = 120, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'b' */
    {.bitmap_index = 122, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'c' */
    {.bitmap_index = 124, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'd' */
    {.bitmap_index = 126, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'e' */
    {.bitmap_index = 128, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'f' */
    {.bitmap_index = 130, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -1}, /* 'g' */
    {.bitmap_index = 132, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'h' */
    {.bitmap_index = 134, .adv_w = 48, .box_w = 1, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'i' */
    {.bitmap_index = 135, .adv_w = 48, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = -1}, /* 'j' */
    {.bitmap_index = 138, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'k' */
    {.bitmap_index = 140, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 'l' */
    {.bitmap_index = 142, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'm' */
    {.bitmap_index = 144, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'n' */
    {.bitmap_index = 146, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'o' */
    {.bitmap_index = 148, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -1}, /* 'p' */
    {.bitmap_index = 150, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -1}, /* 'q' */
    {.bitmap_index = 152, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'r' */
    {.bitmap_index = 154, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 's' */
    {.bitmap_index = 156, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* 't' */
    {.bitmap_index = 158, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'u' */
    {.bitmap_index = 160, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'v' */
    {.bitmap_index = 162, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'w' */
    {.bitmap_index = 164, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'x' */
    {.bitmap_index = 166, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -1}, /* 'y' */
    {.bitmap_index = 168, .adv_w = 48, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 0}, /* 'z' */
    {.bitmap_index = 170, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '{' */
    {.bitmap_index = 172, .adv_w = 48, .box_w = 1, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '|' */
    {.bitmap_index = 173, .adv_w = 48, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = 0}, /* '}' */
    {.bitmap_index = 175, .adv_w = 48, .box_w = 3, .box_h = 2, .ofs_x = 0, .ofs_y = 3}, /* '~' */
};

#if LV_VERSION_CHECK(8, 0, 0)
static lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};

#if LV_VERSION_CHECK(8, 0, 0)
const lv_font_t lv_font_terminal_3x5 = {
#else
lv_font_t lv_font_terminal_3x5 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 6,
    .base_line = 1,
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -1,
    .underline_thickness = 0,
    .dsc = &font_dsc
};

#endif /* LV_FONT_TERMINAL_3X5 */

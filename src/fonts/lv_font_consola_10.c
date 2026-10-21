/*******************************************************************************
 * Size: 10 px
 * Bpp: 2
 * Opts: --font C:/Windows/Fonts/consola.ttf --size 10 --bpp 2 --format lvgl --lv-include lvgl.h --range 0x20-0x7F --no-compress -o C:/VSCODE/linkr-bmc/linkr-bee/src/fonts/lv_font_consola_10.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef LV_FONT_CONSOLA_10
#define LV_FONT_CONSOLA_10 1
#endif

#if LV_FONT_CONSOLA_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0xc8, 0x88, 0x80, 0xd0,

    /* U+0022 "\"" */
    0x9c, 0x9c, 0x0,

    /* U+0023 "#" */
    0x15, 0x47, 0xb8, 0x22, 0x7, 0xb8, 0x22, 0x2,
    0x20,

    /* U+0024 "$" */
    0x2, 0xa, 0xd6, 0x40, 0xb4, 0xa, 0x82, 0x37,
    0xf4, 0x20,

    /* U+0025 "%" */
    0x64, 0x88, 0x94, 0x26, 0x0, 0x80, 0x1a, 0x82,
    0x55, 0x82, 0x80,

    /* U+0026 "&" */
    0x2e, 0x3, 0x30, 0x36, 0x3, 0xc4, 0x9a, 0xc8,
    0x38, 0x3a, 0xc0,

    /* U+0027 "'" */
    0xd8, 0x0,

    /* U+0028 "(" */
    0x0, 0xc, 0x20, 0x20, 0x50, 0x90, 0x50, 0x30,
    0x24, 0x8, 0x0,

    /* U+0029 ")" */
    0x0, 0x60, 0x20, 0x8, 0xc, 0xc, 0xc, 0x18,
    0x30, 0x50, 0x0,

    /* U+002A "*" */
    0x8, 0xb, 0x92, 0xe0, 0x20, 0x0, 0x0,

    /* U+002B "+" */
    0x8, 0x0, 0x80, 0x7f, 0xc0, 0x80, 0x4, 0x0,

    /* U+002C "," */
    0x20, 0xd2, 0x64,

    /* U+002D "-" */
    0xbc,

    /* U+002E "." */
    0x20, 0xd0,

    /* U+002F "/" */
    0x1, 0x80, 0x80, 0x20, 0x24, 0xc, 0x5, 0x3,
    0x0, 0x80,

    /* U+0030 "0" */
    0x2b, 0x6, 0xc, 0x92, 0xca, 0x8c, 0x60, 0xc2,
    0xe0,

    /* U+0031 "1" */
    0x1d, 0x6, 0x40, 0x90, 0x24, 0x9, 0xf, 0xe0,

    /* U+0032 "2" */
    0x2f, 0x0, 0x60, 0x24, 0x18, 0x18, 0x1f, 0xe0,

    /* U+0033 "3" */
    0x2b, 0x0, 0x91, 0xb0, 0x6, 0x1, 0x8e, 0x80,

    /* U+0034 "4" */
    0x7, 0x0, 0xa0, 0x22, 0x5, 0x20, 0xbf, 0xc0,
    0x20,

    /* U+0035 "5" */
    0x3f, 0x4c, 0x3, 0xa0, 0x6, 0x1, 0x8a, 0x80,

    /* U+0036 "6" */
    0xa, 0x4c, 0x7, 0xa5, 0x83, 0x20, 0xca, 0x90,

    /* U+0037 "7" */
    0x7f, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x9, 0x0,

    /* U+0038 "8" */
    0x2b, 0x48, 0x22, 0xa0, 0x99, 0x60, 0xca, 0xd0,

    /* U+0039 "9" */
    0x2a, 0x18, 0x26, 0xc, 0xab, 0x1, 0x4a, 0x40,

    /* U+003A ":" */
    0xd4, 0x4, 0xd0,

    /* U+003B ";" */
    0x34, 0x40, 0x4, 0x38, 0x99, 0x0,

    /* U+003C "<" */
    0x0, 0x0, 0xc1, 0x80, 0xc0, 0xd, 0x0, 0x80,
    0x0,

    /* U+003D "=" */
    0x2a, 0x80, 0x2, 0xa8,

    /* U+003E ">" */
    0x0, 0xa0, 0x18, 0x9, 0x24, 0x90, 0x0,

    /* U+003F "?" */
    0x74, 0x9, 0x9, 0x38, 0x30, 0x0, 0x30,

    /* U+0040 "@" */
    0xa, 0x42, 0x8, 0x5a, 0x59, 0x65, 0xa3, 0x5a,
    0x74, 0x9a, 0x88, 0x0, 0x2a, 0x0,

    /* U+0041 "A" */
    0xe, 0x1, 0xb0, 0x22, 0x3, 0x18, 0x7f, 0xcc,
    0x9,

    /* U+0042 "B" */
    0x3b, 0x48, 0x63, 0xb4, 0x83, 0x20, 0xce, 0x80,

    /* U+0043 "C" */
    0x1f, 0x8c, 0x5, 0x1, 0x40, 0x70, 0x7, 0xe0,

    /* U+0044 "D" */
    0x7a, 0x5, 0xc, 0x50, 0xc5, 0xc, 0x51, 0xc7,
    0xe0,

    /* U+0045 "E" */
    0x3a, 0x4c, 0x3, 0xa4, 0xc0, 0x30, 0xf, 0xd0,

    /* U+0046 "F" */
    0xfd, 0xc0, 0xe9, 0xc0, 0xc0, 0xc0,

    /* U+0047 "G" */
    0x1f, 0x98, 0x9, 0x2e, 0x43, 0x60, 0xcb, 0xe0,

    /* U+0048 "H" */
    0x50, 0xd4, 0x37, 0xfd, 0x43, 0x50, 0xd4, 0x30,

    /* U+0049 "I" */
    0x2f, 0x42, 0x0, 0x80, 0x20, 0x8, 0xf, 0xd0,

    /* U+004A "J" */
    0x3f, 0x40, 0x90, 0x24, 0x9, 0x2, 0xf, 0x80,

    /* U+004B "K" */
    0x21, 0x82, 0x60, 0x38, 0x2, 0xc0, 0x23, 0x2,
    0x18,

    /* U+004C "L" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe,

    /* U+004D "M" */
    0x71, 0xcb, 0x2c, 0x9a, 0x88, 0xc8, 0x80, 0x88,
    0x8,

    /* U+004E "N" */
    0x70, 0xdd, 0x35, 0xcd, 0x57, 0x53, 0xd4, 0x70,

    /* U+004F "O" */
    0x2f, 0x46, 0xc, 0x80, 0x88, 0x8, 0x60, 0xc2,
    0xf0,

    /* U+0050 "P" */
    0x3b, 0x48, 0x32, 0xc, 0xe8, 0x20, 0x8, 0x0,

    /* U+0051 "Q" */
    0x2f, 0x46, 0xc, 0x90, 0x88, 0x8, 0x90, 0xc3,
    0xb4, 0x9, 0x0, 0x28,

    /* U+0052 "R" */
    0x3b, 0x42, 0x18, 0x3e, 0x2, 0x30, 0x21, 0x42,
    0xc,

    /* U+0053 "S" */
    0x2b, 0x58, 0x3, 0x80, 0x9, 0x0, 0xdf, 0xd0,

    /* U+0054 "T" */
    0x7f, 0xc0, 0x80, 0x8, 0x0, 0x80, 0x8, 0x0,
    0x80,

    /* U+0055 "U" */
    0x50, 0xd4, 0x35, 0xd, 0x43, 0x60, 0xca, 0xd0,

    /* U+0056 "V" */
    0xc0, 0x96, 0xc, 0x31, 0x82, 0x20, 0x1b, 0x0,
    0xd0,

    /* U+0057 "W" */
    0x80, 0x88, 0x8, 0x88, 0x89, 0xe8, 0x67, 0x87,
    0x1c,

    /* U+0058 "X" */
    0x60, 0xc2, 0x60, 0xe, 0x0, 0xe0, 0x32, 0x49,
    0xc,

    /* U+0059 "Y" */
    0x90, 0xc3, 0x18, 0x17, 0x0, 0xd0, 0x8, 0x0,
    0x80,

    /* U+005A "Z" */
    0x7f, 0xc0, 0x90, 0x60, 0x30, 0x30, 0x1f, 0xf0,

    /* U+005B "[" */
    0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
    0x78,

    /* U+005C "\\" */
    0x30, 0x8, 0x0, 0x80, 0x20, 0x5, 0x0, 0xc0,
    0x24, 0x2,

    /* U+005D "]" */
    0x6c, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0x6c,

    /* U+005E "^" */
    0xd, 0x8, 0x82, 0x8,

    /* U+005F "_" */
    0xaa, 0x90,

    /* U+0060 "`" */
    0x18, 0x0, 0x0,

    /* U+0061 "a" */
    0x2b, 0x0, 0x22, 0xa9, 0x86, 0x3a, 0x80,

    /* U+0062 "b" */
    0x20, 0x8, 0x2, 0xb4, 0xc3, 0x20, 0xc8, 0x23,
    0xa0,

    /* U+0063 "c" */
    0x1e, 0x4c, 0x2, 0x0, 0xc0, 0x1e, 0x40,

    /* U+0064 "d" */
    0x0, 0x80, 0x22, 0xb8, 0x82, 0x50, 0x98, 0x62,
    0x98,

    /* U+0065 "e" */
    0x2a, 0x8, 0x37, 0xa9, 0x80, 0x2e, 0x40,

    /* U+0066 "f" */
    0x7, 0x80, 0xc0, 0xc, 0x6, 0xe8, 0xc, 0x0,
    0xc0, 0xc, 0x0,

    /* U+0067 "g" */
    0x2b, 0xc2, 0x18, 0x3a, 0x6, 0x0, 0x3e, 0x45,
    0xc, 0x3a, 0x40,

    /* U+0068 "h" */
    0x20, 0x8, 0x3, 0xb4, 0xc2, 0x20, 0x88, 0x22,
    0x8,

    /* U+0069 "i" */
    0xd, 0x0, 0x2, 0xd0, 0x24, 0x9, 0x2, 0x42,
    0xf8,

    /* U+006A "j" */
    0x3, 0x0, 0x42, 0xb0, 0xc, 0x3, 0x0, 0xc0,
    0x30, 0xc, 0x6d, 0x0,

    /* U+006B "k" */
    0x20, 0x2, 0x0, 0x22, 0x82, 0xa0, 0x3c, 0x2,
    0x70, 0x21, 0x80,

    /* U+006C "l" */
    0x2d, 0x2, 0x40, 0x90, 0x24, 0x9, 0x2, 0x42,
    0xf8,

    /* U+006D "m" */
    0xaa, 0x89, 0x98, 0x88, 0x88, 0x88, 0x88, 0x80,

    /* U+006E "n" */
    0x2b, 0x4c, 0x22, 0x8, 0x82, 0x20, 0x80,

    /* U+006F "o" */
    0x2b, 0x46, 0xc, 0x90, 0xc6, 0xc, 0x2a, 0x0,

    /* U+0070 "p" */
    0x2b, 0x4c, 0x32, 0xc, 0x82, 0x3a, 0x8, 0x2,
    0x0,

    /* U+0071 "q" */
    0x0, 0xa, 0xa6, 0x9, 0x42, 0x61, 0x8a, 0xa0,
    0x8, 0x2,

    /* U+0072 "r" */
    0x3a, 0x83, 0xc, 0x30, 0x3, 0x0, 0x30, 0x0,

    /* U+0073 "s" */
    0x2a, 0x4c, 0x1, 0xe0, 0x6, 0x3b, 0x0,

    /* U+0074 "t" */
    0x4, 0x6, 0x7, 0xe8, 0x60, 0x18, 0x6, 0x0,
    0xa8,

    /* U+0075 "u" */
    0x20, 0x88, 0x22, 0x8, 0x86, 0x2a, 0x80,

    /* U+0076 "v" */
    0x50, 0xc3, 0x18, 0x22, 0x1, 0xb0, 0xd, 0x0,

    /* U+0077 "w" */
    0x80, 0x98, 0x88, 0x5d, 0xc6, 0x68, 0x32, 0x80,

    /* U+0078 "x" */
    0x31, 0x81, 0xb0, 0xd, 0x2, 0xb0, 0x71, 0xc0,

    /* U+0079 "y" */
    0x50, 0xc3, 0x18, 0x22, 0x1, 0xb0, 0xd, 0x0,
    0xc0, 0xb0, 0x0,

    /* U+007A "z" */
    0x2b, 0x80, 0xc0, 0x80, 0x90, 0x3a, 0x80,

    /* U+007B "{" */
    0x7, 0x43, 0x0, 0xc0, 0x30, 0x34, 0x3, 0x0,
    0xc0, 0x30, 0x7, 0x40,

    /* U+007C "|" */
    0x88, 0x88, 0x88, 0x88, 0x88,

    /* U+007D "}" */
    0xa0, 0x24, 0x14, 0x14, 0xe, 0x14, 0x14, 0x24,
    0xa0,

    /* U+007E "~" */
    0x78, 0x44, 0x78
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 88, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 88, .box_w = 2, .box_h = 7, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 88, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 7, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 16, .adv_w = 88, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 26, .adv_w = 88, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 37, .adv_w = 88, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 88, .box_w = 2, .box_h = 3, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 50, .adv_w = 88, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 61, .adv_w = 88, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 72, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 79, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 87, .adv_w = 88, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 90, .adv_w = 88, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 91, .adv_w = 88, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 88, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 103, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 128, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 136, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 161, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 169, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 185, .adv_w = 88, .box_w = 2, .box_h = 5, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 88, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 194, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 203, .adv_w = 88, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 207, .adv_w = 88, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 214, .adv_w = 88, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 88, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 235, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 244, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 252, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 88, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 283, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 299, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 307, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 324, .adv_w = 88, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 364, .adv_w = 88, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 376, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 385, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 402, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 419, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 428, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 437, .adv_w = 88, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 88, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 454, .adv_w = 88, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 463, .adv_w = 88, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 473, .adv_w = 88, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 480, .adv_w = 88, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 484, .adv_w = 88, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 486, .adv_w = 88, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 489, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 505, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 512, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 521, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 528, .adv_w = 88, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 539, .adv_w = 88, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 550, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 559, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 568, .adv_w = 88, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 580, .adv_w = 88, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 591, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 608, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 615, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 623, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 632, .adv_w = 88, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 642, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 650, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 657, .adv_w = 88, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 666, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 673, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 681, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 689, .adv_w = 88, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 697, .adv_w = 88, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 708, .adv_w = 88, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 715, .adv_w = 88, .box_w = 5, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 727, .adv_w = 88, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 732, .adv_w = 88, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 741, .adv_w = 88, .box_w = 6, .box_h = 2, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
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
    .bpp = 2,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_consola_10 = {
#else
lv_font_t lv_font_consola_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 11,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_CONSOLA_10*/


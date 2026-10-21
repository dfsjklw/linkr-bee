/*******************************************************************************
 * Size: 8 px
 * Bpp: 2
 * Opts: --font C:/Windows/Fonts/consola.ttf --size 8 --bpp 2 --format lvgl --lv-include lvgl.h --range 0x20-0x7F --no-compress -o C:/VSCODE/linkr-bmc/linkr-bee/src/fonts/lv_font_consola_8.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef LV_FONT_CONSOLA_8
#define LV_FONT_CONSOLA_8 1
#endif

#if LV_FONT_CONSOLA_8

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+0021 "!" */
    0x22, 0x22, 0x2,

    /* U+0022 "\"" */
    0xd6, 0x50,

    /* U+0023 "#" */
    0x21, 0x1e, 0x82, 0x51, 0xe8, 0x18, 0x0,

    /* U+0024 "$" */
    0x4, 0x2d, 0x58, 0x2c, 0x17, 0x69, 0x20,

    /* U+0025 "%" */
    0x62, 0x25, 0x41, 0x80, 0x68, 0x29, 0x21, 0x80,

    /* U+0026 "&" */
    0x28, 0x15, 0x43, 0x81, 0xd4, 0x8a, 0x1a, 0xc0,

    /* U+0027 "'" */
    0x21,

    /* U+0028 "(" */
    0x10, 0x88, 0x20, 0x82, 0x2, 0x0,

    /* U+0029 ")" */
    0x41, 0x42, 0x8, 0x20, 0x85, 0x10,

    /* U+002A "*" */
    0x4, 0x2c, 0x19, 0x0,

    /* U+002B "+" */
    0x8, 0x1b, 0x80, 0x80, 0x10,

    /* U+002C "," */
    0x4, 0x22, 0x0,

    /* U+002D "-" */
    0xa0,

    /* U+002E "." */
    0x6,

    /* U+002F "/" */
    0x2, 0x4, 0x8, 0x14, 0x20, 0x10, 0x0,

    /* U+0030 "0" */
    0x29, 0x20, 0xc9, 0xa2, 0x48, 0x29, 0x0,

    /* U+0031 "1" */
    0x28, 0xc, 0xc, 0xc, 0x2e,

    /* U+0032 "2" */
    0x29, 0x2, 0x9, 0x14, 0x7a,

    /* U+0033 "3" */
    0x29, 0x1, 0x1c, 0x2, 0x29,

    /* U+0034 "4" */
    0xd, 0x9, 0x45, 0x52, 0xa8, 0x5, 0x0,

    /* U+0035 "5" */
    0x39, 0x10, 0x29, 0x2, 0x29,

    /* U+0036 "6" */
    0x29, 0x50, 0x69, 0x42, 0x29,

    /* U+0037 "7" */
    0x6b, 0x1, 0x8, 0x4, 0x20,

    /* U+0038 "8" */
    0x29, 0x52, 0x3d, 0x53, 0x2a,

    /* U+0039 "9" */
    0x29, 0x82, 0x2b, 0x2, 0x28,

    /* U+003A ":" */
    0x60, 0x6,

    /* U+003B ";" */
    0x18, 0x0, 0x7, 0x8, 0x80,

    /* U+003C "<" */
    0x0, 0x8, 0x20, 0x24, 0x9, 0x0,

    /* U+003D "=" */
    0x69, 0x69,

    /* U+003E ">" */
    0x0, 0x24, 0x9, 0x8, 0x20, 0x0,

    /* U+003F "?" */
    0xa0, 0x50, 0x98, 0x42, 0x40,

    /* U+0040 "@" */
    0x15, 0x14, 0x48, 0x6, 0x65, 0xa9, 0x25, 0x48,
    0x0, 0xa0,

    /* U+0041 "A" */
    0x1c, 0xa, 0x2, 0x21, 0xac, 0x82, 0x0,

    /* U+0042 "B" */
    0x69, 0x52, 0x69, 0x53, 0x69,

    /* U+0043 "C" */
    0x2a, 0x50, 0x80, 0x50, 0x2a,

    /* U+0044 "D" */
    0xa9, 0x20, 0x88, 0x22, 0x8, 0xa9, 0x0,

    /* U+0045 "E" */
    0x39, 0x10, 0x39, 0x10, 0x39,

    /* U+0046 "F" */
    0x39, 0x20, 0x39, 0x20, 0x20,

    /* U+0047 "G" */
    0x2a, 0x90, 0x86, 0x82, 0x2a,

    /* U+0048 "H" */
    0x82, 0x82, 0xab, 0x82, 0x82,

    /* U+0049 "I" */
    0x2d, 0x8, 0x8, 0x8, 0x2d,

    /* U+004A "J" */
    0x29, 0x5, 0x5, 0x5, 0x28,

    /* U+004B "K" */
    0x52, 0x16, 0x7, 0x1, 0x60, 0x52, 0x0,

    /* U+004C "L" */
    0x20, 0x20, 0x20, 0x20, 0x3a,

    /* U+004D "M" */
    0x93, 0x29, 0x89, 0xa2, 0x8, 0x81, 0x0,

    /* U+004E "N" */
    0xa2, 0xa2, 0x9a, 0x8a, 0x83,

    /* U+004F "O" */
    0x29, 0x20, 0x88, 0x22, 0x8, 0x29, 0x0,

    /* U+0050 "P" */
    0x6a, 0x52, 0x69, 0x50, 0x50,

    /* U+0051 "Q" */
    0x29, 0x20, 0x88, 0x22, 0x8, 0x66, 0x6, 0x0,
    0xa0,

    /* U+0052 "R" */
    0x69, 0x14, 0x86, 0xc1, 0x54, 0x52, 0x0,

    /* U+0053 "S" */
    0x29, 0x50, 0x28, 0x2, 0x69,

    /* U+0054 "T" */
    0x6e, 0x2, 0x0, 0x80, 0x20, 0x8, 0x0,

    /* U+0055 "U" */
    0x82, 0x82, 0x82, 0x82, 0x29,

    /* U+0056 "V" */
    0xc2, 0x14, 0x82, 0x20, 0xa0, 0x1c, 0x0,

    /* U+0057 "W" */
    0x81, 0x20, 0x49, 0xa2, 0xa8, 0xa3, 0x0,

    /* U+0058 "X" */
    0x52, 0xa, 0x1, 0xc0, 0xa4, 0x93, 0x0,

    /* U+0059 "Y" */
    0x82, 0x8, 0x82, 0x80, 0x20, 0x8, 0x0,

    /* U+005A "Z" */
    0x6b, 0x9, 0x8, 0x20, 0x7a,

    /* U+005B "[" */
    0xa2, 0x8, 0x20, 0x82, 0x8, 0x28,

    /* U+005C "\\" */
    0x10, 0x20, 0x14, 0x8, 0x8, 0x2, 0x1,

    /* U+005D "]" */
    0x70, 0x82, 0x8, 0x20, 0x82, 0x1c,

    /* U+005E "^" */
    0x4, 0x28, 0x12,

    /* U+005F "_" */
    0xaa, 0x0,

    /* U+0060 "`" */
    0x20, 0x0, 0x0,

    /* U+0061 "a" */
    0x29, 0x2a, 0x52, 0x2a,

    /* U+0062 "b" */
    0x50, 0x50, 0x6a, 0x52, 0x52, 0x69,

    /* U+0063 "c" */
    0x29, 0x50, 0x50, 0x29,

    /* U+0064 "d" */
    0x2, 0x2, 0x2a, 0x42, 0x82, 0x2a,

    /* U+0065 "e" */
    0x29, 0x6a, 0x50, 0x29,

    /* U+0066 "f" */
    0xa, 0x5, 0x6, 0xa0, 0x50, 0x14, 0x5, 0x0,

    /* U+0067 "g" */
    0x2b, 0x14, 0x86, 0x81, 0xa4, 0x83, 0x1a, 0x80,

    /* U+0068 "h" */
    0x50, 0x50, 0x69, 0x52, 0x52, 0x52,

    /* U+0069 "i" */
    0x8, 0x0, 0x28, 0x8, 0x8, 0x2d,

    /* U+006A "j" */
    0x9, 0x0, 0x29, 0x5, 0x5, 0x5, 0x4, 0x68,

    /* U+006B "k" */
    0x50, 0x14, 0x5, 0x61, 0xa0, 0x68, 0x14, 0x80,

    /* U+006C "l" */
    0x28, 0x8, 0x8, 0x8, 0x8, 0x2d,

    /* U+006D "m" */
    0xaa, 0x22, 0x88, 0xa2, 0x28,

    /* U+006E "n" */
    0x69, 0x52, 0x52, 0x52,

    /* U+006F "o" */
    0x29, 0x20, 0x88, 0x20, 0xa4,

    /* U+0070 "p" */
    0x6a, 0x52, 0x52, 0x69, 0x50, 0x50,

    /* U+0071 "q" */
    0x0, 0x2a, 0x42, 0x82, 0x2a, 0x2, 0x2,

    /* U+0072 "r" */
    0x2a, 0x8, 0x41, 0x0, 0x40,

    /* U+0073 "s" */
    0x29, 0x20, 0x6, 0x29,

    /* U+0074 "t" */
    0x0, 0x20, 0x79, 0x20, 0x20, 0x19,

    /* U+0075 "u" */
    0x52, 0x52, 0x52, 0x2a,

    /* U+0076 "v" */
    0x82, 0x8, 0x82, 0x80, 0x60,

    /* U+0077 "w" */
    0x81, 0x22, 0x8a, 0xa1, 0x98,

    /* U+0078 "x" */
    0x62, 0xa, 0x2, 0x81, 0x48,

    /* U+0079 "y" */
    0x82, 0x8, 0x82, 0x40, 0x70, 0x14, 0x18, 0x0,

    /* U+007A "z" */
    0x2a, 0x8, 0x20, 0x79,

    /* U+007B "{" */
    0x9, 0x4, 0x14, 0x14, 0x30, 0x14, 0x14, 0x9,

    /* U+007C "|" */
    0x22, 0x22, 0x22, 0x22,

    /* U+007D "}" */
    0x24, 0x8, 0x8, 0x8, 0x9, 0x8, 0x8, 0x24,

    /* U+007E "~" */
    0x61, 0x12, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 70, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 70, .box_w = 2, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 70, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 5, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 12, .adv_w = 70, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 19, .adv_w = 70, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 27, .adv_w = 70, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 35, .adv_w = 70, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 36, .adv_w = 70, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 42, .adv_w = 70, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 48, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 52, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 57, .adv_w = 70, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 60, .adv_w = 70, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 61, .adv_w = 70, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 62, .adv_w = 70, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 69, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 76, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 81, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 86, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 91, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 113, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 118, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 70, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 70, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 130, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 136, .adv_w = 70, .box_w = 4, .box_h = 2, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 138, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 144, .adv_w = 70, .box_w = 3, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 70, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 159, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 183, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 193, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 208, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 244, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 249, .adv_w = 70, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 258, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 265, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 296, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 303, .adv_w = 70, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 310, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 70, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 321, .adv_w = 70, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 328, .adv_w = 70, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 334, .adv_w = 70, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 337, .adv_w = 70, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 339, .adv_w = 70, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 342, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 346, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 352, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 362, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 70, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 374, .adv_w = 70, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 382, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 388, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 394, .adv_w = 70, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 402, .adv_w = 70, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 416, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 421, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 425, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 430, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 436, .adv_w = 70, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 443, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 448, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 70, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 462, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 467, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 472, .adv_w = 70, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 477, .adv_w = 70, .box_w = 5, .box_h = 6, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 485, .adv_w = 70, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 489, .adv_w = 70, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 497, .adv_w = 70, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 501, .adv_w = 70, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 509, .adv_w = 70, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 1}
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
const lv_font_t lv_font_consola_8 = {
#else
lv_font_t lv_font_consola_8 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 8,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if LV_FONT_CONSOLA_8*/


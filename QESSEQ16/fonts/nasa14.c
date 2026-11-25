/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --bpp 1 --size 14 --font /Users/macadmin/Desktop/SC01_BASE_A5600/SC01_BASE_SQUARELINE/assets/Nasalization Rg.otf -o /Users/macadmin/Desktop/SC01_BASE_A5600/SC01_BASE_SQUARELINE/assets/ui_font_ui_font_Nasa14.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "../ui.h"

#ifndef UI_FONT_UI_FONT_NASA14
#define UI_FONT_UI_FONT_NASA14 1
#endif

#if UI_FONT_UI_FONT_NASA14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xfc, 0x30,

    /* U+0022 "\"" */
    0x99, 0x99,

    /* U+0023 "#" */
    0x24, 0x24, 0xff, 0x24, 0x24, 0x24, 0xff, 0x24,
    0x24, 0x24,

    /* U+0024 "$" */
    0x10, 0x21, 0xfe, 0xc, 0x18, 0x1f, 0x3, 0x7,
    0xf8, 0x40, 0x80,

    /* U+0025 "%" */
    0x70, 0x91, 0x32, 0x24, 0x45, 0x7, 0x5c, 0xc,
    0x42, 0x88, 0x91, 0x32, 0x24, 0x38,

    /* U+0026 "&" */
    0x1e, 0x8, 0xc2, 0x30, 0x8c, 0x3e, 0xe, 0x6,
    0xcf, 0x36, 0xc7, 0x31, 0xe7, 0xdc,

    /* U+0027 "'" */
    0xf0,

    /* U+0028 "(" */
    0x34, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x43,

    /* U+0029 ")" */
    0xc2, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x2c,

    /* U+002A "*" */
    0x10, 0x21, 0x51, 0xc7, 0xc2, 0x4, 0x0,

    /* U+002B "+" */
    0x10, 0x10, 0x10, 0xff, 0x10, 0x10, 0x10, 0x10,

    /* U+002C "," */
    0x6d, 0xbc,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x4, 0x30, 0x82, 0x18, 0x41, 0xc, 0x20, 0x86,
    0x10, 0x43, 0x0,

    /* U+0030 "0" */
    0x3e, 0x31, 0xb0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0x63, 0x1f, 0x0,

    /* U+0031 "1" */
    0xe3, 0x33, 0x33, 0x33, 0x33,

    /* U+0032 "2" */
    0xff, 0x0, 0xc0, 0x60, 0xf3, 0xff, 0xfb, 0xe1,
    0x80, 0xc0, 0x7f, 0xc0,

    /* U+0033 "3" */
    0xfe, 0x3, 0x3, 0x3, 0x3c, 0x3, 0x3, 0x3,
    0x3, 0xfe,

    /* U+0034 "4" */
    0x7, 0x84, 0xc4, 0x66, 0x36, 0x1b, 0xf, 0x6,
    0xff, 0x1, 0x80, 0xc0,

    /* U+0035 "5" */
    0xfe, 0xc0, 0xc0, 0xc0, 0xfe, 0x3, 0x3, 0x3,
    0x3, 0xfe,

    /* U+0036 "6" */
    0x3f, 0x30, 0x30, 0x18, 0xf, 0xf6, 0xf, 0x7,
    0x83, 0x61, 0x9f, 0x80,

    /* U+0037 "7" */
    0xfe, 0x3, 0x3, 0x7, 0x6, 0xc, 0x1c, 0x18,
    0x30, 0x30,

    /* U+0038 "8" */
    0x7f, 0x60, 0xf0, 0x78, 0x33, 0xe6, 0xf, 0x7,
    0x83, 0xc1, 0xbf, 0x80,

    /* U+0039 "9" */
    0x7e, 0x61, 0xb0, 0x78, 0x3c, 0x1b, 0xfc, 0x6,
    0x3, 0x3, 0x3f, 0x0,

    /* U+003A ":" */
    0xc0, 0x3,

    /* U+003B ";" */
    0x60, 0x0, 0xdb, 0xc0,

    /* U+003C "<" */
    0x3, 0xc, 0x70, 0xc0, 0xc0, 0x30, 0xe, 0x3,

    /* U+003D "=" */
    0xff, 0x0, 0x0, 0x0, 0xff,

    /* U+003E ">" */
    0xc0, 0x70, 0xc, 0x2, 0x6, 0x1c, 0x60, 0x80,

    /* U+003F "?" */
    0xfc, 0xc, 0x18, 0x33, 0xc6, 0xc, 0x0, 0x0,
    0x60,

    /* U+0040 "@" */
    0x7e, 0x81, 0x81, 0x8f, 0x91, 0x91, 0x91, 0x91,
    0x8e, 0x80, 0x80, 0x7f,

    /* U+0041 "A" */
    0xe, 0x3, 0x60, 0x6c, 0x8, 0x83, 0x18, 0x63,
    0x8, 0x23, 0x6, 0x60, 0xd8, 0xc,

    /* U+0042 "B" */
    0xfe, 0xc3, 0xc3, 0xc3, 0xfc, 0xc3, 0xc3, 0xc3,
    0xc3, 0xfe,

    /* U+0043 "C" */
    0x3f, 0x60, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0x60, 0x3f,

    /* U+0044 "D" */
    0xfe, 0x61, 0xb0, 0x78, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc3, 0x7f, 0x0,

    /* U+0045 "E" */
    0x7f, 0xc0, 0xc0, 0xc0, 0x3f, 0xc0, 0xc0, 0xc0,
    0xc0, 0x7f,

    /* U+0046 "F" */
    0x7f, 0xe0, 0xc0, 0xc0, 0xc0, 0xff, 0xc0, 0xc0,
    0xc0, 0xc0,

    /* U+0047 "G" */
    0x3f, 0x30, 0x30, 0x18, 0xc, 0xf6, 0xf, 0x7,
    0x83, 0x61, 0x9f, 0x80,

    /* U+0048 "H" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3,

    /* U+0049 "I" */
    0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x6, 0xc, 0x18, 0x30, 0x60, 0xc1, 0x83, 0x7,
    0xf8,

    /* U+004B "K" */
    0xc1, 0x61, 0xb1, 0x98, 0xcf, 0xc6, 0x33, 0x9,
    0x86, 0xc3, 0x60, 0xc0,

    /* U+004C "L" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0x7f,

    /* U+004D "M" */
    0x70, 0x76, 0x82, 0xf6, 0x37, 0x91, 0x3c, 0x89,
    0xe4, 0x4f, 0x32, 0x79, 0xb3, 0xc5, 0x1e, 0x38,
    0xc0,

    /* U+004E "N" */
    0x60, 0xf4, 0x3d, 0xf, 0x23, 0xc8, 0xf1, 0x3c,
    0x4f, 0x13, 0xc2, 0xf0, 0x60,

    /* U+004F "O" */
    0x3f, 0x18, 0x6c, 0xf, 0x3, 0xc0, 0xf0, 0x3c,
    0xf, 0x3, 0x61, 0x8f, 0xc0,

    /* U+0050 "P" */
    0xfe, 0x3, 0x3, 0x3, 0x3, 0xfe, 0xc0, 0xc0,
    0xc0, 0xc0,

    /* U+0051 "Q" */
    0x3f, 0x18, 0x6c, 0xf, 0x3, 0xc0, 0xf0, 0x3c,
    0x4f, 0x13, 0x67, 0x8f, 0xc0, 0x30, 0x4,

    /* U+0052 "R" */
    0xfe, 0x3, 0x3, 0x3, 0x3, 0xfe, 0xcc, 0xc6,
    0xc6, 0xc3,

    /* U+0053 "S" */
    0x7f, 0x60, 0x30, 0x18, 0x7, 0xf0, 0xc, 0x6,
    0x3, 0x1, 0xff, 0x80,

    /* U+0054 "T" */
    0xfe, 0x30, 0x60, 0xc1, 0x83, 0x6, 0xc, 0x18,
    0x30,

    /* U+0055 "U" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0x7e,

    /* U+0056 "V" */
    0xc0, 0x6c, 0x19, 0x83, 0x10, 0x43, 0x18, 0x63,
    0x4, 0x40, 0xd8, 0x1a, 0x1, 0xc0,

    /* U+0057 "W" */
    0xc3, 0x8d, 0x1a, 0x34, 0x6c, 0xd9, 0x33, 0x64,
    0xcd, 0x91, 0x36, 0xc4, 0x9b, 0x12, 0x68, 0x68,
    0xe0, 0xe0,

    /* U+0058 "X" */
    0x60, 0xc6, 0x30, 0x66, 0xd, 0x80, 0xe0, 0x1c,
    0x6, 0xc1, 0x8c, 0x71, 0x8c, 0x18,

    /* U+0059 "Y" */
    0x60, 0x98, 0x63, 0x30, 0x68, 0x1e, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+005A "Z" */
    0xff, 0x0, 0xc0, 0x60, 0xf1, 0xf3, 0xe3, 0xc1,
    0x80, 0xc0, 0x3f, 0xc0,

    /* U+005B "[" */
    0xf8, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x8f,

    /* U+005C "\\" */
    0xc1, 0x4, 0x18, 0x20, 0x83, 0x4, 0x10, 0x60,
    0x82, 0xc, 0x10,

    /* U+005D "]" */
    0xf1, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
    0x1f,

    /* U+005E "^" */
    0x31, 0x45, 0x12, 0x8a, 0x30,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0xc9, 0x90,

    /* U+0061 "a" */
    0x7e, 0x3, 0x3, 0x7f, 0xc3, 0xc3, 0xc3, 0x7f,

    /* U+0062 "b" */
    0xc0, 0xc0, 0xc0, 0xfe, 0xc7, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc6, 0x7e,

    /* U+0063 "c" */
    0x7e, 0xc3, 0x6, 0xc, 0x18, 0x18, 0x3f,

    /* U+0064 "d" */
    0x3, 0x3, 0x3, 0x7f, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0x43, 0x7e,

    /* U+0065 "e" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xfe, 0xc0, 0xc0, 0x7f,

    /* U+0066 "f" */
    0x3b, 0x19, 0xf6, 0x31, 0x8c, 0x63, 0x18,

    /* U+0067 "g" */
    0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f,
    0x3, 0x3, 0x7e,

    /* U+0068 "h" */
    0xc0, 0xc0, 0xc0, 0xfe, 0xc7, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0069 "i" */
    0xc3, 0xff, 0xfc,

    /* U+006A "j" */
    0x30, 0x3, 0x33, 0x33, 0x33, 0x33, 0x3e,

    /* U+006B "k" */
    0xc0, 0xc0, 0xc0, 0xc6, 0xc6, 0xcc, 0xf8, 0xcc,
    0xc4, 0xc6, 0xc2,

    /* U+006C "l" */
    0xff, 0xff, 0xfc,

    /* U+006D "m" */
    0xff, 0xec, 0x63, 0xc6, 0x3c, 0x63, 0xc6, 0x3c,
    0x63, 0xc6, 0x3c, 0x63,

    /* U+006E "n" */
    0xfc, 0xc7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,

    /* U+006F "o" */
    0x7e, 0x46, 0xc3, 0xc3, 0xc3, 0xc3, 0x46, 0x7e,

    /* U+0070 "p" */
    0x7e, 0xc6, 0xc3, 0xc3, 0xc3, 0xc3, 0xc7, 0xfe,
    0xc0, 0xc0, 0xc0,

    /* U+0071 "q" */
    0x7e, 0x43, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f,
    0x3, 0x3, 0x3,

    /* U+0072 "r" */
    0x7e, 0x31, 0x8c, 0x63, 0x18,

    /* U+0073 "s" */
    0x7f, 0x83, 0x3, 0xe0, 0x60, 0xc1, 0xfe,

    /* U+0074 "t" */
    0x63, 0x3e, 0xc6, 0x31, 0x8c, 0x61, 0xc0,

    /* U+0075 "u" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e,

    /* U+0076 "v" */
    0xc1, 0xa0, 0xd8, 0x44, 0x63, 0x21, 0x90, 0x58,
    0x38,

    /* U+0077 "w" */
    0xc7, 0xa, 0x2c, 0xd1, 0x26, 0x99, 0x36, 0xc9,
    0xb4, 0x6d, 0xa3, 0x47, 0xe,

    /* U+0078 "x" */
    0x61, 0x99, 0x87, 0x81, 0x81, 0xc1, 0xb0, 0x8c,
    0xc3,

    /* U+0079 "y" */
    0xc1, 0xa0, 0x98, 0xcc, 0x43, 0x61, 0xb0, 0x70,
    0x38, 0x8, 0xc, 0x6, 0x0,

    /* U+007A "z" */
    0xfe, 0x4, 0x19, 0xf7, 0x9c, 0x20, 0x7f,

    /* U+007B "{" */
    0x19, 0x8, 0x42, 0x10, 0x84, 0x26, 0x8, 0x42,
    0x10, 0x84, 0x20, 0xc0,

    /* U+007C "|" */
    0xff, 0xfc,

    /* U+007D "}" */
    0xc1, 0x8, 0x42, 0x10, 0x84, 0x20, 0xc8, 0x42,
    0x10, 0x84, 0x26, 0x0,

    /* U+007E "~" */
    0xf0, 0xf
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 60, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 52, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 86, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 6, .adv_w = 140, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 16, .adv_w = 139, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 27, .adv_w = 189, .box_w = 11, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 171, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 55, .adv_w = 43, .box_w = 1, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 56, .adv_w = 88, .box_w = 4, .box_h = 18, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 65, .adv_w = 88, .box_w = 4, .box_h = 18, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 74, .adv_w = 125, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 81, .adv_w = 139, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 89, .adv_w = 58, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 91, .adv_w = 93, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 92, .adv_w = 54, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 102, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 104, .adv_w = 168, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 75, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 164, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 133, .adv_w = 153, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 143, .adv_w = 158, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 153, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 165, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 146, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 163, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 211, .adv_w = 61, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 61, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 217, .adv_w = 139, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 225, .adv_w = 139, .box_w = 8, .box_h = 5, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 230, .adv_w = 139, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 238, .adv_w = 127, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 247, .adv_w = 172, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 259, .adv_w = 176, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 273, .adv_w = 161, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 283, .adv_w = 153, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 293, .adv_w = 165, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 305, .adv_w = 155, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 149, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 325, .adv_w = 163, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 337, .adv_w = 167, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 347, .adv_w = 63, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 350, .adv_w = 131, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 359, .adv_w = 159, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 371, .adv_w = 139, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 247, .box_w = 13, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 398, .adv_w = 197, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 411, .adv_w = 183, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 424, .adv_w = 152, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 434, .adv_w = 183, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 449, .adv_w = 160, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 459, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 471, .adv_w = 128, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 480, .adv_w = 164, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 490, .adv_w = 176, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 504, .adv_w = 234, .box_w = 14, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 522, .adv_w = 175, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 536, .adv_w = 154, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 549, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 561, .adv_w = 88, .box_w = 4, .box_h = 18, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 570, .adv_w = 102, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 581, .adv_w = 88, .box_w = 4, .box_h = 18, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 590, .adv_w = 120, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 595, .adv_w = 115, .box_w = 7, .box_h = 1, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 596, .adv_w = 77, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 598, .adv_w = 151, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 606, .adv_w = 156, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 617, .adv_w = 135, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 624, .adv_w = 156, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 635, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 643, .adv_w = 84, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 650, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 661, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 672, .adv_w = 58, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 675, .adv_w = 60, .box_w = 4, .box_h = 14, .ofs_x = -1, .ofs_y = -3},
    {.bitmap_index = 682, .adv_w = 135, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 693, .adv_w = 58, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 696, .adv_w = 231, .box_w = 12, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 158, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 716, .adv_w = 150, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 724, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 735, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 746, .adv_w = 104, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 751, .adv_w = 131, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 758, .adv_w = 84, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 765, .adv_w = 157, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 773, .adv_w = 152, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 782, .adv_w = 219, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 795, .adv_w = 153, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 804, .adv_w = 145, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 817, .adv_w = 140, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 824, .adv_w = 88, .box_w = 5, .box_h = 18, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 836, .adv_w = 65, .box_w = 1, .box_h = 14, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 838, .adv_w = 88, .box_w = 5, .box_h = 18, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 850, .adv_w = 139, .box_w = 8, .box_h = 2, .ofs_x = 1, .ofs_y = 4}
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

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 0, 1, 0, 0, 0, 0,
    1, 2, 0, 0, 0, 3, 4, 3,
    0, 5, 6, 7, 8, 9, 10, 11,
    12, 13, 14, 0, 0, 0, 0, 0,
    0, 15, 16, 8, 17, 18, 0, 19,
    20, 0, 0, 21, 22, 23, 24, 25,
    18, 26, 18, 27, 28, 29, 21, 30,
    31, 22, 32, 33, 34, 0, 0, 0,
    0, 0, 35, 36, 0, 0, 37, 38,
    39, 40, 0, 41, 42, 0, 40, 40,
    36, 36, 0, 43, 0, 44, 10, 45,
    46, 42, 45, 47, 34, 0, 0, 0
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 0, 1, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 2, 3, 2,
    0, 4, 5, 6, 0, 7, 8, 9,
    10, 11, 12, 0, 0, 0, 0, 0,
    13, 0, 14, 0, 15, 0, 0, 16,
    15, 0, 0, 17, 0, 18, 19, 19,
    15, 0, 15, 0, 20, 21, 18, 22,
    23, 24, 25, 26, 0, 0, 0, 0,
    0, 0, 27, 0, 28, 28, 28, 29,
    30, 0, 0, 31, 0, 0, 32, 32,
    28, 33, 28, 32, 34, 35, 36, 37,
    37, 38, 39, 40, 0, 0, 0, 0
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, -9, 0, -9, 4, 0, -9, 0,
    0, 9, 0, 0, 0, -22, -6, -6,
    -26, 0, -4, -2, 2, 2, 0, 0,
    4, 0, 0, -6, 0, -6, 0, 0,
    0, -4, 0, 0, 6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 13, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -9, 0, 0, -9, -6, 0, -2, 0,
    -9, 0, -2, -11, 0, 0, -9, 0,
    0, -6, 0, 0, -15, -20, -6, 0,
    -22, -1, -3, -4, 0, -4, 0, 0,
    0, 0, -11, -4, -17, 0, -17, 0,
    0, 0, 0, 0, 0, 0, 6, 0,
    0, -24, 0, 0, 0, -6, 0, 0,
    0, 0, 0, 0, -6, -11, 0, -13,
    -20, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, -9, -4, 0,
    -9, -13, 0, 0, 0, -6, -4, -6,
    0, -11, 0, -7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -5, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -7,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -9, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -35, 0, -2, 0, -5, -7, 0,
    -6, 4, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -9, 0, 0, 0, -4, 0, 0,
    0, -4, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -22, 0, -6, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, -9, 0,
    0, -9, 0, 0, -15, -17, -6, 4,
    -22, -3, -4, -6, 0, -6, 0, 0,
    0, 0, -6, -6, -11, 4, -9, 0,
    0, 0, -9, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -5, 0, 0, -6, 0, 0,
    0, 0, 0, -5, 0, 0, 0, -4,
    -6, -9, 0, 0, 0, 0, 0, 0,
    0, 2, 0, 0, 0, -9, 0, 0,
    -4, 0, 0, 0, -4, -9, -4, -11,
    -15, -4, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -9, 0, -4,
    0, -11, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -11, 0, 0,
    -11, 0, 0, 0, 0, 0, 0, -4,
    0, -4, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -6, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -9,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -9, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -4,
    0, 0, -4, 0, 0, 0, -4, 0,
    0, 0, 0, 0, 0, -4, 0, -2,
    0, 0, -13, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, -11, 0,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, -4, -5, -8, 0, -11, 0, 0,
    0, 0, -3, -6, -6, 0, -6, -6,
    -22, 0, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -9, 0, -4, 0,
    0, -4, 0, 0, -13, -14, -4, 0,
    -22, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -9, 0, -11, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, -9, -4, 0,
    -11, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -8, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -9, 0, 0,
    0, -3, 0, 0, 0, 0, 0, -4,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, 0, 0,
    0, -26, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -16, 0, -3,
    -27, 0, 0, 0, 0, -5, -2, 0,
    -11, -6, -9, -3, 0, -4, 0, -4,
    -4, 0, 0, 0, 0, -4, 0, -2,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -3, 0,
    0, -3, 0, 0, -3, -7, -3, 0,
    -11, -6, -6, -8, 0, -6, 0, 0,
    0, 0, 0, -3, 0, 0, 0, -4,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, -4,
    0, 0, 0, 0, -6, 0, -1, -6,
    -5, -2, -3, 0, 0, -2, 0, 0,
    0, 0, 0, 0, -4, -5, -4, 0,
    2, -15, -6, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -15, -4, -4,
    -20, 0, -2, -2, 4, 4, 2, 0,
    4, 0, -4, -6, 4, -6, 0, -4,
    -6, -6, 4, 0, 4, 0, 4, 0,
    2, -20, -11, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -17, -9, -12,
    -22, 0, -9, -4, 4, 4, 2, 0,
    4, -4, -13, -13, 0, -13, 0, -9,
    -13, -11, 0, 0, 0, 0, 0, -4,
    0, -6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -6, -4, -6,
    -6, 0, -4, 0, 2, 2, 2, 0,
    4, 0, -7, -5, 0, -4, 0, -5,
    -9, -3, 0, 0, 0, 0, 0, -4,
    4, -22, -20, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -22, -15, -18,
    -28, 0, -11, -8, 4, 4, 4, 0,
    4, 0, -13, -22, 0, -23, 0, -15,
    -22, -17, 0, -13, -4, -9, -4, -11,
    0, 0, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    0, 0, 0, 0, 0, -6, 0, 0,
    0, 0, -5, -5, 0, -6, 0, 0,
    0, 0, 0, -1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 15, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -6, -6, -6, 0,
    -6, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -9, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -6, -6, -4, -1,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, 0, -4,
    11, -9, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 11, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -6, 0, 4, -4, 0, 0,
    0, 0, 4, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -9, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 9, 0,
    0, 0, 0, 0, -6, 0, -9, 0,
    -6, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -9, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, 0, 0, 0,
    0, 0, 0, -1, -4, 0, -5, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 11, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -9, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -5, -6, 0, -6, 0, 0,
    0, 0, 0, -5, 4, 0, 0, 0,
    0, -22, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0,
    -1, 0, 4, 0, 4, 0, 4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0,
    0, 0, 4, 0, 4, 4, 0, 0,
    4, -17, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -6, 0, -6, 0, -1,
    -5, -4, 4, 0, 4, 4, 2, 0,
    2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 47,
    .right_class_cnt     = 40,
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
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 1,
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
const lv_font_t ui_font_ui_font_Nasa14 = {
#else
lv_font_t ui_font_ui_font_Nasa14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 18,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_UI_FONT_NASA14*/


/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --font /Users/macadmin/Desktop/SC01_BASE_A5600/SC01_BASE_SQUARELINE/assets/Nasalization Rg.otf -o /Users/macadmin/Desktop/SC01_BASE_A5600/SC01_BASE_SQUARELINE/assets/ui_font_ui_font_Nasa12.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "../ui.h"

#ifndef UI_FONT_UI_FONT_NASA12
#define UI_FONT_UI_FONT_NASA12 1
#endif

#if UI_FONT_UI_FONT_NASA12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xf9,

    /* U+0022 "\"" */
    0xb6, 0x80,

    /* U+0023 "#" */
    0x49, 0x2f, 0xd2, 0x4b, 0xf4, 0x92,

    /* U+0024 "$" */
    0x10, 0x4f, 0xe0, 0xf8, 0x10, 0x7f, 0x10,

    /* U+0025 "%" */
    0x61, 0x24, 0x89, 0x62, 0x50, 0x6b, 0x85, 0x12,
    0x45, 0xe,

    /* U+0026 "&" */
    0x18, 0x24, 0x24, 0x2c, 0x18, 0x7a, 0x8e, 0x84,
    0x7a,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x7c, 0x88, 0x88, 0x88, 0x88, 0x8c, 0x70,

    /* U+0029 ")" */
    0xe3, 0x11, 0x11, 0x11, 0x11, 0x13, 0xe0,

    /* U+002A "*" */
    0x25, 0x5d, 0xf2, 0x0,

    /* U+002B "+" */
    0x10, 0x23, 0xf8, 0x81, 0x2, 0x0,

    /* U+002C "," */
    0x57,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0x80,

    /* U+002F "/" */
    0x8, 0x44, 0x23, 0x10, 0x8c, 0x42, 0x30,

    /* U+0030 "0" */
    0x78, 0x8a, 0xc, 0x18, 0x30, 0x51, 0x1e,

    /* U+0031 "1" */
    0xd5, 0x55,

    /* U+0032 "2" */
    0xfc, 0x4, 0x9, 0xff, 0xd8, 0x20, 0x7f,

    /* U+0033 "3" */
    0xf8, 0x10, 0x41, 0x78, 0x10, 0x7e,

    /* U+0034 "4" */
    0x1e, 0x24, 0x8b, 0x14, 0x2f, 0xc0, 0x81,

    /* U+0035 "5" */
    0xff, 0x2, 0x7, 0xe0, 0x20, 0x40, 0xfe,

    /* U+0036 "6" */
    0x3e, 0x82, 0x4, 0xf, 0xd0, 0x50, 0xbe,

    /* U+0037 "7" */
    0xfe, 0xc, 0x18, 0x60, 0x83, 0xc, 0x30,

    /* U+0038 "8" */
    0x7d, 0x6, 0xc, 0x17, 0xd0, 0x60, 0xbe,

    /* U+0039 "9" */
    0x7d, 0xe, 0xc, 0x17, 0xe0, 0x41, 0x3c,

    /* U+003A ":" */
    0x84,

    /* U+003B ";" */
    0x40, 0x5c,

    /* U+003C "<" */
    0x6, 0x33, 0x84, 0x7, 0x1, 0xc0,

    /* U+003D "=" */
    0xfe, 0x0, 0x7, 0xf0,

    /* U+003E ">" */
    0x80, 0xe0, 0x30, 0x23, 0x98, 0x0,

    /* U+003F "?" */
    0xf8, 0x10, 0x4e, 0x20, 0x0, 0x8,

    /* U+0040 "@" */
    0x7f, 0x6, 0xfd, 0x1a, 0x34, 0x6f, 0xc0, 0x7e,

    /* U+0041 "A" */
    0x1c, 0xb, 0xc, 0x86, 0x42, 0x33, 0x9, 0x7,
    0x83,

    /* U+0042 "B" */
    0xfd, 0x6, 0xc, 0x1f, 0xd0, 0x60, 0xfe,

    /* U+0043 "C" */
    0x7d, 0x8, 0x20, 0x82, 0x4, 0x1f,

    /* U+0044 "D" */
    0xf9, 0xa, 0xc, 0x18, 0x30, 0x61, 0x7c,

    /* U+0045 "E" */
    0x7f, 0x2, 0x4, 0x7, 0xf0, 0x20, 0x3f,

    /* U+0046 "F" */
    0x7e, 0x8, 0x20, 0xfe, 0x8, 0x20,

    /* U+0047 "G" */
    0x7e, 0x82, 0x4, 0x9, 0xd0, 0x50, 0xbe,

    /* U+0048 "H" */
    0x83, 0x6, 0xc, 0x1f, 0xf0, 0x60, 0xc1,

    /* U+0049 "I" */
    0xff,

    /* U+004A "J" */
    0x8, 0x42, 0x10, 0x84, 0x3e,

    /* U+004B "K" */
    0x85, 0x1a, 0x24, 0xcf, 0x11, 0x23, 0x43,

    /* U+004C "L" */
    0x82, 0x8, 0x20, 0x82, 0x8, 0x1f,

    /* U+004D "M" */
    0xe0, 0xf6, 0x36, 0xc6, 0xc8, 0x99, 0x93, 0x36,
    0x66, 0xcc, 0x71,

    /* U+004E "N" */
    0xe1, 0xb1, 0xb1, 0x99, 0x99, 0x8d, 0x8d, 0x87,

    /* U+004F "O" */
    0x7c, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3e,

    /* U+0050 "P" */
    0xfc, 0x4, 0x8, 0x1f, 0xd0, 0x20, 0x40,

    /* U+0051 "Q" */
    0x7c, 0x42, 0x81, 0x81, 0x81, 0x89, 0x4f, 0x3e,
    0x6,

    /* U+0052 "R" */
    0xfc, 0x4, 0x8, 0x1f, 0xd1, 0xa1, 0x41,

    /* U+0053 "S" */
    0x7f, 0x2, 0x4, 0x7, 0xc0, 0x40, 0xfe,

    /* U+0054 "T" */
    0xfc, 0x41, 0x4, 0x10, 0x41, 0x4,

    /* U+0055 "U" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xbe,

    /* U+0056 "V" */
    0xc1, 0xa0, 0xd8, 0x44, 0x62, 0x21, 0x90, 0x58,
    0x38,

    /* U+0057 "W" */
    0xc7, 0x14, 0xd1, 0x4d, 0x34, 0x93, 0x49, 0xb4,
    0x9b, 0x78, 0xa3, 0xe,

    /* U+0058 "X" */
    0x61, 0x99, 0x85, 0x81, 0x81, 0xc1, 0xb0, 0x8c,
    0x83,

    /* U+0059 "Y" */
    0xc6, 0x88, 0xa1, 0xc1, 0x2, 0x4, 0x8,

    /* U+005A "Z" */
    0xfe, 0x2, 0x6, 0x1e, 0x78, 0xe0, 0x80, 0xfe,

    /* U+005B "[" */
    0xf8, 0x88, 0x88, 0x88, 0x88, 0x88, 0xf0,

    /* U+005C "\\" */
    0xc2, 0x10, 0xc2, 0x10, 0xc2, 0x10, 0x42,

    /* U+005D "]" */
    0xf1, 0x11, 0x11, 0x11, 0x11, 0x11, 0xf0,

    /* U+005E "^" */
    0x62, 0xa5, 0x28, 0x80,

    /* U+005F "_" */
    0xfc,

    /* U+0060 "`" */
    0x89, 0x80,

    /* U+0061 "a" */
    0xf8, 0x1f, 0xe1, 0x85, 0xf0,

    /* U+0062 "b" */
    0x81, 0x2, 0x7, 0xe8, 0x30, 0x60, 0xc1, 0x7c,

    /* U+0063 "c" */
    0x7e, 0x8, 0x20, 0x81, 0xf0,

    /* U+0064 "d" */
    0x2, 0x4, 0xb, 0xf8, 0x30, 0x60, 0xc1, 0x7c,

    /* U+0065 "e" */
    0x7a, 0x18, 0x7f, 0x81, 0xf0,

    /* U+0066 "f" */
    0x74, 0x4f, 0x44, 0x44, 0x40,

    /* U+0067 "g" */
    0x7d, 0x6, 0xc, 0x18, 0x2f, 0xc0, 0xbe,

    /* U+0068 "h" */
    0x81, 0x2, 0x7, 0xe8, 0x30, 0x60, 0xc1, 0x82,

    /* U+0069 "i" */
    0xbf,

    /* U+006A "j" */
    0x20, 0x92, 0x49, 0x3c,

    /* U+006B "k" */
    0x82, 0x8, 0x22, 0x9b, 0x89, 0xa2, 0x8c,

    /* U+006C "l" */
    0xff, 0x80,

    /* U+006D "m" */
    0xff, 0xd0, 0x86, 0x10, 0xc2, 0x18, 0x43, 0x8,
    0x40,

    /* U+006E "n" */
    0xfd, 0x6, 0xc, 0x18, 0x30, 0x40,

    /* U+006F "o" */
    0x7d, 0x6, 0xc, 0x18, 0x2f, 0x80,

    /* U+0070 "p" */
    0x7d, 0x6, 0xc, 0x18, 0x3f, 0xa0, 0x40,

    /* U+0071 "q" */
    0x7d, 0x6, 0xc, 0x18, 0x2f, 0xc0, 0x81,

    /* U+0072 "r" */
    0xf8, 0x88, 0x88,

    /* U+0073 "s" */
    0xfe, 0xf, 0x81, 0x7, 0xe0,

    /* U+0074 "t" */
    0x44, 0xf4, 0x44, 0x47,

    /* U+0075 "u" */
    0x83, 0x6, 0xc, 0x18, 0x2f, 0x80,

    /* U+0076 "v" */
    0xc3, 0x42, 0x66, 0x24, 0x3c, 0x18,

    /* U+0077 "w" */
    0xce, 0x29, 0x65, 0x24, 0xa4, 0xb5, 0x96, 0xe1,
    0x80,

    /* U+0078 "x" */
    0x62, 0x34, 0x18, 0x18, 0x24, 0x42,

    /* U+0079 "y" */
    0xc2, 0x42, 0x64, 0x24, 0x28, 0x38, 0x10, 0x10,

    /* U+007A "z" */
    0xfc, 0x8, 0x77, 0x8c, 0x1f, 0x80,

    /* U+007B "{" */
    0x69, 0x24, 0xa2, 0x49, 0x26,

    /* U+007C "|" */
    0xff, 0xe0,

    /* U+007D "}" */
    0xc9, 0x24, 0x8a, 0x49, 0x2c,

    /* U+007E "~" */
    0xe0, 0x70
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 51, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 45, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 73, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 4, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 10, .adv_w = 119, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 17, .adv_w = 162, .box_w = 10, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 27, .adv_w = 147, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 37, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 37, .adv_w = 76, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 44, .adv_w = 76, .box_w = 4, .box_h = 13, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 51, .adv_w = 107, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 55, .adv_w = 119, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 61, .adv_w = 49, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 62, .adv_w = 80, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 63, .adv_w = 46, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 87, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 71, .adv_w = 144, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 65, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 141, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 87, .adv_w = 131, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 135, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 132, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 107, .adv_w = 137, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 114, .adv_w = 125, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 139, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 128, .adv_w = 137, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 135, .adv_w = 52, .box_w = 1, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 136, .adv_w = 52, .box_w = 2, .box_h = 7, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 138, .adv_w = 119, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 144, .adv_w = 119, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 148, .adv_w = 119, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 154, .adv_w = 109, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 147, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 168, .adv_w = 151, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 138, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 184, .adv_w = 131, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 190, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 133, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 127, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 140, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 143, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 54, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 230, .adv_w = 136, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 119, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 243, .adv_w = 212, .box_w = 11, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 254, .adv_w = 169, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 157, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 130, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 157, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 286, .adv_w = 137, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 293, .adv_w = 135, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 110, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 306, .adv_w = 141, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 313, .adv_w = 151, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 322, .adv_w = 201, .box_w = 12, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 150, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 132, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 350, .adv_w = 143, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 358, .adv_w = 76, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 365, .adv_w = 87, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 372, .adv_w = 76, .box_w = 4, .box_h = 13, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 379, .adv_w = 103, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 383, .adv_w = 99, .box_w = 6, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 384, .adv_w = 66, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 386, .adv_w = 129, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 391, .adv_w = 133, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 399, .adv_w = 116, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 404, .adv_w = 133, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 412, .adv_w = 125, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 417, .adv_w = 72, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 133, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 429, .adv_w = 136, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 437, .adv_w = 50, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 438, .adv_w = 52, .box_w = 3, .box_h = 10, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 442, .adv_w = 116, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 449, .adv_w = 50, .box_w = 1, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 451, .adv_w = 198, .box_w = 11, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 460, .adv_w = 136, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 466, .adv_w = 129, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 472, .adv_w = 133, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 479, .adv_w = 133, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 486, .adv_w = 89, .box_w = 4, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 489, .adv_w = 113, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 72, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 498, .adv_w = 135, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 504, .adv_w = 130, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 510, .adv_w = 188, .box_w = 11, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 519, .adv_w = 131, .box_w = 8, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 525, .adv_w = 124, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 533, .adv_w = 120, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 539, .adv_w = 76, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 544, .adv_w = 55, .box_w = 1, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 546, .adv_w = 76, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 551, .adv_w = 119, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 3}
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
    0, -7, 0, -7, 4, 0, -7, 0,
    0, 7, 0, 0, 0, -19, -6, -6,
    -23, 0, -4, -2, 2, 2, 0, 0,
    4, 0, 0, -6, 0, -6, 0, 0,
    0, -4, 0, 0, 6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 11, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -7, 0, 0, -7, -6, 0, -2, 0,
    -7, 0, -2, -9, 0, 0, -7, 0,
    0, -6, 0, 0, -13, -17, -6, 0,
    -19, -1, -2, -4, 0, -4, 0, 0,
    0, 0, -9, -4, -15, 0, -15, 0,
    0, 0, 0, 0, 0, 0, 6, 0,
    0, -21, 0, 0, 0, -6, 0, 0,
    0, 0, 0, 0, -6, -9, 0, -11,
    -17, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, -7, -4, 0,
    -7, -11, 0, 0, 0, -6, -3, -6,
    0, -9, 0, -6, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -4, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -2, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -6,
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
    0, 0, 0, -7, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -30, 0, -2, 0, -4, -6, 0,
    -5, 4, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -7, 0, 0, 0, -3, 0, 0,
    0, -4, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -19, 0, -6, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, -7, 0,
    0, -7, 0, 0, -13, -15, -6, 4,
    -19, -3, -4, -6, 0, -6, 0, 0,
    0, 0, -6, -6, -10, 4, -8, 0,
    0, 0, -7, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -4, 0, 0, -6, 0, 0,
    0, 0, 0, -4, 0, 0, 0, -4,
    -6, -7, 0, 0, 0, 0, 0, 0,
    0, 2, 0, 0, 0, -7, 0, 0,
    -4, 0, 0, 0, -4, -7, -4, -9,
    -13, -4, -3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -7, 0, -4,
    0, -9, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -9, 0, 0,
    -9, 0, 0, 0, 0, 0, 0, -4,
    0, -3, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, -2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -7,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -7, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -4,
    0, 0, -4, 0, 0, 0, -4, 0,
    0, 0, 0, 0, 0, -3, 0, -2,
    0, 0, -11, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, -9, 0,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, -4, -4, -7, 0, -9, 0, 0,
    0, 0, -2, -5, -6, 0, -6, -6,
    -19, 0, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -7, 0, -4, 0,
    0, -3, 0, 0, -12, -12, -4, 0,
    -19, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -8, 0, -10, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, -7, -4, 0,
    -9, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -7, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -8, 0, 0,
    0, -2, 0, 0, 0, 0, 0, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -2, 0, 0,
    0, -23, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -14, 0, -2,
    -23, 0, 0, 0, 0, -4, -2, 0,
    -9, -6, -7, -2, 0, -4, 0, -4,
    -4, 0, 0, 0, 0, -3, 0, -2,
    -1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -3, 0,
    0, -3, 0, 0, -2, -6, -2, 0,
    -9, -5, -5, -7, 0, -5, 0, 0,
    0, 0, 0, -2, 0, 0, 0, -4,
    0, -3, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, -4,
    0, 0, 0, 0, -6, 0, -1, -6,
    -4, -2, -3, 0, 0, -2, 0, 0,
    0, 0, 0, 0, -3, -4, -4, 0,
    2, -13, -6, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -13, -4, -3,
    -17, 0, -2, -1, 4, 4, 2, 0,
    4, 0, -4, -6, 4, -6, 0, -4,
    -6, -6, 4, 0, 4, 0, 4, 0,
    2, -17, -9, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -15, -7, -11,
    -19, 0, -7, -3, 4, 4, 2, 0,
    4, -4, -11, -11, 0, -11, 0, -7,
    -11, -9, 0, 0, 0, 0, 0, -4,
    0, -6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -6, -4, -5,
    -5, 0, -4, 0, 2, 2, 2, 0,
    4, 0, -6, -4, 0, -4, 0, -4,
    -7, -2, 0, 0, 0, 0, 0, -3,
    4, -19, -17, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -19, -13, -16,
    -24, 0, -9, -7, 4, 4, 4, 0,
    4, 0, -11, -19, 0, -20, 0, -13,
    -19, -15, 0, -11, -4, -7, -4, -9,
    0, 0, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    0, 0, 0, 0, 0, -5, 0, 0,
    0, 0, -4, -4, 0, -6, 0, 0,
    0, 0, 0, -1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 13, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -6, -6, -6, 0,
    -6, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -7, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -6, -6, -4, -1,
    0, -4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -3, 0, 0, -4,
    9, -7, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 9, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -5, 0, 4, -3, 0, 0,
    0, 0, 4, 0, 0, 0, 0, 0,
    -4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -7, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 7, 0,
    0, 0, 0, 0, -5, 0, -7, 0,
    -6, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -7, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -2, 0, 0, 0,
    0, 0, 0, -1, -4, 0, -4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, -4, 9, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -7, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -4, -6, 0, -6, 0, 0,
    0, 0, 0, -4, 4, 0, 0, 0,
    0, -19, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0,
    -1, 0, 4, 0, 4, 0, 4, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0,
    0, 0, 4, 0, 4, 4, 0, 0,
    4, -15, -4, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 6, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -6, 0, -5, 0, -1,
    -4, -3, 4, 0, 4, 4, 2, 0,
    2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -2, 0, 0, 0, 0,
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
const lv_font_t ui_font_ui_font_Nasa12 = {
#else
lv_font_t ui_font_ui_font_Nasa12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if UI_FONT_UI_FONT_NASA12*/


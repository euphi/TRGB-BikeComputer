/*******************************************************************************
 * Size: 18 px
 * Bpp: 1
 * Opts: --bpp 1 --size 18 --font SquareLine/assets/schilder.ttf -o src/ui/font/ui_font_SchildGrot18.c --format lvgl -r 0x20-0x7f --symbols üöäÜÖÄß€µ --no-compress --no-prefilter
 ******************************************************************************/

#include <lvgl.h>

#ifndef UI_FONT_SCHILDGROT18
#define UI_FONT_SCHILDGROT18 1
#endif

#if UI_FONT_SCHILDGROT18

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xdb, 0x6d, 0xb6, 0xc0, 0x7e, 0x80,

    /* U+0022 "\"" */
    0xff, 0xfa, 0xa0,

    /* U+0023 "#" */
    0x33, 0xc, 0xc3, 0x33, 0xff, 0xff, 0xcc, 0xc3,
    0x33, 0xff, 0xff, 0xcc, 0xc3, 0x30,

    /* U+0024 "$" */
    0x30, 0xc0, 0x1e, 0xff, 0x3c, 0x30, 0xf9, 0xf0,
    0xc3, 0xcf, 0xf7, 0x80, 0x30, 0xc0,

    /* U+0025 "%" */
    0x70, 0x9f, 0x33, 0x64, 0x6d, 0x8f, 0xa0, 0xec,
    0x1, 0x70, 0x7f, 0xb, 0x63, 0x6c, 0x4f, 0x90,
    0xe2, 0x0,

    /* U+0026 "&" */
    0x78, 0x3f, 0xc, 0xc3, 0x0, 0xc0, 0x30, 0xcf,
    0xff, 0xff, 0xc3, 0x30, 0xcc, 0x33, 0xc, 0xff,
    0xdf, 0xf0,

    /* U+0027 "'" */
    0xfe, 0x80,

    /* U+0028 "(" */
    0x7f, 0x6d, 0xb6, 0xdb, 0x6d, 0xb6, 0xec,

    /* U+0029 ")" */
    0xef, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0xfe,

    /* U+002A "*" */
    0x18, 0x18, 0xdb, 0x7e, 0x3c, 0x7e, 0xdb, 0x18,
    0x18,

    /* U+002B "+" */
    0x18, 0x18, 0x18, 0xff, 0xff, 0x18, 0x18, 0x18,

    /* U+002C "," */
    0xff,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xdf, 0x0,

    /* U+002F "/" */
    0x18, 0xc6, 0x33, 0x18, 0xc6, 0x31, 0x18, 0xc6,
    0x31, 0x18, 0xc6, 0x0,

    /* U+0030 "0" */
    0x79, 0xec, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xf3, 0xf9, 0xc0,

    /* U+0031 "1" */
    0x37, 0xfb, 0x33, 0x33, 0x33, 0x33, 0x33,

    /* U+0032 "2" */
    0x7b, 0xfc, 0xf3, 0xc, 0x31, 0x86, 0x30, 0xc6,
    0x18, 0xff, 0xf0,

    /* U+0033 "3" */
    0x7b, 0xfc, 0xf3, 0xc, 0x73, 0x8e, 0xc, 0x3c,
    0xf3, 0xfd, 0xe0,

    /* U+0034 "4" */
    0x18, 0xe3, 0x8e, 0x79, 0xe7, 0xb6, 0xdb, 0xff,
    0xc6, 0x18, 0x60,

    /* U+0035 "5" */
    0xff, 0xfc, 0x30, 0xc3, 0xf, 0xbf, 0x4c, 0x30,
    0xf3, 0xfd, 0xe0,

    /* U+0036 "6" */
    0x7b, 0xfc, 0xf3, 0xc3, 0xf, 0xbf, 0xcf, 0x3c,
    0xf3, 0xfd, 0xe0,

    /* U+0037 "7" */
    0xff, 0xfc, 0xc7, 0x18, 0x63, 0x8c, 0x30, 0xc6,
    0x18, 0x63, 0x0,

    /* U+0038 "8" */
    0x7b, 0xfc, 0xf3, 0xcf, 0x37, 0x9e, 0xcf, 0x3c,
    0xf3, 0xf9, 0xe0,

    /* U+0039 "9" */
    0x7b, 0xfc, 0xf3, 0xcf, 0x3f, 0xdf, 0xc, 0x3c,
    0xf3, 0xfd, 0xe0,

    /* U+003A ":" */
    0xff, 0x85, 0xf8,

    /* U+003B ";" */
    0x1f, 0xa0, 0x3, 0x6d, 0x0,

    /* U+003C "<" */
    0x4, 0x31, 0xce, 0x71, 0x8e, 0x1c, 0x38, 0x70,
    0xc1, 0x0,

    /* U+003D "=" */
    0xff, 0xff, 0x0, 0x0, 0xff, 0xff,

    /* U+003E ">" */
    0x83, 0xe, 0x1c, 0x38, 0x61, 0xce, 0x73, 0x8c,
    0x20, 0x0,

    /* U+003F "?" */
    0x7b, 0xfc, 0xf3, 0xc, 0x63, 0x8c, 0x30, 0x0,
    0xc, 0x71, 0xc0,

    /* U+0040 "@" */
    0x7f, 0xbf, 0xfc, 0xf, 0x7b, 0xde, 0xf7, 0xbd,
    0xef, 0x7b, 0xde, 0xf7, 0xfd, 0xff, 0x0, 0xff,
    0xdf, 0xf0,

    /* U+0041 "A" */
    0xc, 0xe, 0x7, 0x3, 0x83, 0xe1, 0xb0, 0xd8,
    0x6e, 0x73, 0x31, 0x9f, 0xcf, 0xfe, 0x1e, 0xc,

    /* U+0042 "B" */
    0xfe, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff,
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xfe,

    /* U+0043 "C" */
    0x7e, 0xff, 0xc3, 0xc3, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc3, 0xc3, 0xff, 0x7e,

    /* U+0044 "D" */
    0xfe, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xfe,

    /* U+0045 "E" */
    0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xfc, 0xfc,
    0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff,

    /* U+0046 "F" */
    0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xfc, 0xfc,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,

    /* U+0047 "G" */
    0x7e, 0xff, 0xc3, 0xc3, 0xc0, 0xc0, 0xcf, 0xcf,
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0x7e,

    /* U+0048 "H" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,

    /* U+0049 "I" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0xfe,

    /* U+004B "K" */
    0xc6, 0xce, 0xcc, 0xdc, 0xd8, 0xf0, 0xf0, 0xf0,
    0xf8, 0xd8, 0xcc, 0xcc, 0xc6, 0xc6,

    /* U+004C "L" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff,

    /* U+004D "M" */
    0xc1, 0xe0, 0xf8, 0xfc, 0x7e, 0x3f, 0xbf, 0xdf,
    0xab, 0xd5, 0xee, 0xf6, 0x79, 0x3c, 0x1e, 0xc,

    /* U+004E "N" */
    0xc3, 0xe3, 0xe3, 0xe3, 0xf3, 0xf3, 0xdb, 0xdb,
    0xcf, 0xcf, 0xcf, 0xc7, 0xc7, 0xc3,

    /* U+004F "O" */
    0x7e, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0x7e,

    /* U+0050 "P" */
    0xff, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff,
    0xfe, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,

    /* U+0051 "Q" */
    0x7e, 0x7f, 0xb0, 0xd8, 0x6c, 0x36, 0x1b, 0xd,
    0x86, 0xc3, 0x61, 0xb0, 0xd8, 0x6f, 0xf3, 0xfc,
    0x6, 0x0,

    /* U+0052 "R" */
    0xfe, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff,
    0xfe, 0xc6, 0xc6, 0xc6, 0xc3, 0xc3,

    /* U+0053 "S" */
    0x7e, 0xff, 0xc3, 0xc3, 0xc0, 0xf0, 0x7c, 0x1e,
    0x7, 0x3, 0xc3, 0xc3, 0xff, 0x7e,

    /* U+0054 "T" */
    0xff, 0xff, 0xf0, 0xc0, 0x30, 0xc, 0x3, 0x0,
    0xc0, 0x30, 0xc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0x0,

    /* U+0055 "U" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0x7e,

    /* U+0056 "V" */
    0xc3, 0xe3, 0x63, 0x67, 0x66, 0x66, 0x76, 0x36,
    0x3e, 0x3c, 0x3c, 0x3c, 0x1c, 0x18,

    /* U+0057 "W" */
    0xcd, 0xe6, 0xd3, 0x6d, 0xb7, 0xdb, 0xed, 0xfe,
    0xfe, 0x77, 0x3b, 0x8c, 0xc6, 0x63, 0x31, 0x98,

    /* U+0058 "X" */
    0xc3, 0x63, 0x63, 0x36, 0x36, 0x3c, 0x1c, 0x1c,
    0x1c, 0x34, 0x36, 0x66, 0x63, 0x63,

    /* U+0059 "Y" */
    0xc3, 0xc3, 0x66, 0x66, 0x66, 0x3c, 0x3c, 0x3c,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18,

    /* U+005A "Z" */
    0xff, 0xff, 0x7, 0x6, 0xe, 0xc, 0x18, 0x18,
    0x30, 0x70, 0x60, 0xe0, 0xff, 0xff,

    /* U+005B "[" */
    0x7f, 0x6d, 0xb6, 0xdb, 0x6d, 0xbb,

    /* U+005C "\\" */
    0xc6, 0x30, 0x86, 0x31, 0x8c, 0x61, 0x8c, 0x63,
    0x18, 0x63, 0x18, 0xc0,

    /* U+005D "]" */
    0xdd, 0xb6, 0xdb, 0x6d, 0xb6, 0xfe,

    /* U+005E "^" */
    0x23, 0x95, 0xb0,

    /* U+005F "_" */
    0xff, 0xff,

    /* U+0060 "`" */
    0xd9, 0x80,

    /* U+0061 "a" */
    0x7b, 0xfc, 0xc3, 0x7f, 0xfc, 0xf3, 0xfd, 0xf0,

    /* U+0062 "b" */
    0xc3, 0xc, 0x30, 0xfb, 0xfc, 0xf3, 0xcf, 0x3c,
    0xf3, 0xff, 0xe0,

    /* U+0063 "c" */
    0x7b, 0xfc, 0xf0, 0xc3, 0xc, 0x33, 0xfd, 0xe0,

    /* U+0064 "d" */
    0xc, 0x30, 0xc3, 0x7f, 0xfc, 0xf3, 0xcf, 0x3c,
    0xf3, 0xfd, 0xf0,

    /* U+0065 "e" */
    0x7b, 0xfc, 0xf3, 0xff, 0xfc, 0x33, 0xfd, 0xe0,

    /* U+0066 "f" */
    0x3b, 0xd8, 0xcf, 0x79, 0x8c, 0x63, 0x18, 0xc6,
    0x30,

    /* U+0067 "g" */
    0x7f, 0xfc, 0xf3, 0xcf, 0x3c, 0xf3, 0xfd, 0xf0,
    0xf3, 0xfd, 0xe0,

    /* U+0068 "h" */
    0xc3, 0xc, 0x30, 0xfb, 0xfc, 0xf3, 0xcf, 0x3c,
    0xf3, 0xcf, 0x30,

    /* U+0069 "i" */
    0xf3, 0xff, 0xff, 0xc0,

    /* U+006A "j" */
    0x33, 0x3, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3f,
    0xe0,

    /* U+006B "k" */
    0xc1, 0x83, 0x6, 0xc, 0xd9, 0xb6, 0x7c, 0xf1,
    0xf3, 0xe6, 0x6c, 0xd8, 0xc0,

    /* U+006C "l" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+006D "m" */
    0xff, 0xbf, 0xfc, 0xcf, 0x33, 0xcc, 0xf3, 0x3c,
    0xcf, 0x33, 0xcc, 0xf3, 0x30,

    /* U+006E "n" */
    0xfb, 0xfc, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x30,

    /* U+006F "o" */
    0x7b, 0xfc, 0xf3, 0xcf, 0x3c, 0xf3, 0xfd, 0xe0,

    /* U+0070 "p" */
    0xfb, 0xfc, 0xf3, 0xcf, 0x3c, 0xf3, 0xff, 0xec,
    0x30, 0xc3, 0x0,

    /* U+0071 "q" */
    0x7f, 0xfc, 0xf3, 0xcf, 0x3c, 0xf3, 0xfd, 0xf0,
    0xc3, 0xc, 0x30,

    /* U+0072 "r" */
    0xff, 0xcc, 0xcc, 0xcc, 0xcc,

    /* U+0073 "s" */
    0x7b, 0xfc, 0xf0, 0xf9, 0xf0, 0xf3, 0xfd, 0xe0,

    /* U+0074 "t" */
    0x26, 0x66, 0xff, 0x66, 0x66, 0x66, 0x66,

    /* U+0075 "u" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0xfd, 0xf0,

    /* U+0076 "v" */
    0xc6, 0x8d, 0x9b, 0x26, 0xc5, 0x8f, 0x1c, 0x38,
    0x30,

    /* U+0077 "w" */
    0xcc, 0xf3, 0x26, 0xd9, 0xf6, 0x7f, 0x9f, 0xe7,
    0x39, 0xcc, 0x33, 0xc, 0xc0,

    /* U+0078 "x" */
    0xcc, 0xd9, 0xb1, 0x43, 0x87, 0xe, 0x36, 0x6c,
    0x98,

    /* U+0079 "y" */
    0xc6, 0x8d, 0x9b, 0x26, 0xc7, 0x8f, 0x1c, 0x38,
    0x30, 0x41, 0x83, 0xc, 0x0,

    /* U+007A "z" */
    0xff, 0xf1, 0xc6, 0x30, 0xc6, 0x30, 0xff, 0xf0,

    /* U+007B "{" */
    0x8, 0xce, 0xf7, 0x9c, 0x61,

    /* U+007C "|" */
    0xff, 0xff, 0xf0, 0x3f, 0xff, 0xf0,

    /* U+007D "}" */
    0x86, 0x39, 0xef, 0x73, 0x10,

    /* U+007E "~" */
    0xe3, 0x83, 0xc6,

    /* U+00B5 "µ" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0xff, 0xec,
    0x30, 0xc3, 0xc, 0x0,

    /* U+00C4 "Ä" */
    0x36, 0x1b, 0x0, 0x3, 0x81, 0xc0, 0xe0, 0x70,
    0x6c, 0x36, 0x1b, 0xd, 0x8c, 0x66, 0x33, 0xfb,
    0xff, 0x83, 0xc1, 0x80,

    /* U+00D6 "Ö" */
    0x6c, 0x6c, 0x0, 0x7e, 0xff, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff,
    0x7e,

    /* U+00DC "Ü" */
    0x6c, 0x6c, 0x0, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff,
    0x7e,

    /* U+00DF "ß" */
    0x7b, 0xfc, 0xf3, 0xcf, 0x3d, 0xf7, 0xcf, 0x3c,
    0xf3, 0xdf, 0x6c, 0x30, 0xc3, 0x0,

    /* U+00E4 "ä" */
    0xdb, 0x60, 0x1e, 0xff, 0x30, 0xdf, 0xff, 0x3c,
    0xff, 0x7c,

    /* U+00F6 "ö" */
    0xdb, 0x60, 0x1e, 0xff, 0x3c, 0xf3, 0xcf, 0x3c,
    0xff, 0x78,

    /* U+00FC "ü" */
    0xdb, 0x60, 0x33, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xff, 0x7c,

    /* U+20AC "€" */
    0x3c, 0xfd, 0x9b, 0xf, 0xdf, 0x98, 0x30, 0xfd,
    0xf9, 0x83, 0x37, 0xe7, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 72, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 63, .box_w = 3, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 7, .adv_w = 86, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 10, .adv_w = 175, .box_w = 10, .box_h = 11, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 24, .adv_w = 115, .box_w = 6, .box_h = 18, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 38, .adv_w = 186, .box_w = 11, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 56, .adv_w = 174, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 74, .adv_w = 51, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 76, .adv_w = 79, .box_w = 3, .box_h = 18, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 83, .adv_w = 73, .box_w = 4, .box_h = 18, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 92, .adv_w = 148, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 101, .adv_w = 146, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 109, .adv_w = 51, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 110, .adv_w = 117, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 112, .adv_w = 59, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 114, .adv_w = 98, .box_w = 5, .box_h = 18, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 126, .adv_w = 108, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 137, .adv_w = 71, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 111, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 110, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 101, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 177, .adv_w = 108, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 109, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 115, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 109, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 110, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 63, .box_w = 3, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 235, .adv_w = 63, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 240, .adv_w = 112, .box_w = 6, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 151, .box_w = 8, .box_h = 6, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 256, .adv_w = 117, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 266, .adv_w = 111, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 184, .box_w = 10, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 295, .adv_w = 154, .box_w = 9, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 325, .adv_w = 144, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 353, .adv_w = 144, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 367, .adv_w = 144, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 144, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 395, .adv_w = 147, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 409, .adv_w = 51, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 413, .adv_w = 57, .box_w = 4, .box_h = 18, .ofs_x = -1, .ofs_y = -4},
    {.bitmap_index = 422, .adv_w = 137, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 436, .adv_w = 139, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 450, .adv_w = 170, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 466, .adv_w = 151, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 480, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 508, .adv_w = 160, .box_w = 9, .box_h = 16, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 526, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 540, .adv_w = 141, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 554, .adv_w = 161, .box_w = 10, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 586, .adv_w = 143, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 600, .adv_w = 163, .box_w = 9, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 616, .adv_w = 147, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 630, .adv_w = 141, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 644, .adv_w = 141, .box_w = 8, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 658, .adv_w = 73, .box_w = 3, .box_h = 16, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 664, .adv_w = 98, .box_w = 5, .box_h = 18, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 676, .adv_w = 70, .box_w = 3, .box_h = 16, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 682, .adv_w = 93, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 14},
    {.bitmap_index = 685, .adv_w = 150, .box_w = 8, .box_h = 2, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 687, .adv_w = 64, .box_w = 3, .box_h = 3, .ofs_x = 0, .ofs_y = 14},
    {.bitmap_index = 689, .adv_w = 104, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 697, .adv_w = 107, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 104, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 716, .adv_w = 109, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 727, .adv_w = 104, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 735, .adv_w = 85, .box_w = 5, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 744, .adv_w = 110, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 755, .adv_w = 107, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 766, .adv_w = 52, .box_w = 2, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 770, .adv_w = 58, .box_w = 4, .box_h = 17, .ofs_x = -1, .ofs_y = -4},
    {.bitmap_index = 779, .adv_w = 127, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 792, .adv_w = 51, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 796, .adv_w = 166, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 809, .adv_w = 104, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 817, .adv_w = 104, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 825, .adv_w = 104, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 836, .adv_w = 109, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 847, .adv_w = 64, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 852, .adv_w = 103, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 860, .adv_w = 75, .box_w = 4, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 867, .adv_w = 105, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 875, .adv_w = 122, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 884, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 897, .adv_w = 110, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 906, .adv_w = 122, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 919, .adv_w = 105, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 927, .adv_w = 101, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 932, .adv_w = 59, .box_w = 2, .box_h = 22, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 938, .adv_w = 101, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 943, .adv_w = 98, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 946, .adv_w = 112, .box_w = 6, .box_h = 15, .ofs_x = 1, .ofs_y = -5},
    {.bitmap_index = 958, .adv_w = 154, .box_w = 9, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 978, .adv_w = 143, .box_w = 8, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 995, .adv_w = 143, .box_w = 8, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1012, .adv_w = 127, .box_w = 6, .box_h = 18, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1026, .adv_w = 104, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1036, .adv_w = 104, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1046, .adv_w = 105, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1056, .adv_w = 131, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0xf, 0x21, 0x27, 0x2a, 0x2f, 0x41, 0x47,
    0x1ff7
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 181, .range_length = 8184, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 9, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    34, 55,
    34, 56,
    34, 58,
    34, 87,
    34, 88,
    36, 14,
    39, 13,
    39, 15,
    39, 34,
    39, 89,
    39, 91,
    43, 43,
    44, 14,
    44, 87,
    44, 88,
    45, 14,
    45, 55,
    45, 58,
    45, 87,
    49, 13,
    49, 15,
    53, 13,
    53, 14,
    53, 15,
    53, 27,
    53, 28,
    53, 34,
    53, 66,
    53, 68,
    53, 69,
    53, 70,
    53, 72,
    53, 78,
    53, 79,
    53, 80,
    53, 81,
    53, 82,
    53, 83,
    53, 84,
    53, 86,
    53, 87,
    53, 88,
    53, 89,
    53, 90,
    53, 91,
    55, 13,
    55, 15,
    55, 34,
    56, 13,
    57, 14,
    58, 13,
    58, 14,
    58, 15,
    58, 27,
    58, 28,
    58, 34,
    59, 14,
    66, 53,
    68, 53,
    70, 53,
    71, 13,
    71, 14,
    71, 15,
    71, 27,
    71, 28,
    71, 34,
    71, 66,
    71, 68,
    71, 69,
    71, 70,
    71, 72,
    71, 78,
    71, 79,
    71, 80,
    71, 81,
    71, 82,
    71, 83,
    71, 84,
    71, 86,
    71, 87,
    71, 88,
    71, 89,
    71, 90,
    71, 91,
    72, 43,
    72, 53,
    76, 14,
    78, 53,
    79, 53,
    80, 53,
    81, 53,
    82, 43,
    82, 53,
    83, 59,
    84, 53,
    86, 53,
    87, 13,
    87, 15,
    87, 34,
    87, 59,
    88, 13,
    88, 59,
    89, 53,
    90, 53,
    91, 53
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -22, -14, -17, -16, -14, -42, -43, -43,
    -16, -16, -30, 13, -27, -13, -14, -41,
    -19, -16, -18, -43, -43, -18, -18, -18,
    -20, -20, -16, -45, -53, -43, -53, -53,
    -53, -45, -53, -54, -45, -45, -53, -53,
    -49, -50, -52, -53, -44, -24, -23, -38,
    -13, -14, -22, -15, -22, -18, -19, -35,
    -14, -15, -14, -14, -18, -17, -18, -19,
    -19, -16, -17, -17, -17, -17, -17, -17,
    -18, -17, -17, -17, -22, -21, -22, -17,
    -22, -22, -22, -22, 13, -16, -15, -15,
    -15, -14, -14, 13, -16, -13, -14, -15,
    -22, -21, -15, -22, -13, -14, -14, -16,
    -14
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 105,
    .glyph_ids_size = 0
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
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 2,
    .bpp = 1,
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
const lv_font_t ui_font_SchildGrot18 = {
#else
lv_font_t ui_font_SchildGrot18 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 25,          /*The maximum line height required by the font*/
    .base_line = 5,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_SCHILDGROT18*/


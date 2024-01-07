/*******************************************************************************
 * Size: 24 px
 * Bpp: 1
 * Opts: 
 ******************************************************************************/

#include <lvgl.h>

#ifndef TIMEFONT24
#define TIMEFONT24 1
#endif

#if TIMEFONT24

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xff, 0x3, 0xc0,

    /* U+0022 "\"" */
    0xde, 0xf7, 0xbd, 0xec,

    /* U+0023 "#" */
    0xc, 0xc3, 0x20, 0x98, 0x66, 0x19, 0x9f, 0xf7,
    0xfc, 0x4c, 0x33, 0xc, 0xc3, 0x33, 0xfe, 0xff,
    0x99, 0x86, 0x61, 0x90, 0x64, 0x0,

    /* U+0024 "$" */
    0x8, 0x8, 0x8, 0x7e, 0xef, 0xcb, 0xc8, 0xf8,
    0x7c, 0x1e, 0xf, 0xcb, 0xcb, 0xeb, 0x7e, 0x1c,
    0x8, 0x8, 0x8,

    /* U+0025 "%" */
    0x78, 0x8, 0xfc, 0x18, 0xcc, 0x30, 0xcc, 0x30,
    0xcc, 0x60, 0xcc, 0x40, 0xcc, 0xc0, 0xfc, 0x80,
    0x79, 0xbe, 0x3, 0x3e, 0x2, 0x63, 0x6, 0x63,
    0x4, 0x63, 0xc, 0x63, 0x8, 0x63, 0x10, 0x3e,
    0x30, 0x3e,

    /* U+0026 "&" */
    0xe, 0x1, 0xf0, 0x39, 0x83, 0x18, 0x31, 0x83,
    0x18, 0x3b, 0x1, 0xe0, 0x3c, 0x7, 0xe6, 0xe6,
    0x6c, 0x36, 0xc3, 0xcc, 0x1c, 0xe3, 0xc7, 0xfe,
    0x3e, 0x70,

    /* U+0027 "'" */
    0xff, 0xf0,

    /* U+0028 "(" */
    0x18, 0x8c, 0x46, 0x33, 0x18, 0xc6, 0x31, 0x8c,
    0x63, 0x8c, 0x61, 0xc, 0x21, 0x80,

    /* U+0029 ")" */
    0xc2, 0x18, 0x43, 0x18, 0xc3, 0x18, 0xc6, 0x31,
    0x8c, 0xe6, 0x31, 0x18, 0x8c, 0x0,

    /* U+002A "*" */
    0x32, 0xbf, 0xbe, 0xac, 0xc0,

    /* U+002B "+" */
    0xc, 0x6, 0x3, 0x1, 0x80, 0xc0, 0x63, 0xff,
    0xff, 0xc, 0x6, 0x3, 0x1, 0x80, 0xc0,

    /* U+002C "," */
    0xf7, 0x80,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x8, 0xc6, 0x31, 0x88, 0xc6, 0x31, 0x88, 0xc6,
    0x31, 0x88, 0x46, 0x30,

    /* U+0030 "0" */
    0x3e, 0x3f, 0x98, 0xdc, 0x6c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1f, 0x19,
    0x8c, 0xfe, 0x3e, 0x0,

    /* U+0031 "1" */
    0xc, 0x3e, 0x3f, 0x1, 0x80, 0xc0, 0x60, 0x30,
    0x18, 0xc, 0x6, 0x3, 0x1, 0x80, 0xc0, 0x60,
    0x31, 0xff, 0xff, 0x80,

    /* U+0032 "2" */
    0x3e, 0x3f, 0xb8, 0xf8, 0x3c, 0x1e, 0xc, 0x6,
    0x6, 0x7, 0x7, 0x7, 0x7, 0x3, 0x3, 0x3,
    0x1, 0xff, 0xff, 0x80,

    /* U+0033 "3" */
    0x3e, 0x3f, 0xb8, 0xf8, 0x3c, 0x18, 0xc, 0xc,
    0x3c, 0x1f, 0x1, 0x80, 0x60, 0x3c, 0x1e, 0xf,
    0x8e, 0xfe, 0x3e, 0x0,

    /* U+0034 "4" */
    0xc, 0x3, 0x1, 0x80, 0x64, 0x19, 0xc, 0xc3,
    0x30, 0x8c, 0x63, 0x18, 0xcc, 0x33, 0xff, 0xff,
    0xc0, 0xc0, 0x30, 0xc, 0x3, 0x0,

    /* U+0035 "5" */
    0x7f, 0x3f, 0x98, 0xc, 0x6, 0x3, 0x1, 0xb8,
    0xfe, 0x73, 0xb0, 0xc0, 0x60, 0x3c, 0x1e, 0xd,
    0x8c, 0xfe, 0x3e, 0x0,

    /* U+0036 "6" */
    0x1e, 0x1f, 0x98, 0xec, 0x3c, 0x1e, 0x3, 0x79,
    0xfe, 0xe3, 0x60, 0xf0, 0x78, 0x3c, 0x1e, 0xd,
    0x8c, 0xfe, 0x1e, 0x0,

    /* U+0037 "7" */
    0xff, 0xff, 0xc0, 0xc0, 0x60, 0x60, 0x30, 0x30,
    0x18, 0x8, 0xc, 0x6, 0x2, 0x3, 0x1, 0x80,
    0xc0, 0x60, 0x30, 0x0,

    /* U+0038 "8" */
    0x3e, 0x3f, 0xb8, 0xf8, 0x3c, 0x1e, 0xd, 0x8e,
    0xfe, 0x7f, 0x31, 0xb0, 0x78, 0x3c, 0x1e, 0xf,
    0x8e, 0xfe, 0x3e, 0x0,

    /* U+0039 "9" */
    0x3c, 0x3f, 0x98, 0xd8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0x63, 0xbf, 0xcf, 0x60, 0x3c, 0x1e, 0x1b,
    0x8c, 0xfc, 0x3c, 0x0,

    /* U+003A ":" */
    0xf0, 0x0, 0x3, 0xc0,

    /* U+003B ";" */
    0xf0, 0x0, 0x3, 0xde,

    /* U+003C "<" */
    0x0, 0xc0, 0xf0, 0xf8, 0xf8, 0xf8, 0x38, 0xe,
    0x1, 0xe0, 0x1e, 0x1, 0xe0, 0x1c, 0x1,

    /* U+003D "=" */
    0xff, 0xff, 0xc0, 0x0, 0x0, 0x7, 0xff, 0xfe,

    /* U+003E ">" */
    0x80, 0x38, 0x7, 0x80, 0x78, 0x7, 0x80, 0x70,
    0x3c, 0x3e, 0x3e, 0x3e, 0xe, 0x2, 0x0,

    /* U+003F "?" */
    0x3e, 0x3f, 0x98, 0xf8, 0x3c, 0x1e, 0xc, 0x6,
    0x6, 0x7, 0x3, 0x3, 0x1, 0x80, 0xc0, 0x0,
    0x0, 0x18, 0xc, 0x0,

    /* U+0040 "@" */
    0x3, 0xf8, 0x3, 0xff, 0x81, 0xc0, 0xf0, 0xe0,
    0xe, 0x70, 0xf1, 0x98, 0xfe, 0x6e, 0x31, 0x8f,
    0xc, 0x63, 0xc0, 0x18, 0xf0, 0x3e, 0x3c, 0x3f,
    0x8f, 0x1c, 0x63, 0xc6, 0x18, 0xf1, 0x86, 0x6c,
    0x63, 0x99, 0x9f, 0xfc, 0x63, 0xce, 0x1c, 0x0,
    0x3, 0xc0, 0x0, 0x7f, 0xc0, 0x7, 0xe0, 0x0,

    /* U+0041 "A" */
    0x7, 0x0, 0x38, 0x3, 0x60, 0x1b, 0x0, 0xd8,
    0xc, 0xe0, 0x63, 0x3, 0x18, 0x18, 0xe1, 0x83,
    0xc, 0x18, 0x7f, 0xc7, 0xff, 0x30, 0x19, 0x80,
    0xdc, 0x7, 0xc0, 0x18,

    /* U+0042 "B" */
    0xff, 0x1f, 0xf3, 0x7, 0x60, 0x6c, 0xd, 0x81,
    0xb0, 0x36, 0xc, 0xff, 0x1f, 0xf3, 0x3, 0x60,
    0x3c, 0x7, 0x80, 0xf0, 0x3f, 0xfe, 0xff, 0x80,

    /* U+0043 "C" */
    0xf, 0x83, 0xfc, 0x70, 0xe6, 0x7, 0xe0, 0x3c,
    0x3, 0xc0, 0xc, 0x0, 0xc0, 0xc, 0x0, 0xc0,
    0xc, 0x3, 0xe0, 0x36, 0x7, 0x70, 0xe3, 0xfc,
    0xf, 0x80,

    /* U+0044 "D" */
    0xfe, 0x1f, 0xf3, 0x7, 0x60, 0x6c, 0xf, 0x80,
    0xf0, 0x1e, 0x3, 0xc0, 0x78, 0xf, 0x1, 0xe0,
    0x3c, 0xd, 0x81, 0xb0, 0x77, 0xfc, 0xfe, 0x0,

    /* U+0045 "E" */
    0xff, 0xff, 0xfc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0x0, 0xff, 0xbf, 0xec, 0x3, 0x0, 0xc0,
    0x30, 0xc, 0x3, 0xff, 0xff, 0xc0,

    /* U+0046 "F" */
    0xff, 0xff, 0xfc, 0x3, 0x0, 0xc0, 0x30, 0xc,
    0x3, 0x0, 0xff, 0xbf, 0xec, 0x3, 0x0, 0xc0,
    0x30, 0xc, 0x3, 0x0, 0xc0, 0x0,

    /* U+0047 "G" */
    0xf, 0xc1, 0xff, 0x1e, 0x1c, 0xc0, 0x76, 0x1,
    0xe0, 0xf, 0x0, 0x18, 0x0, 0xc3, 0xfe, 0x1f,
    0xf0, 0x7, 0x80, 0x36, 0x1, 0xb0, 0x1d, 0xc1,
    0xe7, 0xff, 0xf, 0x88,

    /* U+0048 "H" */
    0xc0, 0x78, 0xf, 0x1, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0x3, 0xff, 0xff, 0xff, 0x1, 0xe0,
    0x3c, 0x7, 0x80, 0xf0, 0x1e, 0x3, 0xc0, 0x60,

    /* U+0049 "I" */
    0xff, 0xff, 0xff, 0xff, 0xc0,

    /* U+004A "J" */
    0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
    0x3, 0x3, 0x3, 0x3, 0xc3, 0xc3, 0xe7, 0x7e,
    0x3c,

    /* U+004B "K" */
    0xc0, 0xf8, 0x3b, 0x6, 0x61, 0x8c, 0x71, 0x8c,
    0x33, 0x6, 0xe0, 0xfe, 0x1e, 0xc3, 0x8c, 0x61,
    0x8c, 0x39, 0x83, 0x30, 0x76, 0x6, 0xc0, 0xe0,

    /* U+004C "L" */
    0xc0, 0x60, 0x30, 0x18, 0xc, 0x6, 0x3, 0x1,
    0x80, 0xc0, 0x60, 0x30, 0x18, 0xc, 0x6, 0x3,
    0x1, 0xff, 0xff, 0x80,

    /* U+004D "M" */
    0xe0, 0x3f, 0x81, 0xfc, 0x1f, 0xe0, 0xff, 0x7,
    0xec, 0x2f, 0x63, 0x7b, 0x1b, 0xd8, 0xde, 0x64,
    0xf3, 0x67, 0x9b, 0x3c, 0xd9, 0xe2, 0x8f, 0x1c,
    0x78, 0xe3, 0xc7, 0x18,

    /* U+004E "N" */
    0xc0, 0x7c, 0xf, 0x81, 0xf8, 0x3f, 0x87, 0xb0,
    0xf7, 0x1e, 0x63, 0xce, 0x78, 0xcf, 0x1d, 0xe1,
    0xfc, 0x1f, 0x83, 0xf0, 0x3e, 0x7, 0xc0, 0x60,

    /* U+004F "O" */
    0xf, 0x81, 0xff, 0x1c, 0x1c, 0xc0, 0x6e, 0x3,
    0x60, 0xf, 0x0, 0x78, 0x3, 0xc0, 0x1e, 0x0,
    0xf0, 0x7, 0x80, 0x3e, 0x3, 0x30, 0x19, 0xc1,
    0xc7, 0xfc, 0xf, 0x80,

    /* U+0050 "P" */
    0xff, 0x9f, 0xfb, 0x3, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0x6, 0xff, 0xdf, 0xe3, 0x0, 0x60,
    0xc, 0x1, 0x80, 0x30, 0x6, 0x0, 0xc0, 0x0,

    /* U+0051 "Q" */
    0xf, 0x81, 0xff, 0x1c, 0x1c, 0xc0, 0x6e, 0x3,
    0x60, 0xf, 0x0, 0x78, 0x3, 0xc0, 0x1e, 0x0,
    0xf0, 0x7, 0x80, 0x3e, 0x3, 0x30, 0x19, 0xc1,
    0xc7, 0xfc, 0xf, 0x80, 0xe, 0x0, 0x18, 0x0,
    0x70,

    /* U+0052 "R" */
    0xff, 0x9f, 0xfb, 0x3, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0x6, 0xff, 0xdf, 0xe3, 0xe, 0x60,
    0xcc, 0x1d, 0x81, 0xb0, 0x36, 0x3, 0xc0, 0x60,

    /* U+0053 "S" */
    0x1f, 0xf, 0xfb, 0x83, 0xe0, 0x3c, 0x7, 0x80,
    0x1c, 0x3, 0xf0, 0x1f, 0x80, 0x78, 0x3, 0x80,
    0x3c, 0x7, 0x80, 0xf8, 0x3b, 0xfe, 0x1f, 0x0,

    /* U+0054 "T" */
    0xff, 0xff, 0xfc, 0x18, 0x3, 0x0, 0x60, 0xc,
    0x1, 0x80, 0x30, 0x6, 0x0, 0xc0, 0x18, 0x3,
    0x0, 0x60, 0xc, 0x1, 0x80, 0x30, 0x6, 0x0,

    /* U+0055 "U" */
    0xc0, 0x78, 0xf, 0x1, 0xe0, 0x3c, 0x7, 0x80,
    0xf0, 0x1e, 0x3, 0xc0, 0x78, 0xf, 0x1, 0xe0,
    0x3c, 0x7, 0x81, 0xd8, 0x33, 0xfe, 0x1f, 0x0,

    /* U+0056 "V" */
    0xe0, 0x1b, 0x1, 0xd8, 0xc, 0xc0, 0x63, 0x3,
    0x18, 0x30, 0xc1, 0x87, 0xc, 0x18, 0xc0, 0xc6,
    0x6, 0x30, 0x19, 0x80, 0xd8, 0x6, 0xc0, 0x3e,
    0x0, 0xe0, 0x7, 0x0,

    /* U+0057 "W" */
    0xe0, 0xe0, 0xd8, 0x38, 0x36, 0x1e, 0xd, 0x87,
    0x83, 0x61, 0xb1, 0x9c, 0x6c, 0x63, 0x1b, 0x18,
    0xc4, 0xc6, 0x33, 0x31, 0xe, 0xce, 0xc1, 0xb1,
    0xb0, 0x6c, 0x6c, 0x1b, 0x1b, 0x6, 0x87, 0x81,
    0xe1, 0xe0, 0x38, 0x38, 0xe, 0xe, 0x0,

    /* U+0058 "X" */
    0x60, 0x33, 0x83, 0x8e, 0x18, 0x31, 0x81, 0xcc,
    0x6, 0xc0, 0x3e, 0x0, 0xe0, 0x7, 0x0, 0x3c,
    0x3, 0x60, 0x3b, 0x81, 0x8c, 0x18, 0x70, 0xc1,
    0xcc, 0x6, 0xe0, 0x38,

    /* U+0059 "Y" */
    0xc0, 0x3e, 0x7, 0x60, 0x63, 0xc, 0x30, 0xc1,
    0x98, 0x19, 0x80, 0xf0, 0xf, 0x0, 0x60, 0x6,
    0x0, 0x60, 0x6, 0x0, 0x60, 0x6, 0x0, 0x60,
    0x6, 0x0,

    /* U+005A "Z" */
    0x7f, 0xef, 0xfc, 0x3, 0x0, 0xe0, 0x18, 0x7,
    0x1, 0xc0, 0x30, 0xe, 0x1, 0x80, 0x70, 0xc,
    0x3, 0x80, 0xe0, 0x1c, 0x7, 0xff, 0xff, 0xe0,

    /* U+005B "[" */
    0xff, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcf, 0xf0,

    /* U+005C "\\" */
    0xc6, 0x10, 0x86, 0x31, 0x84, 0x21, 0x8c, 0x63,
    0x8, 0x63, 0x18, 0xc2,

    /* U+005D "]" */
    0xff, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x3f, 0xf0,

    /* U+005E "^" */
    0x1c, 0xe, 0x7, 0x86, 0xc3, 0x61, 0x99, 0x8c,
    0xc6, 0x61, 0xe0, 0xc0,

    /* U+005F "_" */
    0xff, 0xff, 0xfc,

    /* U+0060 "`" */
    0xe6, 0x30,

    /* U+0061 "a" */
    0x3e, 0x1f, 0xcc, 0x33, 0xc, 0x3, 0xf, 0xc7,
    0xf3, 0x8c, 0xc3, 0x30, 0xcc, 0x73, 0xff, 0x79,
    0xc0,

    /* U+0062 "b" */
    0xc0, 0x60, 0x30, 0x18, 0xd, 0xe7, 0xfb, 0x8d,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xf,
    0x8d, 0xfe, 0xde, 0x0,

    /* U+0063 "c" */
    0x3c, 0x7e, 0x67, 0xc3, 0xc3, 0xc0, 0xc0, 0xc0,
    0xc3, 0xc3, 0x67, 0x7e, 0x3c,

    /* U+0064 "d" */
    0x1, 0x80, 0xc0, 0x60, 0x33, 0xdb, 0xfd, 0x8f,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xd,
    0x8e, 0xff, 0x3d, 0x80,

    /* U+0065 "e" */
    0x3e, 0x3f, 0x98, 0xf8, 0x3c, 0x1f, 0xff, 0xff,
    0x80, 0xc1, 0xe0, 0xd8, 0xef, 0xe1, 0xe0,

    /* U+0066 "f" */
    0x3b, 0xd8, 0xcf, 0xfd, 0x8c, 0x63, 0x18, 0xc6,
    0x31, 0x8c, 0x60,

    /* U+0067 "g" */
    0x0, 0xc0, 0x31, 0xf8, 0xfc, 0x73, 0x98, 0x66,
    0x19, 0xce, 0x3f, 0xf, 0x86, 0x1, 0x80, 0x60,
    0x1f, 0xe7, 0xff, 0x3, 0xc0, 0xff, 0xf7, 0xf0,

    /* U+0068 "h" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xde, 0xff, 0xe3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3,

    /* U+0069 "i" */
    0xf0, 0xff, 0xff, 0xff, 0xc0,

    /* U+006A "j" */
    0x33, 0x0, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x3f, 0xe0,

    /* U+006B "k" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc3, 0xc6, 0xce, 0xcc,
    0xd8, 0xf8, 0xf8, 0xec, 0xcc, 0xc6, 0xc6, 0xc3,
    0xc3,

    /* U+006C "l" */
    0xff, 0xff, 0xff, 0xff, 0xc0,

    /* U+006D "m" */
    0xde, 0x7b, 0xfb, 0xfe, 0x38, 0xf0, 0xc3, 0xc3,
    0xf, 0xc, 0x3c, 0x30, 0xf0, 0xc3, 0xc3, 0xf,
    0xc, 0x3c, 0x30, 0xf0, 0xc3, 0xc3, 0xc,

    /* U+006E "n" */
    0xde, 0xff, 0xe3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3,

    /* U+006F "o" */
    0x3e, 0x3f, 0x98, 0xd8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xd8, 0xcf, 0xe3, 0xe0,

    /* U+0070 "p" */
    0xde, 0x7f, 0xb8, 0xd8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xf8, 0xdf, 0xed, 0xe6, 0x3,
    0x1, 0x80, 0xc0, 0x0,

    /* U+0071 "q" */
    0x3d, 0xbf, 0xd8, 0xf8, 0x3c, 0x1e, 0xf, 0x7,
    0x83, 0xc1, 0xe0, 0xd8, 0xef, 0xf3, 0xd8, 0xc,
    0x6, 0x3, 0x1, 0x80,

    /* U+0072 "r" */
    0xdf, 0xf9, 0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8c,
    0x0,

    /* U+0073 "s" */
    0x3c, 0x7e, 0xc7, 0xc3, 0xc0, 0x70, 0x3e, 0x7,
    0x3, 0xc3, 0xe7, 0x7e, 0x3c,

    /* U+0074 "t" */
    0x63, 0x18, 0xcf, 0xfd, 0x8c, 0x63, 0x18, 0xc6,
    0x31, 0x8f, 0x38,

    /* U+0075 "u" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc7, 0xff, 0x7b,

    /* U+0076 "v" */
    0xe1, 0x98, 0x66, 0x19, 0x86, 0x73, 0xc, 0xc3,
    0x30, 0xcc, 0x1e, 0x7, 0x81, 0xe0, 0x70, 0xc,
    0x0,

    /* U+0077 "w" */
    0xc3, 0xf, 0x8e, 0x36, 0x38, 0xd9, 0xe6, 0x67,
    0x99, 0x9e, 0x63, 0x69, 0x8d, 0x3c, 0x34, 0xf0,
    0xf3, 0xc1, 0xcf, 0x7, 0x18, 0x18, 0x60,

    /* U+0078 "x" */
    0x61, 0x98, 0x63, 0x30, 0xdc, 0x1e, 0x7, 0x0,
    0xc0, 0x78, 0x1e, 0xc, 0xc3, 0x31, 0x86, 0xe1,
    0x80,

    /* U+0079 "y" */
    0xe0, 0xd8, 0x66, 0x19, 0xc6, 0x31, 0x8c, 0xc3,
    0x30, 0x6c, 0x1a, 0x7, 0x80, 0xe0, 0x38, 0xc,
    0x3, 0x1, 0xc1, 0xe0, 0x70, 0x0,

    /* U+007A "z" */
    0xff, 0x7f, 0x81, 0x81, 0xc0, 0xc0, 0xc0, 0x60,
    0x60, 0x30, 0x30, 0x18, 0x1f, 0xff, 0xf8,

    /* U+007B "{" */
    0x1c, 0xf3, 0xc, 0x30, 0xc3, 0xc, 0x33, 0x8e,
    0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xf, 0x1c,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0,

    /* U+007D "}" */
    0xe3, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0x71,
    0xcc, 0x30, 0xc3, 0xc, 0x30, 0xc7, 0x3c, 0xe0,

    /* U+007E "~" */
    0x70, 0x3f, 0x18, 0xfc, 0xe,

    /* U+00B5 "µ" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc7, 0xff, 0xfb, 0xc0, 0xc0, 0xc0,
    0xc0,

    /* U+00C4 "Ä" */
    0xc, 0xc0, 0x33, 0x0, 0x0, 0x0, 0x0, 0x7,
    0x80, 0x1e, 0x0, 0x78, 0x3, 0xf0, 0xc, 0xc0,
    0x33, 0x1, 0xce, 0x6, 0x18, 0x18, 0x60, 0x61,
    0x83, 0x3, 0xf, 0xfc, 0x3f, 0xf1, 0x80, 0xe6,
    0x1, 0x98, 0x6, 0xe0, 0x1c,

    /* U+00D6 "Ö" */
    0xd, 0x80, 0x6c, 0x0, 0x0, 0x0, 0x0, 0xf8,
    0x1f, 0xf1, 0xc1, 0xcc, 0x6, 0xe0, 0x36, 0x0,
    0xf0, 0x7, 0x80, 0x3c, 0x1, 0xe0, 0xf, 0x0,
    0x78, 0x3, 0xe0, 0x33, 0x1, 0x9c, 0x1c, 0x7f,
    0xc0, 0xf8, 0x0,

    /* U+00DC "Ü" */
    0x1b, 0x3, 0x60, 0x0, 0x0, 0xc, 0x7, 0x80,
    0xf0, 0x1e, 0x3, 0xc0, 0x78, 0xf, 0x1, 0xe0,
    0x3c, 0x7, 0x80, 0xf0, 0x1e, 0x3, 0xc0, 0x78,
    0x1d, 0x83, 0x3f, 0xe1, 0xf0,

    /* U+00DF "ß" */
    0x3e, 0x1f, 0xce, 0x3b, 0x6, 0xc1, 0xb0, 0x6c,
    0x33, 0x78, 0xdf, 0x30, 0x6c, 0xf, 0x3, 0xc0,
    0xf0, 0x3c, 0x1b, 0x7e, 0xde, 0x0,

    /* U+00E4 "ä" */
    0x36, 0xd, 0x80, 0x0, 0x0, 0x3e, 0x1f, 0xcc,
    0x33, 0xc, 0x3, 0xf, 0xc7, 0xf3, 0x8c, 0xc3,
    0x30, 0xcc, 0x73, 0xff, 0x79, 0xc0,

    /* U+00F6 "ö" */
    0x36, 0x1b, 0x0, 0x0, 0x3, 0xe3, 0xf9, 0x8d,
    0x83, 0xc1, 0xe0, 0xf0, 0x78, 0x3c, 0x1e, 0xd,
    0x8c, 0xfe, 0x3e, 0x0,

    /* U+00FC "ü" */
    0x66, 0x66, 0x0, 0x0, 0xc3, 0xc3, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc7, 0xff,
    0x79,

    /* U+20AC "€" */
    0x7, 0xc3, 0xf1, 0xc0, 0xe0, 0x30, 0x18, 0xf,
    0xf3, 0xfc, 0x60, 0x3f, 0xcf, 0xf1, 0x80, 0x30,
    0xe, 0x1, 0xc0, 0x3f, 0x7, 0xc0,

    /* U+23F1 "⏱" */
    0x0, 0x70, 0x0, 0x3, 0x80, 0x0, 0x8, 0x18,
    0x3, 0xf8, 0xe0, 0x70, 0x77, 0x6, 0x7c, 0xe0,
    0x4c, 0x99, 0x4, 0xa4, 0xa4, 0x69, 0x24, 0xb2,
    0x81, 0x2, 0xb7, 0x8, 0x57, 0x40, 0x40, 0x5a,
    0x2, 0x2, 0xd4, 0x38, 0x56, 0x80, 0x80, 0xb4,
    0x0, 0x5, 0x94, 0x1, 0x4a, 0x80, 0x2, 0x92,
    0x41, 0x24, 0x4a, 0x4a, 0x41, 0x30, 0x64, 0x4,
    0x7c, 0x40, 0x18, 0xc, 0x0, 0x3f, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 88, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 88, .box_w = 2, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 11},
    {.bitmap_index = 10, .adv_w = 175, .box_w = 10, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 32, .adv_w = 175, .box_w = 8, .box_h = 19, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 51, .adv_w = 280, .box_w = 16, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 85, .adv_w = 210, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 111, .adv_w = 60, .box_w = 2, .box_h = 6, .ofs_x = 1, .ofs_y = 11},
    {.bitmap_index = 113, .adv_w = 105, .box_w = 5, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 127, .adv_w = 105, .box_w = 5, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 141, .adv_w = 122, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 11},
    {.bitmap_index = 146, .adv_w = 184, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 161, .adv_w = 88, .box_w = 2, .box_h = 5, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 163, .adv_w = 105, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 165, .adv_w = 88, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 88, .box_w = 5, .box_h = 19, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 178, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 218, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 238, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 258, .adv_w = 175, .box_w = 10, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 280, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 340, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 380, .adv_w = 88, .box_w = 2, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 88, .box_w = 2, .box_h = 16, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 388, .adv_w = 184, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 403, .adv_w = 184, .box_w = 9, .box_h = 7, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 411, .adv_w = 184, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 426, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 319, .box_w = 18, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 494, .adv_w = 210, .box_w = 13, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 522, .adv_w = 210, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 546, .adv_w = 227, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 227, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 596, .adv_w = 210, .box_w = 10, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 618, .adv_w = 192, .box_w = 10, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 640, .adv_w = 245, .box_w = 13, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 668, .adv_w = 227, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 692, .adv_w = 88, .box_w = 2, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 697, .adv_w = 157, .box_w = 8, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 714, .adv_w = 210, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 738, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 758, .adv_w = 262, .box_w = 13, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 786, .adv_w = 227, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 810, .adv_w = 245, .box_w = 13, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 838, .adv_w = 210, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 862, .adv_w = 245, .box_w = 13, .box_h = 20, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 895, .adv_w = 227, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 919, .adv_w = 210, .box_w = 11, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 943, .adv_w = 192, .box_w = 11, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 967, .adv_w = 227, .box_w = 11, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 991, .adv_w = 210, .box_w = 13, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1019, .adv_w = 297, .box_w = 18, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1058, .adv_w = 210, .box_w = 13, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1086, .adv_w = 210, .box_w = 12, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1112, .adv_w = 192, .box_w = 11, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1136, .adv_w = 88, .box_w = 4, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1147, .adv_w = 88, .box_w = 5, .box_h = 19, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 1159, .adv_w = 88, .box_w = 4, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1170, .adv_w = 148, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 1182, .adv_w = 175, .box_w = 11, .box_h = 2, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1185, .adv_w = 0, .box_w = 4, .box_h = 3, .ofs_x = 0, .ofs_y = 14},
    {.bitmap_index = 1187, .adv_w = 175, .box_w = 10, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1204, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1224, .adv_w = 157, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1237, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1257, .adv_w = 175, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1272, .adv_w = 88, .box_w = 5, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1283, .adv_w = 175, .box_w = 10, .box_h = 19, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1307, .adv_w = 175, .box_w = 8, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1324, .adv_w = 88, .box_w = 2, .box_h = 17, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1329, .adv_w = 70, .box_w = 4, .box_h = 21, .ofs_x = -1, .ofs_y = -4},
    {.bitmap_index = 1340, .adv_w = 157, .box_w = 8, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1357, .adv_w = 70, .box_w = 2, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1362, .adv_w = 262, .box_w = 14, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1385, .adv_w = 175, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1398, .adv_w = 175, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1413, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1433, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1453, .adv_w = 105, .box_w = 5, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1462, .adv_w = 157, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1475, .adv_w = 88, .box_w = 5, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1486, .adv_w = 175, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1499, .adv_w = 157, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1516, .adv_w = 227, .box_w = 14, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1539, .adv_w = 157, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1556, .adv_w = 157, .box_w = 10, .box_h = 17, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 1578, .adv_w = 157, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1593, .adv_w = 105, .box_w = 6, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1609, .adv_w = 82, .box_w = 2, .box_h = 21, .ofs_x = 2, .ofs_y = -4},
    {.bitmap_index = 1615, .adv_w = 105, .box_w = 6, .box_h = 21, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1631, .adv_w = 184, .box_w = 10, .box_h = 4, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 1636, .adv_w = 175, .box_w = 8, .box_h = 17, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 1653, .adv_w = 210, .box_w = 14, .box_h = 21, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1690, .adv_w = 245, .box_w = 13, .box_h = 21, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1725, .adv_w = 227, .box_w = 11, .box_h = 21, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 1754, .adv_w = 192, .box_w = 10, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1776, .adv_w = 175, .box_w = 10, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1798, .adv_w = 175, .box_w = 9, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1818, .adv_w = 175, .box_w = 8, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1835, .adv_w = 175, .box_w = 10, .box_h = 17, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1857, .adv_w = 488, .box_w = 21, .box_h = 24, .ofs_x = 5, .ofs_y = -4}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0xf, 0x21, 0x27, 0x2a, 0x2f, 0x41, 0x47,
    0x1ff7, 0x233c
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 181, .range_length = 9021, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 10, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    1, 34,
    1, 53,
    1, 58,
    1, 97,
    18, 18,
    34, 1,
    34, 53,
    34, 55,
    34, 56,
    34, 58,
    34, 87,
    34, 88,
    34, 90,
    39, 13,
    39, 15,
    39, 34,
    39, 64,
    39, 66,
    39, 68,
    39, 70,
    39, 80,
    39, 97,
    39, 101,
    39, 102,
    44, 72,
    45, 1,
    45, 53,
    45, 55,
    45, 56,
    45, 58,
    45, 90,
    49, 1,
    49, 13,
    49, 15,
    49, 34,
    49, 64,
    49, 97,
    51, 53,
    51, 55,
    51, 56,
    51, 58,
    53, 1,
    53, 13,
    53, 14,
    53, 15,
    53, 27,
    53, 28,
    53, 34,
    53, 48,
    53, 64,
    53, 66,
    53, 68,
    53, 70,
    53, 74,
    53, 80,
    53, 83,
    53, 84,
    53, 86,
    53, 88,
    53, 89,
    53, 90,
    53, 91,
    53, 97,
    53, 98,
    53, 101,
    53, 102,
    53, 103,
    53, 104,
    55, 13,
    55, 14,
    55, 15,
    55, 27,
    55, 28,
    55, 34,
    55, 64,
    55, 66,
    55, 68,
    55, 70,
    55, 74,
    55, 80,
    55, 83,
    55, 86,
    55, 90,
    55, 97,
    55, 101,
    55, 102,
    55, 103,
    56, 13,
    56, 14,
    56, 15,
    56, 27,
    56, 28,
    56, 34,
    56, 64,
    56, 66,
    56, 68,
    56, 70,
    56, 80,
    56, 83,
    56, 86,
    56, 90,
    56, 97,
    56, 101,
    56, 102,
    56, 103,
    57, 66,
    57, 68,
    57, 70,
    57, 80,
    57, 101,
    57, 102,
    58, 1,
    58, 13,
    58, 14,
    58, 15,
    58, 27,
    58, 28,
    58, 34,
    58, 64,
    58, 66,
    58, 68,
    58, 70,
    58, 72,
    58, 74,
    58, 80,
    58, 81,
    58, 82,
    58, 84,
    58, 85,
    58, 86,
    58, 87,
    58, 96,
    58, 97,
    58, 101,
    58, 102,
    58, 103,
    71, 71,
    83, 13,
    83, 15,
    83, 64,
    87, 13,
    87, 15,
    87, 64,
    88, 13,
    88, 15,
    88, 64,
    90, 13,
    90, 15,
    90, 64,
    97, 1,
    97, 53,
    97, 55,
    97, 56,
    97, 58,
    97, 87,
    97, 88,
    97, 90
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -17, -5, -5, -17, -23, -17, -23, -23,
    -12, -23, -5, -5, -5, -35, -35, -17,
    -35, -15, -15, -15, -15, -17, -15, -15,
    -8, -12, -23, -23, -23, -23, -12, -5,
    -40, -40, -23, -40, -23, -5, -5, -5,
    -5, -5, -35, -17, -35, -35, -35, -23,
    -5, -35, -29, -35, -35, -7, -35, -12,
    -27, -27, -17, -15, -17, -15, -23, -5,
    -29, -35, -27, -5, -29, -17, -29, -12,
    -12, -23, -29, -23, -17, -17, -5, -17,
    -12, -12, -12, -23, -23, -17, -12, -17,
    -5, -17, -5, -5, -12, -17, -12, -5,
    -5, -5, -5, -5, -3, -12, -12, -5,
    -5, -4, -8, -8, -8, -4, -8, -5,
    -40, -29, -40, -17, -20, -23, -40, -23,
    -25, -25, -15, -12, -25, -23, -29, -23,
    -8, -17, -17, -23, -23, -23, -25, -17,
    -5, -17, -17, -17, -23, -23, -23, -17,
    -17, -17, -23, -23, -23, -17, -23, -23,
    -12, -23, -5, -5, -5
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 157,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LV_VERSION_CHECK(8, 0, 0)
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
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
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LV_VERSION_CHECK(8, 0, 0)
const lv_font_t TimeFont24 = {
#else
lv_font_t TimeFont24 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 25,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if TIMEFONT24*/

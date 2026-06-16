/*
 * single_file_c_hcc2d_encoder_v0.9.0.c
 * HCC2D Encoder — single-file C reference implementation
 * Copyright 2010-2026 Marco Querini
 * SPDX-License-Identifier: Apache-2.0
 * Version 0.9.0
 * Date 2026-05-28
 *
 * Build:  cc -O2 -o hcc2d_encoder single_file_c_hcc2d_encoder_v0.9.0.c -lz
 * Usage:  hcc2d_encoder --help
 *
 * Specification compliance:
 *   Intended to conform to the HCC2D Code Specification version 0.9.0.
 *   Reference specification PDF: https://hcc2d.com/hcc2d_specification_v0.9.0.pdf
 *
 * Description:
 *   This encoder is provided for educational purposes. It is a compact
 *   standalone reference implementation and is unrelated to the HCC2D
 *   Encoder and HCC2D Decoder desktop/mobile apps written in C/C++.
 *
 * Terms of service / no-warranty notice:
 *   This file is provided "as is", without warranties or conditions of any
 *   kind, express or implied, including but not limited to merchantability,
 *   fitness for a particular purpose, and noninfringement. Use of this file
 *   is entirely at your own risk. The author assumes no responsibility or
 *   liability for any damages, losses, claims, unreadable or non-decodable
 *   codes, failed scans, data loss, or other consequences arising from the
 *   use of this file.
 *
 * Apache 2.0 summary:
 *   You may use, copy, modify, and redistribute this file, including for
 *   commercial purposes. You must keep the applicable copyright/license
 *   notices and state significant modifications when redistributing modified
 *   versions. This summary is informational only; the LICENSE file and
 *   Apache License 2.0 text control the actual legal terms.
 *   Full license text: https://www.apache.org/licenses/LICENSE-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <zlib.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

#define MODE_BITS_BYTE     0x4
#define VERSION_INFO_POLY  0x1F25
#define TYPE_INFO_POLY     0x537
#define TYPE_INFO_MASK_PAT 0x5412
#define MAX_BITS           120000   /* generous upper bound for bit buffers   */
#define MAX_BLOCKS         256      /* max RS blocks across all hcc2d configs */
#define MAX_BLOCK_BYTES    160      /* max data+ec codewords per block        */
#define UNASSIGNED         (-1)

/* EC level bits for type-info encoding: L=1 M=0 Q=3 H=2 */
static const int EC_LEVEL_BITS_TAB[4] = {1, 0, 3, 2};

static int ec_level_index(char c) {
    switch (c) {
        case 'L': return 0; case 'M': return 1;
        case 'Q': return 2; case 'H': return 3;
    }
    return -1;
}

static int parse_int_option(const char *name, const char *value,
                            int min_value, int max_value, int *out) {
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' ||
        parsed < min_value || parsed > max_value) {
        fprintf(stderr, "Error: --%s must be an integer from %d to %d.\n",
                name, min_value, max_value);
        return -1;
    }

    *out = (int)parsed;
    return 0;
}

static int is_valid_utf8(const uint8_t *s, size_t len) {
    size_t i = 0;
    while (i < len) {
        uint8_t c = s[i];
        if (c <= 0x7F) {
            i++;
            continue;
        }
        if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= len) return 0;
            if ((s[i + 1] & 0xC0) != 0x80) return 0;
            i += 2;
            continue;
        }
        if (c == 0xE0) {
            if (i + 2 >= len) return 0;
            if (s[i + 1] < 0xA0 || s[i + 1] > 0xBF) return 0;
            if ((s[i + 2] & 0xC0) != 0x80) return 0;
            i += 3;
            continue;
        }
        if (c >= 0xE1 && c <= 0xEC) {
            if (i + 2 >= len) return 0;
            if ((s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80) return 0;
            i += 3;
            continue;
        }
        if (c == 0xED) {
            if (i + 2 >= len) return 0;
            if (s[i + 1] < 0x80 || s[i + 1] > 0x9F) return 0;
            if ((s[i + 2] & 0xC0) != 0x80) return 0;
            i += 3;
            continue;
        }
        if (c >= 0xEE && c <= 0xEF) {
            if (i + 2 >= len) return 0;
            if ((s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80) return 0;
            i += 3;
            continue;
        }
        if (c == 0xF0) {
            if (i + 3 >= len) return 0;
            if (s[i + 1] < 0x90 || s[i + 1] > 0xBF) return 0;
            if ((s[i + 2] & 0xC0) != 0x80 || (s[i + 3] & 0xC0) != 0x80) return 0;
            i += 4;
            continue;
        }
        if (c >= 0xF1 && c <= 0xF3) {
            if (i + 3 >= len) return 0;
            if ((s[i + 1] & 0xC0) != 0x80 ||
                (s[i + 2] & 0xC0) != 0x80 ||
                (s[i + 3] & 0xC0) != 0x80) return 0;
            i += 4;
            continue;
        }
        if (c == 0xF4) {
            if (i + 3 >= len) return 0;
            if (s[i + 1] < 0x80 || s[i + 1] > 0x8F) return 0;
            if ((s[i + 2] & 0xC0) != 0x80 || (s[i + 3] & 0xC0) != 0x80) return 0;
            i += 4;
            continue;
        }
        return 0;
    }
    return 1;
}

/* =========================================================================
 * Color palettes  (spec §5)
 * ========================================================================= */

typedef struct { uint8_t r, g, b; } RGB;

static const RGB PALETTE_4_MODEL1[4] = {
    {0,0,0}, {220,0,0}, {0,200,220}, {255,255,255}
};
static const RGB PALETTE_8_MODEL1[8] = {
    {0,0,0},{200,0,0},{0,130,0},{0,60,180},
    {0,215,235},{255,220,50},{255,130,230},{255,255,255}
};
static const RGB PALETTE_4_MODEL2[4] = {
    {0,0,0},{255,0,255},{0,255,255},{255,255,255}
};
static const RGB PALETTE_8_MODEL2[8] = {
    {0,0,0},{0,0,255},{255,0,0},{255,0,255},
    {0,255,0},{0,255,255},{255,255,0},{255,255,255}
};
static const RGB PALETTE_QR[2] = {
    {0,0,0}, {255,255,255}
};

/* =========================================================================
 * QR Model 2 version table (versions 1–40)
 * Each ECEntry: (ecpb, count1, data1, count2, data2) for L/M/Q/H
 * ========================================================================= */

typedef struct { int ecpb, count1, data1, count2, data2; } ECEntry;

typedef struct {
    int number;
    int align_count;
    int align[7];   /* alignment pattern centers; valid for [0..align_count) */
    ECEntry ec[4];  /* index 0=L 1=M 2=Q 3=H */
} QRVersion;

static const QRVersion QR_VERSIONS[40] = {
    { 1,0,{0},
      {{7,1,19,0,0},{10,1,16,0,0},{13,1,13,0,0},{17,1,9,0,0}}},
    { 2,2,{6,18},
      {{10,1,34,0,0},{16,1,28,0,0},{22,1,22,0,0},{28,1,16,0,0}}},
    { 3,2,{6,22},
      {{15,1,55,0,0},{26,1,44,0,0},{18,2,17,0,0},{22,2,13,0,0}}},
    { 4,2,{6,26},
      {{20,1,80,0,0},{18,2,32,0,0},{26,2,24,0,0},{16,4,9,0,0}}},
    { 5,2,{6,30},
      {{26,1,108,0,0},{24,2,43,0,0},{18,2,15,2,16},{22,2,11,2,12}}},
    { 6,2,{6,34},
      {{18,2,68,0,0},{16,4,27,0,0},{24,4,19,0,0},{28,4,15,0,0}}},
    { 7,3,{6,22,38},
      {{20,2,78,0,0},{18,4,31,0,0},{18,2,14,4,15},{26,4,13,1,14}}},
    { 8,3,{6,24,42},
      {{24,2,97,0,0},{22,2,38,2,39},{22,4,18,2,19},{26,4,14,2,15}}},
    { 9,3,{6,26,46},
      {{30,2,116,0,0},{22,3,36,2,37},{20,4,16,4,17},{24,4,12,4,13}}},
    {10,3,{6,28,50},
      {{18,2,68,2,69},{26,4,43,1,44},{24,6,19,2,20},{28,6,15,2,16}}},
    {11,3,{6,30,54},
      {{20,4,81,0,0},{30,1,50,4,51},{28,4,22,4,23},{24,3,12,8,13}}},
    {12,3,{6,32,58},
      {{24,2,92,2,93},{22,6,36,2,37},{26,4,20,6,21},{28,7,14,4,15}}},
    {13,3,{6,34,62},
      {{26,4,107,0,0},{22,8,37,1,38},{24,8,20,4,21},{22,12,11,4,12}}},
    {14,4,{6,26,46,66},
      {{30,3,115,1,116},{24,4,40,5,41},{20,11,16,5,17},{24,11,12,5,13}}},
    {15,4,{6,26,48,70},
      {{22,5,87,1,88},{24,5,41,5,42},{30,5,24,7,25},{24,11,12,7,13}}},
    {16,4,{6,26,50,74},
      {{24,5,98,1,99},{28,7,45,3,46},{24,15,19,2,20},{30,3,15,13,16}}},
    {17,4,{6,30,54,78},
      {{28,1,107,5,108},{28,10,46,1,47},{28,1,22,15,23},{28,2,14,17,15}}},
    {18,4,{6,30,56,82},
      {{30,5,120,1,121},{26,9,43,4,44},{28,17,22,1,23},{28,2,14,19,15}}},
    {19,4,{6,30,58,86},
      {{28,3,113,4,114},{26,3,44,11,45},{26,17,21,4,22},{26,9,13,16,14}}},
    {20,4,{6,34,62,90},
      {{28,3,107,5,108},{26,3,41,13,42},{30,15,24,5,25},{28,15,15,10,16}}},
    {21,5,{6,28,50,72,94},
      {{28,4,116,4,117},{26,17,42,0,0},{28,17,22,6,23},{30,19,16,6,17}}},
    {22,5,{6,26,50,74,98},
      {{28,2,111,7,112},{28,17,46,0,0},{30,7,24,16,25},{24,34,13,0,0}}},
    {23,5,{6,30,54,78,102},
      {{30,4,121,5,122},{28,4,47,14,48},{30,11,24,14,25},{30,16,15,14,16}}},
    {24,5,{6,28,54,80,106},
      {{30,6,117,4,118},{28,6,45,14,46},{30,11,24,16,25},{30,30,16,2,17}}},
    {25,5,{6,32,58,84,110},
      {{26,8,106,4,107},{28,8,47,13,48},{30,7,24,22,25},{30,22,15,13,16}}},
    {26,5,{6,30,58,86,114},
      {{28,10,114,2,115},{28,19,46,4,47},{28,28,22,6,23},{30,33,16,4,17}}},
    {27,5,{6,34,62,90,118},
      {{30,8,122,4,123},{28,22,45,3,46},{30,8,23,26,24},{30,12,15,28,16}}},
    {28,6,{6,26,50,74,98,122},
      {{30,3,117,10,118},{28,3,45,23,46},{30,4,24,31,25},{30,11,15,31,16}}},
    {29,6,{6,30,54,78,102,126},
      {{30,7,116,7,117},{28,21,45,7,46},{30,1,23,37,24},{30,19,15,26,16}}},
    {30,6,{6,26,52,78,104,130},
      {{30,5,115,10,116},{28,19,47,10,48},{30,15,24,25,25},{30,23,15,25,16}}},
    {31,6,{6,30,56,82,108,134},
      {{30,13,115,3,116},{28,2,46,29,47},{30,42,24,1,25},{30,23,15,28,16}}},
    {32,6,{6,34,60,86,112,138},
      {{30,17,115,0,0},{28,10,46,23,47},{30,10,24,35,25},{30,19,15,35,16}}},
    {33,6,{6,30,58,86,114,142},
      {{30,17,115,1,116},{28,14,46,21,47},{30,29,24,19,25},{30,11,15,46,16}}},
    {34,6,{6,34,62,90,118,146},
      {{30,13,115,6,116},{28,14,46,23,47},{30,44,24,7,25},{30,59,16,1,17}}},
    {35,7,{6,30,54,78,102,126,150},
      {{30,12,121,7,122},{28,12,47,26,48},{30,39,24,14,25},{30,22,15,41,16}}},
    {36,7,{6,24,50,76,102,128,154},
      {{30,6,121,14,122},{28,6,47,34,48},{30,46,24,10,25},{30,2,15,64,16}}},
    {37,7,{6,28,54,80,106,132,158},
      {{30,17,122,4,123},{28,29,46,14,47},{30,49,24,10,25},{30,24,15,46,16}}},
    {38,7,{6,32,58,84,110,136,162},
      {{30,4,122,18,123},{28,13,46,32,47},{30,48,24,14,25},{30,42,15,32,16}}},
    {39,7,{6,26,54,82,110,138,166},
      {{30,20,117,4,118},{28,40,47,7,48},{30,43,24,22,25},{30,10,15,67,16}}},
    {40,7,{6,30,58,86,114,142,170},
      {{30,19,118,6,119},{28,18,47,31,48},{30,34,24,34,25},{30,20,15,61,16}}},
};

/* =========================================================================
 * GF(256) for Reed-Solomon  (primitive polynomial 0x011D)
 * ========================================================================= */

static int gf_exp[512];
static int gf_log[256];

static void gf256_init(void) {
    int x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = x;
        gf_log[x] = i;
        x <<= 1;
        if (x & 0x100) x ^= 0x011D;
    }
    for (int i = 255; i < 512; i++)
        gf_exp[i] = gf_exp[i - 255];
}

static int gf_mul(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}

/* =========================================================================
 * BitBuffer — each element is a single bit stored as uint8_t 0 or 1
 * ========================================================================= */

typedef struct { uint8_t *bits; int count; } BitBuffer;

static void bb_alloc(BitBuffer *bb) {
    bb->bits = (uint8_t *)malloc(MAX_BITS);
    bb->count = 0;
}

static void bb_free(BitBuffer *bb) {
    free(bb->bits);
    bb->bits = NULL;
    bb->count = 0;
}

static void bb_append_bit(BitBuffer *bb, int bit) {
    bb->bits[bb->count++] = (uint8_t)(bit & 1);
}

static void bb_append_bits(BitBuffer *bb, int value, int count) {
    for (int s = count - 1; s >= 0; s--)
        bb_append_bit(bb, (value >> s) & 1);
}

/* Pack bb->bits[bit_offset..] into num_bytes bytes, zero-padding the last. */
static void bb_to_bytes(const BitBuffer *bb, int bit_offset, int num_bytes, uint8_t *out) {
    for (int bi = 0; bi < num_bytes; bi++) {
        int val = 0;
        for (int k = 0; k < 8; k++) {
            int src = bit_offset + bi * 8 + k;
            val = (val << 1) | (src < bb->count ? bb->bits[src] : 0);
        }
        out[bi] = (uint8_t)val;
    }
}

/* =========================================================================
 * Reed-Solomon polynomial arithmetic and EC generation
 * ========================================================================= */

static void poly_mul(const int *a, int la, const int *b, int lb, int *out) {
    int n = la + lb - 1;
    for (int i = 0; i < n; i++) out[i] = 0;
    for (int i = 0; i < la; i++)
        for (int j = 0; j < lb; j++)
            out[i + j] ^= gf_mul(a[i], b[j]);
}

static void rs_gen_poly(int degree, int *out, int *out_len) {
    out[0] = 1;
    *out_len = 1;
    int tmp[64];
    for (int i = 0; i < degree; i++) {
        int factor[2] = {1, gf_exp[i]};
        poly_mul(out, *out_len, factor, 2, tmp);
        (*out_len)++;
        memcpy(out, tmp, (size_t)(*out_len) * sizeof(int));
    }
}

static void rs_remainder(const uint8_t *data, int data_len, int ec_words, uint8_t *rem) {
    int gen[64], gen_len;
    rs_gen_poly(ec_words, gen, &gen_len);
    memset(rem, 0, (size_t)ec_words);
    for (int i = 0; i < data_len; i++) {
        int factor = data[i] ^ rem[0];
        memmove(rem, rem + 1, (size_t)(ec_words - 1));
        rem[ec_words - 1] = 0;
        if (factor)
            for (int j = 0; j < ec_words; j++)
                rem[j] ^= (uint8_t)gf_mul(gen[j + 1], factor);
    }
}

/* =========================================================================
 * Matrix helpers
 * ========================================================================= */

#define CELL(mat, dim, y, x) ((mat)[(y) * (dim) + (x)])

static int *alloc_matrix(int dim) {
    int *m = (int *)malloc((size_t)dim * dim * sizeof(int));
    for (int i = 0; i < dim * dim; i++) m[i] = UNASSIGNED;
    return m;
}

static void set_if_inside(int *mat, int dim, int x, int y, int val) {
    if (x >= 0 && x < dim && y >= 0 && y < dim)
        CELL(mat, dim, y, x) = val;
}

/* 7×7 finder pattern plus its 1-module white separator ring */
static void embed_finder(int *mat, int dim, int x0, int y0) {
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 7; x++) {
            int dx = abs(x - 3), dy = abs(y - 3);
            CELL(mat, dim, y0 + y, x0 + x) = ((dx > dy ? dx : dy) != 2) ? 1 : 0;
        }
    for (int i = -1; i <= 7; i++) {
        set_if_inside(mat, dim, x0 + i, y0 - 1, 0);
        set_if_inside(mat, dim, x0 + i, y0 + 7, 0);
        set_if_inside(mat, dim, x0 - 1, y0 + i, 0);
        set_if_inside(mat, dim, x0 + 7, y0 + i, 0);
    }
}

static void embed_finders(int *mat, int dim) {
    embed_finder(mat, dim, 0,       0);
    embed_finder(mat, dim, dim - 7, 0);
    embed_finder(mat, dim, 0,       dim - 7);
}

static void embed_dark_module(int *mat, int dim) {
    CELL(mat, dim, dim - 8, 8) = 1;
}

/* 5×5 alignment pattern */
static void embed_alignment(int *mat, int dim, int x0, int y0) {
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++) {
            int dx = abs(x - 2), dy = abs(y - 2);
            CELL(mat, dim, y0 + y, x0 + x) = ((dx > dy ? dx : dy) != 1) ? 1 : 0;
        }
}

static void embed_alignments(int *mat, int dim, const int *centers, int count) {
    if (count < 1) return;
    int last = centers[count - 1];
    for (int yi = 0; yi < count; yi++)
        for (int xi = 0; xi < count; xi++) {
            int cy = centers[yi], cx = centers[xi];
            if ((cx == 6 && cy == 6) || (cx == 6 && cy == last) || (cx == last && cy == 6))
                continue;
            embed_alignment(mat, dim, cx - 2, cy - 2);
        }
}

static void embed_timing(int *mat, int dim) {
    for (int i = 8; i < dim - 8; i++) {
        int bit = ((i + 1) % 2) ? 1 : 0;
        CELL(mat, dim, 6, i) = bit;
        CELL(mat, dim, i, 6) = bit;
    }
}

/* =========================================================================
 * BCH error-correction codes for type-info and version-info fields
 * ========================================================================= */

static int msb_pos(int v) {
    int p = 0;
    while (v) { p++; v >>= 1; }
    return p;
}

static int bch_code(int value, int poly) {
    int msb = msb_pos(poly);
    value <<= (msb - 1);
    while (msb_pos(value) >= msb)
        value ^= poly << (msb_pos(value) - msb);
    return value;
}

/* Type-info embedding: 15-bit field encoding EC level + mask.
 * Coordinates list and placement logic replicate the QR spec exactly. */
static void embed_type_info(int *mat, int dim, char ec_level, int mask_pattern) {
    static const int cx[15] = {8,8,8,8,8,8,8,8,7,5,4,3,2,1,0};
    static const int cy[15] = {0,1,2,3,4,5,7,8,8,8,8,8,8,8,8};
    int ti = (EC_LEVEL_BITS_TAB[ec_level_index(ec_level)] << 3) | mask_pattern;
    int val = ((ti << 10) | bch_code(ti, TYPE_INFO_POLY)) ^ TYPE_INFO_MASK_PAT;
    /* enumerate(reversed(15-bit MSB-first list)) → iterate i=0..14, bit=(val>>i)&1 */
    for (int i = 0; i < 15; i++) {
        int bit = (val >> i) & 1;
        CELL(mat, dim, cy[i], cx[i]) = bit;
        if (i < 8)
            CELL(mat, dim, 8, dim - i - 1) = bit;
        else
            CELL(mat, dim, dim - 7 + (i - 8), 8) = bit;
    }
}

/* Version-info embedding for versions 7+: 18-bit field. */
static void embed_version_info(int *mat, int dim, int ver) {
    if (ver < 7) return;
    int val = (ver << 12) | bch_code(ver, VERSION_INFO_POLY);
    /* bit_index starts at 17 (LSB of 18-bit array) and decrements → reads val bit-by-bit from LSB */
    int t = 0;
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 3; j++) {
            int bit = (val >> t++) & 1;
            CELL(mat, dim, dim - 11 + j, i) = bit;
            CELL(mat, dim, i, dim - 11 + j) = bit;
        }
}

/* =========================================================================
 * QR data mask formulas
 * ========================================================================= */

static int get_mask_bit(int mask, int x, int y) {
    switch (mask) {
        case 0: return (y + x) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (y + x) % 3 == 0;
        case 4: return ((y / 2) + (x / 3)) % 2 == 0;
        case 5: return (y * x) % 6 == 0;
        case 6: return ((y * x) % 6) < 3;
        case 7: return (y + x + ((y * x) % 3)) % 2 == 0;
    }
    return 0;
}

/* QR zig-zag data placement walk */
static void embed_data_bits(int *mat, int dim, const uint8_t *data_bits, int num_bits,
                             int mask_pattern) {
    int bit_idx = 0, direction = -1, x = dim - 1, y = dim - 1;
    while (x > 0) {
        if (x == 6) x--;
        while (y >= 0 && y < dim) {
            for (int xx = x; xx >= x - 1; xx--) {
                if (CELL(mat, dim, y, xx) != UNASSIGNED) continue;
                int bit = (bit_idx < num_bits) ? data_bits[bit_idx] : 0;
                bit_idx++;
                if (get_mask_bit(mask_pattern, xx, y)) bit ^= 1;
                CELL(mat, dim, y, xx) = bit;
            }
            y += direction;
        }
        direction = -direction;
        y += direction;
        x -= 2;
    }
}

/* =========================================================================
 * QR mask penalty rules
 * ========================================================================= */

static int penalty_rule1(const int *mat, int dim) {
    int penalty = 0;
    for (int horiz = 0; horiz <= 1; horiz++) {
        for (int i = 0; i < dim; i++) {
            int run = 0, prev = -1;
            for (int j = 0; j < dim; j++) {
                int bit = horiz ? CELL(mat, dim, i, j) : CELL(mat, dim, j, i);
                if (bit == prev) {
                    run++;
                } else {
                    if (run >= 5) penalty += 3 + (run - 5);
                    run = 1;
                    prev = bit;
                }
            }
            if (run >= 5) penalty += 3 + (run - 5);
        }
    }
    return penalty;
}

static int penalty_rule2(const int *mat, int dim) {
    int penalty = 0;
    for (int y = 0; y < dim - 1; y++)
        for (int x = 0; x < dim - 1; x++) {
            int v = CELL(mat, dim, y, x);
            if (v == CELL(mat, dim, y,     x + 1) &&
                v == CELL(mat, dim, y + 1, x    ) &&
                v == CELL(mat, dim, y + 1, x + 1))
                penalty += 3;
        }
    return penalty;
}

/* Rule 3: penalise finder-like runs with white quiet zone.
 * Replicates the Python behaviour including the asymmetry between left (non-empty
 * check) and right (empty-matches-empty, i.e. edge of symbol counts as white). */
static int penalty_rule3(const int *mat, int dim) {
    static const int finder[7] = {1, 0, 1, 1, 1, 0, 1};
    int penalties = 0;

    /* horizontal */
    for (int y = 0; y < dim; y++) {
        for (int x = 0; x <= dim - 7; x++) {
            int match = 1;
            for (int k = 0; k < 7; k++)
                if (CELL(mat, dim, y, x + k) != finder[k]) { match = 0; break; }
            if (!match) continue;

            int ls = (x >= 4) ? x - 4 : 0;
            int ll = x - ls;
            int rs = x + 7;
            int rl = (rs + 4 <= dim) ? 4 : dim - rs;
            if (rl < 0) rl = 0;

            int lwhite = 1;
            for (int k = 0; k < ll; k++)
                if (CELL(mat, dim, y, ls + k)) { lwhite = 0; break; }
            int rwhite = 1;
            for (int k = 0; k < rl; k++)
                if (CELL(mat, dim, y, rs + k)) { rwhite = 0; break; }

            /* left check: only fires when there are cells and they're all white.
             * right check: fires when empty (edge) OR all white. */
            if ((ll > 0 && lwhite) || (rl == 0 || rwhite))
                penalties++;
        }
    }

    /* vertical */
    for (int x = 0; x < dim; x++) {
        for (int y = 0; y <= dim - 7; y++) {
            int match = 1;
            for (int k = 0; k < 7; k++)
                if (CELL(mat, dim, y + k, x) != finder[k]) { match = 0; break; }
            if (!match) continue;

            int ts = (y >= 4) ? y - 4 : 0;
            int tl = y - ts;
            int bs = y + 7;
            int bl = (bs + 4 <= dim) ? 4 : dim - bs;
            if (bl < 0) bl = 0;

            int twhite = 1;
            for (int k = 0; k < tl; k++)
                if (CELL(mat, dim, ts + k, x)) { twhite = 0; break; }
            int bwhite = 1;
            for (int k = 0; k < bl; k++)
                if (CELL(mat, dim, bs + k, x)) { bwhite = 0; break; }

            if ((tl > 0 && twhite) || (bl == 0 || bwhite))
                penalties++;
        }
    }

    return penalties * 40;
}

static int penalty_rule4(const int *mat, int dim) {
    int total = dim * dim, dark = 0;
    for (int i = 0; i < total; i++) dark += mat[i];
    return abs(dark * 2 - total) * 10 / total * 10;
}

static int mask_penalty(const int *mat, int dim) {
    return penalty_rule1(mat, dim) + penalty_rule2(mat, dim)
         + penalty_rule3(mat, dim) + penalty_rule4(mat, dim);
}

/* =========================================================================
 * Payload framing
 * HCC2D uses BYTE mode (0100) || 16-bit count || payload bytes.
 * QR BYTE mode uses an 8-bit count for versions 1-9 and 16-bit for 10-40.
 * ========================================================================= */

static void build_payload_bits(BitBuffer *bb, const uint8_t *payload, int len,
                                int count_bits) {
    bb_append_bits(bb, MODE_BITS_BYTE, 4);
    bb_append_bits(bb, len, count_bits);
    for (int i = 0; i < len; i++)
        bb_append_bits(bb, payload[i], 8);
}

static int qr_byte_count_bits(int version) {
    return version <= 9 ? 8 : 16;
}

static void terminate_bits(BitBuffer *bb, int num_data_bytes) {
    int capacity = num_data_bytes * 8;
    int pad = capacity - bb->count;
    if (pad < 0) pad = 0;
    if (pad > 4) pad = 4;
    for (int i = 0; i < pad; i++) bb_append_bit(bb, 0);
    while (bb->count % 8) bb_append_bit(bb, 0);
    static const uint8_t PAD[2] = {0xEC, 0x11};
    int pi = 0;
    while (bb->count < capacity) { bb_append_bits(bb, PAD[pi % 2], 8); pi++; }
}

/* =========================================================================
 * RS block layout and interleaving
 * ========================================================================= */

static void get_block_layout(int num_total, int num_data, int num_blocks, int block_id,
                              int *data_len_out, int *ec_len_out) {
    int g2 = num_total % num_blocks;
    int g1 = num_blocks - g2;
    int t1 = num_total / num_blocks;
    int d1 = num_data  / num_blocks;
    if (block_id < g1) {
        *data_len_out = d1;       *ec_len_out = t1 - d1;
    } else {
        *data_len_out = d1 + 1;   *ec_len_out = t1 - d1;  /* ec same: (t1+1)-(d1+1)=t1-d1 */
    }
}

/* Compute parity per block, then interleave: all data bytes across blocks,
 * then all EC bytes across blocks.  Returns a newly-allocated BitBuffer. */
static BitBuffer interleave_ec(const BitBuffer *data_bb, int num_total,
                                int num_data, int num_blocks) {
    static uint8_t bdata[MAX_BLOCKS][MAX_BLOCK_BYTES];
    static uint8_t bec  [MAX_BLOCKS][MAX_BLOCK_BYTES];
    int bdata_len[MAX_BLOCKS], bec_len[MAX_BLOCKS];

    uint8_t *src = (uint8_t *)malloc((size_t)num_data);
    bb_to_bytes(data_bb, 0, num_data, src);

    int offset = 0, max_d = 0, max_e = 0;
    for (int b = 0; b < num_blocks; b++) {
        int dl, el;
        get_block_layout(num_total, num_data, num_blocks, b, &dl, &el);
        memcpy(bdata[b], src + offset, (size_t)dl);
        rs_remainder(bdata[b], dl, el, bec[b]);
        bdata_len[b] = dl;
        bec_len[b]   = el;
        offset += dl;
        if (dl > max_d) max_d = dl;
        if (el > max_e) max_e = el;
    }
    free(src);

    BitBuffer out;
    bb_alloc(&out);
    for (int i = 0; i < max_d; i++)
        for (int b = 0; b < num_blocks; b++)
            if (i < bdata_len[b]) bb_append_bits(&out, bdata[b][i], 8);
    for (int i = 0; i < max_e; i++)
        for (int b = 0; b < num_blocks; b++)
            if (i < bec_len[b]) bb_append_bits(&out, bec[b][i], 8);
    return out;
}

/* =========================================================================
 * Plane extraction and inversion
 * ========================================================================= */

/* Extract every plane_count-th bit starting at plane_offset into out[].
 * Returns the number of bits written. */
static int extract_plane_bits(const uint8_t *bits, int total, int plane_offset,
                               int plane_count, uint8_t *out) {
    int n = 0;
    for (int i = plane_offset; i < total; i += plane_count)
        out[n++] = bits[i];
    return n;
}

/* =========================================================================
 * HCC2D Color Palette Pattern border  (spec §)
 *
 * row/col range from -1 to dim (inclusive): the 1-module border around the
 * inner dim×dim QR grid.  The cycling spans around finder-free edges carry
 * the palette-index pattern; all other border cells default to white (period-1).
 * ========================================================================= */

static int border_color(int row, int col, int dim, int period) {
    if (row == -1 && col >= 8 && col < dim - 8)
        return (col - 8) % period;
    if (row == dim && col >= 8 && col < dim)
        return (col - 8) % period;
    if (col == -1) {
        int s = dim - 9;
        if (row >= 8 && row <= s)
            return (s - row) % period;
    }
    if (col == dim && row >= 8 && row < dim)
        return (row - 8) % period;
    return period - 1;
}

/* =========================================================================
 * Matrix construction
 * ========================================================================= */

static int *build_matrix(const uint8_t *data_bits, int num_bits, char ec_level,
                          const QRVersion *qrv, int mask_pattern) {
    int dim = 17 + 4 * qrv->number;
    int *mat = alloc_matrix(dim);
    embed_finders(mat, dim);
    embed_dark_module(mat, dim);
    embed_alignments(mat, dim, qrv->align, qrv->align_count);
    embed_timing(mat, dim);
    embed_type_info(mat, dim, ec_level, mask_pattern);
    embed_version_info(mat, dim, qrv->number);
    embed_data_bits(mat, dim, data_bits, num_bits, mask_pattern);
    for (int i = 0; i < dim * dim; i++)
        if (mat[i] == UNASSIGNED) mat[i] = 0;
    return mat;
}

/* Return a boolean (0/1) map of all reserved function-module cells.
 * Used by render_modules to decide whether a cell is rendered in black/white
 * (from plane 0) or as a multi-bit colour index (from all planes). */
static int *build_function_pattern(const QRVersion *qrv) {
    int dim = 17 + 4 * qrv->number;
    int *mat = alloc_matrix(dim);
    embed_finders(mat, dim);
    embed_dark_module(mat, dim);
    embed_alignments(mat, dim, qrv->align, qrv->align_count);
    embed_timing(mat, dim);
    embed_type_info(mat, dim, 'L', 0);
    if (qrv->number >= 7)
        embed_version_info(mat, dim, qrv->number);
    int *fp = (int *)malloc((size_t)dim * dim * sizeof(int));
    for (int i = 0; i < dim * dim; i++)
        fp[i] = (mat[i] != UNASSIGNED) ? 1 : 0;
    free(mat);
    return fp;
}

/* HCC2D chooses one shared mask by scoring inverted plane-0 with QR penalty rules. */
static int choose_mask(const uint8_t *proxy_bits, int proxy_len, char ec_level,
                        const QRVersion *qrv) {
    int best = 0, best_p = -1;
    int dim = 17 + 4 * qrv->number;
    for (int m = 0; m < 8; m++) {
        int *mat = build_matrix(proxy_bits, proxy_len, ec_level, qrv, m);
        int p = mask_penalty(mat, dim);
        free(mat);
        if (best_p < 0 || p < best_p) { best_p = p; best = m; }
    }
    return best;
}

/* =========================================================================
 * Module rendering: combine 2 or 3 plane matrices into a colour index grid
 * and attach the HCC2D Color Palette Pattern border.
 * Output is (dim+2) × (dim+2).
 * ========================================================================= */

static int *render_modules(int **planes, int plane_count, const QRVersion *qrv, int period) {
    int dim   = 17 + 4 * qrv->number;
    int full  = dim + 2;
    int white = period - 1;
    int *out  = (int *)malloc((size_t)full * full * sizeof(int));
    for (int i = 0; i < full * full; i++) out[i] = white;

    int *fp = build_function_pattern(qrv);

    for (int ry = -1; ry <= dim; ry++) {
        for (int rx = -1; rx <= dim; rx++) {
            int color;
            if (rx >= 0 && rx < dim && ry >= 0 && ry < dim) {
                int idx = ry * dim + rx;
                if (fp[idx]) {
                    /* Function modules: black when plane-0 dark, white otherwise */
                    color = planes[0][idx] ? 0 : white;
                } else {
                    /* Data modules: combine plane bits into palette index */
                    color = 0;
                    for (int p = 0; p < plane_count; p++)
                        color = (color << 1) | planes[p][idx];
                }
            } else {
                color = border_color(ry, rx, dim, period);
            }
            out[(ry + 1) * full + (rx + 1)] = color;
        }
    }
    free(fp);
    return out;
}

/* =========================================================================
 * Rasterization: expand modules to pixels, add quiet zone
 * ========================================================================= */

static uint8_t *rasterize(const int *modules, int full_dim, int scale, int quiet_zone,
                            int background, int *out_w, int *out_h) {
    int img_mods = full_dim + quiet_zone * 2;
    int img_size = img_mods * scale;
    uint8_t *px  = (uint8_t *)malloc((size_t)img_size * img_size);
    memset(px, (uint8_t)background, (size_t)img_size * img_size);
    for (int my = 0; my < full_dim; my++) {
        for (int mx = 0; mx < full_dim; mx++) {
            int color = modules[my * full_dim + mx];
            int bx = (mx + quiet_zone) * scale;
            int by = (my + quiet_zone) * scale;
            for (int dy = 0; dy < scale; dy++)
                memset(px + (size_t)(by + dy) * img_size + bx, (uint8_t)color, (size_t)scale);
        }
    }
    *out_w = img_size;
    *out_h = img_size;
    return px;
}

/* =========================================================================
 * Minimal indexed PNG output: IHDR + PLTE + IDAT + IEND
 * ========================================================================= */

static void write_u32be(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF; p[1] = (v >> 16) & 0xFF;
    p[2] = (v >>  8) & 0xFF; p[3] =  v         & 0xFF;
}

static uint8_t *png_make_chunk(const char *type4, const uint8_t *data, uint32_t dlen,
                                size_t *chunk_len) {
    *chunk_len = 4 + 4 + dlen + 4;
    uint8_t *buf = (uint8_t *)malloc(*chunk_len);
    write_u32be(buf, dlen);
    memcpy(buf + 4, type4, 4);
    if (dlen > 0) memcpy(buf + 8, data, dlen);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type4, 4);
    if (dlen > 0) crc = crc32(crc, data, dlen);
    write_u32be(buf + 8 + dlen, (uint32_t)crc);
    return buf;
}

static uint8_t *build_png(int width, int height, const uint8_t *indices,
                            const RGB *palette, int pal_size, size_t *out_len) {
    /* Build filter-byte-0 scanlines */
    size_t raw_len = (size_t)height * (1 + width);
    uint8_t *raw = (uint8_t *)malloc(raw_len);
    for (int y = 0; y < height; y++) {
        raw[(size_t)y * (width + 1)] = 0;
        memcpy(raw + (size_t)y * (width + 1) + 1,
               indices + (size_t)y * width, (size_t)width);
    }

    uLong cbound = compressBound((uLong)raw_len);
    uint8_t *comp = (uint8_t *)malloc(cbound);
    uLong comp_len = cbound;
    compress2(comp, &comp_len, raw, (uLong)raw_len, 9);
    free(raw);

    /* IHDR */
    uint8_t ihdr[13];
    write_u32be(ihdr,     (uint32_t)width);
    write_u32be(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8; ihdr[9] = 3; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;

    /* PLTE */
    uint8_t *plte = (uint8_t *)malloc((size_t)pal_size * 3);
    for (int i = 0; i < pal_size; i++) {
        plte[i * 3]     = palette[i].r;
        plte[i * 3 + 1] = palette[i].g;
        plte[i * 3 + 2] = palette[i].b;
    }

    static const uint8_t sig[8] = {0x89,'P','N','G','\r','\n',0x1a,'\n'};
    size_t l1, l2, l3, l4;
    uint8_t *c1 = png_make_chunk("IHDR", ihdr,  13,                    &l1);
    uint8_t *c2 = png_make_chunk("PLTE", plte,  (uint32_t)(pal_size*3),&l2);
    uint8_t *c3 = png_make_chunk("IDAT", comp,  (uint32_t)comp_len,    &l3);
    uint8_t *c4 = png_make_chunk("IEND", NULL,  0,                     &l4);
    free(plte); free(comp);

    *out_len = 8 + l1 + l2 + l3 + l4;
    uint8_t *result = (uint8_t *)malloc(*out_len);
    uint8_t *p = result;
    memcpy(p, sig, 8); p += 8;
    memcpy(p, c1, l1); p += l1;
    memcpy(p, c2, l2); p += l2;
    memcpy(p, c3, l3); p += l3;
    memcpy(p, c4, l4); p += l4;
    free(c1); free(c2); free(c3); free(c4);
    return result;
}

/* =========================================================================
 * HCC2DF file-wrapper payload
 * Layout: magic(6) + wrapper_version(1) + compression_flag(1) +
 *         filename_len(1) + filename + content
 * Content is zlib-compressed iff file >= 128 bytes and compressed < 90%.
 * ========================================================================= */

static uint8_t *read_file_bytes(const char *input_path, size_t *out_len, char *err) {
    FILE *f = fopen(input_path, "rb");
    if (!f) { snprintf(err, 256, "cannot open: %s", input_path); return NULL; }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    if (fsz < 0) {
        fclose(f);
        snprintf(err, 256, "cannot determine file size: %s", input_path);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    uint8_t *content = (uint8_t *)malloc(fsz ? (size_t)fsz : 1);
    if (fread(content, 1, (size_t)fsz, f) != (size_t)fsz) {
        free(content); fclose(f);
        snprintf(err, 256, "read error: %s", input_path);
        return NULL;
    }
    fclose(f);
    *out_len = (size_t)fsz;
    return content;
}

static uint8_t *build_hcc2df_payload(const char *filename, const uint8_t *content,
                                      size_t content_len, size_t *out_len, char *err) {
    const char *fn = strrchr(filename, '/');
    if (!fn) fn = strrchr(filename, '\\');
    fn = fn ? fn + 1 : filename;
    size_t fnl = strlen(fn);
    if (fnl == 0 || fnl > 127) {
        snprintf(err, 256, "filename must be 1-127 UTF-8 bytes");
        return NULL;
    }
    if (!is_valid_utf8((const uint8_t *)fn, fnl)) {
        snprintf(err, 256, "filename must be valid UTF-8");
        return NULL;
    }

    uint8_t  cflag    = 0;
    const uint8_t *stored = content;
    size_t   stored_l = content_len;
    uint8_t *compressed = NULL;

    if (content_len >= 128) {
        uLong cb = compressBound((uLong)content_len);
        compressed = (uint8_t *)malloc(cb);
        uLong cl   = cb;
        int zrc = compress2(compressed, &cl, content, (uLong)content_len, Z_DEFAULT_COMPRESSION);
        if (zrc == Z_OK && (double)cl < (double)content_len * 0.9) {
            stored = compressed; stored_l = (size_t)cl; cflag = 1;
        }
    }

    size_t pl = 6 + 1 + 1 + 1 + fnl + stored_l;
    uint8_t *payload = (uint8_t *)malloc(pl);
    uint8_t *p = payload;
    memcpy(p, "HCC2DF", 6);   p += 6;
    *p++ = 0x01;               /* wrapper version */
    *p++ = cflag;
    *p++ = (uint8_t)fnl;
    memcpy(p, fn, fnl);        p += fnl;
    memcpy(p, stored, stored_l);

    if (compressed) free(compressed);

    *out_len = pl;
    return payload;
}

/* =========================================================================
 * EC block capacity helpers
 * ========================================================================= */

static int ec_total_sym(const ECEntry *e) {
    return e->count1 * (e->data1 + e->ecpb) + e->count2 * (e->data2 + e->ecpb);
}

static int ec_total_cw(const ECEntry *e) {
    return e->ecpb * (e->count1 + e->count2);
}

static int ec_num_blocks(const ECEntry *e) {
    return e->count1 + e->count2;
}

/* Compute the HCC2D ECEntry for a given QR base version by multiplying block counts. */
static ECEntry hcc2d_ec(const ECEntry *base, int multiplier) {
    ECEntry h;
    h.ecpb   = base->ecpb;
    h.count1 = base->count1 * multiplier;
    h.data1  = base->data1;
    h.count2 = base->count2 * multiplier;
    h.data2  = base->data2;
    return h;
}

/* =========================================================================
 * End-to-end HCC2D encode
 * ========================================================================= */

typedef struct {
    const char *mode;
    char  ec_level;
    int   version_number;
    int   inner_dim;
    int   full_dim;
    int   mask_pattern;
    int   width, height;
    uint8_t *pixels;  /* palette-index raster, width×height bytes */
} EncodedSymbol;

static int encode_hcc2d(const uint8_t *payload, int payload_len,
                         const char *mode, char ec_level, int version,
                         int scale, int quiet_zone,
                         EncodedSymbol *sym, char *err) {
    int plane_count   = (strcmp(mode, "hcc2d4") == 0) ? 2 : 3;
    int border_period = plane_count == 2 ? 4 : 8;
    int multiplier    = plane_count;
    int ec_idx        = ec_level_index(ec_level);
    int white_idx     = border_period - 1;

    /* Choose / validate version ------------------------------------------ */
    /* Required data capacity (bytes):
     *   4-bit mode + 16-bit count + 8*n payload bits → ceil((20+8n)/8) = n+3 */
    int needed = payload_len + 3;

    const QRVersion *qrv = NULL;
    ECEntry hec = {0};

    if (version > 0) {
        if (version < 1 || version > 40) {
            snprintf(err, 256, "version must be 1-40"); return -1;
        }
        qrv = &QR_VERSIONS[version - 1];
        hec = hcc2d_ec(&qrv->ec[ec_idx], multiplier);
        int data_bytes = ec_total_sym(&hec) - ec_total_cw(&hec);
        if (data_bytes < needed) {
            snprintf(err, 256,
                     "%s payload is %d bytes but version %d at EC %c fits at most %d bytes",
                     mode, payload_len, version, ec_level, data_bytes - 3);
            return -1;
        }
    } else {
        for (int v = 1; v <= 40; v++) {
            const QRVersion *qv = &QR_VERSIONS[v - 1];
            ECEntry he = hcc2d_ec(&qv->ec[ec_idx], multiplier);
            int data_bytes = ec_total_sym(&he) - ec_total_cw(&he);
            if (data_bytes >= needed) { qrv = qv; hec = he; break; }
        }
        if (!qrv) {
            snprintf(err, 256,
                     "%s payload is %d bytes and does not fit in any supported version at EC %c",
                     mode, payload_len, ec_level);
            return -1;
        }
    }

    int num_blocks = ec_num_blocks(&hec);
    int total_sym  = ec_total_sym(&hec);
    int data_bytes = total_sym - ec_total_cw(&hec);

    /* 1. Frame and pad the payload */
    BitBuffer bits;
    bb_alloc(&bits);
    build_payload_bits(&bits, payload, payload_len, 16);
    terminate_bits(&bits, data_bytes);

    /* 2. Generate EC and interleave */
    BitBuffer final = interleave_ec(&bits, total_sym, data_bytes, num_blocks);
    bb_free(&bits);

    int total_bits = final.count;

    /* 3. Extract per-plane bit streams */
    uint8_t *plane_bits[3];
    int      plane_lens[3];
    for (int p = 0; p < plane_count; p++) {
        plane_bits[p] = (uint8_t *)malloc((size_t)(total_bits / plane_count + 2));
        plane_lens[p] = extract_plane_bits(final.bits, total_bits, p, plane_count,
                                           plane_bits[p]);
    }

    /* 4. Choose mask using inverted plane-0 as proxy */
    int proxy_len = plane_lens[0];
    uint8_t *proxy = (uint8_t *)malloc((size_t)proxy_len);
    for (int i = 0; i < proxy_len; i++) proxy[i] = plane_bits[0][i] ^ 1;
    int mask_pattern = choose_mask(proxy, proxy_len, ec_level, qrv);
    free(proxy);

    /* 5. Build one QR-compatible matrix per plane with the shared mask */
    int *plane_mats[3];
    for (int p = 0; p < plane_count; p++)
        plane_mats[p] = build_matrix(plane_bits[p], plane_lens[p],
                                     ec_level, qrv, mask_pattern);

    for (int p = 0; p < plane_count; p++) free(plane_bits[p]);
    bb_free(&final);

    /* 6. Combine planes into colour modules and attach the palette border */
    int *modules = render_modules(plane_mats, plane_count, qrv, border_period);
    for (int p = 0; p < plane_count; p++) free(plane_mats[p]);

    /* 7. Rasterize */
    int dim      = 17 + 4 * qrv->number;
    int full_dim = dim + 2;
    int w, h;
    uint8_t *pixels = rasterize(modules, full_dim, scale, quiet_zone, white_idx, &w, &h);
    free(modules);

    sym->mode           = mode;
    sym->ec_level       = ec_level;
    sym->version_number = qrv->number;
    sym->inner_dim      = dim;
    sym->full_dim       = full_dim;
    sym->mask_pattern   = mask_pattern;
    sym->width          = w;
    sym->height         = h;
    sym->pixels         = pixels;
    return 0;
}

static int encode_qr(const uint8_t *payload, int payload_len,
                     char ec_level, int version, int scale, int quiet_zone,
                     EncodedSymbol *sym, char *err) {
    int ec_idx = ec_level_index(ec_level);

    const QRVersion *qrv = NULL;
    ECEntry qec = {0};

    if (version > 0) {
        if (version < 1 || version > 40) {
            snprintf(err, 256, "version must be 1-40"); return -1;
        }
        qrv = &QR_VERSIONS[version - 1];
        qec = qrv->ec[ec_idx];
        int data_bytes = ec_total_sym(&qec) - ec_total_cw(&qec);
        int count_bits = qr_byte_count_bits(version);
        int needed_bits = 4 + count_bits + payload_len * 8;
        if (needed_bits > data_bytes * 8) {
            int max_payload = (data_bytes * 8 - 4 - count_bits) / 8;
            if (max_payload < 0) max_payload = 0;
            snprintf(err, 256,
                     "qr payload is %d bytes but version %d at EC %c fits at most %d bytes",
                     payload_len, version, ec_level, max_payload);
            return -1;
        }
    } else {
        for (int v = 1; v <= 40; v++) {
            const QRVersion *qv = &QR_VERSIONS[v - 1];
            ECEntry qe = qv->ec[ec_idx];
            int data_bytes = ec_total_sym(&qe) - ec_total_cw(&qe);
            int needed_bits = 4 + qr_byte_count_bits(v) + payload_len * 8;
            if (needed_bits <= data_bytes * 8) { qrv = qv; qec = qe; break; }
        }
        if (!qrv) {
            snprintf(err, 256,
                     "qr payload is %d bytes and does not fit in any supported version at EC %c",
                     payload_len, ec_level);
            return -1;
        }
    }

    int num_blocks = ec_num_blocks(&qec);
    int total_sym  = ec_total_sym(&qec);
    int data_bytes = total_sym - ec_total_cw(&qec);

    BitBuffer bits;
    bb_alloc(&bits);
    build_payload_bits(&bits, payload, payload_len, qr_byte_count_bits(qrv->number));
    terminate_bits(&bits, data_bytes);

    BitBuffer final = interleave_ec(&bits, total_sym, data_bytes, num_blocks);
    bb_free(&bits);

    int mask_pattern = choose_mask(final.bits, final.count, ec_level, qrv);
    int *mat = build_matrix(final.bits, final.count, ec_level, qrv, mask_pattern);
    bb_free(&final);

    int dim = 17 + 4 * qrv->number;
    int *modules = (int *)malloc((size_t)dim * dim * sizeof(int));
    for (int i = 0; i < dim * dim; i++)
        modules[i] = mat[i] ? 0 : 1;
    free(mat);

    int w, h;
    uint8_t *pixels = rasterize(modules, dim, scale, quiet_zone, 1, &w, &h);
    free(modules);

    sym->mode           = "qr";
    sym->ec_level       = ec_level;
    sym->version_number = qrv->number;
    sym->inner_dim      = dim;
    sym->full_dim       = dim;
    sym->mask_pattern   = mask_pattern;
    sym->width          = w;
    sym->height         = h;
    sym->pixels         = pixels;
    return 0;
}

/* =========================================================================
 * CLI
 * ========================================================================= */

static void print_usage(const char *prog) {
    printf(
"HCC2D Encoder - single-file C reference implementation\n"
"Copyright 2010-2026 Marco Querini  |  SPDX-License-Identifier: Apache-2.0  |  Version 0.9.0\n\n"
"Specification compliance:\n"
"  Intended to conform to the HCC2D Code Specification version 0.9.0.\n"
"  Reference specification PDF: https://hcc2d.com/hcc2d_specification_v0.9.0.pdf\n\n"
"Usage:\n"
"  %s [options] output.png\n\n"
"Source (one required):\n"
"  --text STRING            UTF-8 text bytes to encode\n"
"  --input-file PATH        file bytes to encode\n\n"
"Payload format:\n"
"  --file-format {hcc2df,raw}\n"
"                           hcc2df wraps --text or --input-file in an HCC2DF\n"
"                           container with name/compression metadata (default)\n"
"                           raw encodes the text/file bytes directly\n\n"
"Symbol mode:\n"
"  --mode {qr,hcc2d4,hcc2d8}\n"
"                           qr = standard black/white QR Code\n"
"                           hcc2d4 = HCC2D 4-colour, 2 bits/module (default)\n"
"                           hcc2d8 = HCC2D 8-colour, 3 bits/module\n"
"                           Note: --mode qr always creates a standard QR symbol;\n"
"                           --file-format only changes the bytes stored inside it.\n\n"
"Options:\n"
"  --ec-level {L,M,Q,H}    error-correction level: L~7%% M~15%% Q~25%% H~30%%\n"
"                           (default: Q)\n"
"  --version N              symbol version 1-40; 0 = smallest fitting version\n"
"                           (default: 0)\n"
"  --scale PX               pixels per module in the output PNG (min: 6, default: 12)\n"
"  --quiet-zone N           modules of white margin outside the symbol (default: 4)\n"
"  --palette {model1,model2}\n"
"                           HCC2D colour palette: model1 = screen (default),\n"
"                           model2 = print/CMYK. Ignored for --mode qr.\n"
"  --help                   show this help and exit\n\n"
"Examples:\n"
"  %s --text \"hello\" --mode qr --file-format raw qr.png\n"
"  %s --text \"hello\" --mode hcc2d4 hcc2d4.png\n"
"  %s --input-file photo.jpg --mode hcc2d8 --file-format hcc2df hcc2d8.png\n",
        prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    gf256_init();

    /* Defaults */
    const char *opt_text       = NULL;
    const char *opt_input_file = NULL;
    const char *opt_mode       = "hcc2d4";
    const char *opt_ec_level   = "Q";
    const char *opt_palette    = "model1";
    const char *opt_file_format = "hcc2df";
    int opt_version    = 0;
    int opt_scale      = 12;
    int opt_quiet_zone = 4;

    static struct option long_opts[] = {
        {"text",       required_argument, 0, 't'},
        {"input-file", required_argument, 0, 'f'},
        {"mode",       required_argument, 0, 'm'},
        {"ec-level",   required_argument, 0, 'e'},
        {"version",    required_argument, 0, 'v'},
        {"scale",      required_argument, 0, 's'},
        {"quiet-zone", required_argument, 0, 'q'},
        {"palette",    required_argument, 0, 'p'},
        {"file-format",required_argument, 0, 'F'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    if (argc <= 1) { print_usage(argv[0]); return 0; }

    int c;
    while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (c) {
            case 't': opt_text       = optarg; break;
            case 'f': opt_input_file = optarg; break;
            case 'm': opt_mode       = optarg; break;
            case 'e': opt_ec_level   = optarg; break;
            case 'v':
                if (parse_int_option("version", optarg, 0, 40, &opt_version) != 0) return 1;
                break;
            case 's':
                if (parse_int_option("scale", optarg, 6, 1000, &opt_scale) != 0) return 1;
                break;
            case 'q':
                if (parse_int_option("quiet-zone", optarg, 0, 1000, &opt_quiet_zone) != 0) return 1;
                break;
            case 'p': opt_palette    = optarg; break;
            case 'F': opt_file_format = optarg; break;
            case 'h': print_usage(argv[0]); return 0;
            default:
                fprintf(stderr, "Error: unknown option. Use --help.\n"); return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: output path required.\n"
                        "Usage: %s [options] output.png\n", argv[0]);
        return 1;
    }
    if (optind + 1 < argc) {
        fprintf(stderr, "Error: unexpected extra argument: %s\n", argv[optind + 1]);
        return 1;
    }
    const char *output_path = argv[optind];

    /* Validate options */
    if (!opt_text && !opt_input_file) {
        fprintf(stderr, "Error: --text or --input-file is required.\n"); return 1;
    }
    if (opt_text && opt_input_file) {
        fprintf(stderr, "Error: --text and --input-file are mutually exclusive.\n"); return 1;
    }
    if (strcmp(opt_mode, "qr") != 0 &&
        strcmp(opt_mode, "hcc2d4") != 0 &&
        strcmp(opt_mode, "hcc2d8") != 0) {
        fprintf(stderr, "Error: --mode must be qr, hcc2d4, or hcc2d8.\n"); return 1;
    }
    if (strlen(opt_ec_level) != 1 || ec_level_index(opt_ec_level[0]) < 0) {
        fprintf(stderr, "Error: --ec-level must be L, M, Q, or H.\n"); return 1;
    }
    if (opt_scale < 6) {
        fprintf(stderr, "Error: --scale must be at least 6 pixels per module.\n"); return 1;
    }
    if (opt_quiet_zone < 0) {
        fprintf(stderr, "Error: --quiet-zone must be >= 0.\n"); return 1;
    }
    if (strcmp(opt_palette, "model1") != 0 && strcmp(opt_palette, "model2") != 0) {
        fprintf(stderr, "Error: --palette must be model1 or model2.\n"); return 1;
    }
    if (strcmp(opt_file_format, "hcc2df") != 0 && strcmp(opt_file_format, "raw") != 0) {
        fprintf(stderr, "Error: --file-format must be hcc2df or raw.\n"); return 1;
    }

    char ec_level   = opt_ec_level[0];
    int  plane_count = (strcmp(opt_mode, "hcc2d8") == 0) ? 3 : 2;

    const RGB *palette;
    int pal_size;
    if (strcmp(opt_mode, "qr") == 0) {
        palette = PALETTE_QR;
        pal_size = 2;
    } else if (strcmp(opt_palette, "model1") == 0) {
        palette = (plane_count == 2) ? PALETTE_4_MODEL1 : PALETTE_8_MODEL1;
        pal_size = 1 << plane_count;
    } else {
        palette = (plane_count == 2) ? PALETTE_4_MODEL2 : PALETTE_8_MODEL2;
        pal_size = 1 << plane_count;
    }

    /* Build payload */
    uint8_t *payload = NULL;
    size_t   payload_len = 0;
    const char *payload_kind = NULL;
    uint8_t *source = NULL;
    size_t   source_len = 0;
    const char *source_name = NULL;
    char err[256] = "";

    if (opt_text) {
        source_len = strlen(opt_text);
        source = (uint8_t *)malloc(source_len ? source_len : 1);
        memcpy(source, opt_text, source_len);
        source_name = "text.txt";
    } else {
        source = read_file_bytes(opt_input_file, &source_len, err);
        if (!source) { fprintf(stderr, "Error: %s\n", err); return 1; }
        source_name = opt_input_file;
    }

    if (source_len == 0) {
        fprintf(stderr, "Error: payload is empty.\n"); free(source); return 1;
    }

    if (strcmp(opt_file_format, "hcc2df") == 0) {
        payload = build_hcc2df_payload(source_name, source, source_len, &payload_len, err);
        free(source);
        if (!payload) { fprintf(stderr, "Error: %s\n", err); return 1; }
        payload_kind = opt_text ? "hcc2df-text" : "hcc2df-file";
    } else {
        payload = source;
        payload_len = source_len;
        payload_kind = opt_text ? "raw-text-bytes" : "raw-file-bytes";
    }

    if (payload_len > INT_MAX) {
        fprintf(stderr, "Error: payload is too large for this encoder.\n");
        free(payload);
        return 1;
    }

    /* Encode */
    EncodedSymbol sym = {0};
    int ret;
    if (strcmp(opt_mode, "qr") == 0) {
        ret = encode_qr(payload, (int)payload_len,
                        ec_level, opt_version, opt_scale, opt_quiet_zone,
                        &sym, err);
    } else {
        ret = encode_hcc2d(payload, (int)payload_len,
                           opt_mode, ec_level, opt_version,
                           opt_scale, opt_quiet_zone,
                           &sym, err);
    }
    free(payload);
    if (ret != 0) {
        fprintf(stderr, "Error: %s\n", err); return 1;
    }

    /* Write PNG */
    size_t   png_len;
    uint8_t *png_data = build_png(sym.width, sym.height, sym.pixels,
                                   palette, pal_size, &png_len);
    free(sym.pixels);

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        fprintf(stderr, "Error: cannot write %s\n", output_path);
        free(png_data); return 1;
    }
    fwrite(png_data, 1, png_len, out);
    fclose(out);
    free(png_data);

    printf("wrote %s mode=%s ec=%c version=%d mask=%d palette=%s size=%dx%d payload=%s\n",
           output_path, sym.mode, sym.ec_level, sym.version_number,
           sym.mask_pattern, opt_palette, sym.width, sym.height, payload_kind);
    return 0;
}

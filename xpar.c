/*
   Copyright (C) 2022-2024 Kamila Szewczyk

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "common.h"
#include "trans.h"
#include "platform.h"
#include "crc32c.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

// ============================================================================
//  Implementation of Reed-Solomon codes. Follows the BCH view. Original code
//  was written by Phil Karn, KA9Q, in 1999. This is a modified version due to
//  Kamila Szewczyk which exhibits significantly better performance.
// ============================================================================
static u8 LOG[256], EXP[256], PROD[256][256], PROD_GEN[256][32], DP[256][256];
static void gf256_gentab(u8 poly) {
  for (int l = 0, b = 1; l < 255; l++) {
    LOG[b] = l;  EXP[l] = b;
    if ((b <<= 1) >= 256)
      b = (b - 256) ^ poly;
  }
  LOG[0] = 255; EXP[255] = 0;
  for (int i = 0; i < 256; i++)
    for (int j = 0; j < 256; j++)
      PROD[i][j] = (i && j) ? EXP[(LOG[i] + LOG[j]) % 255] : 0,
      DP[i][j] = (i != 255 && j) ? EXP[(i + LOG[j]) % 255] : 0;
  static const u8 gen[T] = {
    1, 91, 127, 86, 16, 30, 13, 235, 97, 165, 8, 42, 54, 86, 171, 32,
    113, 32, 171, 86, 54, 42, 8, 165, 97, 235, 13, 30, 16, 86, 127, 91
  };
  for (int i = 0; i < 256; i++)
    for (int j = 0; j < T; j++)
      PROD_GEN[i][j] = PROD[i][gen[j]];
}
static u8 gf256_div(u8 a, u8 b) {
  if (!a || !b) return 0;
  int d = LOG[a] - LOG[b];
  return EXP[d < 0 ? d + 255 : d];
}
void rse32(u8 data[K], u8 out[N]) {
  memset(out + K, 0, N - K);
  for (int i = K - 1; i >= 0; i--) {
    u8 x = data[i] ^ out[K + T - 1];
    for (int j = T - 1; j > 0; j--)
      out[K + j] = out[K + j - 1] ^ PROD_GEN[x][j];
    out[K] = PROD_GEN[x][0];
  }
  memcpy(out, data, K);
}
int rsd32(u8 data[N]) {
  int deg_lambda, el, deg_omega = 0;
  int i, j, r, k, syn_error, count;
  u8 q, tmp, num1, num2, den, discr_r;
  u8 lambda[T + 1] = { 0 }, omega[T + 1] = { 0 }, eras_pos[T] = { 0 };
  u8 t[T + 1], s[T], root[T], reg[T + 1] = { 0 };
  u8 b_backing[3 * T + 1] = { 0 }, * b = b_backing + 2 * T;
  memset(s, data[0], 32);
  // Fast syndrome computation: idea discovered by Marshall Lochbaum.
  for (int jb = 0; jb < 51; jb++) {
    u8 a5 = 255, t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0;
    for (j = jb; j < 255; j += 51) {
      if (j == 0 || !data[j]) continue;
      tmp = data[j];
      tmp = PROD[EXP[(212 * j) % 255]][tmp];
      u8 a1 = EXP[(11 * j) % 255]; // a_j1
      u8 a2 = PROD[a1][a1];        // a_j2
      if (a5 == 255) a5 = PROD[a2][PROD[a1][a2]];
      t1 ^=                tmp;  // t1 = sum of t_j1, j = jb (mod 51)
      t2 ^=       PROD[a1][tmp]; // etc.
      t3 ^= tmp = PROD[a2][tmp];
      t4 ^=       PROD[a1][tmp];
      t5 ^=       PROD[a2][tmp];
    }
    if (a5 == 255) continue; // No j values do anything (unlikely)
    for (i = 0; ; i += 5) {
      s[i ] ^= t1; t1 = PROD[a5][t1];
      s[i+1] ^= t2; t2 = PROD[a5][t2];
      if (i+2 == 32) break;
      s[i+2] ^= t3; t3 = PROD[a5][t3];
      s[i+3] ^= t4; t4 = PROD[a5][t4];
      s[i+4] ^= t5; t5 = PROD[a5][t5];
    }
  }
  for (syn_error = 0, i = 0; i < 32; i++) syn_error |= s[i];
  if (!syn_error) return 0;
  lambda[0] = 1;  r = el = 0;  memcpy(b, lambda, 33);
  while (++r <= 32) {
    for (discr_r = 0, i = 0; i < r; i++)
      discr_r ^= PROD[lambda[i]][s[r - i - 1]];
    if (!discr_r) --b; else {
      t[0] = lambda[0];
      COPY(t[i + 1], lambda[i + 1] ^ PROD[discr_r][b[i]], 32);
      if (2 * el <= r - 1) {
        el = r - el;
        COPY(b[i], gf256_div(lambda[i], discr_r), 33);
      } else --b;
      memcpy(lambda, t, 33);
    }
  }
  for (deg_lambda = 0, i = 0; i < 33; i++)
    if (lambda[i]) deg_lambda = i, reg[i] = lambda[i];
  for (count = 0, i = 1, k = 139; i <= 255; i++, k = (k + 139) % 255) {
    for (q = 1, j = deg_lambda; j > 0; j--)
      q ^= reg[j] = DP[j][reg[j]];
    if (q) continue;
    root[count] = i, eras_pos[count] = k;
    if (++count == deg_lambda) break; // Early exit.
  }
  if (deg_lambda != count) return -1;
  for (i = 0; i < 32; i++) {
    for (tmp = 0, j = MIN(deg_lambda, i); j >= 0; j--)
      tmp ^= PROD[s[i - j]][lambda[j]];
    if (tmp) deg_omega = i, omega[i] = tmp;
  }
  for (j = count - 1; j >= 0; j--) {
    for (num1 = 0, i = deg_omega; i >= 0; i--)
      num1 ^= DP[(i * root[j]) % 255][omega[i]];
    for (den = 0, i = MIN(deg_lambda, T - 1) & ~1; i >= 0; i -= 2)
      den ^= DP[(i * root[j]) % 255][lambda[i + 1]];
    if (den == 0) return -1;
    data[eras_pos[j]] ^= gf256_div(DP[(root[j] * 111) % 255][num1], den);
  }
  return count;
}
// ============================================================================
//  Processing. We apply a few strategies that depend on some specifics of the
//  process at hand:
//  - If we can map the file to memory and have multi-core capabilities, we use
//    work-stealing parallelism to encode the data in chunks as big as the 
//    interlacing factor.
//    - If the output file is seekable, we pre-allocate the space and seek
//      across the file to write whichever chunks are ready (1).
//    - If the output file is not seekable, we write the chunks in order (2).
//  - If we can map the file to memory but can not perform parallel encoding,
//    we encode the data in a serial fashion (3).
//  - If the file can not be mapped, we assume that the output also can not be
//    mapped (4).
// ============================================================================
typedef struct {
  const char * input_name, * output_name;
  int interlacing;
  bool force, quiet, verbose, no_map;
} options_t;
static int compute_interlacing_bs(int ifactor) {
  assert(1 <= ifactor); assert(ifactor <= 3);
  switch (ifactor) {
    default: case 1: return 1; break;
    case 2: return N; break;
    case 3: return N * N; break;
  }
}
static void do_interlacing(u8 * out_buffer, int ifactor) {
  assert(1 <= ifactor); assert(ifactor <= 3);
  switch (ifactor) {
    case 2: trans2D(out_buffer); break;
    case 3: trans3D(out_buffer); break;
  }
}
static void write_header(FILE * des, int ifactor) {
  u8 h[K] = { 0 }, out[N];
  h[0] = 'X'; h[1] = 'P'; h[2] = XPAR_MAJOR; h[3] = XPAR_MINOR;
  h[4] = ifactor + '0';
  rse32(h, out); xfwrite(h, 5, des); xfwrite(out + K, N - K, des);
}
static int parse_header(u8 out[N], int force, int ifactor_override) {
  if (out[0] != 'X' || out[1] != 'P') {
    fprintf(stderr, "Invalid header.\n");
    if (!force) exit(1);
  }
  out[0] = 'X'; out[1] = 'P'; memset(out + 5, 0, K - 5);
  if(rsd32(out) < 0) {
    fprintf(stderr, "Invalid header.\n");
    if (!force) exit(1);
  }
  int ifactor = out[4] - '0';
  if (ifactor < 1 || ifactor > 3) {
    fprintf(stderr, "Invalid interlacing factor.\n");
    if (!force) exit(1); else return ifactor_override;
  }
  return ifactor;
}
static int read_header(FILE * des, int force, int ifactor_override) {
  u8 out[N]; xfread(out, 5, des); xfread(out + K, N - K, des);
  return parse_header(out, force, ifactor_override);
}
static int read_header_from_map(mmap_t map, int force, int ifactor_override) {
  if (map.size < 5 + N - K) {
    fprintf(stderr, "Truncated file.\n"); exit(1); // Can't recover from that.
  }
  u8 out[N]; memcpy(out, map.map, 5); memcpy(out + K, map.map + 5, N - K);
  return parse_header(out, force, ifactor_override);
}
typedef struct { u32 bytes, crc; } block_hdr;
static void write_block_header(FILE * des, block_hdr h) {
  u8 b[8]; b[0] = 'X';
  if (h.bytes > 0xFFFFFF) {
    fprintf(stderr, "Could not write the header: block too big.\n"); exit(1);
  }
  b[1] = h.bytes >> 16; b[2] = h.bytes >> 8; b[3] = h.bytes;
  b[4] = h.crc >> 24; b[5] = h.crc >> 16; b[6] = h.crc >> 8; b[7] = h.crc;
  xfwrite(b, 8, des);
}
static block_hdr parse_block_header(u8 b[8], bool force) {
  block_hdr h;  bool valid = b[0] == 'X';
  if (!valid) {
    fprintf(stderr, "Invalid block header.\n");
    if(!force) exit(1);
    h.bytes = 0xFFFFFF; h.crc = 0; return h;
  } else {
    h.bytes = (b[1] << 16) | (b[2] << 8) | b[3];
    h.crc = (b[4] << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
    return h;
  }
}
void encode4(FILE * in, FILE * out, int ifactor) {
  u8 * in_buffer, * out_buffer;
  int ibs = compute_interlacing_bs(ifactor);
  in_buffer = xmalloc(ibs * K), out_buffer = xmalloc(ibs * N);
  block_hdr bhdr;  write_header(out, ifactor);
  for (size_t n; n = xfread(in_buffer, ibs * K, in); ) {
    if(n < ibs * K) memset(in_buffer + n, 0, ibs * K - n);
    for (int i = 0; i < ibs; i++)
      rse32(in_buffer + i * K, out_buffer + i * N);
    do_interlacing(out_buffer, ifactor);
    xfwrite(out_buffer, ibs * N, out);
    bhdr.bytes = n; bhdr.crc = crc32c(in_buffer, n);
    write_block_header(out, bhdr);
  }
  free(in_buffer), free(out_buffer); xfclose(out);
}
#ifdef XPAR_ALLOW_MAPPING
void encode3(mmap_t in, FILE * out, int ifactor) {
  u8 * in_buffer, * out_buffer;
  int ibs = compute_interlacing_bs(ifactor);
  in_buffer = xmalloc(ibs * K), out_buffer = xmalloc(ibs * N);
  block_hdr bhdr;  write_header(out, ifactor);
  for (sz n;
       n = MIN(in.size, ibs * K), memcpy(in_buffer, in.map, n), n;
       in.size -= n, in.map += n) {
    if(n < ibs * K) memset(in_buffer + n, 0, ibs * K - n);
    for (int i = 0; i < ibs; i++)
      rse32(in_buffer + i * K, out_buffer + i * N);
    do_interlacing(out_buffer, ifactor);
    xfwrite(out_buffer, ibs * N, out);
    bhdr.bytes = n; bhdr.crc = crc32c(in_buffer, n);
    write_block_header(out, bhdr);
  }
  free(in_buffer), free(out_buffer); xfclose(out);
}
#endif
void decode4(FILE * in, FILE * out, int force, int ifactor_override,
             bool quiet, bool verbose) {
  u8 * in_buffer, * out_buffer;  int laces = 0, ecc = 0;
  block_hdr bhdr; u8 tmp[8];
  int ifactor = read_header(in, force, ifactor_override);
  sz ibs = compute_interlacing_bs(ifactor);
  in_buffer = xmalloc(ibs * N), out_buffer = xmalloc(ibs * K);
  for (sz n; n = xfread(in_buffer, ibs * N, in); laces++) {
    if(n < ibs * N) {
      if (!quiet)
        fprintf(stderr, "Short read, lace %u (bytes %u-%u).\n",
          laces, laces * ibs * N, laces * ibs * N + n - 1);
      if (!force) exit(1);
      memset(in_buffer + n, 0, ibs * N - n);
    }
    if(xfread(tmp, 8, in) != 8) {
      if (!quiet)
        fprintf(stderr, "Short read (block header), lace %u (bytes %u-%u).\n",
          laces, laces * ibs * N, laces * ibs * N + n - 1);
      if (!force) exit(1);
    }
    bhdr = parse_block_header(tmp, force);
    do_interlacing(in_buffer, ifactor);
    for (int i = 0; i < ibs; i++) {
      int n = rsd32(in_buffer + i * N);
      if (n < 0) {
        const unsigned lace_ibs = laces * ibs + i;
        if (!quiet)
          fprintf(stderr,
            "Block %u (lace %u, bytes %u-%u) irrecoverable.\n",
            lace_ibs, laces, lace_ibs * N, lace_ibs * N + N - 1);
        if (!force) exit(1);
      } else ecc += n;
      memcpy(out_buffer + i * K, in_buffer + i * N, K);
    }
    sz size = MIN(ibs * K, bhdr.bytes);
    u32 crc = crc32c(out_buffer, size);
    if (crc != bhdr.crc) {
      if (!quiet)
        fprintf(stderr, "CRC mismatch (%08X vs %08X), block %u (lace %u, bytes %u-%u).\n",
          bhdr.crc, crc, laces * ibs, laces, laces * ibs * N, laces * ibs * N + size - 1);
      if (!force) exit(1);
    }
    xfwrite(out_buffer, size, out);
  }
  free(in_buffer), free(out_buffer); xfclose(out);
  if (!quiet && verbose)
    fprintf(stderr, "Decoded %u laces, %u errors corrected.\n", laces, ecc);
}
#ifdef XPAR_ALLOW_MAPPING
void decode3(mmap_t in, FILE * out, int force, int ifactor_override,
             bool quiet, bool verbose) {
  u8 * in_buffer, * out_buffer;  int laces = 0, ecc = 0;
  block_hdr bhdr; u8 tmp[8];
  int ifactor = read_header_from_map(in, force, ifactor_override);
  in.size -= 5 + N - K; in.map += 5 + N - K; // Skip the header.
  sz ibs = compute_interlacing_bs(ifactor);
  in_buffer = xmalloc(ibs * N), out_buffer = xmalloc(ibs * K);
  for (sz n;
      n = MIN(in.size, ibs * N), memcpy(in_buffer, in.map, n),
          in.size -= n, in.map += n, n
      ; laces++) {
    if(n < ibs * N) {
      if (!quiet)
        fprintf(stderr, "Short read, lace %u (bytes %u-%u).\n",
          laces, laces * ibs * N, laces * ibs * N + n - 1);
      if (!force) exit(1);
      memset(in_buffer + n, 0, ibs * N - n);
    }
    if (in.size < 8) {
      if (!quiet)
        fprintf(stderr, "Short read (block header), lace %u (bytes %u-%u).\n",
          laces, laces * ibs * N, laces * ibs * N + n - 1);
      if (!force) exit(1);
      memcpy(tmp, in.map, in.size); in.map += in.size; in.size = 0;
    } else {
      memcpy(tmp, in.map, 8); in.size -= 8; in.map += 8;
    }
    bhdr = parse_block_header(tmp, force);
    do_interlacing(in_buffer, ifactor);
    for (int i = 0; i < ibs; i++) {
      int n = rsd32(in_buffer + i * N);
      if (n < 0) {
        const unsigned lace_ibs = laces * ibs + i;
        if (!quiet)
          fprintf(stderr,
            "Block %u (lace %u, bytes %u-%u) irrecoverable.\n",
            lace_ibs, laces, lace_ibs * N, lace_ibs * N + N - 1);
        if (!force) exit(1);
      } else ecc += n;
      memcpy(out_buffer + i * K, in_buffer + i * N, K);
    }
    sz size = MIN(ibs * K, bhdr.bytes);
    u32 crc = crc32c(out_buffer, size);
    if (crc != bhdr.crc) {
      if (!quiet)
        fprintf(stderr, "CRC mismatch, block %u (lace %u, bytes %u-%u).\n",
          laces * ibs, laces, laces * ibs * N, laces * ibs * N + size - 1);
      if (!force) exit(1);
    }
    xfwrite(out_buffer, size, out);
  }
  free(in_buffer), free(out_buffer); xfclose(out);
  if (!quiet && verbose)
    fprintf(stderr, "Decoded %u laces, %u errors corrected.\n", laces, ecc);
}
#endif
// ============================================================================
//  Command-line stub.
// ============================================================================
static struct stat validate_file(const char * filename) {
  struct stat st;
  if (stat(filename, &st) == -1) { perror("stat"); exit(1); }
  if (S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Input is a directory.\n"); exit(1);
  }
  return st;
}
static void do_encode(options_t o) {
  FILE * out = stdout;
  if(o.output_name) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    int exists = stat(o.output_name, &st);
    if (st.st_size && exists != -1 && !o.force) {
      fprintf(stderr,
        "Output file `%s' exists and is not empty.\n", o.output_name);
      exit(1);
    }
    if (!(out = fopen(o.output_name, "wb"))) { perror("fopen"); exit(1); }
  }
  if (o.input_name) {
    struct stat st = validate_file(o.input_name);
    if(!o.no_map) {
      #if defined(XPAR_ALLOW_MAPPING)
      mmap_t map = xpar_map(o.input_name);
      if (map.map) {
        encode3(map, out, o.interlacing);
        xpar_unmap(&map);
        return;
      }
      #endif
    }
    FILE * in = fopen(o.input_name, "rb");
    if (!in) { perror("fopen"); exit(1); }
    encode4(in, out, o.interlacing);
  } else {
    encode4(stdin, out, o.interlacing);
  }
}
static void do_decode(options_t o) {
  FILE * out = stdout;
  if(o.output_name) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    int exists = stat(o.output_name, &st);
    if (st.st_size && exists != -1 && !o.force) {
      fprintf(stderr,
        "Output file `%s' exists and is not empty.\n", o.output_name);
      exit(1);
    }
    if (!(out = fopen(o.output_name, "wb"))) { perror("fopen"); exit(1); }
  }
  if (o.input_name) {
    struct stat st = validate_file(o.input_name);
    if(!o.no_map) {
      #if defined(XPAR_ALLOW_MAPPING)
      mmap_t map = xpar_map(o.input_name);
      if (map.map) {
        decode3(map, out, o.force, o.interlacing, o.quiet, o.verbose);
        xpar_unmap(&map);
        return;
      }
      #endif
    }
    FILE * in = fopen(o.input_name, "rb");
    if (!in) { perror("fopen"); exit(1); }
    decode4(in, out, o.force, o.interlacing, o.quiet, o.verbose);
  } else {
    decode4(stdin, out, o.force, o.interlacing, o.quiet, o.verbose);
  }
}
static void version() {
  printf(
    "xpar %d.%d. Copyright (C) by Kamila Szewczyk, 2022-2024.\n"
    "Licensed under the terms of GNU GPL version 3, available online at:\n"
    " <https://www.gnu.org/licenses/gpl-3.0.en.html>\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n",
    XPAR_MAJOR, XPAR_MINOR
  );
}
static void help() {
  printf(
    "xpar - an error/erasure code system guarding data integrity.\n"
    "Usage: xpar [flags] files...\n"
    "Operations:\n"
    "  -e, --encode         add parity bits to a specified file\n"
    "  -d, --decode         recover the original data\n"
    "  -h, --help           display an usage overview\n"
    "  -f, --force          force operation: ignore errors, overwrite files\n"
    "  -v, --verbose        verbose mode (display more information)\n"
    "  -q, --quiet          quiet mode (display less information)\n"
    "  -V, --version        display version information\n"
#if defined(XPAR_ALLOW_MAPPING)
    "      --no-mmap        unconditionally disable memory mapping\n"
#endif
    "Extra flags:\n"
    "  -c, --stdout         force writing to standard output\n"
    "  -i N, --interlace=N  change the interlacing setting (1,2,3)\n"
    "\n"
    "Interlacing transposes the input data into larger blocks allowing\n"
    "the codes to accomplish higher burst error correction capabilities.\n"
    "The default interlacing factor is 1, which means no interlacing.\n"
    "The interlacing factor of two allows correction of 4080 contiguous\n"
    "errors in a 65025 byte block.\n"
    "\n"
    "Report bugs to: https://github.com/kspalaiologos/xpar\n"
    "Or contact the author: Kamila Szewczyk <kspalaiologos@gmail.com>\n"
  );
}
enum mode_t { MODE_NONE, MODE_ENCODING, MODE_DECODING };
int main(int argc, char * argv[]) {
  gf256_gentab(0x87);  platform_init();
  enum { FLAG_NO_MMAP = CHAR_MAX + 1 };
  const char * sopt = "Vhvedcfi:";
  const struct option lopt[] = {
    { "version", no_argument, NULL, 'V' },
    { "verbose", no_argument, NULL, 'v' },
    { "stdout", no_argument, NULL, 'c' },
    { "quiet", no_argument, NULL, 'q' },
    { "help", no_argument, NULL, 'h' },
    { "encode", no_argument, NULL, 'e' },
    { "decode", no_argument, NULL, 'd' },
    { "force", no_argument, NULL, 'f' },
#if defined(XPAR_ALLOW_MAPPING)
    { "no-mmap", no_argument, NULL, FLAG_NO_MMAP },
#endif
    { "interlacing", required_argument, NULL, 'i' },
    { NULL, 0, NULL, 0 }
  };
  bool verbose = false, quiet = false, force = false, force_stdout = false;
  bool no_map = false;
  int mode = MODE_NONE, interlacing = 1;
  for (int c; (c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1; ) {
    switch (c) {
      case 'V': version(); return 0;
      case 'v':
        if (quiet) { fprintf(stderr, "Conflicting options.\n"); exit(1); }
        verbose = true; break;
      case 'q':
        if (verbose) { fprintf(stderr, "Conflicting options.\n"); exit(1); }
        quiet = true; break;
      case 'h': help(); return 0;
      case 'e':
        if(mode == MODE_NONE) { mode = MODE_ENCODING; break; }
        else {
          fprintf(stderr, "Multiple operation modes specified.\n"); exit(1);
        }
      case 'd':
        if(mode == MODE_NONE) { mode = MODE_DECODING; break; }
        else {
          fprintf(stderr, "Multiple operation modes specified.\n"); exit(1);
        }
      case 'f': force = true; break;
      case 'c': force_stdout = true; break;
      case FLAG_NO_MMAP: no_map = true; break;
      case 'i':
        interlacing = atoi(optarg);
        if (interlacing < 1 || interlacing > 3) {
          fprintf(stderr, "Invalid interlacing factor.\n"); exit(1);
        }
        break;
      default: exit(1); break;
    }
  }
  if (mode == MODE_NONE) {
    fprintf(stderr, "No operation mode specified.\n"); exit(1);
  }
  char * f1 = NULL, * f2 = NULL;
  while (optind < argc) {
    if (!f1) f1 = argv[optind++];
    else if (!f2) f2 = argv[optind++];
    else {
      fprintf(stderr, "Too many arguments.\n"); exit(1);
    }
  }
  char * input_file = NULL, * output_file = NULL;
  switch(mode) {
    case MODE_ENCODING:
      if (!f2) {
        input_file = f1;
        if (force_stdout) output_file = NULL;
        else asprintf(&output_file, "%s.xpa", f1);
      } else input_file = f1, output_file = f2;
      break;
    case MODE_DECODING:
      if (!f2) {
        input_file = f1;
        if (force_stdout) output_file = NULL;
        else {
          sz len = strlen(f1);
          if (len < 5 || strcmp(f1 + len - 4, ".xpa")) {
            fprintf(stderr, "Unknown file type.\n"); exit(1);
          }
          output_file = strndup(f1, len - 4);
        }
      } else input_file = f1, output_file = f2;
      break;
  }
  if (verbose) {
    printf("Input file: %s\n", input_file);
    printf("Output file: %s\n", output_file);
    printf("Interlacing factor override: %d\n", interlacing);
  }
  options_t options = {
    .input_name = input_file, .output_name = output_file,
    .interlacing = interlacing,
    .force = force, .quiet = quiet, .verbose = verbose,
    .no_map = no_map
  };
  volatile struct timeval start, end;
  gettimeofday((struct timeval *) &start, NULL);
  switch(mode) {
    case MODE_ENCODING:
      do_encode(options); break;
    case MODE_DECODING:
      do_decode(options);
      break;
  }
  gettimeofday((struct timeval *) &end, NULL);
  if (verbose) {
    double elapsed = (end.tv_sec - start.tv_sec) +
      (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Elapsed time: %.6f seconds.\n", elapsed);
  }
  if (output_file != f2) free(output_file);
}
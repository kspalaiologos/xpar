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
#include "jmode.h"
#include "smode.h"
#include "platform.h"
#include "crc32c.h"

#include <limits.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

// ============================================================================
//  Command-line stub.
// ============================================================================
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
    "Usage (joint mode):\n"
    "  xpar -Je/-Jd [...] <in>              (adds/removes .xpa in output)\n"
    "  xpar -Je/-Jd [...] <in> <out>        (produces <out>)\n"
    "Usage (sharded mode):\n"
    "  xpar -Se [...] <in>                  (produces <in>.xpa.XXX)\n"
    "  xpar -Se --out-prefix=# [...] <in>   (produces #.xpa.XXX)\n"
    "  xpar -Sd [...] <out> <in>.001 ...    (produces <out>)\n"
    "\n"
    "Mode selection:\n"
    "  -J,   --joint        use the joint mode (default)\n"
    "  -S,   --sharded      use the sharded mode\n"
    "  -e,   --encode       add parity bits to a specified file\n"
    "  -d,   --decode       recover the original data\n"
    "Options:\n"
    "  -h,   --help         display an usage overview\n"
    "  -f,   --force        force operation: ignore errors, overwrite files\n"
    "  -v,   --verbose      verbose mode (display more information)\n"
    "  -q,   --quiet        quiet mode (display less information)\n"
    "  -V,   --version      display version information\n"
#if defined(XPAR_ALLOW_MAPPING)
    "        --no-mmap      unconditionally disable memory mapping\n"
#endif
    "Joint mode only:\n"
    "  -c,   --stdout       force writing to standard output\n"
    "  -i #, --interlace=#  change the interlacing setting (1,2,3)\n"
    "Sharded mode encoding options:\n"
    "        --dshards=#    set the number of data shards (< 128)\n"
    "        --pshards=#    set the number of parity shards (< 64)\n"
    "        --out-prefix=# set the output file prefix\n"
    "\n"
    "In joint mode:\n"
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
  jmode_gf256_gentab(0x87);  smode_gf256_gentab(0x87);
  platform_init();
  enum { FLAG_NO_MMAP = CHAR_MAX + 1, FLAG_DSHARDS, FLAG_PSHARDS,
         FLAG_OUT_PREFIX };
  const char * sopt = "VJShvedcfi:";
  const struct option lopt[] = {
    { "version", no_argument, NULL, 'V' },
    { "verbose", no_argument, NULL, 'v' },
    { "joint", no_argument, NULL, 'J' },
    { "sharded", no_argument, NULL, 'S' },
    { "stdout", no_argument, NULL, 'c' },
    { "quiet", no_argument, NULL, 'q' },
    { "help", no_argument, NULL, 'h' },
    { "encode", no_argument, NULL, 'e' },
    { "decode", no_argument, NULL, 'd' },
    { "force", no_argument, NULL, 'f' },
    { "dshards", required_argument, NULL, FLAG_DSHARDS },
    { "pshards", required_argument, NULL, FLAG_PSHARDS },
    { "out-prefix", required_argument, NULL, FLAG_OUT_PREFIX },
#if defined(XPAR_ALLOW_MAPPING)
    { "no-mmap", no_argument, NULL, FLAG_NO_MMAP },
#endif
    { "interlacing", required_argument, NULL, 'i' },
    { NULL, 0, NULL, 0 }
  };
  bool verbose = false, quiet = false, force = false, force_stdout = false;
  bool no_map = false, joint = false, sharded = false;
  int mode = MODE_NONE, interlacing = -1, dshards = -1, pshards = -1;
  const char * out_prefix = NULL;
  for (int c; (c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1; ) {
    switch (c) {
      case 'V': version(); return 0;
      case 'J':
        if (sharded) goto conflict;  joint = true; break;
      case 'S':
        if (joint) goto conflict;  sharded = true; break;
      case 'v':
        if (quiet) goto conflict;  verbose = true; break;
      case 'q':
        if (verbose) goto conflict;  quiet = true; break;
      case 'h': help(); return 0;
      case 'e':
        if(mode == MODE_NONE) mode = MODE_ENCODING;
        else goto opmode_conflict;  break;
      case 'd':
        if(mode == MODE_NONE) mode = MODE_DECODING;
        else goto opmode_conflict;  break;
      case 'f': force = true; break;
      case 'c': force_stdout = true; break;
      case FLAG_NO_MMAP: no_map = true; break;
      case 'i':
        interlacing = atoi(optarg);
        if (interlacing < 1 || interlacing > 3)
          FATAL("Invalid interlacing factor.");
        break;
      case FLAG_DSHARDS:
        dshards = atoi(optarg);
        if (dshards < 1 || dshards >= MAX_DATA_SHARDS)
          FATAL("Invalid number of data shards.");
        break;
      case FLAG_PSHARDS:
        pshards = atoi(optarg);
        if (pshards < 1 || pshards >= MAX_PARITY_SHARDS)
          FATAL("Invalid number of parity shards.");
        break;
      case FLAG_OUT_PREFIX: out_prefix = optarg; break;
      default: exit(1); break;
      conflict: FATAL("Conflicting options.");
      opmode_conflict: FATAL("Multiple operation modes specified.");
    }
  }
  if (mode == MODE_NONE)
    FATAL("No operation mode specified.");
  if (!joint && !sharded) joint = true;
  if (joint) {
    if (dshards != -1 || pshards != -1 || out_prefix)
      FATAL("Sharded mode options in joint mode.");
    if (interlacing == -1) interlacing = 1;
    char * f1 = NULL, * f2 = NULL;
    while (optind < argc) {
      if (!f1) f1 = argv[optind++];
      else if (!f2) f2 = argv[optind++];
      else FATAL("Too many arguments.");
    }
    char * input_file = NULL, * output_file = NULL;
    if (f1) switch(mode) {
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
            if (len < 5 || strcmp(f1 + len - 4, ".xpa"))
              FATAL("Unknown file type.");
            output_file = strndup(f1, len - 4);
          }
        } else input_file = f1, output_file = f2;
        break;
    }
    joint_options_t options = {
      .input_name = input_file, .output_name = output_file,
      .interlacing = interlacing,
      .force = force, .quiet = quiet, .verbose = verbose,
      .no_map = no_map
    };
    volatile struct timeval start, end;
    gettimeofday((struct timeval *) &start, NULL);
    switch(mode) {
      case MODE_ENCODING: do_joint_encode(options); break;
      case MODE_DECODING: do_joint_decode(options); break;
    }
    gettimeofday((struct timeval *) &end, NULL);
    if (verbose) {
      double elapsed = (end.tv_sec - start.tv_sec) +
        (end.tv_usec - start.tv_usec) / 1000000.0;
      printf("Elapsed time: %.6f seconds.\n", elapsed);
    }
    if (output_file != f2) free(output_file);
  } else {
    if (interlacing != -1 || force_stdout)
      FATAL("Joint mode options in sharded mode.");
    volatile struct timeval start, end;
    gettimeofday((struct timeval *) &start, NULL);
    switch(mode) {
      case MODE_ENCODING: {
        if (dshards == -1 || pshards == -1)
          FATAL("Number of data and parity shards not specified.");
        if (optind >= argc) FATAL("No input file specified.");
        const char * input_file = argv[optind++];
        if (optind < argc) FATAL("Too many arguments.");
        if (!out_prefix) out_prefix = input_file;
        sharded_encoding_options_t opt = {
          .input_name = input_file, .output_prefix = out_prefix,
          .dshards = dshards, .pshards = pshards,
          .force = force, .quiet = quiet, .verbose = verbose,
          .no_map = no_map
        };
        sharded_encode(opt);
        break;
      }
      case MODE_DECODING: {
        if (optind >= argc) FATAL("No output file specified.");
        const char * output_file = argv[optind++];
        if (optind >= argc) FATAL("No input files specified.");
        sharded_decoding_options_t opt = {
          .output_file = output_file,
          .input_files = (const char **) argv + optind,
          .force = force, .quiet = quiet, .verbose = verbose,
          .no_map = no_map, .n_input_shards = argc - optind
        };
        sharded_decode(opt);
        break;
      }
    }
    gettimeofday((struct timeval *) &end, NULL);
    if (verbose) {
      double elapsed = (end.tv_sec - start.tv_sec) +
        (end.tv_usec - start.tv_usec) / 1000000.0;
      printf("Elapsed time: %.6f seconds.\n", elapsed);
    }
  }
}
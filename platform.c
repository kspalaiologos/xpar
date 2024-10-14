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

#include "platform.h"

#if !defined(HAVE_ASPRINTF)
int asprintf(char **strp, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (len < 0) return -1;
  *strp = malloc(len + 1);
  if (!*strp) return -1;
  va_start(ap, fmt);
  len = vsnprintf(*strp, len + 1, fmt, ap);
  va_end(ap);
  return len;
}
#endif

#if !defined(HAVE_GETOPT_LONG)
/*
  Copyright 2005-2014 Rich Felker, et al.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

char * optarg;
int optind = 1, opterr = 1, optopt, __optpos, optreset = 0;

#define optpos __optpos

static void __getopt_msg(const char * a, const char * b, const char * c, size_t l) {
  FILE * f = stderr;
  fputs(a, f) >= 0 && fwrite(b, strlen(b), 1, f) && fwrite(c, 1, l, f) == l && putc('\n', f);
}

int getopt(int argc, char * const argv[], const char * optstring) {
  int i, c, d;
  int k, l;
  char * optchar;

  if (!optind || optreset) {
    optreset = 0;
    __optpos = 0;
    optind = 1;
  }

  if (optind >= argc || !argv[optind]) return -1;

  if (argv[optind][0] != '-') {
    if (optstring[0] == '-') {
      optarg = argv[optind++];
      return 1;
    }
    return -1;
  }

  if (!argv[optind][1]) return -1;

  if (argv[optind][1] == '-' && !argv[optind][2]) return optind++, -1;

  if (!optpos) optpos++;
  c = argv[optind][optpos], k = 1;
  optchar = argv[optind] + optpos;
  optopt = c;
  optpos += k;

  if (!argv[optind][optpos]) {
    optind++;
    optpos = 0;
  }

  if (optstring[0] == '-' || optstring[0] == '+') optstring++;

  i = 0;  d = 0;
  do {
    d = optstring[i], l = 1;
    if (l > 0) i += l;
    else i++;
  } while (l && d != c);

  if (d != c) {
    if (optstring[0] != ':' && opterr) __getopt_msg(argv[0], ": unrecognized option: ", optchar, k);
    return '?';
  }
  if (optstring[i] == ':') {
    if (optstring[i + 1] == ':') optarg = 0;
    else if (optind >= argc) {
      if (optstring[0] == ':') return ':';
      if (opterr) __getopt_msg(argv[0], ": option requires an argument: ", optchar, k);
      return '?';
    }
    if (optstring[i + 1] != ':' || optpos) {
      optarg = argv[optind++] + optpos;
      optpos = 0;
    }
  }
  return c;
}

static void permute(char * const * argv, int dest, int src) {
  char ** av = (char **)argv;
  char * tmp = av[src];
  int i;
  for (i = src; i > dest; i--) av[i] = av[i - 1];
  av[dest] = tmp;
}

static int __getopt_long_core(int argc, char * const * argv, const char * optstring, const struct option * longopts,
                              int * idx, int longonly) {
  optarg = 0;
  if (longopts && argv[optind][0] == '-' &&
      ((longonly && argv[optind][1] && argv[optind][1] != '-') || (argv[optind][1] == '-' && argv[optind][2]))) {
    int colon = optstring[optstring[0] == '+' || optstring[0] == '-'] == ':';
    int i, cnt, match;
    char * opt;
    for (cnt = i = 0; longopts[i].name; i++) {
      const char * name = longopts[i].name;
      opt = argv[optind] + 1;
      if (*opt == '-') opt++;
      for (; *name && *name == *opt; name++, opt++)
        ;
      if (*opt && *opt != '=') continue;
      match = i;
      if (!*name) {
        cnt = 1;
        break;
      }
      cnt++;
    }
    if (cnt == 1) {
      i = match;
      optind++;
      optopt = longopts[i].val;
      if (*opt == '=') {
        if (!longopts[i].has_arg) {
          if (colon || !opterr) return '?';
          __getopt_msg(argv[0], ": option does not take an argument: ", longopts[i].name,
                      strlen(longopts[i].name));
          return '?';
        }
        optarg = opt + 1;
      } else if (longopts[i].has_arg == required_argument) {
        if (!(optarg = argv[optind])) {
          if (colon) return ':';
          if (!opterr) return '?';
          __getopt_msg(argv[0], ": option requires an argument: ", longopts[i].name,
                       strlen(longopts[i].name));
          return '?';
        }
        optind++;
      }
      if (idx) *idx = i;
      if (longopts[i].flag) {
        *longopts[i].flag = longopts[i].val;
        return 0;
      }
      return longopts[i].val;
    }
    if (argv[optind][1] == '-') {
      if (!colon && opterr)
        __getopt_msg(argv[0], cnt ? ": option is ambiguous: " : ": unrecognized option: ", argv[optind] + 2,
                       strlen(argv[optind] + 2));
      optind++;
      return '?';
    }
  }
  return getopt(argc, argv, optstring);
}

static int __getopt_long(int argc, char * const * argv, const char * optstring, const struct option * longopts,
                         int * idx, int longonly) {
  int ret, skipped, resumed;
  if (!optind || optreset) {
    optreset = 0;
    __optpos = 0;
    optind = 1;
  }
  if (optind >= argc || !argv[optind]) return -1;
  skipped = optind;
  if (optstring[0] != '+' && optstring[0] != '-') {
    int i;
    for (i = optind;; i++) {
      if (i >= argc || !argv[i]) return -1;
      if (argv[i][0] == '-' && argv[i][1]) break;
    }
    optind = i;
  }
  resumed = optind;
  ret = __getopt_long_core(argc, argv, optstring, longopts, idx, longonly);
  if (resumed > skipped) {
    int i, cnt = optind - resumed;
    for (i = 0; i < cnt; i++) permute(argv, skipped, optind - 1);
    optind = skipped + cnt;
  }
  return ret;
}

int getopt_long(int argc, char * const * argv, const char * optstring, const struct option * longopts, int * idx) {
  return __getopt_long(argc, argv, optstring, longopts, idx, 0);
}

int getopt_long_only(int argc, char * const * argv, const char * optstring, const struct option * longopts, int * idx) {
  return __getopt_long(argc, argv, optstring, longopts, idx, 1);
}
#endif

#if defined(HAVE_MMAP) || defined(HAVE_CREATEFILEMAPPINGA)
#if defined(HAVE_MMAP)
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
#else
  #include <windows.h>
#endif

// Both of the functions below add a single page (4K) of 
// padding to the end of the file. Some of the code performs
// controlled buffer overflows by a few bytes (i.e. when
// the total # of shards doesn't divide the size of the file
// evenly), so we need to make sure that we don't run into
// any issues with the last page.

mmap_t xpar_map(const char * filename) {
  mmap_t mappedFile = { NULL, 0 };
#if defined(HAVE_CREATEFILEMAPPINGA)
  HANDLE fileHandle = CreateFileA(
    filename, GENERIC_READ, FILE_SHARE_READ, NULL,
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
  );
  if (fileHandle == INVALID_HANDLE_VALUE) return mappedFile;
  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(fileHandle, &fileSize)) {
    CloseHandle(fileHandle); return mappedFile;
  }
  fileSize.QuadPart += 4096;
  HANDLE fileMapping = CreateFileMappingA(
    fileHandle, NULL, PAGE_READONLY,
    fileSize.HighPart, fileSize.LowPart, NULL
  );
  if (!fileMapping) {
    CloseHandle(fileHandle); return mappedFile;
  }
  mappedFile.map = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
  if (!mappedFile.map) {
    CloseHandle(fileMapping); CloseHandle(fileHandle); return mappedFile;
  }
  mappedFile.size = (size_t) fileSize.QuadPart;
  CloseHandle(fileMapping);
  CloseHandle(fileHandle);
#else
  int fd = open(filename, O_RDONLY);
  if (fd == -1) return mappedFile;
  struct stat st;
  if (fstat(fd, &st) == -1) {
    close(fd); return mappedFile;
  }
  mappedFile.size = (size_t) st.st_size;
  mappedFile.map = mmap(NULL, mappedFile.size + 4096, PROT_READ, MAP_SHARED, fd, 0);
  if (mappedFile.map == MAP_FAILED) {
    close(fd); return mappedFile;
  }
  close(fd);
  // Reserve a single page at the end of the allocation

#endif
  return mappedFile;
}

void xpar_unmap(mmap_t * file) {
  #if defined(HAVE_MMAP)
    if (file->map) munmap(file->map, file->size + 4096);
  #else
    if (file->data) UnmapViewOfFile(file->data);
  #endif
  file->map = NULL; file->size = 0;
}
#endif

#if defined(HAVE_SETMODE) && defined(HAVE_IO_H)
  #include <io.h>
  #include <fcntl.h>
#endif

void platform_init(void) {
  #if defined(HAVE_SETMODE) && defined(HAVE_IO_H)
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
  #endif
}

#include <errno.h>
void xfclose(FILE * des) {
  if(fflush(des)) FATAL_PERROR("fflush");
#if defined(HAVE_COMMIT) && defined(HAVE_IO_H)
  if (commit(fileno(des))) FATAL_PERROR("commit");
#elif defined(HAVE_FSYNC)
  // EINVAL means that we tried to fsync something that can't be synced.
  if (fsync(fileno(des))) {
    if (errno != EINVAL) FATAL_PERROR("fsync");
    errno = 0;
  }
#endif
  if (fclose(des) == EOF) FATAL_PERROR("fclose");
}
void xfwrite(const void * ptr, sz size, FILE * stream) {
  if (fwrite(ptr, 1, size, stream) != size) FATAL_PERROR("fwrite");
  if (ferror(stream)) FATAL_PERROR("fwrite");
}
sz xfread(void * ptr, sz size, FILE * stream) {
  sz n = fread(ptr, 1, size, stream);
  if (ferror(stream)) FATAL_PERROR("fread");
  return n;
}
void * xmalloc(sz size) {
  void * ptr = malloc(size);
  if (!ptr) FATAL_PERROR("malloc");
  return ptr;
}
void notty(FILE * des) {
  #if defined(HAVE_ISATTY)
  if (isatty(fileno(des)))
    FATAL("Refusing to read/write binary data from/to a terminal.");
  errno = 0; // isatty sets errno to weird values on some platforms.
  #endif
}
bool is_seekable(FILE * des) {
  return fseek(des, 0, SEEK_CUR) != -1;
}
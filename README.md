# xpar
xpar - an error/erasure code system guarding data integrity.
Licensed under the terms of GNU GPL version 3 or later - see COPYING.
Report issues to Kamila Szewczyk <kspalaiologos@gmail.com>.
Project homepage: https://github.com/kspalaiologos/xpar

xpar in solid mode generates a slightly inflated (by about 12%) parity-guarded
file from a given data file. Such a file can be recovered as long as no more
than about 6.2% of the data is corrupted. xpar internally uses a (255,223)-RS
code over an 8-bit Galois field.

## Building

```
# If using a git clone (not needed for source packages), first...
$ ./bootstrap

# All...
$ ./configure
$ make
$ sudo make install
```

## Usage

Consult the man page.

## Disclaimer

The file format will change until the stable version v1.0 is reached.
Do not use xpar for critical data, do not expect backwards or forwards
compatibility until then. Currently tested only on Zen4 x86_64 Linux.

# Development 

A rough outline of some development-related topics below.

## Roadmap

- Need to implement the parallel variant.
- Need to provide automatic testing for all configurations.
- Write a proper readme, manpages, etc.
- Make sure that this builds on Windows and port over the assembly code.
- Fuzz to find segfaults.
- 32- vs 64-bit code: determine if there's any compatibility issues.

Low priority:
- Should probably not pad to the full interlacing block size.
- Add assembly routines for hot spots in the program.

## Repository management

As it stands:
- `contrib/` - holds scripts and other non-source files that are not present
  in the distribution tarball and not supported.
- `NEWS` - will contain the release notes for each release and needs to be
  modified before each release.
- `ChangeLog` - generated via `make update-ChangeLog`; intended to be
  re-generated before each release.

Code style:
- Two space indent, brace on the same line, middle pointers - `char * p;`.


EXTRA_DIST = COPYING README
bin_PROGRAMS = xpar
xpar_SOURCES = platform.c xpar.c crc32c.c trans.c
if XPAR_X86_64
xpar_SOURCES += crc32c-x86_64.S
endif
xpar_CFLAGS = @extra_cflags@
xpar_LDFLAGS = @extra_ldflags@
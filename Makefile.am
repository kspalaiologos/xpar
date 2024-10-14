EXTRA_DIST = COPYING README.md AUTHORS xpar-torture.sh
bin_PROGRAMS = xpar
noinst_HEADERS = platform.h crc32c.h jmode.h smode.h common.h
xpar_SOURCES = platform.c xpar.c crc32c.c jmode.c smode.c
if XPAR_X86_64
xpar_SOURCES += xpar-x86_64.S
endif
xpar_CFLAGS = @extra_cflags@
xpar_LDFLAGS = @extra_ldflags@

.PHONY: update-ChangeLog
update-ChangeLog:
	contrib/gitlog-to-changelog --since=1970-01-01 > ChangeLog

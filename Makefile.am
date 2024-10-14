EXTRA_DIST = README.md
bin_PROGRAMS = xpar
noinst_HEADERS = platform.h crc32c.h jmode.h smode.h common.h
xpar_SOURCES = platform.c xpar.c crc32c.c jmode.c smode.c
xpar_CFLAGS = @extra_cflags@
xpar_LDFLAGS = @extra_ldflags@

if XPAR_X86_64
xpar_SOURCES += xpar-x86_64.asm
SUFFIXES = .asm
.asm.o:
	$(NASM) $(NAFLAGS) -o $@ $<
endif

# Developer convenience targets
.PHONY: update-ChangeLog
update-ChangeLog:
	contrib/gitlog-to-changelog --since=1970-01-01 > ChangeLog

.PHONY: self-check
self-check:
	./xpar -Jef xpar && ./xpar -Jdf xpar.xpa xpar.org \
		&& cmp xpar xpar.org && rm xpar.org xpar.xpa
	./xpar -Jef -i 2 xpar && ./xpar -Jdf xpar.xpa xpar.org \
		&& cmp xpar xpar.org && rm xpar.org xpar.xpa

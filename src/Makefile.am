CFLAGS = -O2 -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Werror -pedantic -Wno-implicit-fallthrough -Werror=format-overflow=0

lib_LTLIBRARIES = libsparkey.la
libsparkey_la_SOURCES = endiantools.h hashheader.h logheader.h \
MurmurHash3.h buf.c hashalgorithms.c hashiter.c hashwriter.c \
logreader.c returncodes.c util.c buf.h hashalgorithms.h hashiter.h \
sparkey.h util.h endiantools.c \
hashheader.c hashreader.c logheader.c logwriter.c MurmurHash3.c \
sparkey-internal.h compress.c

pkginclude_HEADERS = sparkey.h

bin_PROGRAMS = sparkey bench
sparkey_SOURCES = main.c
bench_SOURCES = bench.c
LDADD = libsparkey.la

check_PROGRAMS = testvlq testhash testutil testsystem
TESTS = testvlq testhash testutil testsystem

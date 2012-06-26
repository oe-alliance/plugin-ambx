bin_PROGRAMS = testambx unittest grabvid
lib_LTLIBRARIES = libgrabproc.la libambx.la
TESTS = unittest

libgrabproc_includedir = $(includedir)/grabproc
libgrabproc_include_HEADERS = grabber.h colorproc.h yuvrgb.h
libambx_includedir = $(includedir)/ambx
libambx_include_HEADERS = ambxlib.h

testambx_SOURCES = testambx.c
testambx_LDADD = -lambx

unittest_SOURCES = unittest.c Fader.h Fader.c
unittest_LDADD = -lgrabproc

grabvid_SOURCES = grab.c
grabvid_LDADD = -lgrabproc

libambx_la_SOURCES = ambxlib.c ambxlib.h
libambx_la_LIBADD = -lusb

libgrabproc_la_SOURCES = grabber.h grabber_common.c grabber_@CHIPSET@.c colorproc.c colorproc.h filehelper.h yuvrgb.h yuvrgb.c filehelper.h filehelper.c

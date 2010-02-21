bin_PROGRAMS = testambx ambxgrab unittest grabvid ambx
TESTS = unittest

testambx_SOURCES = testambx.c ambxlib.c ambxlib.h
testambx_LDADD = -lusb

ambxgrab_SOURCES = ambxgrab.c ambxlib.c ambxlib.h colorproc.c colorproc.h
ambxgrab_LDADD = -lusb

unittest_SOURCES = unittest.c colorproc.c colorproc.h filehelper.h filehelper.c

grabvid_SOURCES = grab.c grabber.h grabber_@MACHINE@.c colorproc.c colorproc.h filehelper.h filehelper.c yuvrgb.h yuvrgb.c
ambx_SOURCES = ambx.c grabber.h grabber_@MACHINE@.c colorproc.c colorproc.h filehelper.h filehelper.c yuvrgb.h yuvrgb.c ambxlib.c ambxlib.h
ambx_LDADD = -lusb


bin_PROGRAMS = testambx ambxgrab unittest grab
TESTS = unittest

testambx_SOURCES = testambx.c ambxlib.c ambxlib.h
testambx_LDADD = -lusb

ambxgrab_SOURCES = ambxgrab.c ambxlib.c ambxlib.h colorproc.c colorproc.h
ambxgrab_LDADD = -lusb

unittest_SOURCES = unittest.c colorproc.c colorproc.h

grab_SOURCES = grab.cpp xilleon.cpp

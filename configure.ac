AC_PREREQ(2.52)
AC_INIT(amBX,0.0.4,nowhere,amBXlib)
AM_INIT_AUTOMAKE()

AC_ARG_WITH(machine,
        [  --with-machine=MACHINE    machine target for compilation [[dm7025,dm8000]]],
        [MACHINE="$withval" CFLAGS="$CFLAGS -DMACHINE=$withval"],[MACHINE="stub"])
AC_SUBST(MACHINE)
AC_PROG_CC
AC_PROG_CXX
AC_CHECK_HEADERS([usb.h])
AC_CONFIG_FILES(Makefile)
AC_OUTPUT

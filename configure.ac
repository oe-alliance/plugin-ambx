AC_PREREQ(2.52)
AC_INIT(amBX,0.0.4,nowhere,amBXlib)
AM_INIT_AUTOMAKE()

AC_ARG_WITH(machine,
        [  --with-machine=MACHINE    machine target for compilation [[dm7025,dm8000]]],
        [MACHINE="$withval"],[MACHINE="stub"])
AC_SUBST(MACHINE)
if test "${MACHINE}" = "dm7025"
then
  AC_DEFINE(COLOR_BGR,1,[Use BGR colorspace])
else
  AC_DEFINE(SWAP_UV,1,[UV are reversed as VU in chroma])
fi
AC_PROG_CC
AM_PROG_LIBTOOL
AC_CHECK_HEADERS([usb.h pthread.h])
AC_CONFIG_FILES(Makefile)
AC_OUTPUT

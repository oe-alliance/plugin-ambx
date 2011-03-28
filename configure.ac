AC_PREREQ(2.52)
AC_INIT(amBX,0.0.4,nowhere,amBXlib)
AM_INIT_AUTOMAKE()

AC_ARG_WITH(machine,
        [  --with-chipset=CHIPSET    chipset target for compilation [[bcm7400,bcm7405,...]]],
        [CHIPSET="$withval"],[CHIPSET="stub"])
AC_SUBST(CHIPSET)
if test "${CHIPSET}" = "ati"
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

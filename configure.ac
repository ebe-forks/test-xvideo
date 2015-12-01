AC_INIT(xvideo-tests, 0.0.1)
AC_PREREQ(2.59)

AC_CONFIG_SRCDIR(src/test-xvideo.c)
AM_INIT_AUTOMAKE()

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

AC_CHECK_HEADERS([X11/extensions/Xvlib.h],[],[AC_MSG_ERROR([Cannot find X headers])],[[#include <X11/Xlib.h>]])

ENABLE_DEBUG=no
AC_ARG_ENABLE(debug,
              [  --enable-debug enable debugging, default is 'no'],
              ENABLE_DEBUG=$enableval,
              ENABLE_DEBUG=no)
if test x$ENABLE_DEBUG = xyes ; then
    DEBUG_FLAG=__DEBUG
else
    DEBUG_FLAG=__NO_DEBUG
fi
AC_SUBST(DEBUG_FLAG)

AC_CONFIG_FILES([
Makefile
    src/Makefile
    data/Makefile
])

AC_OUTPUT()

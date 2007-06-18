dnl **************************************************************
dnl Process this file with autoconf to produce a configure script.
dnl **************************************************************
AC_INIT(xvideo-tests, 0.0.1)
AC_PREREQ(2.59)

AC_CONFIG_SRCDIR(README)
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_TARGET
AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

MAJOR_VERSION=0
MINOR_VERSION=0
MICRO_VERSION=1

SGALIB_VERSION="$MAJOR_VERSION.$MINOR_VERSION.$MICRO_VERSION"
AC_SUBST(SGALIB_VERSION)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

AC_CHECK_HEADERS([X11/Xlib.h X11/extensions/Xvlib.h])
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

echo  ===================================
echo          configuration of this package:
echo          host            :    $host
echo          target          :    $target
echo          CC              :    $CC
echo          CFLAGS          :    $CFLAGS
echo          KERNEL_HEADERS_DIR  :    $KERNEL_HEADERS_DIR
echo          enable debug    :    $ENABLE_DEBUG
echo          prefix          :    $prefix
echo ""
echo    Now, type 'make' to compile, and 'make install' to install
echo  ===================================

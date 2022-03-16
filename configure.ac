#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([apns], [1.0.0.1], [gcarter@openaprs.net])
AC_CONFIG_SRCDIR([src/ApnsAbstract.cpp])
AC_CONFIG_HEADERS([include/apns/config.h])
AM_INIT_AUTOMAKE
LT_INIT
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_LANG([C++])

prefix="/usr/local"
#bindir="`pwd`"
#libdir="`pwd`/lib"
#includedir="`pwd`/include"

ofincludedir="/usr/local/include/apns"
oflibdir="/usr/local/lib"

local_include="`pwd`/include/apns"

CPPFLAGS="$CPPFLAGS -I/usr/include -I$local_include"
CXXFLAGS="-Wall -pipe -g -I$includedir -I$ofincludedir -I$local_include"
CXX="g++"
LIBS="$LIBS"

# Checks for libraries.

AC_CHECK_LIB([pthread], [pthread_create], [], [
               echo "pthread library is required for this program"
               exit 1
             ])

AC_CHECK_LIB(ssl, SSL_library_init)

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_SUBST(bindir)
AC_SUBST(LIBTOOL_DEPS)

LIBS="$LIBS -L`pwd`/lib $DEPS_LIBS -L$oflibdir -lopenframe"

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/apns/Makefile
                 test/Makefile
               ])
AC_OUTPUT

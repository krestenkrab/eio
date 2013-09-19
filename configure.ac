# this is example-file: configure.ac

# initial information about the project
AC_INIT([eio],[0.1],[krab@trifork.com])

# check if the source folder is correct
AC_CONFIG_SRCDIR([src/myClass.cpp])

##
##
##   MANDATORY for everything
##
##
LIBEVENT_MINIMUM=2.0.10
AC_SUBST(LIBEVENT_MINIUM)
OPENSSL_MINIMUM=0.9.4
AC_SUBST(OPENSSL_MINIMUM)

# Checks for programs

# check for C++ compiler and the library compiler
AC_PROG_CXX
AC_PROG_RANLIB

# automake initialisation (mandatory) and check for minimal automake API version 1.9
AM_INIT_AUTOMAKE([1.9 foreign])

# use the C++ compiler for the following checks
AC_LANG([C++])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string])
AC_CHECK_HEADERS([iostream])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

PKG_CHECK_MODULES(OPENSSL, [openssl >= $OPENSSL_MINIMUM])
PKG_CHECK_MODULES(LIBEVENT, [libevent >= $LIBEVENT_MINIMUM])

# distribute additional compiler and linker flags
# --> set these variables instead of CXXFLAGS or LDFLAGS
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])
AC_SUBST([LIBS])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])

# generate the final Makefile etc.
AC_OUTPUT

AC_PREREQ([2.69])
AC_INIT([idiom], [0.1], [support@bitptr.org])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_CHECK_FUNCS([strlcpy reallocarray])
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([JSON_GLIB], [json-glib-1.0])
PKG_CHECK_MODULES([GTK], [gtk+-3.0])
AC_CONFIG_FILES([Makefile src/Makefile share/idiom/Makefile man/Makefile])
AC_OUTPUT

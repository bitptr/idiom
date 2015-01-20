AC_PREREQ([2.69])
AC_INIT([idiom], [0.1], [support@bitptr.org])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_CHECK_FUNCS([strlcpy reallocarray])
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([JSON_GLIB], [json-glib-1.0])
PKG_CHECK_MODULES([GTK], [gtk+-3.0])
AC_CONFIG_FILES([
                 Makefile
                 src/Makefile
                 share/idiom/Makefile
                 man/Makefile
                 data/Makefile
                 data/icons/Makefile
                 data/icons/hicolor/16x16/Makefile
                 data/icons/hicolor/16x16/apps/Makefile
                 data/icons/hicolor/22x22/Makefile
                 data/icons/hicolor/22x22/apps/Makefile
                 data/icons/hicolor/24x24/Makefile
                 data/icons/hicolor/24x24/apps/Makefile
                 data/icons/hicolor/256x256/Makefile
                 data/icons/hicolor/256x256/apps/Makefile
                 data/icons/hicolor/32x32/Makefile
                 data/icons/hicolor/32x32/apps/Makefile
                 data/icons/hicolor/48x48/Makefile
                 data/icons/hicolor/48x48/apps/Makefile
                 data/icons/hicolor/8x8/Makefile
                 data/icons/hicolor/8x8/apps/Makefile
                 data/icons/hicolor/Makefile
                 data/icons/hicolor/scalable/Makefile
                 data/icons/hicolor/scalable/apps/Makefile
                 ])
AC_OUTPUT

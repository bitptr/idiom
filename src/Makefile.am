AM_CPPFLAGS = -DPKGDATADIR=\"$(pkgdatadir)\" $(GTK_CFLAGS) $(CURL_CFLAGS) $(JSON_GLIB_CFLAGS)
AM_LDFLAGS = $(GTK_LIBS) $(CURL_LIBS) $(JSON_GLIB_LIBS)
AM_CFLAGS = -Wall -Wextra -pedantic-errors -Wno-unused-parameter -Werror
bin_PROGRAMS = idiom
dist_idiom_SOURCES = main.c pathnames.h compat.h
